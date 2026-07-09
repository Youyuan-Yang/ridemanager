import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    property string title: ""
    property var tableModel: null
    property var rows: []
    property var columns: []
    property string selectedRole: ""
    property var selectedValue: undefined
    property string emptyText: "暂无数据"
    signal rowClicked(var row)

    radius: 8
    color: "#0b1929"
    border.width: 1
    border.color: "#1f3f61"
    clip: true

    function totalColumnUnits() {
        var total = 0
        for (var i = 0; i < columns.length; ++i) {
            total += columns[i].width || 1
        }
        return Math.max(total, 1)
    }

    function columnWidth(column) {
        return Math.max(74, (contentWidth() * (column.width || 1)) / totalColumnUnits())
    }

    function contentWidth() {
        return Math.max(root.width - 28, 1)
    }

    function rowCount() {
        if (tableModel && tableModel.count !== undefined) {
            return tableModel.count
        }
        return rows ? rows.length : 0
    }

    function rowAt(index) {
        if (tableModel && tableModel.get) {
            return tableModel.get(index)
        }
        return rows && index >= 0 && index < rows.length ? rows[index] : ({})
    }

    function displayValue(row, column) {
        var value = row[column.role]
        if (value === undefined || value === null || value === "") {
            return "--"
        }
        if (column.format === "percent") {
            return Math.round(Number(value) * 100) + "%"
        }
        if (column.format === "number") {
            var precision = column.precision === undefined ? 1 : column.precision
            return Number(value).toFixed(precision)
        }
        return String(value)
    }

    function statusColor(value) {
        var text = String(value).toLowerCase()
        if (text === "danger" || text === "failed" || text === "error") {
            return "#ff6475"
        }
        if (text === "warning" || text === "pending") {
            return "#ffd45a"
        }
        if (text === "normal" || text === "completed" || text === "info") {
            return "#56e39a"
        }
        if (text === "debug") {
            return "#67c7ff"
        }
        return ""
    }

    function cellColor(row, column, selected) {
        if (selected) {
            return "#ffffff"
        }
        var value = row[column.role]
        if (column.role === "riskLevel" || column.role === "status" || column.role === "level") {
            var stateColor = statusColor(value)
            if (stateColor.length > 0) {
                return stateColor
            }
        }
        if (column.role === "confidence") {
            var confidence = Number(value)
            if (confidence >= 0.85) {
                return "#56e39a"
            }
            if (confidence >= 0.7) {
                return "#ffd45a"
            }
            return "#ff8a96"
        }
        return "#dcecff"
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 26

            Text {
                Layout.fillWidth: true
                text: root.title
                color: "#eef7ff"
                font.pixelSize: 16
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }

            Text {
                text: root.rowCount() + " 条"
                color: "#8fc9ff"
                font.pixelSize: 12
                font.weight: Font.DemiBold
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 34
            radius: 6
            color: "#12304c"
            border.width: 1
            border.color: "#244d70"

            Row {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10

                Repeater {
                    model: root.columns

                    delegate: Text {
                        required property var modelData
                        width: root.columnWidth(modelData)
                        height: parent.height
                        verticalAlignment: Text.AlignVCenter
                        text: modelData.title
                        color: "#b8d5ee"
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                    }
                }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ListView {
                id: listView
                anchors.fill: parent
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                model: root.rowCount()
                spacing: 6

                delegate: Rectangle {
                    id: rowItem
                    required property int index
                    property var rowData: root.rowAt(index)
                    property bool selected: root.selectedRole.length > 0
                                            && rowData[root.selectedRole] !== undefined
                                            && rowData[root.selectedRole] === root.selectedValue

                    width: ListView.view.width
                    height: 42
                    radius: 6
                    color: selected ? "#1d4b73"
                                    : rowMouse.containsMouse ? "#143654"
                                                              : (index % 2 === 0 ? "#0f253b" : "#0b1c2e")
                    border.width: selected || rowMouse.containsMouse ? 1 : 0
                    border.color: selected ? "#4fc0ff" : "#2f6389"

                    Row {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10

                        Repeater {
                            model: root.columns

                            delegate: Text {
                                required property var modelData
                                width: root.columnWidth(modelData)
                                height: parent.height
                                verticalAlignment: Text.AlignVCenter
                                text: root.displayValue(rowItem.rowData, modelData)
                                color: root.cellColor(rowItem.rowData, modelData, rowItem.selected)
                                font.pixelSize: 13
                                font.weight: modelData.role === "riskLevel"
                                             || modelData.role === "status"
                                             || modelData.role === "level"
                                             || modelData.role === "confidence"
                                             ? Font.DemiBold : Font.Normal
                                elide: Text.ElideMiddle
                            }
                        }
                    }

                    MouseArea {
                        id: rowMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.rowClicked(rowItem.rowData)
                    }
                }
            }

            Text {
                anchors.centerIn: parent
                visible: root.rowCount() === 0
                text: root.emptyText
                color: "#8fb2d0"
                font.pixelSize: 14
            }
        }
    }
}
