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

webui_path = 'webui'

# Helper: read flag in all contexts
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

# (delay EMBED_WEBUI evaluation until after helper parsers are defined)

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
	if flag_enabled('CM_ENABLE_RUNTIME_ANALOG_SLIDERS'):
		return True
	# Backward compatibility: either int or float
	return flag_enabled('CM_ENABLE_RUNTIME_INT_SLIDERS') or flag_enabled('CM_ENABLE_RUNTIME_FLOAT_SLIDERS')

# Evaluate embed flag now that helpers are loaded
EMBED_WEBUI = flag_enabled('CM_EMBED_WEBUI')

# Map firmware flags to frontend env vars (use combined flag)
sliders_on = sliders_enabled_combined()
state_btn_on = flag_enabled('CM_ENABLE_RUNTIME_STATE_BUTTONS')
buttons_on = flag_enabled('CM_ENABLE_RUNTIME_BUTTONS')
checkboxes_on = flag_enabled('CM_ENABLE_RUNTIME_CHECKBOXES')
num_inputs_on = flag_enabled('CM_ENABLE_RUNTIME_NUMBER_INPUTS') or True  # default to True when flag missing
feature_env = {
	'VITE_ENABLE_WS_PUSH': '1' if flag_enabled('CM_ENABLE_WS_PUSH') else '0',
	'VITE_ENABLE_RUNTIME_ANALOG_SLIDERS': '1' if sliders_on else '0',
	'VITE_ENABLE_RUNTIME_STATE_BUTTONS': '1' if state_btn_on else '0',
	'VITE_ENABLE_SYSTEM_PROVIDER': '1' if flag_enabled('CM_ENABLE_SYSTEM_PROVIDER') else '0',
}

print(f"[extra_script] Flags: embed_webui={EMBED_WEBUI} sliders_on={sliders_on} state_btn_on={state_btn_on} buttons_on={buttons_on} checkboxes_on={checkboxes_on} number_inputs_on={num_inputs_on}")

def select_component(target_rel: str, enabled_rel: str, disabled_rel: str, enabled: bool):
	"""Copy either the enabled or disabled template to the target component path."""
	target = Path(webui_path) / target_rel
	src = Path(webui_path) / (enabled_rel if enabled else disabled_rel)
	if not src.exists():
		raise FileNotFoundError(f"Missing template: {src}")
	target.write_text(src.read_text(encoding='utf-8'), encoding='utf-8')

