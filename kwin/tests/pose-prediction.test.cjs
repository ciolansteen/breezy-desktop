const {test} = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const context = vm.createContext({});
vm.runInContext(fs.readFileSync(path.join(__dirname, '../src/qml/PosePrediction.js'), 'utf8'), context);
const zero = {x: 0, y: 0, z: 0};

test('wrapped yaw and roll use the shortest signed delta', () => {
    const rates = context.angularRates({x: 0, y: -179, z: 179}, {x: 0, y: 179, z: -179}, 16);
    assert.equal(rates.yawDegrees, 2 / 16);
    assert.equal(rates.rollDegrees, -2 / 16);
});
test('ordinary motion keeps the original rates', () => {
    const rates = context.angularRates({x: 4, y: 8, z: -12}, zero, 16);
    assert.equal(rates.pitchDegrees, 4 / 16);
    assert.equal(rates.yawDegrees, 8 / 16);
    assert.equal(rates.rollDegrees, -12 / 16);
});
test('missing, zero and invalid sample intervals cannot poison camera matrices', () => {
    for (const dt of [0, -1, NaN, Infinity]) assert.equal(context.angularRates(zero, zero, dt), null);
    assert.equal(context.angularRates(null, zero, 16), null);
    assert.equal(context.angularRates({x: NaN, y: 0, z: 0}, zero, 16), null);
});
test('default and override prediction include valid host age', () => {
    assert.equal(context.predictionMs(1000, [10, 1, 8, 40], -1, 1005), 15);
    assert.equal(context.predictionMs(1000, [10, 1, 8, 40], 20, 1005), 25);
});
test('stale data and override respect the existing device prediction cap', () => {
    assert.equal(context.predictionMs(1000, [10, 1, 8, 40], -1, 2000), 40);
    assert.equal(context.predictionMs(1000, [10, 1, 8, 40], 100, 1005), 40);
});
test('a future host timestamp never creates negative extrapolation', () => {
    assert.equal(context.predictionMs(1005, [10, 1, 8, 40], -1, 1000), 10);
});
test('malformed clocks and configuration produce finite safe prediction', () => {
    assert.equal(context.predictionMs(NaN, [10, 1, 8, 40], -1, 1000), 0);
    assert.equal(context.predictionMs(1000, null, -1, 1005), 0);
    assert.equal(context.predictionMs(1000, [NaN, 1, 8, 40], -1, 1005), 0);
});
