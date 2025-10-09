import os
import subprocess
import re
import gzip

from pathlib import Path

# Try to access PlatformIO/SCons build environment to read CPPDEFINES
try:
	from SCons.Script import Import  # type: ignore
	Import('env')  # provides "env" from PlatformIO
except Exception:
	env = None

# Absolute path to npm and node
npm_cmd = r'C:\Program Files\nodejs\npm.cmd'
node_exe = r'C:\Program Files\nodejs\node.exe'

# Note: If "vite" is not found, run "npm install" in the webui folder first!
# Automatic installation if node_modules is missing:
webui_path = 'webui'
subprocess.run([npm_cmd, 'install'], cwd=webui_path, check=True)

def _get_define_value_from_scons(defines, name: str):
	if not defines:
		return None
	for d in defines:
		if isinstance(d, tuple) and len(d) == 2:
			k, v = d
			if k == name:
				return v
		elif isinstance(d, str):
			# bare define like 'NAME' -> treat as truthy
			if d == name:
				return 1
	return None

def flag_enabled(name: str) -> bool:
	# Prefer PlatformIO's SCons env if available
	if env is not None:
		try:
			defines = env.get('CPPDEFINES', [])
			val = _get_define_value_from_scons(defines, name)
			if val is not None:
				return str(val).lower() in ('1', 'true', 'yes', 'on')
		except Exception:
			pass
	# Fallback: inspect OS environment aggregates (may be empty in PIO)
	build_flags = os.environ.get('PLATFORMIO_BUILD_FLAGS', '') + ' ' + os.environ.get('BUILD_FLAGS', '')
	return re.search(rf'{re.escape(name)}=1(\s|$)', build_flags) is not None

# Map firmware flags to frontend env vars
feature_env = {
	'VITE_ENABLE_WS_PUSH': '1' if flag_enabled('CM_ENABLE_WS_PUSH') else '0',
	'VITE_ENABLE_RUNTIME_INT_SLIDERS': '1' if flag_enabled('CM_ENABLE_RUNTIME_INT_SLIDERS') else '0',
	'VITE_ENABLE_RUNTIME_FLOAT_SLIDERS': '1' if flag_enabled('CM_ENABLE_RUNTIME_FLOAT_SLIDERS') else '0',
	'VITE_ENABLE_RUNTIME_STATE_BUTTONS': '1' if flag_enabled('CM_ENABLE_RUNTIME_STATE_BUTTONS') else '0',
	'VITE_ENABLE_SYSTEM_PROVIDER': '1' if flag_enabled('CM_ENABLE_SYSTEM_PROVIDER') else '0',
}

def maybe_stub(rel_path: str):
	full = Path(webui_path) / rel_path
	if not full.exists():
		return
	# Minimal valid Vue 3 SFC stub without exports (script setup must not export default)
	stub_code = """<template></template>
<script setup>
// stubbed because feature disabled at build time
</script>
"""
	full.write_text(stub_code, encoding='utf-8')

# If feature disabled -> stub component (pre-build pruning)
# NOTE: Disabled to avoid overwriting local component edits; uncomment if you want automatic stubbing
# if feature_env['VITE_ENABLE_RUNTIME_INT_SLIDERS'] == '0' and feature_env['VITE_ENABLE_RUNTIME_FLOAT_SLIDERS'] == '0':
#     maybe_stub('src/components/runtime/RuntimeSlider.vue')
# if feature_env['VITE_ENABLE_RUNTIME_STATE_BUTTONS'] == '0':
#     maybe_stub('src/components/runtime/RuntimeStateButton.vue')

# Prepare environment
env_vars = os.environ.copy()
env_vars.update(feature_env)

# 1. Build Vue app with feature env
subprocess.run([npm_cmd, 'run', 'build'], cwd='webui', check=True, env=env_vars)

# 1b. Gzip compress dist/index.html to reduce flash footprint before header conversion
dist_index = Path(webui_path) / 'dist' / 'index.html'
if dist_index.exists():
	raw = dist_index.read_bytes()
	gz_path = dist_index.with_suffix('.html.gz')
	with gzip.open(gz_path, 'wb', compresslevel=9) as gz:
		gz.write(raw)
	# Optionally replace original with compressed if header generator expects index.html
	# Here we keep both; webui_to_header.js could be adapted to pick .gz

# 2. Run Node.js script to convert (consider adapting to use .gz later)
subprocess.run([node_exe, 'webui_to_header.js'], cwd='.', check=True)
