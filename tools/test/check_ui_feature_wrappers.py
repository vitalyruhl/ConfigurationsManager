#!/usr/bin/env python3
"""Exercise each shipped wrapper through the existing isolated matrix consumer.

Requires run_ui_feature_matrix.py artifacts and existing dependencies. No upload,
package installation, or product configuration changes are performed.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--matrix', type=Path, default=ROOT / '.local' / 'ui-feature-matrix')
    parser.add_argument('--pio', default='pio')
    args = parser.parse_args()
    consumer = args.matrix.resolve() / 'consumer'
    ini = consumer / 'platformio.ini'
    original = ini.read_text()
    tools = consumer / 'tools'
    tools.mkdir(exist_ok=True)
    artifacts = args.matrix.resolve() / 'wrapper-contracts'
    artifacts.mkdir(exist_ok=True)
    wrappers = {'template': ROOT / 'tools' / 'precompile_wrapper_template.py',
                'solar-inverter-example': ROOT / 'examples' / 'SolarInverterLimiter' / 'tools' / 'precompile_wrapper.py',
                'boiler-example': ROOT / 'examples' / 'BoilerSaver' / 'tools' / 'precompile_wrapper.py'}
    results = {}
    environment = os.environ.copy()
    environment['CM_LOCAL_LIB'] = str(ROOT)
    try:
        for name, source in wrappers.items():
            wrapper = tools / (name + '.py')
            shutil.copy2(source, wrapper)
            config = re.sub(r'-DCM_ENABLE_LIVE_UI=[01]', '-DCM_ENABLE_LIVE_UI=0', original)
            config = re.sub(r'-DCM_ENABLE_SETTINGS_UI=[01]', '-DCM_ENABLE_SETTINGS_UI=1', config)
            config = re.sub(r'extra_scripts = .*', 'extra_scripts = pre:' + wrapper.as_posix(), config)
            ini.write_text(config)
            result = subprocess.run([args.pio, 'run', '-e', 'usb', '-t', 'compiledb'], cwd=consumer,
                                    env=environment, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                    text=True, errors='replace')
            (artifacts / (name + '.log')).write_text(result.stdout, encoding='utf-8')
            assert result.returncode == 0, name + ': see wrapper log'
            library = consumer / '.pio' / 'libdeps' / 'usb' / 'ESP32 Configuration Manager'
            header = (library / 'src' / 'html_content.h').read_bytes()
            assert b'#define CM_WEBUI_LIVE_UI 0' in header, name
            assert b'#define CM_WEBUI_SETTINGS_UI 1' in header, name
            results[name] = {'pass': True, 'mode': '01', 'header_sha256': hashlib.sha256(header).hexdigest()}
            print(name, 'SCons wrapper PASS', flush=True)
    finally:
        ini.write_text(original)
    (artifacts / 'results.json').write_text(json.dumps(results, indent=2) + '\n')


if __name__ == '__main__':
    main()
