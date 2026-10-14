import QtQuick
import QtQuick3D
import "../src/qml" as Breezy

Item {
    width: 32
    height: 32
    property QtObject effect: QtObject {
        property list<int> displayResolution: [1920, 1080]
        property real lensDistanceRatio: 0.05
        property bool sbsEnabled: false
        property bool customBannerEnabled: false
        property bool smoothFollowEnabled: false
        property list<real> lookAheadConfig: [10, 1, 8, 40]
        property real lookAheadOverride: -1
        property real diagonalFOV: 52
        property int poseTimeElapsedMs: 16
        property real poseTimestamp: Date.now()
        property bool poseHasPosition: true
        property vector3d posePosition: Qt.vector3d(0, 0, 0)
        property list<quaternion> poseOrientations: [
            Qt.quaternion(Math.cos(-179 * Math.PI / 360), 0, Math.sin(-179 * Math.PI / 360), 0),
            Qt.quaternion(Math.cos(179 * Math.PI / 360), 0, Math.sin(179 * Math.PI / 360), 0)
        ]
    }
    CustomCamera { id: camera }
    Breezy.CameraController {
        id: controller
        camera: camera
        fovDetails: ({lensDistancePixels: 1, fullScreenDistancePixels: 100})
    }
    Component.onCompleted: Qt.callLater(function() {
        const rates = controller.ratesOfChange(effect.poseOrientations);
        if (!rates || Math.abs(rates.yawDegrees - 2 / 16) > 0.001
            || controller.lookAheadMS(Date.now() - 1000, effect.lookAheadConfig, -1) !== 40) {
            console.error("FAIL actual CameraController: quaternion/Qt sequence/cap integration");
            Qt.exit(1);
            return;
        }
        effect.poseTimeElapsedMs = 0;
        if (controller.ratesOfChange(effect.poseOrientations) !== null) {
            console.error("FAIL actual CameraController: zero interval");
            Qt.exit(1);
            return;
        }
        console.log("PASS actual CameraController: wrapped Qt quaternions, typed list configuration, prediction cap, zero interval");
        Qt.exit(0);
    })
}
