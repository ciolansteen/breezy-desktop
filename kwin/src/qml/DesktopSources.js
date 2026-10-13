.pragma library

function resolveCaptureSource(screen, targetScreen, sourceOutputName, workspaceScreens) {
    if (screen !== targetScreen || !sourceOutputName) return screen;
    return workspaceScreens.find(candidate => candidate.name === sourceOutputName) || screen;
}

function workspaceIndex(displayIndex, captureScreens, workspaceScreens) {
    if (displayIndex < 0 || displayIndex >= captureScreens.length) return -1;
    return workspaceScreens.indexOf(captureScreens[displayIndex]);
}
