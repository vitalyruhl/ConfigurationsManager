import os
import subprocess
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

def _get_define_from_scons(name: str, default: str) -> str:
	"""Read a -D define from PlatformIO/SCons if available; return default otherwise."""
	if env is None:
		return default
	try:
		defines = env.get('CPPDEFINES', [])
		for d in defines:
			if isinstance(d, tuple) and len(d) == 2:
				k, v = d
				if k == name:
					return str(v)
			elif isinstance(d, str) and d == name:
				return '1'
	except Exception:
		return default
	return default


# v3.x: WebUI is always built as a full bundle.
# Only logging-related settings are optionally forwarded to the frontend build.
FEATURE_ENV = {
	'VITE_CM_ENABLE_LOGGING': _get_define_from_scons('CM_ENABLE_LOGGING', '1'),
	'VITE_CM_ENABLE_VERBOSE_LOGGING': _get_define_from_scons('CM_ENABLE_VERBOSE_LOGGING', '0'),
}


# Always embed/build the WebUI bundle (no feature pruning).
EMBED_WEBUI = True


if EMBED_WEBUI:
	# Ensure node modules present only when embedding
	subprocess.run([npm_cmd, 'install'], cwd=webui_path, check=True)

	# Prepare environment
	env_vars = os.environ.copy()
	env_vars.update(FEATURE_ENV)

	# Passwords are transmitted in plaintext over HTTP in v3.0.0.

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
		# Pass environment variables to header generator
		subprocess.run([node_exe, str(generator)], cwd=str(root_dir), check=True, env=env_vars)
	except Exception as e:
		raise RuntimeError(f"Failed to run webui_to_header.js: {e}")
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
