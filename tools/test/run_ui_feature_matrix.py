#!/usr/bin/env python3
"""Build the same isolated ESP32 consumer in four UI modes, without uploads.

Requires existing PlatformIO/Node dependencies and an existing PubSubClient copy.
Artifacts are kept under .local for review; no repository configuration is edited.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import zlib

ROOT = Path(__file__).resolve().parents[2]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(command, cwd, log, env=None):
    result = subprocess.run(command, cwd=cwd, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors='replace')
    log.write_text(result.stdout, encoding='utf-8')
    if result.returncode:
        print(result.stdout[-10000:])
        raise RuntimeError(f'Command failed ({result.returncode}); see {log}')
    return result.stdout


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mqtt-library', required=True, type=Path, help='Existing PubSubClient directory; never downloaded.')
    parser.add_argument('--output', type=Path, default=ROOT / '.local' / 'ui-feature-matrix')
    parser.add_argument('--modes', nargs='+', default=['11', '01', '10', '00'])
    parser.add_argument('--pio', default='pio')
    parser.add_argument('--dependencies', type=Path, default=ROOT / '.pio' / 'libdeps' / 'usb')
    args = parser.parse_args()
    if any(mode not in ('11', '01', '10', '00') for mode in args.modes):
        parser.error('Modes must be 11, 01, 10 or 00.')
    if not (args.mqtt_library / 'src' / 'PubSubClient.h').is_file():
        parser.error('An existing PubSubClient dependency is required.')
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    consumer = output / 'consumer'
    libdeps = consumer / '.pio' / 'libdeps' / 'usb'
    packages = consumer / 'packages'
    library = packages / 'ESP32 Configuration Manager'
    library.mkdir(parents=True, exist_ok=True)
    for dependency in ('ArduinoJson', 'AsyncTCP-esphome', 'ESPAsyncWebServer-esphome'):
        source = args.dependencies / dependency
        if not source.is_dir():
            raise RuntimeError(f'Existing dependency missing: {source}')
        shutil.copytree(source, packages / dependency, dirs_exist_ok=True)
    shutil.copytree(args.mqtt_library, packages / 'PubSubClient', dirs_exist_ok=True)
    for directory in ('src', 'webui'):
        shutil.copytree(ROOT / directory, library / directory, dirs_exist_ok=True,
                        ignore=shutil.ignore_patterns('node_modules', 'dist', '.vite'))
    (library / 'tools').mkdir(exist_ok=True)
    for name in ('preCompile_script.py', 'webui_features.py', 'webui_to_header.js'):
        shutil.copy2(ROOT / 'tools' / name, library / 'tools' / name)
    shutil.copy2(ROOT / 'library.json', library / 'library.json')
    (consumer / 'src').mkdir(exist_ok=True)
    shutil.copy2(ROOT / 'test' / 'fixtures' / 'ui-feature-consumer' / 'main.cpp', consumer / 'src' / 'main.cpp')
    env = os.environ.copy()
    env['CM_LOCAL_LIB'] = str(ROOT)
    dependency_lines = '\n  '.join('file://' + (packages / name).as_posix() for name in
        ('ArduinoJson', 'AsyncTCP-esphome', 'ESPAsyncWebServer-esphome', 'PubSubClient', 'ESP32 Configuration Manager'))
    library = libdeps / 'ESP32 Configuration Manager'
    source_files = list((ROOT / 'src').rglob('*.h')) + list((ROOT / 'src').rglob('*.cpp')) + list((ROOT / 'webui' / 'src').rglob('*'))
    source_files += [ROOT / 'webui' / 'vite.config.mjs', ROOT / 'webui' / 'package.json', ROOT / 'webui' / 'package-lock.json']
    source_files += [ROOT / 'tools' / name for name in ('preCompile_script.py', 'webui_features.py', 'webui_to_header.js', 'precompile_wrapper.py')]
    source_files += [ROOT / 'library.json', ROOT / 'tools' / 'precompile_wrapper_template.py']
    source_files += [ROOT / 'examples' / name / 'tools' / 'precompile_wrapper.py' for name in ('SolarInverterLimiter', 'BoilerSaver')]
    source_manifest = {p.relative_to(ROOT).as_posix(): sha(p) for p in source_files
                       if p.is_file() and p != ROOT / 'src' / 'html_content.h'}
    (output / 'source-freeze.json').write_text(json.dumps(source_manifest, indent=2) + '\n')
    results = {}
    previous = output / 'matrix.json'
    if previous.exists():
        results = json.loads(previous.read_text()).get('modes', {})
    for mode in args.modes:
        destination = output / mode
        destination.mkdir(exist_ok=True)
        flags = ['-std=gnu++17', '-DCM_ENABLE_LOGGING=1', '-DCM_ENABLE_VERBOSE_LOGGING=0', '-Ddevelopment',
                 f'-DCM_ENABLE_LIVE_UI={mode[0]}', f'-DCM_ENABLE_SETTINGS_UI={mode[1]}']
        ini = ('[env:usb]\nplatform = platformio/espressif32@7.0.1\nboard = nodemcu-32s\nframework = arduino\n'
               'build_unflags = -std=gnu++11\nbuild_flags =\n  ' + '\n  '.join(flags) + '\n'
               'lib_deps =\n  ' + dependency_lines + '\n'
               f'extra_scripts = pre:{(ROOT / "tools" / "precompile_wrapper.py").as_posix()}\n')
        (consumer / 'platformio.ini').write_text(ini)
        command = [args.pio, 'run', '-e', 'usb']
        build = run(command, consumer, destination / 'build.log', env)
        elf = consumer / '.pio' / 'build' / 'usb' / 'firmware.elf'
        binary = elf.with_suffix('.bin')
        shutil.copy2(elf, destination / 'firmware.elf')
        shutil.copy2(binary, destination / 'firmware.bin')
        shutil.copy2(library / 'src' / 'html_content.h', destination / 'html_content.h')
        dist = destination / 'dist'
        if dist.exists():
            assert dist.resolve().parent == destination.resolve() and destination.parent == output
            shutil.rmtree(dist)
        shutil.copytree(library / 'webui' / 'dist', dist)
        header = (destination / 'html_content.h').read_text()
        payload = bytes(int(value, 16) for value in re.findall(r'0x([0-9a-f]{2})', header))
        page = zlib.decompress(payload, 16 + zlib.MAX_WBITS)
        (destination / 'inlined.html').write_bytes(page)
        assert f'#define CM_WEBUI_LIVE_UI {mode[0]}' in header
        assert f'#define CM_WEBUI_SETTINGS_UI {mode[1]}' in header
        run([args.pio, 'run', '-e', 'usb', '-t', 'compiledb'], consumer, destination / 'compiledb.log', env)
        database = json.loads((consumer / 'compile_commands.json').read_text())
        web_command = next(item['command'] for item in database if item['file'].replace('\\', '/').endswith('/web/WebServer.cpp'))
        preprocess = re.sub(r' -o (?:"[^"]*"|\S+)', '', web_command).replace(' -c ', ' -E ')
        preprocessed = destination / 'webserver.ii'
        run(preprocess + f' -o "{preprocessed}"', consumer, destination / 'preprocess.log', env)
        compiled = preprocessed.read_text(errors='replace')
        routes = sorted(set(re.findall(r'(?:server->on|AsyncCallbackJsonWebHandler)\("([^"\n]+)"', compiled)))
        for path in ('/config_raw', '/config.json', '/config/apply', '/config/save', '/config/apply_all', '/config/save_all', '/config/reset', '/config/password', '/dev/export'):
            assert (path in routes) == (mode[1] == '1'), (mode, path, routes)
        for path in ('/runtime.json', '/runtime_meta.json', '/live_pages.json', '/live_layout.json'):
            assert (path in routes) == (mode[0] == '1'), (mode, path)
        for path in ('/config/auth', '/ui_capabilities', '/appinfo', '/reboot'):
            assert path in routes, (mode, path)
        for member in ('runtimeProviders', 'runtimeMetadataState', 'runtimeMetaJsonCache', 'runtimeButtons', 'livePages', 'livePlacements', 'wsSubscriptions'):
            assert bool(re.search(r'\b' + member + r'\b', compiled)) == (mode[0] == '1'), (mode, member)
        for member in ('settingsPages', 'settingsPlacements', 'categoryPrettyName'):
            assert bool(re.search(r'\b' + member + r'\b', compiled)) == (mode[1] == '1'), (mode, member)
        assert 'runtimeAlarms' in compiled and 'registeredStorageKeys' in compiled
        graph_env = env.copy()
        graph_env.update({'NODE_ENV': 'development', 'VITE_CM_ENABLE_LIVE_UI': mode[0], 'VITE_CM_ENABLE_SETTINGS_UI': mode[1],
                          'VITE_CM_ENABLE_LOGGING': '1', 'VITE_CM_ENABLE_VERBOSE_LOGGING': '0'})
        run(['node', str(ROOT / 'tools' / 'test' / 'ui_feature_bundle_graph.mjs'), str(library / 'webui'), str(destination / 'bundle-graph.json')],
            ROOT, destination / 'bundle-graph.log', graph_env)
        graph = json.loads((destination / 'bundle-graph.json').read_text())
        for component in ('RuntimeDashboard.vue', 'runtimeDataClient.mjs'):
            assert any(name.endswith('/' + component) for name in graph['modules']) == (mode[0] == '1'), (mode, component)
        assert any(name.endswith('/runtime-controls.css') for name in graph['modules']) == (mode != '00')
        assert any(name.endswith('/theme.css') for name in graph['modules'])
        for component in ('App.vue', 'Category.vue', 'Setting.vue'):
            assert any(name.endswith('/' + component) for name in graph['modules']) == (mode[1] == '1'), (mode, component)
        assert any(name.endswith('/OtaController.vue') for name in graph['modules'])
        toolchain = Path.home() / '.platformio' / 'packages' / 'toolchain-xtensa-esp32' / 'bin'
        nm = toolchain / ('xtensa-esp32-elf-nm.exe' if os.name == 'nt' else 'xtensa-esp32-elf-nm')
        symbols = run([str(nm), '-S', '-C', '--defined-only', str(elf)], ROOT, destination / 'symbols.txt')
        if mode[0] == '0':
            for symbol in ('CompactRuntimeMetaStore::', 'RuntimeMetadataState::', 'ConfigManagerWeb::setupRuntimeActionRoutes', 'GuiOutput::makePayload_'):
                assert symbol not in symbols, (mode, symbol)
        size = re.search(r'RAM:.*?used (\d+) bytes.*?Flash:.*?used (\d+) bytes', build, re.S)
        if not size:
            raise RuntimeError('PlatformIO size evidence missing.')
        results[mode] = {'command': command, 'compiler_flags': flags, 'firmware_sha256': sha(destination / 'firmware.elf'),
                         'frontend_node_env': 'development',
                         'header_sha256': sha(destination / 'html_content.h'), 'gzip_sha256': hashlib.sha256(payload).hexdigest(),
                         'gzip_bytes': len(payload), 'inlined_minified_bytes': len(page),
                         'js_bytes': sum(p.stat().st_size for p in (destination / 'dist' / 'assets').glob('*.js')),
                         'css_bytes': sum(p.stat().st_size for p in (destination / 'dist' / 'assets').glob('*.css')),
                         'unminified_js_bytes': graph['unminified_js_bytes'], 'unminified_css_bytes': graph['unminified_css_bytes'],
                         'source_freeze_sha256': sha(output / 'source-freeze.json'),
                         'static_ram_bytes': int(size.group(1)), 'flash_bytes': int(size.group(2)),
                         'firmware_bin_bytes': binary.stat().st_size, 'routes': routes,
                         'note': 'Build and compile-elimination evidence only; no device heap or hardware claim.'}
        previous.write_text(json.dumps({'consumer_sha256': sha(consumer / 'src' / 'main.cpp'), 'modes': results}, indent=2) + '\n')
        print(mode, json.dumps({key: results[mode][key] for key in ('static_ram_bytes', 'flash_bytes', 'gzip_bytes')}), flush=True)


if __name__ == '__main__':
    main()
