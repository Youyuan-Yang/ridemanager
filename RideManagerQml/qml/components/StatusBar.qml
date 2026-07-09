import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    property string databaseStatus: ""
    property string databaseEndpoint: ""
    property string lastRefreshTime: "--"
    property bool busy: false
    readonly property bool hasError: databaseStatus.indexOf("错误") >= 0
                                     || databaseStatus.indexOf("失败") >= 0
                                     || databaseStatus.indexOf("无法") >= 0

    color: "#06101a"
    border.width: 1
    border.color: "#17324d"

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 18
        anchors.rightMargin: 18
        spacing: 14

        Rectangle {
            Layout.preferredWidth: 9
            Layout.preferredHeight: 9
            radius: 5
            color: root.hasError ? "#ff5c70" : (root.busy ? "#ffd35a" : "#31d07f")
            opacity: root.busy ? 0.45 : 1.0

            SequentialAnimation on opacity {
                running: root.busy
                loops: Animation.Infinite
                NumberAnimation { to: 0.25; duration: 460 }
                NumberAnimation { to: 1.0; duration: 460 }
            }
        }

        Text {
            Layout.fillWidth: true
            text: (root.databaseEndpoint.length > 0
                   ? (root.databaseStatus + " | " + root.databaseEndpoint)
                   : root.databaseStatus).replace(/\s+/g, " ")
            color: root.hasError ? "#ffb3bd" : "#cfe3f6"
            font.pixelSize: 13
            elide: Text.ElideRight
            wrapMode: Text.NoWrap
            verticalAlignment: Text.AlignVCenter
        }

        BusyIndicator {
            Layout.preferredWidth: 24
            Layout.preferredHeight: 24
            running: root.busy
            visible: true
            opacity: root.busy ? 1.0 : 0.0
        }

        Text {
            text: "最近刷新 " + root.lastRefreshTime
            color: "#7f9fbf"
            font.pixelSize: 13
        }
    }
}
