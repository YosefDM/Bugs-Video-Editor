# Collage stickers

Still Noto Emoji images (2D, 512 px) from https://github.com/googlefonts/noto-emoji, copyright
Google LLC, reduced to 256 colours to keep the set small. The Israel flag is Noto's
`third_party/region-flags/png/IL.png`. The project's README says its image resources are under
the Apache License 2.0 and its fonts under the SIL Open Font License 1.1, but its top-level
LICENSE file is the OFL, so both texts are included here.

`stickers.json` is the **allow-list**. BUGS! shows only the stickers listed there, in that order,
whatever else is in this folder or in Shotcut's `elements/`.

The list has the standard yellow version of every Unicode 18.0 emoji (no skin-tone repeats), minus
anything inappropriate or not tznius. Left out:

- immodest clothing and figures, such as the bikini, underwear, showgirls, bath and sauna,
  mermaids, breastfeeding, the ballet and flamenco dancers, and women with bare arms or in
  swimsuits
- crude gestures (the middle finger)
- romance: kissing couples and couples with a heart
- people and relationships against the Torah: same-sex couples and families, pride and
  transgender flags, pregnant man, cross-dressing
- other religions' worship symbols and holidays: church, cross, mosque, temples, Santa,
  Christmas tree, prayer beads
- the evil-eye bead

Waiting for the owner's decision, so left out until then: country flags (only Israel is in),
jack-o-lantern, the devil faces, crystal ball, mage and yin yang.

Every figure of a person, and all clothing, was checked by eye. Anything new goes through the same
check. To add one, copy its PNG from `2D/png/512/` here as `<code points>.png`, then add it to
`stickers.json`.
