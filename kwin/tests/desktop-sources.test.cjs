const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const qmlDir = path.join(__dirname, '../src/qml');
const helperPath = path.join(qmlDir, 'DesktopSources.js');
const helpers = {};
if (fs.existsSync(helperPath)) {
    vm.runInNewContext(fs.readFileSync(helperPath, 'utf8').replace(/^\.pragma library\s*/, ''), helpers);
}
function functionBody(text, name) {
    const start = text.indexOf(`function ${name}(`);
    assert.notEqual(start, -1);
    const begin = text.indexOf('{', start);
    let depth = 1;
    let end = begin + 1;
    while (depth && end < text.length) {
        if (text[end] === '{') depth++;
        if (text[end] === '}') depth--;
        end++;
    }
    assert.equal(depth, 0);
    return text.slice(start, end);
}
function runFocus(displayScreens, captureScreens, workspaceScreens, selected, state = {}) {
    const effect = state.effect || { poseOrientations: [{}], posePosition: { times: () => ({}) }, zoomOnFocusEnabled: false };
    const desktop = state.desktop || {
        fovDetails: { fullScreenDistancePixels: 1 },
        monitorPlacements: displayScreens.map(() => ({ centerLook: {} })),
        sizeAdjustedScreens: displayScreens,
        focusedMonitorIndex: -1,
        lookingAtMonitorIndex: -1
    };
    state.effect = effect;
    state.desktop = desktop;
    const context = {
        effect, breezyDesktop: desktop, screens: displayScreens, captureScreens,
        smoothFollowEnabled: false,
        DesktopSources: helpers,
        KWinComponents: { Workspace: { screens: workspaceScreens } },
        displays: { eusToNwuQuat: x => x, eusToNwuVector: x => x, findFocusedMonitor: () => selected }
    };
    vm.runInNewContext(functionBody(fs.readFileSync(path.join(qmlDir, 'BreezyDesktop.qml'), 'utf8'), 'updateFocus') + '\nupdateFocus();', context);
    return effect.lookingAtScreenIndex;
}
const panel = { name: 'panel', model: 'Panel', geometry: { x: 0, y: 0, width: 1280, height: 800 } };
const glasses = { name: 'glasses', model: 'VITURE', geometry: { x: 1920, y: 1080, width: 1920, height: 1080 } };
const virtual = { name: 'Virtual-BreezyDesktop_1', geometry: { x: 3840, y: 1080, width: 1920, height: 1080 } };
const all = [panel, glasses, virtual];
let passed = 0;
function test(name, fn) { fn(); passed++; console.log(`PASS ${name}`); }
// Execute the production QML function, not a reimplementation of its indexing.
test('non-XR output before glasses maps focused display to workspace identity', () => {
    assert.equal(runFocus([glasses, virtual], [glasses, virtual], all, 0), 1);
});
test('content mirror directs central input focus to desktop source', () => {
    assert.equal(runFocus([glasses, virtual], [panel, virtual], all, 0), 0);
});
test('virtual focus survives reordered workspace outputs', () => {
    assert.equal(runFocus([glasses, virtual], [panel, virtual], [virtual, panel, glasses], 1), 0);
});
test('removed source never becomes a stale workspace index', () => {
    assert.equal(runFocus([glasses], [panel], [glasses], 0), -1);
});
test('invalid display indices fail closed', () => {
    assert.equal(runFocus([glasses], [glasses], all, 99), -1);
});
test('unchanged gaze re-resolves identity after output reorder', () => {
    const state = {};
    assert.equal(runFocus([glasses], [glasses], all, 0, state), 1);
    assert.equal(runFocus([glasses], [glasses], [glasses, panel, virtual], 0, state), 0);
});
test('blank source preserves ordinary capture', () => {
    assert.equal(helpers.resolveCaptureSource(glasses, glasses, '', all), glasses);
});
test('explicit source is resolved by name, not ordinal or assumed panel name', () => {
    assert.equal(helpers.resolveCaptureSource(glasses, glasses, 'panel', all), panel);
});
test('virtual capture is unchanged by central source selection', () => {
    assert.equal(helpers.resolveCaptureSource(virtual, glasses, 'panel', all), virtual);
});
test('missing source falls back to the original screen', () => {
    assert.equal(helpers.resolveCaptureSource(glasses, glasses, 'missing', all), glasses);
});
test('source replug resolves the new live object', () => {
    const replacement = { ...panel };
    assert.equal(helpers.resolveCaptureSource(glasses, glasses, 'panel', [replacement, glasses]), replacement);
});
test('source identity does not modify unequal placement dimensions', () => {
    const original = JSON.stringify(glasses.geometry);
    helpers.resolveCaptureSource(glasses, glasses, 'panel', all);
    assert.equal(JSON.stringify(glasses.geometry), original);
});
function screenFilter(source) {
    const text = fs.readFileSync(path.join(qmlDir, 'main.qml'), 'utf8');
    const start = text.indexOf('property var screens:');
    const begin = text.indexOf('function(screen)', start);
    const brace = text.indexOf('{', begin);
    let depth = 1, end = brace + 1;
    while (depth) {
        if (text[end] === '{') depth++;
        if (text[end] === '}') depth--;
        end++;
    }
    return vm.runInNewContext('(' + text.slice(begin, end) + ')', {
        targetScreenSupported: true, desktopMirrorSource: source, targetScreen: glasses,
        developerMode: false, mirrorPhysicalDisplays: true, supportedModels: ['VITURE']
    });
}
test('actual main QML filter does not duplicate the central capture source', () => {
    assert.deepEqual(all.filter(screenFilter(panel)), [glasses, virtual]);
});
test('actual main QML filter retains default physical mirroring behavior', () => {
    assert.deepEqual(all.filter(screenFilter(glasses)), all);
});
test('actual cursor shader bindings use capture rather than placement coordinates', () => {
    const text = fs.readFileSync(path.join(qmlDir, 'BreezyDesktopDisplay.qml'), 'utf8');
    const display = { screen: glasses, captureScreen: panel, cursorPos: { x: 640, y: 400 } };
    const value = name => vm.runInNewContext(text.match(new RegExp('property real ' + name + ': ([^\\n]+)'))[1], { display });
    assert.equal(value('cursorX') / value('screenWidth'), 0.5);
    assert.equal(value('cursorY') / value('screenHeight'), 0.5);
});
console.log(`${passed} behavioral tests passed`);
