"""[MOCKED!] SCons environment; production feature resolution and hashing."""
import importlib.util
from pathlib import Path
import tempfile
import unittest
import shutil
import json
import hashlib
import os
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('webui_features', ROOT / 'tools' / 'webui_features.py')
features = importlib.util.module_from_spec(spec)
spec.loader.exec_module(features)


class FeatureContracts(unittest.TestCase):
    def test_defaults_and_all_four_global_modes(self):
        self.assertEqual(features.feature_values(environ={})['CM_ENABLE_LIVE_UI'], '1')
        for live in ('0', '1'):
            for settings in ('0', '1'):
                flags = {'CPPDEFINES': [('CM_ENABLE_LIVE_UI', live), ('CM_ENABLE_SETTINGS_UI', settings)]}
                result = features.feature_values(flags, {})
                self.assertEqual((result['CM_ENABLE_LIVE_UI'], result['CM_ENABLE_SETTINGS_UI']), (live, settings))
                self.assertEqual(features.feature_values(environ={'VITE_CM_ENABLE_LIVE_UI': live, 'CM_ENABLE_SETTINGS_UI': settings}), result)

    def test_invalid_flags_fail_before_building(self):
        with self.assertRaises(ValueError):
            features.feature_values(environ={'CM_ENABLE_LIVE_UI': 'false'})

    def test_global_compiler_defaults_override_unrelated_shell_environment(self):
        self.assertEqual(features.feature_values({'CPPDEFINES': []}, {'VITE_CM_ENABLE_LIVE_UI': '0'})['CM_ENABLE_LIVE_UI'], '1')

    def test_no_node_required_when_embedding_is_off_or_cache_matches(self):
        local = ROOT / '.local'
        local.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='ui-feature-precompile-', dir=local) as directory:
            root = Path(directory)
            for name in ('webui', 'src', 'tools', '.pio'):
                (root / name).mkdir()
            for name in ('preCompile_script.py', 'webui_features.py', 'webui_to_header.js'):
                shutil.copy2(ROOT / 'tools' / name, root / 'tools' / name)
            env = {**os.environ, 'PATH': '', 'CM_EMBED_WEBUI': '0'}
            result = subprocess.run([sys.executable, str(root / 'tools' / 'preCompile_script.py')], cwd=root, env=env, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            env['CM_EMBED_WEBUI'] = '1'
            values = features.feature_values(environ=env)
            header = root / 'src' / 'html_content.h'
            cache = {'fingerprint': features.source_fingerprint(root, {**values, 'NODE_ENV': env.get('NODE_ENV', 'production')}),
                     'header_sha256': hashlib.sha256(header.read_bytes()).hexdigest()}
            (root / '.pio' / 'cm-webui-build.json').write_text(json.dumps(cache))
            result = subprocess.run([sys.executable, str(root / 'tools' / 'preCompile_script.py')], cwd=root, env=env, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn('cache matches', result.stdout)

    def test_flags_and_source_changes_invalidate_cache(self):
        local = ROOT / '.local'
        local.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='ui-feature-tools-', dir=local) as directory:
            root = Path(directory)
            (root / 'webui').mkdir()
            (root / 'tools').mkdir()
            source = root / 'webui' / 'App.vue'
            source.write_text('first')
            for name in ('preCompile_script.py', 'webui_to_header.js', 'webui_features.py'):
                (root / 'tools' / name).write_text('test')
            values = features.feature_values(environ={})
            initial = features.source_fingerprint(root, values)
            self.assertNotEqual(initial, features.source_fingerprint(root, {**values, 'CM_ENABLE_LIVE_UI': '0'}))
            source.write_text('changed')
            self.assertNotEqual(initial, features.source_fingerprint(root, values))
            changed = features.source_fingerprint(root, values)
            (root / 'webui' / 'dist').mkdir()
            (root / 'webui' / 'dist' / 'index.html').write_text('generated')
            self.assertEqual(changed, features.source_fingerprint(root, values))


if __name__ == '__main__':
    unittest.main()
