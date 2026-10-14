#!/usr/bin/env python3
"""Real KWin reload test on private D-Bus, HOME and /dev/shm.

Synthetic driver data only; this does not validate live headset tracking.
Usage: python nested-reload-test.py BUILD_DIRECTORY SOURCE_DIRECTORY
"""
import os
import json
import pathlib
import shutil
import struct
import subprocess
import sys
import tempfile
import time


def inside():
    from PySide6.QtCore import QCoreApplication
    from PySide6.QtDBus import QDBusConnection, QDBusMessage
    app = QCoreApplication([])
    log = pathlib.Path(os.environ['HOME']) / 'kwin.log'
    with log.open('w') as output:
        process = subprocess.Popen(['kwin_wayland', '--virtual', '--width', '640', '--height', '480', '--no-lockscreen', '--no-global-shortcuts', '--no-kactivities'], stdout=output, stderr=output)
        bus = QDBusConnection.sessionBus()
        path = '/com/xronlinux/BreezyDesktop'
        interface = 'com.xronlinux.BreezyDesktop'

        def call(method, arguments=()):
            result = subprocess.run([os.environ['KCM_RELOAD_TEST'], '--call', method, *map(str, arguments)], text=True, capture_output=True, timeout=6)
            assert result.returncode == 0, (method, result.stderr)
            return json.loads(result.stdout)

        status = {}
        try:
            deadline = time.monotonic() + 20
            while time.monotonic() < deadline:
                if process.poll() is not None:
                    raise AssertionError('KWin exited: ' + log.read_text())
                if bus.interface().isServiceRegistered('org.kde.KWin').value():
                    reply = bus.call(QDBusMessage.createMethodCall('org.kde.KWin', path, 'org.freedesktop.DBus.Introspectable', 'Introspect'))
                    if reply.type() != QDBusMessage.MessageType.ErrorMessage:
                        break
                time.sleep(.1)
            else:
                raise AssertionError('Breezy did not load: ' + log.read_text())
            introspection = reply.arguments()[0]
            assert 'ReloadScene' in introspection and 'ReloadStatus' in introspection, 'RED: verified scene reload API absent'
            # Initial inactive reload must not enable the driver.
            accepted = call('ReloadScene')
            assert isinstance(accepted, dict), repr(accepted)
            assert accepted['state'] == 'pending', accepted
            deadline = time.monotonic() + 5
            while time.monotonic() < deadline:
                app.processEvents()
                status = call('ReloadStatus')
                if status['state'] != 'pending':break
                time.sleep(.05)
            assert status['state'] == 'inactive', status
            assert status['loadedBuildId'] == status['installedBuildId'] and status['loadedBuildId'], status
            if os.environ.get('KCM_RELOAD_TEST'):
                subprocess.run([os.environ['KCM_RELOAD_TEST'], os.environ['KCM_RELOAD_PLUGIN']], check=True, timeout=15)
            # Enable the existing renderer using an explicitly synthetic IPC buffer.
            buffer = bytearray(186)
            buffer[0:2] = bytes([5, 1])
            struct.pack_into('<4f', buffer, 2, 0, 1, 0, 0)
            struct.pack_into('<2I', buffer, 18, 640, 480)
            struct.pack_into('<f', buffer, 26, 52)
            struct.pack_into('<f', buffer, 30, .05)
            struct.pack_into('<4f', buffer, 37, 0, 0, 0, 1)
            struct.pack_into('<4f', buffer, 121, .01, 0, 0, .99995)
            struct.pack_into('<4f', buffer, 137, .01, 0, 0, .99995)
            struct.pack_into('<4f', buffer, 169, 32, 16, 0, 0)
            # No identity/reset quaternion: avoids synthetic recenter side effects.
            def publish():
                struct.pack_into('<Q', buffer, 113, int(time.time() * 1000))
                parity = 0
                for value in buffer[113:185]:parity ^= value
                buffer[185] = parity
                pathlib.Path('/dev/shm/breezy_desktop_imu').write_bytes(buffer)
            publish()
            for _ in range(20):
                publish();time.sleep(.05)
            for size in ((640, 480), (800, 480), (640, 600), (800, 600)):
                call('AddVirtualDisplay', size)
            time.sleep(.1)
            before = sorted(call('ListVirtualDisplays'), key=lambda entry: entry['id'])
            assert len(before) == 4, before
            assert [entry['id'] for entry in before] == ['BreezyDesktop_1', 'BreezyDesktop_2', 'BreezyDesktop_3', 'BreezyDesktop_4'], before
            # The XR lifecycle must remember virtual outputs across disable/enable while removing
            # them from KWin during disable.
            buffer[1] = 0
            deadline = time.monotonic() + 4
            while time.monotonic() < deadline:
                publish();app.processEvents();time.sleep(.05)
                if call('ListVirtualDisplays') == []:break
            assert call('ListVirtualDisplays') == [], 'disable did not remove virtual outputs'
            profile = pathlib.Path(os.environ['XDG_STATE_HOME']) / 'breezy_desktop/virtual-displays.json'
            assert profile.exists(), 'disable did not persist virtual display profile'
            saved = json.loads(profile.read_text())
            assert [entry['id'] for entry in saved['entries']] == [entry['id'] for entry in before], saved
            profile.write_text('{ deliberately corrupt profile')
            buffer[1] = 1
            deadline = time.monotonic() + 5
            while time.monotonic() < deadline:
                publish();app.processEvents();time.sleep(.05)
                if call('ListVirtualDisplays'):break
            assert call('ListVirtualDisplays') == [], 'corrupt profile must not restore virtual outputs'
            buffer[1] = 0
            deadline = time.monotonic() + 4
            while time.monotonic() < deadline:
                publish();app.processEvents();time.sleep(.05)
                if pathlib.Path(profile).read_text() == '{ deliberately corrupt profile':break
            assert profile.read_text() == '{ deliberately corrupt profile', 'corrupt profile was overwritten'
            profile.write_text(json.dumps(saved))
            buffer[1] = 1
            deadline = time.monotonic() + 5
            restored = []
            while time.monotonic() < deadline:
                publish();app.processEvents();time.sleep(.05)
                restored = sorted(call('ListVirtualDisplays'), key=lambda entry: entry['id'])
                if len(restored) == 4:break
            assert restored == before, (before, restored)
            for attempt in range(3):
                pair = subprocess.run([os.environ['KCM_RELOAD_TEST'], '--busy'], text=True, capture_output=True, timeout=6)
                assert pair.returncode == 0, pair.stderr
                accepted, busy = json.loads(pair.stdout)
                assert accepted['state'] == 'pending', accepted
                assert busy['state'] == 'busy', busy
                deadline = time.monotonic() + 5
                while time.monotonic() < deadline:
                    publish();app.processEvents();time.sleep(.03)
                    status = call('ReloadStatus')
                    if status['state'] != 'pending':break
                assert status['state'] == 'ready' and status['outputsPreserved'], status
                assert sorted(call('ListVirtualDisplays'), key=lambda entry: entry['id']) == before, 'virtual output handles changed'
                assert process.poll() is None, 'KWin crashed'
            qml = pathlib.Path(os.environ['XDG_DATA_HOME']) / 'kwin/effects/breezy_desktop/qml/main.qml'
            original = qml.read_text()
            qml.write_text('This is deliberately invalid QML for the isolated negative test.\n')
            call('ReloadScene')
            deadline = time.monotonic() + 5
            while time.monotonic() < deadline:
                publish();time.sleep(.03)
                status = call('ReloadStatus')
                if status['state'] != 'pending':break
            assert status['state'] == 'error' and not status['running'], status
            assert sorted(call('ListVirtualDisplays'), key=lambda entry: entry['id']) == before, 'QML failure deleted virtual displays'
            qml.write_text(original)
            call('ReloadScene')
            deadline = time.monotonic() + 5
            while time.monotonic() < deadline:
                publish();time.sleep(.03)
                status = call('ReloadStatus')
                if status['state'] != 'pending':break
            assert status['state'] == 'ready' and status['outputsPreserved'], status
            assert sorted(call('ListVirtualDisplays'), key=lambda entry: entry['id']) == before
            loadedId = status['loadedBuildId']
            plugin = pathlib.Path(status['pluginPath'])
            replacement = plugin.with_suffix('.updated.so')
            shutil.copy2(os.environ['BUILDID_UPGRADE_FIXTURE'], replacement)
            os.replace(replacement, plugin)
            status = call('ReloadStatus')
            assert status['loadedBuildId'] == loadedId and not status['binaryCurrent'], status
            assert status['installedBuildId'] and status['installedBuildId'] != loadedId, status
            subprocess.run([os.environ['KCM_RELOAD_TEST'], '--expect-stale', os.environ['KCM_RELOAD_PLUGIN']], check=True, timeout=15)
            print('PASS real isolated KWin: inactive reload, build identity, KCM click, busy guard, 3 repeated reloads, four virtual outputs, fresh QML error detection and recovery')
        finally:
            process.terminate()
            try:process.wait(timeout=5)
            except subprocess.TimeoutExpired:process.kill();process.wait()
            print(log.read_text()[-5000:])