if EMBED_WEBUI:
	# Select appropriate component variants based on flags
	select_component(
		'src/components/runtime/RuntimeSlider.vue',
		'src/components/runtime/templates/RuntimeSlider.enabled.vue',
		'src/components/runtime/templates/RuntimeSlider.disabled.vue',
		feature_env['VITE_ENABLE_RUNTIME_ANALOG_SLIDERS'] == '1'
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

	# Number input (optional pruning, enabled by flag default)
	select_component(
		'src/components/runtime/RuntimeNumberInput.vue',
		'src/components/runtime/templates/RuntimeNumberInput.enabled.vue',
		'src/components/runtime/templates/RuntimeNumberInput.disabled.vue',
		num_inputs_on
	)

if EMBED_WEBUI:
	# Ensure node modules present only when embedding
	subprocess.run([npm_cmd, 'install'], cwd=webui_path, check=True)

	# Prepare environment
	env_vars = os.environ.copy()
	env_vars.update(feature_env)
	
	# Extract ENCRYPTION_SALT from src/salt.h file
	encryption_salt = None
	
	# Method 1: Try to read from project's src/salt.h
	try:
		project_dir = Path(os.environ.get('PROJECT_DIR', os.getcwd()))
		salt_file = project_dir / 'src' / 'salt.h'
		if salt_file.exists():
			content = salt_file.read_text(encoding='utf-8', errors='ignore')
			# Look for #define ENCRYPTION_SALT "value"
			match = re.search(r'#define\s+ENCRYPTION_SALT\s+"([^"]+)"', content)
			if match:
				encryption_salt = match.group(1)
				print(f"[extra_script] Found ENCRYPTION_SALT in project src/salt.h (length: {len(encryption_salt)})")
	except Exception as e:
		print(f"[extra_script] Note: Could not read project src/salt.h: {e}")
	
	# Method 2: Try to read from library's src/salt.h (fallback)
	if not encryption_salt:
		try:
			lib_salt_file = Path('src') / 'salt.h'
			if lib_salt_file.exists():
				content = lib_salt_file.read_text(encoding='utf-8', errors='ignore')
				match = re.search(r'#define\s+ENCRYPTION_SALT\s+"([^"]+)"', content)
				if match:
					encryption_salt = match.group(1)
					print(f"[extra_script] Using default ENCRYPTION_SALT from library (length: {len(encryption_salt)})")
		except Exception as e:
			print(f"[extra_script] Note: Could not read library src/salt.h: {e}")
	
	if encryption_salt:
		env_vars['CM_ENCRYPTION_SALT'] = str(encryption_salt)
	else:
		print("[extra_script] No ENCRYPTION_SALT found - passwords will be transmitted in plaintext.")

	# 1. Build Vue app with feature env
	subprocess.run([npm_cmd, 'run', 'build'], cwd='webui', check=True, env=env_vars)

	# 1b. Gzip compress dist/index.html to reduce flash footprint before header conversion
	dist_index = Path(webui_path) / 'dist' / 'index.html'
	if dist_index.exists():
		raw = dist_index.read_bytes()
		gz_path = dist_index.with_suffix('.html.gz')
		with gzip.open(gz_path, 'wb', compresslevel=9) as gz:
			gz.write(raw)

	# 2. Generate header from built assets (invoke the moved generator in tools/)
	try:
		# Determine project root: prefer PlatformIO env, else current working directory
		if env is not None:
			proj_dir = env.get('PROJECT_DIR') or env.subst('$PROJECT_DIR')
			root_dir = Path(str(proj_dir))
		else:
			root_dir = Path.cwd()
		generator = root_dir / 'tools' / 'webui_to_header.js'
		if not generator.exists():
			raise FileNotFoundError(f"Header generator not found at {generator}")
		# Pass environment variables (including CM_ENCRYPTION_SALT) to header generator
		subprocess.run([node_exe, str(generator)], cwd=str(root_dir), check=True, env=env_vars)
	except Exception as e:
		raise RuntimeError(f"Failed to run webui_to_header.js: {e}")
else:
	# Skipping WebUI build and header generation (external UI mode)
	print("[extra_script] CM_EMBED_WEBUI=0 -> Skipping webui build and header generation.")
	# Overwrite header with a tiny stub to avoid accidentally keeping old embedded content
	header_path = Path('src') / 'html_content.h'
	stub = (
		"#pragma once\n"
		"#include <pgmspace.h>\n\n"
		"// Minimal stub when embedded WebUI is disabled\n"
		"const char WEB_HTML[] PROGMEM =\n"
		'R"HTML(<!DOCTYPE html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>WebUI disabled</title><style>body{font-family:sans-serif;background:#111;color:#ddd;margin:2rem} .note{padding:1rem;border:1px solid #333;border-radius:8px;background:#1a1a1a}</style></head><body><h3>Embedded WebUI disabled</h3><div class="note">Use external WebUI. APIs are still available on this device.</div></body></html>)HTML";\n'
	)
	try:
		header_path.write_text(stub, encoding='utf-8')
		print(f"[extra_script] Wrote stub header: {header_path}")
	except Exception as e:
		print(f"[extra_script] Warning: failed to write stub header: {e}")

	# Also remove any previously built object to force relink without embedded assets
	try:
		env_name = os.environ.get('PIOENV') or os.environ.get('PLATFORMIO_ENV') or ''
		if env_name:
			obj_dir = Path('.pio') / 'build' / env_name / 'src'
			for fname in ['html_content.cpp.o', 'html_content.cpp.o.d']:
				fpath = obj_dir / fname
				if fpath.exists():
					fpath.unlink()
					print(f"[extra_script] Removed stale object: {fpath}")
	except Exception as e:
		print(f"[extra_script] Warning: could not remove old object files: {e}")
