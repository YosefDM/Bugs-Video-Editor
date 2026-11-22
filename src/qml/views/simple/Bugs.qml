import QtQuick

// Real bugs crawling over the window: photographs, moved the way bugs move. They run in bursts,
// stop, turn, and sway a little with each stride. Now and then a spider leaps out at the screen,
// freezes for a moment, then runs off. Clicks pass straight through all of this.
Item {
    id: root

    // Each photo faces up. Length is on screen, in pixels; speed in pixels a second.
    readonly property var kinds: [
        { image: "bugs/ant.png", length: 54, speed: 150, sway: 5, pauses: 0.5, weight: 5 },
        { image: "bugs/cockroach.png", length: 74, speed: 260, sway: 3, pauses: 0.35, weight: 2 },
        { image: "bugs/beetle.png", length: 46, speed: 55, sway: 2, pauses: 0.25, weight: 2 }
    ]
    readonly property int maxCrawlers: 12
    // About one bug for every 120,000 square pixels: 9 or so on a laptop screen.
    readonly property int wanted: Math.max(4, Math.min(maxCrawlers, Math.round(width * height / 120000)))
    property real nextSpider: 0

    function pickKind() {
        var total = 0;
        for (var i = 0; i < kinds.length; ++i)
            total += kinds[i].weight;
        var r = Math.random() * total;
        for (var j = 0; j < kinds.length; ++j) {
            r -= kinds[j].weight;
            if (r < 0)
                return j;
        }
        return 0;
    }

    function between(a, b) {
        return a + Math.random() * (b - a);
    }

    // Degrees, 0 up, clockwise: the way the photos face when they are not turned.
    function headingTo(x0, y0, x1, y1) {
        return Math.atan2(x1 - x0, -(y1 - y0)) * 180 / Math.PI;
    }

    function wrap(angle) {
        while (angle > 180)
            angle -= 360;
        while (angle < -180)
            angle += 360;
        return angle;
    }

    // Starts a bug just outside an edge, heading in.
    function spawn(bug) {
        bug.kind = pickKind();
        var k = kinds[bug.kind];
        var size = k.length * between(0.8, 1.2);
        bug.length = size;
        var margin = size;
        var edge = Math.floor(Math.random() * 4);
        var x, y;
        if (edge === 0) { x = between(0, width); y = -margin; }
        else if (edge === 1) { x = width + margin; y = between(0, height); }
        else if (edge === 2) { x = between(0, width); y = height + margin; }
        else { x = -margin; y = between(0, height); }
        bug.cx = x;
        bug.cy = y;
        bug.heading = headingTo(x, y, between(width * 0.2, width * 0.8), between(height * 0.2, height * 0.8));
        bug.turnRate = 0;
        bug.cruise = k.speed * between(0.75, 1.3);
        bug.speedNow = bug.cruise * 0.6;
        bug.pauseLeft = 0;
        bug.runLeft = between(0.6, 3.0);
        bug.inside = false;
        bug.waitLeft = 0;
        bug.alive = true;
    }

    function stepCrawler(bug, dt) {
        if (!bug.alive) {
            bug.waitLeft -= dt;
            if (bug.waitLeft <= 0 && bug.index < wanted)
                spawn(bug);
            return;
        }
        var k = kinds[bug.kind];
        if (bug.pauseLeft > 0) {
            // Standing still, with now and then a small twitch.
            bug.pauseLeft -= dt;
            bug.speedNow = 0;
            if (Math.random() < dt * 3)
                bug.heading += between(-6, 6);
            if (bug.pauseLeft <= 0) {
                bug.runLeft = between(0.5, 3.5);
                // Often off in a new direction after a stop.
                if (Math.random() < 0.5)
                    bug.heading += between(-110, 110);
            }
        } else {
            bug.runLeft -= dt;
            if (bug.runLeft <= 0 && Math.random() < k.pauses) {
                bug.pauseLeft = between(0.15, 1.4);
            } else if (bug.runLeft <= 0) {
                bug.runLeft = between(0.5, 3.0);
                bug.cruise = k.speed * between(0.7, 1.4);
            }
            // Wandering: the turn rate drifts, so the path curves and wiggles.
            bug.turnRate += between(-1, 1) * 900 * dt;
            bug.turnRate *= Math.pow(0.08, dt);
            bug.turnRate = Math.max(-220, Math.min(220, bug.turnRate));
            // Once inside, it keeps away from the edges for a while before it wanders off.
            if (bug.inside && bug.runLeft > 0.3) {
                var pad = bug.length * 1.5;
                var toward = null;
                if (bug.cx < pad || bug.cx > width - pad || bug.cy < pad || bug.cy > height - pad)
                    toward = headingTo(bug.cx, bug.cy, width / 2, height / 2);
                if (toward !== null && Math.random() < 0.6)
                    bug.turnRate += wrap(toward - bug.heading) * 2.5 * dt * 10;
            }
            bug.heading = wrap(bug.heading + bug.turnRate * dt);
            bug.speedNow += (bug.cruise - bug.speedNow) * Math.min(1, dt * 6);
            var rad = bug.heading * Math.PI / 180;
            var dist = bug.speedNow * dt;
            bug.cx += Math.sin(rad) * dist;
            bug.cy -= Math.cos(rad) * dist;
            bug.stride += dist / (bug.length * 0.18);
        }
        var m = bug.length * 1.5;
        var onScreen = bug.cx > -m && bug.cx < width + m && bug.cy > -m && bug.cy < height + m;
        if (bug.cx > 0 && bug.cx < width && bug.cy > 0 && bug.cy < height)
            bug.inside = true;
        if (bug.inside && !onScreen) {
            bug.alive = false;
            bug.waitLeft = between(0.3, 4);
        }
        // Never wander off for ever before coming in.
        if (!bug.inside && !onScreen && bug.runLeft < -10)
            spawn(bug);
    }

    // The spider: leaps in from an edge, freezes, then runs away.
    function startSpider() {
        var s = spider;
        var side = Math.min(width, height);
        s.length = side * between(0.45, 0.6);
        var edge = Math.floor(Math.random() * 4);
        var tx, ty;
        if (edge === 0) { tx = between(width * 0.25, width * 0.75); ty = height * between(0.25, 0.4); s.fromX = tx + between(-80, 80); s.fromY = -s.length * 0.5; }
        else if (edge === 1) { tx = width * between(0.6, 0.75); ty = between(height * 0.3, height * 0.7); s.fromX = width + s.length * 0.5; s.fromY = ty + between(-80, 80); }
        else if (edge === 2) { tx = between(width * 0.25, width * 0.75); ty = height * between(0.6, 0.75); s.fromX = tx + between(-80, 80); s.fromY = height + s.length * 0.5; }
        else { tx = width * between(0.25, 0.4); ty = between(height * 0.3, height * 0.7); s.fromX = -s.length * 0.5; s.fromY = ty + between(-80, 80); }
        s.toX = tx;
        s.toY = ty;
        s.cx = s.fromX;
        s.cy = s.fromY;
        s.heading = headingTo(s.fromX, s.fromY, tx, ty);
        s.phase = "leap";
        s.clock = 0;
        s.holdFor = between(0.5, 1.0);
        s.alive = true;
    }

    function stepSpider(dt) {
        var s = spider;
        if (!s.alive) {
            nextSpider -= dt;
            if (nextSpider <= 0) {
                startSpider();
                nextSpider = bugs.quickSpiders ? between(6, 10) : between(45, 150);
            }
            return;
        }
        s.clock += dt;
        if (s.phase === "leap") {
            // A quick leap with a little overshoot, growing as it comes at the screen.
            var t = Math.min(1, s.clock / 0.26);
            var c = 1.8;
            var e = 1 + (c + 1) * Math.pow(t - 1, 3) + c * Math.pow(t - 1, 2);
            s.cx = s.fromX + (s.toX - s.fromX) * e;
            s.cy = s.fromY + (s.toY - s.fromY) * e;
            s.grow = 0.55 + 0.45 * e;
            if (t >= 1) {
                s.phase = "hold";
                s.clock = 0;
            }
        } else if (s.phase === "hold") {
            // Frozen, with a twitch now and then.
            s.grow = 1;
            if (Math.random() < dt * 8)
                s.heading += between(-4, 4);
            if (s.clock >= s.holdFor) {
                // Turns to the nearest edge and runs.
                var away = [
                    { d: s.cy, h: 0 }, { d: width - s.cx, h: 90 },
                    { d: height - s.cy, h: 180 }, { d: s.cx, h: -90 }
                ].sort(function(a, b) { return a.d - b.d; })[0];
                s.runHeading = away.h + between(-25, 25);
                s.startHeading = s.heading;
                s.phase = "turn";
                s.clock = 0;
            }
        } else if (s.phase === "turn") {
            var u = Math.min(1, s.clock / 0.14);
            s.heading = s.startHeading + wrap(s.runHeading - s.startHeading) * u;
            if (u >= 1) {
                s.phase = "run";
                s.clock = 0;
            }
        } else if (s.phase === "run") {
            var speed = Math.max(width, height) * 1.1;
            var r = s.heading * Math.PI / 180;
            s.cx += Math.sin(r) * speed * dt;
            s.cy -= Math.cos(r) * speed * dt;
            s.stride += speed * dt / (s.length * 0.12);
            var m = s.length;
            if (s.cx < -m || s.cx > width + m || s.cy < -m || s.cy > height + m)
                s.alive = false;
        }
    }

    onVisibleChanged: {
        if (visible) {
            for (var i = 0; i < crawlers.count; ++i) {
                var bug = crawlers.itemAt(i);
                bug.alive = false;
                bug.waitLeft = between(0, 2.5);
            }
            spider.alive = false;
            nextSpider = bugs.quickSpiders ? 3 : between(15, 40);
        }
    }

    FrameAnimation {
        running: root.visible && root.width > 0
        onTriggered: {
            // Long gaps, such as after the laptop sleeps, are taken as one ordinary step.
            var dt = Math.min(frameTime, 0.05);
            for (var i = 0; i < crawlers.count; ++i)
                root.stepCrawler(crawlers.itemAt(i), dt);
            root.stepSpider(dt);
        }
    }

    Repeater {
        id: crawlers

        model: root.maxCrawlers

        delegate: Image {
            required property int index

            property int kind: 0
            property bool alive: false
            property real length: 50
            property real cx: -1000
            property real cy: -1000
            property real heading: 0
            property real turnRate: 0
            property real cruise: 100
            property real speedNow: 0
            property real pauseLeft: 0
            property real runLeft: 0
            property real waitLeft: 0
            property real stride: 0
            property bool inside: false

            visible: alive
            source: root.kinds[kind].image
            // The photos are a little bigger than the bug, for its shadow.
            width: length * 1.25
            height: width
            sourceSize.width: 192
            fillMode: Image.PreserveAspectFit
            smooth: true
            mipmap: true
            x: cx - width / 2
            y: cy - height / 2
            // Its body sways a little with each stride, as a walking insect's does.
            rotation: heading + (speedNow > 1 ? Math.sin(stride) * root.kinds[kind].sway : 0)
        }
    }

    Image {
        id: spider

        property bool alive: false
        property real length: 300
        property real cx: -2000
        property real cy: -2000
        property real fromX: 0
        property real fromY: 0
        property real toX: 0
        property real toY: 0
        property real heading: 0
        property real startHeading: 0
        property real runHeading: 0
        property real grow: 1
        property real stride: 0
        property real clock: 0
        property real holdFor: 0.7
        property string phase: ""

        visible: alive
        z: 10
        source: "bugs/spider.png"
        width: length * grow
        height: width
        sourceSize.width: 900
        fillMode: Image.PreserveAspectFit
        smooth: true
        mipmap: true
        x: cx - width / 2
        y: cy - height / 2
        rotation: heading + (phase === "run" ? Math.sin(stride) * 3 : 0)
    }
}
