import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    property int currentIndex: 0
    property bool compact: false
    signal pageSelected(int index)

    color: "#06101a"

    readonly property var pages: [
        "安全决策",
        "摄像头检测",
        "传感器数据",
        "执行器命令",
        "系统日志"
    ]

    Rectangle {
        anchors.right: parent.right
        width: 1
        height: parent.height
        color: "#19314a"
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: root.compact ? 10 : 18
        spacing: root.compact ? 10 : 18

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            Text {
                text: "RideManager"
                color: "#eef7ff"
                font.pixelSize: root.compact ? 17 : 22
                font.weight: Font.DemiBold
            }

            Text {
                text: "SAFETY COCKPIT"
                color: "#52a8ff"
                font.pixelSize: root.compact ? 9 : 11
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8

            Repeater {
                model: root.pages

                delegate: Rectangle {
                    id: navItem
                    required property int index
                    required property string modelData

                    Layout.fillWidth: true
                    height: root.compact ? 38 : 46
                    radius: 8
                    color: root.currentIndex === index ? "#112b47" : "transparent"
                    border.width: root.currentIndex === index ? 1 : 0
                    border.color: "#2f83cf"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 14
                        anchors.rightMargin: root.compact ? 6 : 10
                        spacing: root.compact ? 6 : 10

                        Rectangle {
                            width: 4
                            height: root.compact ? 18 : 22
                            radius: 2
                            color: root.currentIndex === index ? "#51b3ff" : "#31475f"
                        }

                        Text {
                            text: "0" + (index + 1)
                            color: root.currentIndex === index ? "#91d4ff" : "#56738f"
                            font.pixelSize: root.compact ? 10 : 11
                            font.weight: Font.DemiBold
                        }

                        Text {
                            Layout.fillWidth: true
                            text: modelData
                            color: root.currentIndex === index ? "#eef7ff" : "#9bb2c8"
                            font.pixelSize: root.compact ? 13 : 15
                            elide: Text.ElideRight
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.pageSelected(index)
                    }
                }
            }
        }

        Item { Layout.fillHeight: true }

        Rectangle {
            Layout.fillWidth: true
            height: root.compact ? 58 : 82
            radius: 8
            color: "#0b1b2d"
            border.width: 1
            border.color: "#1f3d5e"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: root.compact ? 8 : 12
                spacing: root.compact ? 2 : 5

                Text {
                    text: "Runtime"
                    color: "#7398b8"
                    font.pixelSize: root.compact ? 10 : 12
                }

                Text {
                    text: "Qt 6.4+ / C++ / QML"
                    color: "#e6f3ff"
                    font.pixelSize: root.compact ? 11 : 13
                    elide: Text.ElideRight
                }

                Text {
                    text: "PostgreSQL Data Link"
                    color: "#55c7ff"
                    font.pixelSize: root.compact ? 10 : 12
                }
            }
        }
    }
}
