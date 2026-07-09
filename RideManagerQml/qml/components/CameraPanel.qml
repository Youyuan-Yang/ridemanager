import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    property string cameraId: "CAM_FRONT"
    property string title: "前向道路"
    property string subtitle: "Front perception"
    property var findingsModel: null
    property string riskLevel: "Normal"
    property var currentFindings: []

    radius: 8
    color: "#081827"
    border.width: 1
    border.color: root.findingCount() > 0 ? root.accentColor() : "#294863"
    clip: true

    function reloadFindings() {
        currentFindings = findingsModel ? findingsModel.rowsForCamera(cameraId) : []
    }

    function findingCount() {
        return currentFindings ? currentFindings.length : 0
    }

    function percent(value) {
        return Math.round(Number(value) * 100) + "%"
    }

    function hasBox(finding) {
        return Number(finding.boxWidth) > 0.001 && Number(finding.boxHeight) > 0.001
    }

    function accentColor() {
        if (riskLevel === "Danger")
            return "#ff5c70"
        if (riskLevel === "Warning")
            return "#ffd35a"
        return "#4ae29a"
    }

    function averageConfidence() {
        if (findingCount() === 0)
            return "--"
        var total = 0
        for (var i = 0; i < currentFindings.length; ++i)
            total += Number(currentFindings[i].confidence)
        return percent(total / currentFindings.length)
    }

    function strongestLabel() {
        if (findingCount() === 0)
            return "无识别结果"
        var strongest = currentFindings[0]
        for (var i = 1; i < currentFindings.length; ++i) {
            if (Number(currentFindings[i].confidence) > Number(strongest.confidence))
                strongest = currentFindings[i]
        }
        return strongest.label + " " + percent(strongest.confidence)
    }

    function latestObservedAt() {
        if (findingCount() === 0)
            return "--"
        return currentFindings[currentFindings.length - 1].observedAt
    }

    Component.onCompleted: reloadFindings()
    onCameraIdChanged: reloadFindings()

    Connections {
        target: root.findingsModel
        function onCountChanged() { root.reloadFindings() }
        function onFindingsChanged() { root.reloadFindings() }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 38
            spacing: 8

            Rectangle {
                Layout.preferredWidth: 34
                Layout.preferredHeight: 34
                radius: 6
                color: "#12334c"
                border.width: 1
                border.color: "#2e668c"

                Text {
                    anchors.centerIn: parent
                    text: root.cameraId === "CAM_FACE" ? "F" : (root.cameraId === "CAM_BACK" ? "B" : "R")
                    color: "#8bd8ff"
                    font.pixelSize: 15
                    font.weight: Font.Black
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 1

                Text {
                    Layout.fillWidth: true
                    text: root.title
                    color: "#f4fbff"
                    font.pixelSize: 15
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }

                Text {
                    Layout.fillWidth: true
                    text: root.cameraId + " | " + root.subtitle
                    color: "#94b9d9"
                    font.pixelSize: 11
                    elide: Text.ElideRight
                }
            }

            Rectangle {
                Layout.preferredWidth: 76
                Layout.preferredHeight: 28
                radius: 6
                color: Qt.rgba(0.12, 0.35, 0.43, 0.55)
                border.width: 1
                border.color: root.accentColor()

                Text {
                    anchors.centerIn: parent
                    text: root.findingCount() + " 条数据"
                    color: "#f4fbff"
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 42
            spacing: 6

            Repeater {
                model: [
                    { label: "最高置信", value: root.strongestLabel() },
                    { label: "平均置信", value: root.averageConfidence() },
                    { label: "最近观测", value: root.latestObservedAt() }
                ]

                delegate: Rectangle {
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 5
                    color: "#0d263b"
                    border.width: 1
                    border.color: "#254b68"

                    Column {
                        anchors.fill: parent
                        anchors.margins: 6
                        spacing: 1

                        Text {
                            width: parent.width
                            text: modelData.label
                            color: "#769bb9"
                            font.pixelSize: 9
                            elide: Text.ElideRight
                        }

                        Text {
                            width: parent.width
                            text: modelData.value
                            color: "#eaf7ff"
                            font.pixelSize: 11
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                        }
                    }
                }
            }
        }

        Rectangle {
            id: detectionMap
            Layout.fillWidth: true
            Layout.preferredHeight: 112
            radius: 6
            color: "#04101b"
            border.width: 1
            border.color: "#23445f"
            clip: true

            Repeater {
                model: 7
                Rectangle {
                    x: index * detectionMap.width / 6
                    width: 1
                    height: detectionMap.height
                    color: "#1d3c55"
                    opacity: 0.46
                }
            }

            Repeater {
                model: 5
                Rectangle {
                    y: index * detectionMap.height / 4
                    width: detectionMap.width
                    height: 1
                    color: "#1d3c55"
                    opacity: 0.46
                }
            }

            Text {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.margins: 7
                text: "归一化检测区域 / 坐标可视化"
                color: "#6f94b2"
                font.pixelSize: 9
            }

            Item {
                id: detectionLayer
                anchors.fill: parent
                anchors.margins: 8

                Repeater {
                    model: root.currentFindings

                    delegate: Rectangle {
                        required property var modelData
                        visible: root.hasBox(modelData)
                        x: Math.max(0, Math.min(detectionLayer.width - width,
                                               Number(modelData.boxX) * detectionLayer.width))
                        y: Math.max(16, Math.min(detectionLayer.height - height,
                                                Number(modelData.boxY) * detectionLayer.height))
                        width: Math.max(24, Math.min(detectionLayer.width,
                                                    Number(modelData.boxWidth) * detectionLayer.width))
                        height: Math.max(18, Math.min(detectionLayer.height,
                                                     Number(modelData.boxHeight) * detectionLayer.height))
                        color: "transparent"
                        border.width: 2
                        border.color: root.accentColor()

                        Rectangle {
                            anchors.left: parent.left
                            anchors.bottom: parent.top
                            height: 18
                            width: Math.min(detectionLayer.width, boxLabel.implicitWidth + 10)
                            radius: 3
                            color: root.accentColor()

                            Text {
                                id: boxLabel
                                anchors.centerIn: parent
                                text: modelData.label + " " + root.percent(modelData.confidence)
                                color: "#06111a"
                                font.pixelSize: 9
                                font.weight: Font.Black
                            }
                        }
                    }
                }
            }

            Text {
                anchors.centerIn: parent
                visible: root.findingCount() === 0
                text: "当前时刻无此摄像头识别数据"
                color: "#7f9db8"
                font.pixelSize: 12
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 68
            radius: 6
            color: "#0a1d2e"
            border.width: 1
            border.color: "#203e58"

            ListView {
                anchors.fill: parent
                anchors.margins: 6
                clip: true
                spacing: 4
                model: root.currentFindings

                delegate: Rectangle {
                    required property var modelData
                    width: ListView.view.width
                    height: 25
                    radius: 4
                    color: "#102a40"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 7
                        anchors.rightMargin: 7
                        spacing: 6

                        Text {
                            Layout.fillWidth: true
                            text: modelData.label
                            color: "#f4fbff"
                            font.pixelSize: 11
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                        }

                        Text {
                            text: root.percent(modelData.confidence)
                            color: "#62d5ff"
                            font.pixelSize: 11
                        }

                        Text {
                            text: modelData.observedAt
                            color: "#91aec7"
                            font.pixelSize: 10
                        }
                    }
                }
            }
        }
    }
}
