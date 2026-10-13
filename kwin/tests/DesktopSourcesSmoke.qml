import QtQuick
import "../src/qml/DesktopSources.js" as DesktopSources

QtObject {
    id: root
    property QtObject panel: QtObject { property string name: "panel" }
    property QtObject replacementPanel: QtObject { property string name: "panel" }
    property QtObject glasses: QtObject { property string name: "glasses" }
    property QtObject virtualDisplay: QtObject { property string name: "Virtual-BreezyDesktop_1" }
    property var workspaceScreens: [panel, glasses, virtualDisplay]
    property var captureScreen: DesktopSources.resolveCaptureSource(glasses, glasses, "panel", workspaceScreens)
    property int checksPassed: 0

    function check(condition, message) {
        if (!condition) throw new Error(message);
        checksPassed++;
    }
    function runChecks() {
        check(captureScreen === panel, "configured source is not the panel");
        check(DesktopSources.resolveCaptureSource(glasses, glasses, "", workspaceScreens) === glasses, "default changed");
        check(DesktopSources.resolveCaptureSource(virtualDisplay, glasses, "panel", workspaceScreens) === virtualDisplay, "virtual changed");
        check(DesktopSources.workspaceIndex(0, [panel, virtualDisplay], workspaceScreens) === 0, "source focus wrong");
        check(DesktopSources.workspaceIndex(1, [panel, virtualDisplay], workspaceScreens) === 2, "virtual focus wrong");
        workspaceScreens = [virtualDisplay, replacementPanel, glasses];
        check(captureScreen === replacementPanel, "binding retained the retired source");
        check(DesktopSources.workspaceIndex(0, [replacementPanel], workspaceScreens) === 1, "reordered focus wrong");
        workspaceScreens = [glasses, virtualDisplay];
        check(captureScreen === glasses, "missing-source fallback failed");
        check(DesktopSources.workspaceIndex(0, [replacementPanel], workspaceScreens) === -1, "stale source index accepted");
        check(DesktopSources.workspaceIndex(99, [glasses], workspaceScreens) === -1, "invalid index accepted");
    }
}
