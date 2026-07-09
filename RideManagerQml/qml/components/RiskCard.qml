import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: card
    property string riskLevel: "Normal"
    property string decisionId: ""
    property bool compact: false
    property color accentColor: riskLevel === "Danger" ? "#ff4358"
                               : riskLevel === "Warning" ? "#ffcc45"
                               : "#31d07f"
    property real severity: riskLevel === "Danger" ? 0.94
                            : riskLevel === "Warning" ? 0.62
                            : 0.28

    radius: 8
    color: "#0c1a2b"
    border.width: 1
    border.color: Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.66)
    clip: true

    Behavior on accentColor {
        ColorAnimation { duration: 260; easing.type: Easing.OutCubic }
    }

    gradient: Gradient {
        GradientStop { position: 0.0; color: "#132742" }
        GradientStop { position: 1.0; color: "#07101d" }
    }

    onRiskLevelChanged: pulse.restart()

    SequentialAnimation {
        id: pulse
        NumberAnimation { target: card; property: "scale"; to: 1.025; duration: 120; easing.type: Easing.OutCubic }
        NumberAnimation { target: card; property: "scale"; to: 1.0; duration: 220; easing.type: Easing.OutCubic }
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: 1
        radius: card.radius - 1
        color: "transparent"
        border.width: 1
        border.color: Qt.rgba(card.accentColor.r, card.accentColor.g, card.accentColor.b, 0.14)
    }

    Canvas {
        id: gauge
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.rightMargin: compact ? 10 : 24
        width: compact ? 86 : 190
        height: width

        onPaint: {
            var ctx = getContext("2d")
            var cx = width / 2
            var cy = height / 2
            var radius = width * 0.36
            ctx.clearRect(0, 0, width, height)
            ctx.lineCap = "round"
            ctx.lineWidth = compact ? 9 : 13
            ctx.strokeStyle = "rgba(95, 132, 166, 0.20)"
            ctx.beginPath()
            ctx.arc(cx, cy, radius, Math.PI * 0.78, Math.PI * 2.22)
            ctx.stroke()
            ctx.strokeStyle = card.accentColor
            ctx.beginPath()
            ctx.arc(cx, cy, radius, Math.PI * 0.78, Math.PI * (0.78 + 1.44 * card.severity))
            ctx.stroke()
        }

        Connections {
            target: card
            function onAccentColorChanged() { gauge.requestPaint() }
            function onSeverityChanged() { gauge.requestPaint() }
        }
    }

    ColumnLayout {
        anchors.left: parent.left
        anchors.right: gauge.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.leftMargin: compact ? 16 : 28
        anchors.rightMargin: compact ? 10 : 28
        anchors.topMargin: compact ? 16 : 28
        anchors.bottomMargin: compact ? 14 : 28
        spacing: compact ? 6 : 12

        Label {
            text: "当前风险等级"
            color: "#8fb2d0"
            font.pixelSize: 14
        }

        Text {
            text: card.riskLevel
            color: card.accentColor
            font.pixelSize: compact ? 34 : 54
            font.weight: Font.Black
            elide: Text.ElideRight
            Layout.fillWidth: true
        }

        Text {
            Layout.fillWidth: true
            text: card.riskLevel === "Danger"
                  ? "危险：建议立即触发声光/制动预案"
                  : card.riskLevel === "Warning"
                    ? "警告：保持跟踪并提高驾驶员提示强度"
                    : "正常：感知链路稳定，未检测到紧急风险"
            color: "#d7e7f8"
            font.pixelSize: compact ? 13 : 16
            wrapMode: Text.WordWrap
        }

        Item { Layout.fillHeight: true }

        Text {
            Layout.fillWidth: true
            text: card.decisionId.length > 0 ? ("Decision " + card.decisionId) : "Decision --"
            color: "#6788a8"
            elide: Text.ElideMiddle
            font.pixelSize: compact ? 10 : 12
        }
    }
}
