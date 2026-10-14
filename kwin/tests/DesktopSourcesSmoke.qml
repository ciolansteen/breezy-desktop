import QtQuick
import "../src/qml/DesktopSources.js" as DesktopSources

QtObject {
    id: root
    property QtObject panel: QtObject { property string name: "panel" }
    property QtObject replacementPanel: QtObject { property string name: "panel" }
    property QtObject glasses: QtObject { property string name: "glasses" }
    property QtObject virtualDisplay: QtObject { property string name: "Virtual-BreezyDesktop_1" }
    property var workspaceScreens: [panel, glasses, virtualDisplay]
    property var sceneScreens: [glasses, virtualDisplay]
    property int focusedWorkspaceIndex: DesktopSources.workspaceIndex(0, sceneScreens, workspaceScreens)
    property int checksPassed: 0

    function check(condition, message) {
        if (!condition) throw new Error(message);
        checksPassed++;
    }
    function runChecks() {
        check(focusedWorkspaceIndex === 1, "filtered scene focus wrong");
        check(DesktopSources.workspaceIndex(1, sceneScreens, workspaceScreens) === 2, "virtual focus wrong");
        workspaceScreens = [virtualDisplay, replacementPanel, glasses];
        check(focusedWorkspaceIndex === 2, "binding did not follow output reorder");
        check(DesktopSources.workspaceIndex(1, sceneScreens, workspaceScreens) === 0, "reordered virtual focus wrong");
        sceneScreens = [panel];
        check(focusedWorkspaceIndex === -1, "retired output accepted");
        sceneScreens = [replacementPanel];
        check(focusedWorkspaceIndex === 1, "replacement output focus wrong");
        check(DesktopSources.workspaceIndex(-1, sceneScreens, workspaceScreens) === -1, "negative index accepted");
        check(DesktopSources.workspaceIndex(99, sceneScreens, workspaceScreens) === -1, "invalid index accepted");
    }
}
