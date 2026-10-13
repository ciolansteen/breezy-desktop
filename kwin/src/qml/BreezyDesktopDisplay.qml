import QtQuick
import QtQuick3D

Model {
    id: display

    required property var screen
    property var captureScreen: screen
    required property var sizeAdjustedScreen
    required property var monitorPlacement
    required property int index
    required property var fovDetails

    property string cursorImageSource: effect.cursorImageSource
    property size cursorImageSize: effect.cursorImageSize
    property point cursorPos: effect.cursorPos

    Displays {
        id: displays
    }

    // Default to simple rectangle source so we work on older Qt6
    // We'll attempt to dynamically load CurvableDisplayMesh.qml in onCompleted
    source: "#Rectangle"

    Component.onCompleted: {
        try {
            const component = Qt.createComponent(Qt.resolvedUrl("CurvableDisplayMesh.qml"), Component.PreferSynchronous);
            if (component.status === Component.Ready) {
                const mesh = component.createObject(display, {
                    fovDetails: Qt.binding(() => display.fovDetails),
                    monitorGeometry: Qt.binding(() => display.sizeAdjustedScreen ? display.sizeAdjustedScreen.geometry : null),
                    fovConversionFns: Qt.binding(() => displays.fovConversionFns)
                });
                if (mesh) {
                    display.source = "";
                    display.geometry = mesh;
                    effect.curvedDisplaySupported = true;
                }
            } else {
                console.error("Breezy - CurvableDisplayMesh not available:", component.errorString());
                effect.curvedDisplaySupported = false;
            }
        } catch (e) {
            console.error("Breezy - CurvableDisplayMesh loading error:", e);
            effect.curvedDisplaySupported = false;
        }
    }
    materials: [
        CustomMaterial {
            id: customMat
            depthDrawMode: CustomMaterial.AlwaysDepthDraw
            shadingMode: CustomMaterial.Unshaded

            property real screenWidth: display.captureScreen.geometry.width
            property real screenHeight: display.captureScreen.geometry.height
            property real cursorX: display.cursorPos.x - display.captureScreen.geometry.x
            property real cursorY: display.cursorPos.y - display.captureScreen.geometry.y
            property real cursorW: display.cursorImageSize.width
            property real cursorH: display.cursorImageSize.height
            property bool showCursor: cursorX >= 0 && cursorX < screenWidth && cursorY >= 0 && cursorY < screenHeight

            property TextureInput desktopTex: TextureInput {
                texture: Texture {
                    sourceItem: DesktopView {
                        screen: display.captureScreen
                        width: display.captureScreen.geometry.width
                        height: display.captureScreen.geometry.height
                    }
                }
            }
            property TextureInput cursorTex: TextureInput {
                texture: Texture {
                    sourceItem: Image {
                        source: effect.cursorImageSource
                        width: effect.cursorImageSize.width
                        height: effect.cursorImageSize.height
                    }
                }
            }

            fragmentShader: "cursorOverlay.frag"
            vertexShader: "cursorOverlay.vert"
        }
    ]
}
