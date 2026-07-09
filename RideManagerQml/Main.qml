import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import "qml/components"

ApplicationWindow {
    id: window
    width: Screen.width > 0 ? Screen.width : 1280
    height: Screen.height > 0 ? Screen.height : 720
    minimumWidth: 640
    minimumHeight: 360
    visibility: Window.FullScreen
    visible: true
    title: qsTr("RideManagerQml")
    color: "#07111e"
    font.family: "PingFang SC"

    palette.window: "#07111e"
    palette.windowText: "#f4fbff"
    palette.text: "#f4fbff"
    palette.button: "#173657"
    palette.buttonText: "#f4fbff"
    palette.base: "#0b1929"
    palette.highlight: "#52a8ff"
    palette.highlightedText: "#06101a"

    readonly property bool databaseConnected: appController.databaseStatus.indexOf("已连接") >= 0
    readonly property bool databaseHasError: appController.databaseStatus.indexOf("错误") >= 0
                                             || appController.databaseStatus.indexOf("失败") >= 0
                                             || appController.databaseStatus.indexOf("无法") >= 0
    readonly property bool compactUi: width <= 1100 || height <= 640
    readonly property int sideNavWidth: compactUi ? 188 : 232
    readonly property int contentMargin: compactUi ? 12 : 24
    readonly property int pageSpacing: compactUi ? 8 : 14
    readonly property int headerHeight: compactUi ? 66 : 86
    readonly property int topCardHeight: compactUi ? 210 : 260
    readonly property int cameraGridColumns: 3
    readonly property int cameraGridRows: 1
    readonly property int cameraPanelHeight: compactUi ? 230 : 320
    readonly property int cameraGridHeight: cameraGridRows * cameraPanelHeight
                                            + Math.max(0, cameraGridRows - 1) * pageSpacing
    readonly property int sensorTrendHeight: compactUi ? 240 : 220
    readonly property int statusBarHeight: compactUi ? 34 : 42
    property bool navOpen: false

    function openNavigation() {
        navOpen = true
        navAutoHide.restart()
    }

    function closeNavigation() {
        navAutoHide.stop()
        navOpen = false
    }

    Timer {
        id: navAutoHide
        interval: 3600
        repeat: false
        onTriggered: window.navOpen = false
    }

    readonly property var decisionColumns: [
        { title: "决策ID", role: "decisionId", width: 2.0 },
        { title: "风险等级", role: "riskLevel", width: 1.0 },
        { title: "决策时间", role: "decidedAt", width: 1.7 }
    ]
    readonly property var cameraColumns: [
        { title: "摄像头", role: "cameraId", width: 1.1 },
        { title: "标签", role: "label", width: 2.0 },
        { title: "置信度", role: "confidence", width: 1.0, format: "percent" },
        { title: "检测框 x,y,w,h", role: "bbox", width: 1.4 },
        { title: "观测时间", role: "observedAt", width: 1.2 }
    ]
    readonly property var cameraFeeds: [
        {
            id: "CAM_FRONT",
            title: "前向道路摄像头",
            subtitle: "车道 / 行人 / 前车风险"
        },
        {
            id: "CAM_FACE",
            title: "座舱驾驶员摄像头",
            subtitle: "分心 / 疲劳 / 视线监测"
        },
        {
            id: "CAM_BACK",
            title: "后向盲区摄像头",
            subtitle: "后车 / 盲区 / 贴近风险"
        }
    ]
    readonly property var sensorColumns: [
        { title: "传感器", role: "sensorName", width: 1.1 },
        { title: "指标", role: "metricDisplay", width: 1.6 },
        { title: "数值", role: "value", width: 1.0, format: "number", precision: 2 },
        { title: "单位", role: "unit", width: 0.8 },
        { title: "观测时间", role: "observedAt", width: 1.1 }
    ]
    readonly property var actuatorColumns: [
        { title: "执行器", role: "actuatorName", width: 1.0 },
        { title: "命令", role: "commandType", width: 1.7 },
        { title: "状态", role: "status", width: 1.0 },
        { title: "请求时间", role: "requestedAt", width: 1.0 }
    ]
    readonly property var eventColumns: [
        { title: "来源", role: "source", width: 1.0 },
        { title: "级别", role: "level", width: 0.8 },
        { title: "消息", role: "message", width: 2.4 },
        { title: "时间", role: "occurredAt", width: 0.9 }
    ]

    component CockpitButton: Button {
        id: buttonControl
        implicitHeight: 38
        leftPadding: 16
        rightPadding: 16

        contentItem: Text {
            text: buttonControl.text
            color: buttonControl.enabled ? "#f4fbff" : "#7891a9"
            font.pixelSize: 14
            font.weight: Font.DemiBold
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        background: Rectangle {
            radius: 7
            color: buttonControl.enabled
                   ? (buttonControl.down ? "#225f93" : "#17456f")
                   : "#152536"
            border.width: 1
            border.color: buttonControl.enabled ? "#4da8e8" : "#293d52"
        }
    }

    component CockpitComboBox: ComboBox {
        id: comboControl
        implicitHeight: 38
        font.pixelSize: 13

        contentItem: Text {
            text: comboControl.displayText
            color: "#f4fbff"
            font.pixelSize: 13
            verticalAlignment: Text.AlignVCenter
            leftPadding: 12
            rightPadding: 28
            elide: Text.ElideRight
        }

        indicator: Text {
            x: comboControl.width - width - 12
            anchors.verticalCenter: parent.verticalCenter
            text: "v"
            color: "#8fc9ff"
            font.pixelSize: 13
            font.weight: Font.DemiBold
        }

        background: Rectangle {
            radius: 7
            color: "#102842"
            border.width: 1
            border.color: comboControl.activeFocus ? "#57bdff" : "#2b5577"
        }

        delegate: ItemDelegate {
            id: comboDelegate
            width: comboControl.width
            height: 34
            highlighted: comboControl.highlightedIndex === index

            contentItem: Text {
                text: modelData
                color: comboDelegate.highlighted ? "#06101a" : "#f4fbff"
                font.pixelSize: 13
                verticalAlignment: Text.AlignVCenter
                leftPadding: 10
                elide: Text.ElideRight
            }

            background: Rectangle {
                color: comboDelegate.highlighted ? "#58c1ff" : "#102842"
            }
        }

        popup: Popup {
            y: comboControl.height + 4
            width: comboControl.width
            implicitHeight: contentItem.implicitHeight
            padding: 1

            contentItem: ListView {
                clip: true
                implicitHeight: Math.min(contentHeight, 180)
                model: comboControl.popup.visible ? comboControl.delegateModel : null
                currentIndex: comboControl.highlightedIndex
            }

            background: Rectangle {
                radius: 7
                color: "#0b1d2f"
                border.width: 1
                border.color: "#32618b"
            }
        }
    }

    background: Rectangle {
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#0f2033" }
            GradientStop { position: 0.52; color: "#081524" }
            GradientStop { position: 1.0; color: "#03070d" }
        }

        Rectangle {
            width: parent.width
            height: 1
            anchors.top: parent.top
            color: "#173655"
            opacity: 0.7
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: window.pageSpacing

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: window.headerHeight

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: window.contentMargin
                    anchors.rightMargin: window.contentMargin
                    anchors.topMargin: window.compactUi ? 8 : 16
                    anchors.bottomMargin: 6
                    spacing: window.compactUi ? 10 : 16

                    CockpitButton {
                        Layout.preferredWidth: window.compactUi ? 58 : 70
                        text: "导航"
                        onClicked: window.openNavigation()
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 5

                        Text {
                            text: "RideManager 智能驾驶安全监控系统"
                            color: "#eef7ff"
                            font.pixelSize: window.compactUi ? 21 : 27
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                        }

                        Text {
                            text: "直连 RideManager PostgreSQL | 10Hz 后台线程读取实时安全数据"
                            color: "#9bc7e9"
                            font.pixelSize: window.compactUi ? 11 : 13
                            elide: Text.ElideRight
                        }
                    }

                    Rectangle {
                        Layout.preferredWidth: window.compactUi ? 178 : 210
                        Layout.preferredHeight: window.compactUi ? 34 : 38
                        radius: 7
                        color: "#102842"
                        border.width: 1
                        border.color: "#3a7dac"

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            spacing: 8

                            Rectangle {
                                Layout.preferredWidth: 9
                                Layout.preferredHeight: 9
                                radius: 5
                                color: window.databaseConnected ? "#56e39a"
                                                               : (window.databaseHasError ? "#ff5c70" : "#ffd35a")
                            }

                            Text {
                                Layout.fillWidth: true
                                text: window.databaseConnected ? "PostgreSQL ONLINE"
                                                              : (window.databaseHasError
                                                                 ? "PostgreSQL OFFLINE"
                                                                 : "PostgreSQL CONNECTING")
                                color: "#f4fbff"
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                            }
                        }
                    }

                    CockpitButton {
                        Layout.preferredWidth: window.compactUi ? 74 : 86
                        text: "刷新"
                        enabled: !appController.busy
                        onClicked: appController.refresh()
                    }

                    CockpitButton {
                        Layout.preferredWidth: window.compactUi ? 64 : 76
                        text: "退出"
                        onClicked: Qt.quit()
                    }
                }
            }

            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.leftMargin: window.contentMargin
                Layout.rightMargin: window.contentMargin
                currentIndex: appController.currentPage

                Item {
                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 14

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.fillHeight: false
                            Layout.minimumHeight: window.topCardHeight
                            Layout.preferredHeight: window.topCardHeight
                            Layout.maximumHeight: window.topCardHeight
                            spacing: window.pageSpacing

                            RiskCard {
                                Layout.preferredWidth: window.compactUi ? 330 : 340
                                Layout.fillHeight: true
                                compact: true
                                riskLevel: appController.currentRiskLevel
                                decisionId: appController.selectedDecisionId
                            }

                            ColumnLayout {
                                Layout.preferredWidth: window.compactUi ? 150 : 210
                                Layout.fillHeight: true
                                spacing: window.compactUi ? 6 : 10

                                StatTile {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    title: "安全决策"
                                    value: String(appController.safetyDecisionModel.count)
                                    subtitle: "数据库全部记录"
                                    accentColor: "#52a8ff"
                                }

                                StatTile {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    title: "识别目标"
                                    value: String(appController.currentDecisionCameraFindingModel.count)
                                    subtitle: "当前决策合计"
                                    accentColor: "#42e28a"
                                }

                                StatTile {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    title: "数据源"
                                    value: "PostgreSQL"
                                    subtitle: window.databaseConnected ? "RideManager 已连接"
                                                                       : "等待数据库服务"
                                    accentColor: window.databaseHasError ? "#ff5c70" : "#31d07f"
                                }
                            }

                            DataTable {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                title: "当前决策摄像头检测"
                                tableModel: appController.currentDecisionCameraFindingModel
                                columns: window.cameraColumns
                                emptyText: "当前安全决策暂无 camera_findings 数据"
                            }
                        }

                        DataTable {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            title: "全部安全决策"
                            tableModel: appController.safetyDecisionModel
                            columns: window.decisionColumns
                            selectedRole: "decisionId"
                            selectedValue: appController.selectedDecisionId
                            emptyText: "暂无 safety_decisions 数据"
                            onRowClicked: function(row) {
                                appController.selectDecision(row.decisionId)
                            }
                        }
                    }
                }

                Item {
                    ScrollView {
                        id: cameraScroll
                        anchors.fill: parent
                        clip: true
                        contentWidth: availableWidth

                        ColumnLayout {
                            width: cameraScroll.availableWidth
                            spacing: 14

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: window.compactUi ? 44 : 56
                                radius: 8
                                color: "#0b1d2f"
                                border.width: 1
                                border.color: "#285172"

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 16
                                    anchors.rightMargin: 16
                                    spacing: 14

                                    Text {
                                        Layout.fillWidth: true
                                        text: "三路摄像头实时识别"
                                        color: "#f4fbff"
                                        font.pixelSize: window.compactUi ? 16 : 20
                                        font.weight: Font.DemiBold
                                        elide: Text.ElideRight
                                    }

                                    Text {
                                        text: "决策 " + (appController.selectedDecisionId.length > 0 ? appController.selectedDecisionId : "--")
                                        color: "#9fc2e2"
                                        font.pixelSize: 12
                                        elide: Text.ElideMiddle
                                        Layout.maximumWidth: window.compactUi ? 160 : 310
                                    }

                                    Text {
                                        text: "实时目标 " + appController.liveCameraFindingModel.count
                                        color: "#57d8ff"
                                        font.pixelSize: 13
                                        font.weight: Font.DemiBold
                                    }
                                }
                            }

                            GridLayout {
                                Layout.fillWidth: true
                                Layout.fillHeight: false
                                Layout.minimumHeight: window.cameraGridHeight
                                Layout.preferredHeight: window.cameraGridHeight
                                Layout.maximumHeight: window.cameraGridHeight
                                columns: window.cameraGridColumns
                                columnSpacing: window.pageSpacing
                                rowSpacing: window.pageSpacing

                                Repeater {
                                    model: window.cameraFeeds

                                    delegate: CameraPanel {
                                        required property var modelData

                                        Layout.fillWidth: true
                                        Layout.preferredHeight: window.cameraPanelHeight
                                        cameraId: modelData.id
                                        title: modelData.title
                                        subtitle: modelData.subtitle
                                        findingsModel: appController.liveCameraFindingModel
                                        riskLevel: appController.currentRiskLevel
                                    }
                                }
                            }

                            DataTable {
                                Layout.fillWidth: true
                                Layout.preferredHeight: window.compactUi ? 230 : 300
                                title: "全部摄像头识别数据"
                                tableModel: appController.cameraFindingModel
                                columns: window.cameraColumns
                                emptyText: "暂无 camera_findings"
                            }
                        }
                    }
                }

                Item {
                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 14

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.preferredHeight: window.compactUi ? 38 : 44
                            spacing: window.compactUi ? 6 : 10

                            Label {
                                text: "传感器"
                                color: "#9bb8d2"
                            }

                            CockpitComboBox {
                                id: sensorSelector
                                Layout.preferredWidth: window.compactUi ? 116 : 150
                                model: appController.sensorNames
                                onActivated: {
                                    Qt.callLater(function() {
                                        metricSelector.currentIndex = 0
                                        appController.loadSensorMetric(sensorSelector.currentText,
                                                                     metricSelector.currentText)
                                    })
                                }
                            }

                            Label {
                                text: "指标"
                                color: "#9bb8d2"
                            }

                            CockpitComboBox {
                                id: metricSelector
                                Layout.preferredWidth: window.compactUi ? 146 : 190
                                model: appController.metricNamesForSensor(sensorSelector.currentText)
                                onActivated: appController.loadSensorMetric(sensorSelector.currentText, currentText)
                            }

                            CockpitButton {
                                text: "加载曲线"
                                onClicked: appController.loadSensorMetric(sensorSelector.currentText, metricSelector.currentText)
                            }

                            Text {
                                Layout.fillWidth: true
                                text: appController.sensorDescription(sensorSelector.currentText)
                                      + " | 当前指标 "
                                      + appController.metricDisplayName(metricSelector.currentText)
                                color: "#b7d8f2"
                                font.pixelSize: 13
                                elide: Text.ElideRight
                            }
                        }

                        SensorTrend {
                            Layout.fillWidth: true
                            Layout.preferredHeight: window.sensorTrendHeight
                            readingModel: appController.sensorReadingModel
                            visibleWindowMs: 30000
                            title: appController.sensorDescription(sensorSelector.currentText)
                                   + " / "
                                   + appController.metricDisplayName(metricSelector.currentText)
                            accentColor: appController.sensorAccentColor(sensorSelector.currentText)
                        }

                        DataTable {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            title: "全部传感器读数"
                            rows: appController.sensorReadingRows
                            columns: window.sensorColumns
                            emptyText: "暂无 sensor_readings 数据"
                        }
                    }
                }

                Item {
                    DataTable {
                        anchors.fill: parent
                        title: "执行器命令"
                        rows: appController.actuatorCommandRows
                        columns: window.actuatorColumns
                        emptyText: "暂无 actuator_commands 数据"
                    }
                }

                Item {
                    DataTable {
                        anchors.fill: parent
                        title: "系统日志"
                        rows: appController.systemEventRows
                        columns: window.eventColumns
                        emptyText: "暂无 system_events 数据"
                    }
                }
            }

            StatusBar {
                Layout.fillWidth: true
                Layout.preferredHeight: window.statusBarHeight
                databaseStatus: appController.databaseStatus
                databaseEndpoint: appController.databaseEndpoint
                lastRefreshTime: appController.lastRefreshTime
                busy: appController.busy
            }
        }
    }

    Item {
        id: navOverlay
        anchors.fill: parent
        z: 80
        visible: window.navOpen

        Rectangle {
            anchors.fill: parent
            color: "#000914"
            opacity: 0.24

            MouseArea {
                anchors.fill: parent
                onClicked: window.closeNavigation()
            }
        }

        SideNav {
            id: sideNav
            width: window.sideNavWidth
            height: parent.height
            x: window.navOpen ? 0 : -width
            compact: window.compactUi
            currentIndex: appController.currentPage
            onPageSelected: function(index) {
                appController.currentPage = index
                window.closeNavigation()
            }

            Behavior on x {
                NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
            }
        }
    }
}
