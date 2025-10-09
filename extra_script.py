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
	# Fallback 1: inspect OS environment aggregates (may be empty in PIO)
	build_flags = (os.environ.get('PLATFORMIO_BUILD_FLAGS', '') + ' ' + os.environ.get('BUILD_FLAGS', '')).strip()
	if build_flags:
		return re.search(rf'(?:\s|^){re.escape(name)}=1(\s|$)', build_flags) is not None or re.search(rf'(?:\s|^)-D{re.escape(name)}(?:=1)?(\s|$)', build_flags) is not None

	# Fallback 2: parse platformio.ini for the active env (PIOENV)
	try:
		ini_path = Path('platformio.ini')
		if ini_path.exists():
			pioenv = os.environ.get('PIOENV') or os.environ.get('PLATFORMIO_ENV')
			if pioenv:
				defines = _parse_ini_defines_for_env(ini_path, pioenv)
			else:
				defines = _parse_ini_defines_all_envs(ini_path)
			if name in defines:
				v = defines.get(name)
				return (v is None) or (str(v).lower() in ('1', 'true', 'yes', 'on'))
	except Exception:
		pass
	return False

def _parse_ini_defines_for_env(ini_path: Path, env_name: str):
	"""Minimal parser to extract -DNAME[=VALUE] tokens from build_flags of [env:env_name]."""
	content = ini_path.read_text(encoding='utf-8', errors='ignore').splitlines()
	in_env = False
	collecting_flags = False
	tokens = []
	for line in content:
		s = line.strip()
		if s.startswith('[') and s.endswith(']'):
			in_env = (s == f'[env:{env_name}]')
			collecting_flags = False
			continue
		if not in_env:
			continue
		# detect start of build_flags
		if s.startswith('build_flags'):
			collecting_flags = True
			# possible inline after '='
			parts = line.split('=', 1)
			if len(parts) == 2:
				rhs = parts[1]
				tokens.extend(rhs.strip().split())
			continue
		# stop collecting when a new key of the section appears
		if collecting_flags and s and not line.startswith(('\t', ' ', ';', '#')) and '=' in line:
			collecting_flags = False
		if collecting_flags:
			# continuation lines usually indented or on separate lines
			if s and not s.startswith((';', '#')):
				tokens.extend(s.split())
	defines = {}
	for t in tokens:
		if t.startswith('-D'):
			kv = t[2:]
			if '=' in kv:
				k, v = kv.split('=', 1)
				defines[k.strip()] = v.strip()
			else:
				defines[kv.strip()] = None
	return defines

def _parse_ini_defines_all_envs(ini_path: Path):
	"""Parse -D defines from build_flags across all [env:*] sections and merge them (last wins)."""
	content = ini_path.read_text(encoding='utf-8', errors='ignore').splitlines()
	in_env = False
	collecting_flags = False
	tokens = []
	for line in content:
		s = line.strip()
		if s.startswith('[') and s.endswith(']'):
			in_env = s.startswith('[env:')
			collecting_flags = False
			continue
		if not in_env:
			continue
		if s.startswith('build_flags'):
			collecting_flags = True
			parts = line.split('=', 1)
			if len(parts) == 2:
				rhs = parts[1]
				tokens.extend(rhs.strip().split())
			continue
		if collecting_flags and s and not line.startswith(('\t', ' ', ';', '#')) and '=' in line:
			collecting_flags = False
		if collecting_flags:
			if s and not s.startswith((';', '#')):
				tokens.extend(s.split())
	defines = {}
	for t in tokens:
		if t.startswith('-D'):
			kv = t[2:]
			if '=' in kv:
				k, v = kv.split('=', 1)
				defines[k.strip()] = v.strip()
			else:
				defines[kv.strip()] = None
	return defines

def sliders_enabled_combined() -> bool:
    # New combined flag takes precedence
    if flag_enabled('CM_ENABLE_RUNTIME_ALALOG_SLIDERS'):
        return True
    # Backward compatibility: either int or float
    return flag_enabled('CM_ENABLE_RUNTIME_INT_SLIDERS') or flag_enabled('CM_ENABLE_RUNTIME_FLOAT_SLIDERS')

# Map firmware flags to frontend env vars (use combined flag)
sliders_on = sliders_enabled_combined()
state_btn_on = flag_enabled('CM_ENABLE_RUNTIME_STATE_BUTTONS')
buttons_on = flag_enabled('CM_ENABLE_RUNTIME_BUTTONS')
checkboxes_on = flag_enabled('CM_ENABLE_RUNTIME_CHECKBOXES')
num_inputs_on = True  # optional future flag e.g., CM_ENABLE_RUNTIME_NUMBER_INPUTS
feature_env = {
	'VITE_ENABLE_WS_PUSH': '1' if flag_enabled('CM_ENABLE_WS_PUSH') else '0',
	'VITE_ENABLE_RUNTIME_ALALOG_SLIDERS': '1' if sliders_on else '0',
	'VITE_ENABLE_RUNTIME_STATE_BUTTONS': '1' if state_btn_on else '0',
	'VITE_ENABLE_SYSTEM_PROVIDER': '1' if flag_enabled('CM_ENABLE_SYSTEM_PROVIDER') else '0',
}

print(f"[extra_script] Flags: sliders_on={sliders_on} state_btn_on={state_btn_on} buttons_on={buttons_on} checkboxes_on={checkboxes_on}")

def select_component(target_rel: str, enabled_rel: str, disabled_rel: str, enabled: bool):
	"""Copy either the enabled or disabled template to the target component path."""
	target = Path(webui_path) / target_rel
	src = Path(webui_path) / (enabled_rel if enabled else disabled_rel)
	if not src.exists():
		raise FileNotFoundError(f"Missing template: {src}")
	target.write_text(src.read_text(encoding='utf-8'), encoding='utf-8')

# Select appropriate component variants based on flags
select_component(
	'src/components/runtime/RuntimeSlider.vue',
	'src/components/runtime/templates/RuntimeSlider.enabled.vue',
	'src/components/runtime/templates/RuntimeSlider.disabled.vue',
	feature_env['VITE_ENABLE_RUNTIME_ALALOG_SLIDERS'] == '1'
)

select_component(
	'src/components/runtime/RuntimeStateButton.vue',
	'src/components/runtime/templates/RuntimeStateButton.enabled.vue',
	'src/components/runtime/templates/RuntimeStateButton.disabled.vue',
	feature_env['VITE_ENABLE_RUNTIME_STATE_BUTTONS'] == '1'
)

# Action button
select_component(
	'src/components/runtime/RuntimeActionButton.vue',
	'src/components/runtime/templates/RuntimeActionButton.enabled.vue',
	'src/components/runtime/templates/RuntimeActionButton.disabled.vue',
	buttons_on
)

# Checkbox
select_component(
	'src/components/runtime/RuntimeCheckbox.vue',
	'src/components/runtime/templates/RuntimeCheckbox.enabled.vue',
	'src/components/runtime/templates/RuntimeCheckbox.disabled.vue',
	checkboxes_on
)

# Number input (optional pruning, currently always on)
select_component(
	'src/components/runtime/RuntimeNumberInput.vue',
	'src/components/runtime/templates/RuntimeNumberInput.enabled.vue',
	'src/components/runtime/templates/RuntimeNumberInput.disabled.vue',
	num_inputs_on
)

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
