# Applies the GitHub settings BUGS! Video Editor uses as a public repository.
# Needs the GitHub CLI signed in as a repository admin (the Windows build one-liner sets that up).
# Safe to run again: every step sets a value rather than adding one.
#
# Run in PowerShell:
#   & "$env:ProgramFiles\GitHub CLI\gh.exe" api -H "Accept: application/vnd.github.raw" "repos/YosefDM/Bugs-Video-Editor/contents/scripts/bugs/configure-github.ps1?ref=main" | Out-String | iex
#
# It does not make the repository public; that stays your decision. Run it once more after making
# it public, because secret scanning only becomes available then.

$ErrorActionPreference = 'Continue'
$Repo = 'YosefDM/Bugs-Video-Editor'
$Gh = 'gh'
if ($env:ProgramFiles -and (Test-Path (Join-Path $env:ProgramFiles 'GitHub CLI\gh.exe'))) {
    $Gh = Join-Path $env:ProgramFiles 'GitHub CLI\gh.exe'
}
$Failed = 0

function Step($name, [scriptblock]$action) {
    $output = & $action 2>&1 | Out-String
    if ($LASTEXITCODE -eq 0) {
        Write-Host "  [ok]   $name" -ForegroundColor Green
    } else {
        $script:Failed++
        Write-Host "  [fail] $name" -ForegroundColor Red
        Write-Host ("         " + ($output.Trim() -split "`n" | Select-Object -First 2) -join ' ')
    }
}

function Api($method, $path, $json) {
    # An empty path means the repository itself, with no trailing slash.
    $path = if ($path) { "/$path" } else { '' }
    if ($json) {
        $file = New-TemporaryFile
        [IO.File]::WriteAllText($file, $json)
        & $Gh api -X $method -H 'Accept: application/vnd.github+json' "repos/$Repo$path" --input $file
        $code = $LASTEXITCODE
        Remove-Item $file
        $global:LASTEXITCODE = $code
    } else {
        & $Gh api -X $method -H 'Accept: application/vnd.github+json' "repos/$Repo$path"
    }
}

# Replaces a ruleset with the same name, so running this again updates it.
function Ruleset($json) {
    $name = ($json | ConvertFrom-Json).name
    $existing = (& $Gh api "repos/$Repo/rulesets" | ConvertFrom-Json) | Where-Object { $_.name -eq $name }
    if ($existing) {
        Api PUT "rulesets/$($existing.id)" $json
    } else {
        Api POST 'rulesets' $json
    }
}

Write-Host "`nGeneral" -ForegroundColor Cyan
Step 'Description, topics, merge settings' {
    & $Gh repo edit $Repo `
        --description 'A simple, friendly video and photo editor for Windows. Trim, cut, crop, caption, label and make collages. Works offline and never changes your originals.' `
        --add-topic video-editor --add-topic photo-editor --add-topic collage --add-topic windows `
        --add-topic offline --add-topic qt --add-topic mlt --add-topic ffmpeg --add-topic beginner-friendly `
        --enable-issues --enable-wiki=false --enable-projects=false --enable-discussions=false `
        --enable-merge-commit --enable-squash-merge=false --enable-rebase-merge=false `
        --delete-branch-on-merge --allow-update-branch
}

Write-Host "`nProtecting main and release tags" -ForegroundColor Cyan
# Changes reach main through pull requests that pass CI. GitHub Actions is integration 15368.
Step 'main: pull requests with passing CI, no force pushes or deletion' {
    Ruleset @'
{
  "name": "Protect main",
  "target": "branch",
  "enforcement": "active",
  "conditions": { "ref_name": { "include": ["~DEFAULT_BRANCH"], "exclude": [] } },
  "rules": [
    { "type": "deletion" },
    { "type": "non_fast_forward" },
    { "type": "pull_request", "parameters": {
        "required_approving_review_count": 0,
        "dismiss_stale_reviews_on_push": false,
        "require_code_owner_review": false,
        "require_last_push_approval": false,
        "required_review_thread_resolution": false,
        "allowed_merge_methods": ["merge"] } },
    { "type": "required_status_checks", "parameters": {
        "strict_required_status_checks_policy": false,
        "required_status_checks": [
          { "context": "format", "integration_id": 15368 },
          { "context": "build-and-test", "integration_id": 15368 } ] } }
  ]
}
'@
}
# A released version must never change under someone who downloaded it.
Step 'Release tags (v1.2.3) cannot be moved or deleted' {
    Ruleset @'
{
  "name": "Release tags are permanent",
  "target": "tag",
  "enforcement": "active",
  "conditions": { "ref_name": { "include": ["refs/tags/v*"], "exclude": [] } },
  "rules": [ { "type": "deletion" }, { "type": "non_fast_forward" }, { "type": "update" } ]
}
'@
}

Write-Host "`nGitHub Actions" -ForegroundColor Cyan
# Only GitHub's own actions and the MSYS2 setup the Windows build uses.
Step 'Only GitHub-made actions and msys2/setup-msys2 may run' {
    Api PUT 'actions/permissions' '{ "enabled": true, "allowed_actions": "selected" }'
    if ($LASTEXITCODE -eq 0) {
        Api PUT 'actions/permissions/selected-actions' '{ "github_owned_allowed": true, "verified_allowed": false, "patterns_allowed": ["msys2/setup-msys2@*"] }'
    }
}
Step 'Workflows get read-only access unless they ask for more' {
    Api PUT 'actions/permissions/workflow' '{ "default_workflow_permissions": "read", "can_approve_pull_request_reviews": false }'
}
Step 'Pull requests from outside contributors wait for your approval before CI runs (public only)' {
    Api PUT 'actions/permissions/fork-pr-contributor-approval' '{ "approval_policy": "all_external_contributors" }'
}

Write-Host "`nSecurity" -ForegroundColor Cyan
Step 'Private vulnerability reporting, the Security tab form (public only)' { Api PUT 'private-vulnerability-reporting' }
Step 'Dependabot alerts' { Api PUT 'vulnerability-alerts' }
Step 'Dependabot security fixes' { Api PUT 'automated-security-fixes' }
Step 'Secret scanning and push protection (public only)' {
    Api PATCH '' '{ "security_and_analysis": { "secret_scanning": { "status": "enabled" }, "secret_scanning_push_protection": { "status": "enabled" } } }'
}

if ($Failed -eq 0) {
    Write-Host "`nAll settings applied." -ForegroundColor Green
} else {
    Write-Host "`n$Failed step(s) did not apply. Steps marked (public only) fail while the repository is private; run this again after making it public." -ForegroundColor Yellow
}
