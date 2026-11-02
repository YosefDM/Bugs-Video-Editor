import QtQuick
import "Theme.js" as Theme

Canvas {
    id: root

    property string name
    property color color: "white"
    property real strokeWidth: 1.8
    property bool filled: false
    property int size: 20

    width: size
    height: size
    antialiasing: true
    onColorChanged: requestPaint()
    onNameChanged: requestPaint()
    onSizeChanged: requestPaint()
    onPaint: {
        var ctx = getContext("2d");
        ctx.reset();
        ctx.scale(width / 24, height / 24);
        ctx.lineWidth = strokeWidth;
        ctx.lineCap = "round";
        ctx.lineJoin = "round";
        ctx.strokeStyle = color;
        ctx.fillStyle = color;
        ctx.path = Theme.icons[name] || "";
        if (filled)
            ctx.fill();
        else
            ctx.stroke();
    }
}
