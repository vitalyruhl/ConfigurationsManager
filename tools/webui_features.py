"""Resolve compiler/UI flags and fingerprint the exact embedded UI inputs."""
import hashlib
import json
import os
from pathlib import Path

DEFAULTS = {'CM_ENABLE_LIVE_UI': '1', 'CM_ENABLE_SETTINGS_UI': '1',
            'CM_EMBED_WEBUI': '1', 'CM_ENABLE_LOGGING': '1', 'CM_ENABLE_VERBOSE_LOGGING': '0'}


def feature_values(build_env=None, environ=None):
    environ = os.environ if environ is None else environ
    values = {key: str(environ.get(key, environ.get('VITE_' + key, default)))
              for key, default in DEFAULTS.items()}
    if build_env is not None:
        values = DEFAULTS.copy()
        defines = list(build_env.get('CPPDEFINES', []))
        if hasattr(build_env, 'ParseFlags'):
            defines = list(build_env.ParseFlags(build_env.get('BUILD_FLAGS', '')).get('CPPDEFINES', [])) + defines
        for define in defines:
            if isinstance(define, (tuple, list)) and len(define) == 2 and define[0] in values:
                values[define[0]] = str(define[1])
            elif isinstance(define, str) and define in values:
                values[define] = '1'
    for key, value in values.items():
        if value not in ('0', '1'):
            raise ValueError(f'{key} must be 0 or 1')
    return values


def source_fingerprint(root, features):
    root = Path(root)
    digest = hashlib.sha256(json.dumps(features, sort_keys=True).encode())
    files = [path for path in (root / 'webui').rglob('*')
             if path.is_file() and not {'node_modules', 'dist', '.vite'}.intersection(path.relative_to(root / 'webui').parts)]
    files += [root / 'tools' / name for name in ('preCompile_script.py', 'webui_to_header.js', 'webui_features.py')]
    for path in sorted(files):
        digest.update(path.relative_to(root).as_posix().encode())
        digest.update(path.read_bytes())
    return digest.hexdigest()
