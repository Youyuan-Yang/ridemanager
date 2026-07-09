import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    property var readingModel: null
    property string title: "传感器趋势"
    property color accentColor: "#52a8ff"
    property var rows: []
    property real visibleWindowMs: 12000
    property real latestObservedMs: 0
    property real animatedRightEdgeMs: 0
    property bool chartPrimed: false

    radius: 8
    color: "#0b1d2f"
    border.width: 1
    border.color: "#214263"

    function latestTimestamp() {
        var latest = 0
        for (var i = 0; i < rows.length; ++i) {
            var ms = Number(rows[i].observedAtMs)
            if (isFinite(ms) && ms > latest) {
                latest = ms
            }
        }
        return latest
    }

    function earliestTimestamp() {
        var earliest = 0
        for (var i = 0; i < rows.length; ++i) {
            var ms = Number(rows[i].observedAtMs)
            if (isFinite(ms) && ms > 0) {
                earliest = earliest === 0 ? ms : Math.min(earliest, ms)
            }
        }
        return earliest
    }

    function effectiveWindowMs() {
        var latest = latestTimestamp()
        var earliest = earliestTimestamp()
        if (latest > 0 && earliest > 0) {
            return Math.max(1000, latest - earliest + 1000)
        }
        return visibleWindowMs
    }

    function rightEdgeMs() {
        return animatedRightEdgeMs > 0 ? animatedRightEdgeMs : latestObservedMs
    }

    function rowInWindow(row) {
        var edge = rightEdgeMs()
        var ms = Number(row.observedAtMs)
        if (edge <= 0 || !isFinite(ms) || ms <= 0) {
            return true
        }
        return ms >= edge - effectiveWindowMs() && ms <= edge + 250
    }

    function sampleX(row, index, left, plotW) {
        var edge = rightEdgeMs()
        var ms = Number(row.observedAtMs)
        var earliest = earliestTimestamp()
        var latest = latestTimestamp()
        if (rows.length <= 1) {
            return left + plotW * 0.5
        }
        if (latest > earliest && edge > 0 && isFinite(ms) && ms > 0) {
            var windowMs = effectiveWindowMs()
            var windowStart = edge - windowMs
            return left + ((ms - windowStart) / windowMs) * plotW
        }
        return left + plotW * index / (rows.length - 1)
    }

    function reload() {
        rows = readingModel && readingModel.allRows ? readingModel.allRows() : []
        var latest = latestTimestamp()
        latestObservedMs = latest

        if (latest > 0) {
            if (!chartPrimed
                    || animatedRightEdgeMs <= 0
                    || Math.abs(latest - animatedRightEdgeMs) > effectiveWindowMs()) {
                chartPrimed = false
                animatedRightEdgeMs = latest
                chartPrimed = true
            } else if (latest > animatedRightEdgeMs) {
                animatedRightEdgeMs = latest
            } else {
                chart.requestPaint()
            }
        } else {
            chart.requestPaint()
        }
    }

    function minValue() {
        var found = false
        var value = 0
        for (var i = 0; i < rows.length; ++i) {
            if (!rowInWindow(rows[i])) {
                continue
            }
            var current = Number(rows[i].value)
            if (!isFinite(current)) {
                continue
            }
            value = found ? Math.min(value, current) : current
            found = true
        }
        return found ? value : 0
    }

    function maxValue() {
        var found = false
        var value = 1
        for (var i = 0; i < rows.length; ++i) {
            if (!rowInWindow(rows[i])) {
                continue
            }
            var current = Number(rows[i].value)
            if (!isFinite(current)) {
                continue
            }
            value = found ? Math.max(value, current) : current
            found = true
        }
        return found ? value : 1
    }

    Component.onCompleted: reload()

    onAnimatedRightEdgeMsChanged: chart.requestPaint()

    Behavior on animatedRightEdgeMs {
        enabled: root.chartPrimed
        NumberAnimation {
            duration: 90
            easing.type: Easing.Linear
        }
    }

    Connections {
        target: root.readingModel
        function onReadingsChanged() { root.reload() }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 30

            Text {
                Layout.fillWidth: true
                text: root.title
                color: "#f4fbff"
                font.pixelSize: 16
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }

            Text {
                text: rows.length > 0
                      ? ("范围 " + root.minValue().toFixed(1) + " - " + root.maxValue().toFixed(1))
                      : "暂无曲线"
                color: "#8fc9ff"
                font.pixelSize: 12
                font.weight: Font.DemiBold
            }
        }

        Canvas {
            id: chart
            Layout.fillWidth: true
            Layout.fillHeight: true

            onPaint: {
                var ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)

                var left = 42
                var right = 14
                var top = 12
                var bottom = 28
                var plotW = Math.max(1, width - left - right)
                var plotH = Math.max(1, height - top - bottom)

                ctx.strokeStyle = "rgba(130, 176, 216, 0.20)"
                ctx.lineWidth = 1
                for (var i = 0; i < 4; ++i) {
                    var y = top + plotH * i / 3
                    ctx.beginPath()
                    ctx.moveTo(left, y)
                    ctx.lineTo(left + plotW, y)
                    ctx.stroke()
                }

                if (!root.rows || root.rows.length === 0) {
                    return
                }

                var minV = root.minValue()
                var maxV = root.maxValue()
                var span = Math.max(0.0001, maxV - minV)
                var edge = root.rightEdgeMs()
                var windowMs = root.effectiveWindowMs()
                var farLeft = edge > 0 ? edge - windowMs * 2 : 0

                ctx.save()
                ctx.beginPath()
                ctx.rect(left, top, plotW, plotH)
                ctx.clip()

                ctx.strokeStyle = root.accentColor
                ctx.lineWidth = 2
                ctx.lineJoin = "round"
                ctx.lineCap = "round"
                ctx.beginPath()
                var started = false
                for (var p = 0; p < root.rows.length; ++p) {
                    var row = root.rows[p]
                    var ms = Number(row.observedAtMs)
                    if (edge > 0 && isFinite(ms) && ms > 0 && ms < farLeft) {
                        continue
                    }
                    var x = root.sampleX(row, p, left, plotW)
                    var value = Number(row.value)
                    if (!isFinite(value)) {
                        continue
                    }
                    var yy = top + plotH - ((value - minV) / span) * plotH
                    if (!started) {
                        ctx.moveTo(x, yy)
                        started = true
                    } else {
                        ctx.lineTo(x, yy)
                    }
                }
                if (started) {
                    ctx.stroke()
                }

                var dotStride = root.rows.length > 180 ? Math.ceil(root.rows.length / 120) : 1
                var dotRadius = root.rows.length > 180 ? 0.9 : (root.rows.length > 90 ? 1.2 : 2.0)
                ctx.fillStyle = root.accentColor
                ctx.globalAlpha = 0.82
                for (var dot = 0; dot < root.rows.length; ++dot) {
                    var dotRow = root.rows[dot]
                    if (dot % dotStride !== 0 && dot !== root.rows.length - 1) {
                        continue
                    }
                    if (!root.rowInWindow(dotRow)) {
                        continue
                    }
                    var dx = root.sampleX(dotRow, dot, left, plotW)
                    var dv = Number(dotRow.value)
                    if (!isFinite(dv)) {
                        continue
                    }
                    var dy = top + plotH - ((dv - minV) / span) * plotH
                    ctx.beginPath()
                    ctx.arc(dx, dy, dotRadius, 0, Math.PI * 2)
                    ctx.fill()
                }
                ctx.restore()
            }
        }
    }
}
