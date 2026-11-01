#!/usr/bin/env python3
"""Build the enclosing CM checkout's embedded Settings UI without copying it."""
import json
import os
from pathlib import Path
import subprocess
import sys

try:
    from SCons.Script import Import
    Import("env")
except ImportError:
    env = None


def run_precompile(project_dir, active_env, defines, dependencies=()):
    project_dir = Path(project_dir).resolve()
    if isinstance(dependencies, str):
        dependencies = dependencies.splitlines()
    selected = [str(item).strip() for item in dependencies]
    sources = [item for item in selected if item.startswith("symlink://")]
    if sources != ["symlink://../.."]:
        raise ValueError("Declare exactly one enclosing CM checkout: symlink://../..")
    library_dir = (project_dir / "../..").resolve()
    metadata = json.loads((library_dir / "library.json").read_text(encoding="utf-8"))
    if metadata.get("name") != "ESP32 Configuration Manager":
        raise ValueError("The enclosing checkout must be ESP32 Configuration Manager")
    script = library_dir / "tools/preCompile_script.py"
    if not script.is_file():
        raise FileNotFoundError(f"Resolved CM precompile script missing: {script}")
    child_env = os.environ.copy()
    expected = {"CM_ENABLE_LIVE_UI": "0", "CM_ENABLE_SETTINGS_UI": "1", "CM_EMBED_WEBUI": "1"}
    for feature, default in expected.items():
        value = str(defines.get(feature, default))
        if value != default:
            raise ValueError("SIL requires embedded Settings-only WebUI: Live=0, Settings=1")
        child_env[feature] = value
        child_env["VITE_" + feature] = value
    for feature in ("CM_ENABLE_LOGGING", "CM_ENABLE_VERBOSE_LOGGING"):
        child_env[feature] = str(defines.get(feature, "0"))
        child_env["VITE_" + feature] = child_env[feature]
    child_env.update(PROJECT_DIR=str(project_dir), PIOENV=active_env, PLATFORMIO_ENV=active_env)
    print(f"[I] CM source precompile: {library_dir}", flush=True)
    subprocess.run([sys.executable, str(script)], cwd=library_dir, env=child_env, check=True)


if env is not None:
    # Use the compiler-aware archiver for the application's link-time optimization.
    env.Replace(AR="xtensa-esp32-elf-gcc-ar", RANLIB="xtensa-esp32-elf-gcc-ranlib")
    flags = list(env.ParseFlags(env.get("BUILD_FLAGS", "")).get("CPPDEFINES", []))
    flags += list(env.get("CPPDEFINES", []))
    defines = {item[0]: item[1] for item in flags if isinstance(item, (tuple, list)) and len(item) == 2}
    run_precompile(env.get("PROJECT_DIR"), env.get("PIOENV"), defines, env.GetProjectOption("lib_deps", []))
elif __name__ == "__main__":
    run_precompile(Path(__file__).resolve().parents[1], os.environ.get("PIOENV", "usb"), {}, ["symlink://../.."])