def outside():
    build, source = map(pathlib.Path, sys.argv[1:3])
    # Source-order contract, not a simulated live driver race.
    cpp = (source / 'kwin/src/breezydesktopeffect.cpp').read_text()
    body = cpp.split('QVariantMap BreezyDesktopEffect::reloadScene()', 1)[1].split('BreezyDesktopEffect::BreezyDesktopEffect()', 1)[0]
    before, queued = body.split('QTimer::singleShot(0, this,', 1)
    assert 'const bool wasRunning' not in before, 'driver state captured before queued execution'
    assert 'wasRunning' not in queued.split(']()', 1)[0], 'stale driver state captured by lambda'
    assert queued.index('const bool wasRunning') < queued.index('setRunning(false)'), 'state snapshot after stopping'

    with tempfile.TemporaryDirectory(prefix='breezy-reload-test-') as name:
        root = pathlib.Path(name)
        data = root / 'share'
        effect = data / 'kwin/effects/breezy_desktop'
        effect.mkdir(parents=True)
        shutil.copytree(source / 'kwin/src/qml', effect / 'qml')
        # Fixture runner intentionally replaces driver I/O only in the sandbox.
        (effect / 'xrdriveripc.py').write_text('# synthetic test fixture\n')
        (effect / 'xrdriveripc_runner.py').write_text("import json,os\nprint(json.dumps({'connected_device_pose_has_position':False} if os.getenv('BREEZY_METHOD')=='retrieve_driver_state' else {}))\n")
        plugins = root / 'plugins/kwin/effects/plugins'
        plugins.mkdir(parents=True)
        shutil.copy2(build / 'bin/kwin/effects/plugins/breezy_desktop.so', plugins / 'breezy_desktop.so')
        kcm = root / 'plugins/plasma/kcms/breezy_desktop_config.so'
        kcm.parent.mkdir(parents=True)
        shutil.copy2(build / 'bin/plasma/kcms/breezy_desktop_config.so', kcm)
        runtime = root / 'runtime';runtime.mkdir(mode=0o700)
        config = root / 'config';config.mkdir()
        (config / 'kwinrc').write_text('[Plugins]\nbreezy_desktopEnabled=true\n[Effect-breezy_desktop]\nDeveloperMode=true\nRemoveVirtualDisplaysOnDisable=true\nAntialiasingQuality=0\n')
        env = dict(os.environ, HOME=str(root), XDG_CONFIG_HOME=str(config), XDG_DATA_HOME=str(data), XDG_STATE_HOME=str(root/'state'), XDG_CACHE_HOME=str(root/'cache'), XDG_RUNTIME_DIR=str(runtime), QT_PLUGIN_PATH=str(root/'plugins')+':/usr/lib/qt6/plugins', XDG_SESSION_CLASS='user', QT_QPA_PLATFORM='offscreen', LIBGL_ALWAYS_SOFTWARE='1', QSG_RHI_BACKEND='opengl')
        for key in ('DBUS_SESSION_BUS_ADDRESS','WAYLAND_DISPLAY','DISPLAY'):env.pop(key,None)
        if (build / 'bin/kcm-reload-test').exists():
            env.update(KCM_RELOAD_TEST=str(build / 'bin/kcm-reload-test'), KCM_RELOAD_PLUGIN=str(kcm), BUILDID_UPGRADE_FIXTURE=str(build / 'bin/buildid-fixture2.so'))
        command = ['bwrap','--unshare-all','--ro-bind','/','/','--tmpfs','/tmp','--bind',str(root),str(root),'--dev','/dev','--tmpfs','/dev/shm','--','dbus-run-session','--',sys.executable,str(pathlib.Path(__file__).resolve()),'--inside']
        result = subprocess.run(command, env=env, timeout=70)
        raise SystemExit(result.returncode)

if __name__ == '__main__':
    if sys.argv[1:] == ['--inside']:inside()
    else:outside()
