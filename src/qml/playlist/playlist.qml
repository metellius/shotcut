import QtQuick 2.2
import QtQuick.Window 2.1
import QtQuick.Controls 1.1
import QtQuick.Layouts 1.1
import QtQuick.Controls.Private 1.0
import org.shotcut.qml 1.0

Rectangle {
    id: root

    function incrementIndex() {
        list.currentIndex = Math.min(list.currentIndex + 1, list.count - 1)
    }
    function decrementIndex() {
        list.currentIndex = Math.max(list.currentIndex - 1, 0)
    }
    function setIndex(index) {
        list.currentIndex = index
    }
    function index() {
        return list.currentIndex;
    }
    function startEditingComment() {
        if (list.currentItem) {
            list.currentItem.startEditingComment();
        }
    }
    function handleDrop(dropevent, index) {
        var mimeData = {};
        for (var idx in dropevent.formats)
        {
            var format = dropevent.formats[idx];
            var data = dropevent.getDataAsString(format);
            if (data.length == 0)
            data = dropevent.getDataAsArrayBuffer(format);
            mimeData[format] = data;
        }
        var data = {
            urls: dropevent.urls,
            index: index,
            mimeData: mimeData
        }
        var result = playlistdock.handleDrop(data);
        dropevent.accept();
    }

    property color shotcutBlue: Qt.rgba(23/255, 92/255, 118/255, 1.0)
    property real zoomFactor: 1.0
    property bool updateButtonEnabled

    color: activePalette.window
    SystemPalette { id: activePalette }

    Keys.onPressed: {
        if (event.modifiers == Qt.NoModifier) {
            if (event.key == Qt.Key_Up) {
                list.currentIndex -= 1
                event.accepted = true;
            } else if (event.key == Qt.Key_Down) {
                list.currentIndex += 1
                event.accepted = true;
            }
        }
    }

    Window {
        id: clipPreview
        function open(item) {
            var itemPos = item.mapToItem(null,0,0)
            x = playlistdock.widgetPos.x + itemPos.x + list.width - 30;
            y = playlistdock.widgetPos.y + clipPreview.height / 2 + itemPos.y;
            y = playlistdock.widgetPos.y + itemPos.y + item.height / 2 - clipPreview.height / 2;
            clipPreview.delegate = item;
            show()
        }
        property Item delegate
        readonly property int extraMargins: 6
        flags: Qt.ToolTip
        visible: false
        width: previewBg.x + previewBg.width
        height: stripRow.height + clipPreview.extraMargins * 2
        color: "transparent"
        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            onExited: clipPreview.close()
        }

        Rectangle {
            id: previewBg
            anchors {
                top: parent.top
                bottom: parent.bottom
                left: arrow.right
                leftMargin: -1
            }
            width: stripRow.width + 2 * clipPreview.extraMargins
            color: activePalette.window
            border.width: 1
            border.color: activePalette.shadow
        }
        Row {
            id: stripRow
            anchors {
                centerIn: previewBg
            }
            spacing: clipPreview.extraMargins
            Repeater {
                id: filmstripRepeater
                model: 5
                Image {
                    id: img
                    width: 800 / filmstripRepeater.model
                    height: width / profile.aspectRatio
                    sourceSize.width: width
                    sourceSize.height: height
                    source: {
                        if (!clipPreview.delegate || !clipPreview.delegate.modelData)
                            return "";
                        else
                            return 'image://thumbnail/' +
                                clipPreview.delegate.modelData.service +
                                '/' + clipPreview.delegate.modelData.resource +
                                '#' + Math.floor(clipPreview.delegate.modelData.inPoint +
                                (index / (filmstripRepeater.model - 1)) *
                                clipPreview.delegate.modelData.duration)
                    }
                    property bool loaded: status == Image.Ready
                    Behavior on loaded {
                        NumberAnimation {
                            target: img
                            property: "opacity"
                            from: 0
                            to: 1
                            duration: 500
                        }
                    }
                    Image {
                        anchors.centerIn: parent
                        source: "qrc:/icons/light/32x32/document-open-recent.png"
                        z: -1
                    }
                }
            }
        }
        Canvas {
            id: arrow
            width: 10
            height: 14
            anchors.verticalCenter: parent.verticalCenter
            Component.onCompleted: requestPaint()
            onPaint: {
                var ctx = getContext("2d");
                ctx.fillStyle = previewBg.color;
                ctx.beginPath();
                ctx.moveTo(0,height / 2);
                ctx.lineTo(width,0);
                ctx.lineTo(width,height);
                ctx.closePath();
                ctx.fill();
                ctx.beginPath();
                ctx.moveTo(width,0);
                ctx.lineTo(0,height / 2);
                ctx.lineTo(width,height);
                ctx.strokeStyle = previewBg.border.color;
                ctx.stroke();
            }
        }
    }
    DropArea {
        anchors.fill: parent
        onEntered: {
            if (drag.formats.indexOf("application/playlist-rowindex") != -1)
                drag.accept(Qt.MoveAction);
            else
                drag.accept(Qt.CopyAction);
        }
        onDropped: root.handleDrop(drop, 0);
    }

    ScrollView {
        id: scrollView
        __wheelAreaScrollSpeed: 70
        anchors {
            top: parent.top
            right: parent.right
            left: parent.left
            bottom: toolBar.top
        }
        ListView {
            id: list
            Behavior on contentY {
                NumberAnimation {
                    easing.type: Easing.OutQuad
                    duration: 70
                }
            }
            model: playlistmodel
            flickDeceleration: 10000
            boundsBehavior: Flickable.StopAtBounds
            delegate: Rectangle {
                id: delegate

                function startEditingComment() {
                    commentEdit.startEditing();
                }

                property var modelData: model
                property real thumbOffset: 0
                property string imageSource: 'image://thumbnail/' + service + '/' + resource + '#' + (inPoint + Math.floor(delegate.thumbOffset * duration))
                property string friendlyDuration: {
                    var secs = Math.floor(duration / profile.fps);
                    var str = secs > 60 ? qsTr("%1 min").arg(Math.floor(secs / 60)) : "";
                    if (secs % 60)
                        str += " " + qsTr("%1 sec").arg(secs % 60);
                    return str;
                }

                width: ListView.view.width
                color: ListView.isCurrentItem ? root.shotcutBlue : activePalette.window

                MouseArea {
                    id: area
                    objectName: index
                    anchors.fill: parent
                    hoverEnabled: true
                    drag.target: Item {}
                    onPositionChanged: previewOpenTimer.restart()
                    onEntered: previewOpenTimer.restart()
                    onExited: clipPreview.close()
                    onPressed: {
                        list.currentIndex = index
                        delegate.forceActiveFocus();
                    }
                    onDoubleClicked: playlistdock.openClipAt(index)
                    onWheel: {
                        previewOpenTimer.stop()
                        if (wheel.modifiers == Qt.ControlModifier) {
                            wheel.accepted = true;
                            if (wheel.angleDelta.y > 0 && root.zoomFactor < 2.0)
                                root.zoomFactor += 0.1;
                            else if (wheel.angleDelta.y < 0 && root.zoomFactor > 0.2)
                                root.zoomFactor -= 0.1;
                        } else {
                            wheel.accepted = false;
                        }
                    }
                    Drag.active: area.drag.active

                    Drag.dragType: Drag.Automatic
                    Drag.onDragFinished: view.releaseMouseGrab(root);
                    Drag.mimeData: {
                        "application/mlt+xml": playlistmodel.mltXmlForRow(index),
                        "text/plain": duration,
                        "application/playlist-rowindex": index
                    }
                    Connections {
                        target: list
                        onContentYChanged: {
                            clipPreview.close()
                        }
                    }
                    HoverEventListener {
                        id: hoverListener
                        listen: previewOpenTimer.running
                        widget: playlistwidget
                        onExited: {
                            previewOpenTimer.stop()
                            clipPreview.close()
                        }
                    }
                    Timer {
                        id: previewOpenTimer
                        running: parent.containsMouse
                        interval: 500
                        repeat: false
                        onTriggered: {
                            if (commentEdit.visible || !area.containsMouse ||
                                    area.drag.active)
                                return;
                            clipPreview.open(delegate)
                        }
                    }
                    Loader {
                        sourceComponent: bigThumbComponent ? bigThumbComponent
                                        : hiddenThumbsItem.checked ? hiddenThumbsComponent
                                        : bigThumbComponent
                        anchors.fill: parent
                    }
                    Component {
                        id: bigThumbComponent
                        Item {
                            Binding {
                                target: delegate
                                property: "height"
                                value: thumb.height + 20
                            }
                            Image {
                                id: thumb
                                anchors {
                                    verticalCenter: parent.verticalCenter
                                    left: parent.left
                                    leftMargin: 5
                                }

                                width: 160 * root.zoomFactor
                                height: 90 * root.zoomFactor
                                fillMode: Image.PreserveAspectFit
                                source: delegate.imageSource
                            }
                            Column {
                                anchors {
                                    left: thumb.right
                                    leftMargin: 10
                                    top: thumb.top
                                    right: parent.right
                                }
                                Label {
                                    maximumLineCount: 3
                                    width: parent.width
                                    text: displayRole
                                    font.bold: true
                                    wrapMode: Text.Wrap
                                }
                                Label {
                                    text: delegate.friendlyDuration
                                }
                                Label {
                                    width: parent.width
                                    text: commentEdit.visible ? "" : comment
                                }
                            }
                        }
                    }
                    Component {
                        id: hiddenThumbsComponent
                        Item {
                            Binding {
                                target: delegate
                                property: "height"
                                value: infoColumn.height
                            }
                            Column {
                                id: infoColumn
                                width: parent.width / 2
                                Label {
                                    maximumLineCount: 1
                                    width: parent.width
                                    text: displayRole
                                    font.bold: true
                                    wrapMode: Text.Wrap
                                }
                                Label {
                                    text: delegate.friendlyDuration
                                }
                            }
                            Label {
                                width: parent.width / 2
                                anchors.left: infoColumn.right
                                anchors.verticalCenter: infoColumn.verticalCenter
                                text: commentEdit.visible ? "" : comment
                                maximumLineCount: 2
                                horizontalAlignment: Text.AlignRight
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.IBeamCursor
                                    onClicked: commentEdit.startEditing();
                                }
                            }
                        }
                    }
                    TextArea {
                        id: commentEdit
                        function startEditing() {
                            visible = true
                            forceActiveFocus()
                            selectAll()
                        }
                        anchors.fill: parent
                        anchors.margins: 10
                        visible: false
                        text: comment
                        onTextChanged: playlistmodel.setComment(text, index);
                        onActiveFocusChanged: visible = activeFocus
                        onVisibleChanged: {
                            if (visible) {
                                text = comment;
                            }
                        }
                        Keys.onPressed: {
                            if (event.key == Qt.Key_Return &&
                                    event.modifiers == Qt.ControlModifier ||
                                    event.key == Qt.Key_Escape) {
                                event.accepted = true;
                                delegate.forceActiveFocus()
                            }
                        }
                        Label {
                            anchors {
                                bottom: parent.bottom
                                bottomMargin: 8
                                right: parent.right
                                rightMargin: 8
                            }
                            text: qsTr('Editing comment\nPress Escape or Ctrl+Enter when finished')
                            color: activePalette.shadow
                            visible: commentEdit.text == ""
                            horizontalAlignment: Text.AlignRight
                        }
                    }
                    ToolButton {
                        id: editButton
                        anchors {
                            bottom: parent.bottom
                            bottomMargin: 8
                            right: parent.right
                            rightMargin: 8
                        }

                        opacity: !commentEdit.visible && area.containsMouse && !hoverListener.hasExited
                        implicitWidth: 28
                        implicitHeight: 24
                        iconName: 'insert-text'
                        iconSource: 'qrc:///icons/oxygen/32x32/actions/insert-text.png'
                        tooltip: qsTr('Edit comment')
                        onClicked: commentEdit.startEditing();
                        Behavior on opacity { NumberAnimation {} }
                    }
                }
                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.RightButton
                    onPressed: {
                        list.currentIndex = index
                        delegate.forceActiveFocus();
                        previewOpenTimer.stop()
                        menu.updateActionsAndShow()
                    }
                }
                Rectangle {
                    width: parent.width
                    height: parent.height * 0.2
                    opacity: dropArea.containsDrag && dropArea.draggingTopHalf
                    gradient: Gradient {
                        GradientStop { position: 0.000; color: "white" }
                        GradientStop { position: 1.000; color: "transparent" }
                    }
                    Behavior on opacity { NumberAnimation {} }
                }
                Rectangle {
                    width: parent.width
                    height: parent.height * 0.2
                    anchors.bottom: parent.bottom
                    opacity: dropArea.containsDrag && !dropArea.draggingTopHalf
                    gradient: Gradient {
                        GradientStop { position: 0.000; color: "transparent" }
                        GradientStop { position: 1.000; color: "white" }
                    }
                    Behavior on opacity { NumberAnimation {} }
                }
                DropArea {
                    id: dropArea
                    property bool draggingTopHalf: drag.y < delegate.height / 2
                    anchors.fill: parent
                    onEntered: {
                        if (drag.formats.indexOf("application/playlist-rowindex") != -1)
                            drag.accept(Qt.MoveAction);
                        else
                            drag.accept(Qt.CopyAction);
                    }
                    onDropped: {
                        root.handleDrop(drop, draggingTopHalf ? index : index + 1);
                    }
                }
            }
        }
    }
    Label {
        visible: list.count == 0
        anchors.fill: scrollView
        wrapMode: Text.WordWrap
        anchors.margins: 10
        text: qsTr("Double-click a playlist item to open it in the player.\n\n" +
            "You can freely preview clips without necessarily adding them to the playlist or closing it.\n\n" +
            "To trim or adjust a playlist item Double-click to open it, make the changes, and click the Update icon.\n\n" +
            "Drag-n-drop to rearrange the items.")
    }
    RowLayout {
        id: toolBar
        height: 30
        width: parent.width
        anchors.bottom: parent.bottom

        ToolButton {
            id: addButton
            implicitWidth: 28
            implicitHeight: 24
            iconName: 'list-add'
            iconSource: 'qrc:///icons/oxygen/16x16/actions/list-add.png'
            tooltip: qsTr('Add something to the playlist')
            onClicked: playlistdock.appendCut()
        }
        ToolButton {
            id: removeButton
            implicitWidth: 28
            implicitHeight: 24
            iconName: 'list-remove'
            iconSource: 'qrc:///icons/oxygen/16x16/actions/list-remove.png'
            tooltip: qsTr('Remove cut')
            onClicked: playlistdock.removeCut()
        }
        ToolButton {
            id: updateButton
            implicitWidth: 28
            implicitHeight: 24
            iconName: 'dialog-ok'
            iconSource: 'qrc:///icons/oxygen/16x16/actions/dialog-ok.png'
            tooltip: qsTr('Update')
            enabled: root.updateButtonEnabled
            onClicked: playlistdock.updateCut()
        }
        ToolButton {
            id: menuButton
            implicitWidth: 28
            implicitHeight: 24
            iconName: 'format-justify-fill'
            iconSource: 'qrc:///icons/oxygen/16x16/actions/format-justify-fill.png'
            tooltip: qsTr('Display a menu of additional actions')
            onClicked: menu.updateActionsAndShow()
        }
        Item {
            Layout.fillWidth: true
        }
    }
    ExclusiveGroup { id: thumbnailGroup }
    Menu {
        id: menu
        function updateActionsAndShow() {
            playlistdock.reevaluateClipActions();
            popup();
        }
        onAboutToHide: view.releaseMouseGrab(root)
        MenuItem {
            id: gotoItem
            text: qsTr('Goto')
            visible: playlistdock.activeClipActions & PlaylistDock.Goto
            onTriggered: playlistdock.on_actionGoto_triggered()
        }
        MenuItem {
            id: insertItem
            text: qsTr('Insert clip')
            visible: playlistdock.activeClipActions & PlaylistDock.InsertCut
            onTriggered: playlistdock.on_actionInsertCut_triggered()
        }
        // MenuItem { //TODO: not supported yet
        //     id: insertItem
        //     text: qsTr('Insert blank')
        //     visible: playlistdock.activeClipActions & PlaylistDock.InsertBlank
        //     onTriggered: playlistdock.on_actionInsertBlank_triggered()
        // }
        MenuItem {
            id: openItem
            text: qsTr('Open as clip')
            visible: playlistdock.activeClipActions & PlaylistDock.Open
            onTriggered: playlistdock.on_actionOpen_triggered()
        }
        MenuItem {
            id: removeItem
            text: qsTr('Remove')
            visible: playlistdock.activeClipActions & PlaylistDock.Remove
            onTriggered: playlistdock.removeCut()
        }
        MenuItem {
            id: updateItem
            text: qsTr('Replace')
            visible: playlistdock.activeClipActions & PlaylistDock.Update
            onTriggered: playlistdock.replaceCut()
        }
        MenuSeparator {
            visible: gotoItem.visible || openItem.visible || removeItem.visible || updateItem.visible
        }
        MenuItem {
            text: qsTr('Remove all')
            onTriggered: playlistdock.removeAll()
        }
        MenuItem {
            text: qsTr('Add all to timeline')
            onTriggered: playlistdock.addAll()
        }
        Menu {
            title: qsTr('Thumbnails')
            MenuItem {
                id: hiddenThumbsItem
                text: qsTr('Hidden')
                objectName: "hidden"
                exclusiveGroup: thumbnailGroup
                checked: settings.playlistThumbnails == objectName
                checkable: true
                onTriggered: settings.playlistThumbnails = objectName
            }
            MenuItem {
                id: largeThumbsItem
                text: qsTr('Large')
                objectName: "large"
                exclusiveGroup: thumbnailGroup
                checked: settings.playlistThumbnails == objectName
                checkable: true
                onTriggered: settings.playlistThumbnails = objectName
            }
        }
    }

}
