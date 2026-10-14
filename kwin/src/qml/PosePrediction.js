function shortestAngleDelta(end, start) {
    const delta = end - start;
    return ((delta + 180) % 360 + 360) % 360 - 180;
}

function angularRates(end, start, elapsed) {
    if (!end || !start || !Number.isFinite(elapsed) || elapsed <= 0) return null;
    for (const axis of ["x", "y", "z"]) {
        if (!Number.isFinite(end[axis]) || !Number.isFinite(start[axis])) return null;
    }
    const pitch = shortestAngleDelta(end.x, start.x) / elapsed;
    const yaw = shortestAngleDelta(end.y, start.y) / elapsed;
    const roll = shortestAngleDelta(end.z, start.z) / elapsed;
    if (![pitch, yaw, roll].every(Number.isFinite)) return null;
    return {eulerEnd: end, eulerStart: start,
        pitchDegrees: pitch, yawDegrees: yaw, rollDegrees: roll,
        pitch: pitch * Math.PI / 180, yaw: yaw * Math.PI / 180, roll: roll * Math.PI / 180};
}

function predictionMs(timestamp, config, override, now) {
    if (!config || config.length < 4 || !Number.isFinite(timestamp) || !Number.isFinite(now)) return 0;
    const base = override === -1 ? config[0] : override;
    if (!Number.isFinite(base)) return 0;
    let horizon = Math.max(0, base + Math.max(0, now - timestamp));
    // The advertised device cap also bounds explicit overrides.
    if (Number.isFinite(config[3]) && config[3] > 0) horizon = Math.min(horizon, config[3]);
    return Number.isFinite(horizon) ? horizon : 0;
}
