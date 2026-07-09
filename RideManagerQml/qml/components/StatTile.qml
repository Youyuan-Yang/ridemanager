import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    property string title: ""
    property string value: "--"
    property string subtitle: ""
    property color accentColor: "#52a8ff"

    radius: 8
    color: "#0b1d2f"
    border.width: 1
    border.color: "#214263"

    RowLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 10

        Rectangle {
            Layout.preferredWidth: 4
            Layout.fillHeight: true
            radius: 2
            color: root.accentColor
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            Text {
                text: root.title
                color: "#9fc2e2"
                font.pixelSize: 12
                elide: Text.ElideRight
            }

            Text {
                text: root.value
                color: "#f4fbff"
                font.pixelSize: 22
                font.weight: Font.Black
                elide: Text.ElideRight
            }

            Text {
                text: root.subtitle
                color: "#7f9fbd"
                font.pixelSize: 11
                elide: Text.ElideRight
            }
        }
    }
}
