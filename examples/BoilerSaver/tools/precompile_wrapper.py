"""Run the declared CM source hook, including the shared sibling checkout."""
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
    package = project_dir / ".pio/libdeps" / active_env / "ESP32 Configuration Manager"
    sources = [str(item).strip()[len("symlink://"):] for item in dependencies
               if str(item).strip().startswith("symlink://")]
    if len(sources) > 1:
        raise ValueError("Declare exactly one shared CM source checkout")
    if sources:
        package = (project_dir / sources[0]).resolve()
    script = package / "tools/preCompile_script.py"
    if not script.is_file():
        raise FileNotFoundError(f"Resolved CM precompile script missing: {script}")
    child_env = os.environ.copy()
    for feature in ("CM_ENABLE_LIVE_UI", "CM_ENABLE_SETTINGS_UI", "CM_EMBED_WEBUI"):
        value = str(defines.get(feature, "1"))
        if value not in ("0", "1"):
            raise ValueError(f"{feature} must be 0 or 1")
        child_env[feature] = value
        child_env["VITE_" + feature] = value
    for feature in ("CM_ENABLE_LOGGING", "CM_ENABLE_VERBOSE_LOGGING"):
        child_env[feature] = str(defines.get(feature, "0"))
        child_env["VITE_" + feature] = child_env[feature]
    child_env.update(PROJECT_DIR=str(project_dir), PIOENV=active_env, PLATFORMIO_ENV=active_env)
    print(f"[I] CM package precompile: {package}", flush=True)
    subprocess.run([sys.executable, str(script)], cwd=package, env=child_env, check=True)


if env is not None:
    flags = list(env.ParseFlags(env.get("BUILD_FLAGS", "")).get("CPPDEFINES", []))
    flags += list(env.get("CPPDEFINES", []))
    defines = {item[0]: item[1] for item in flags if isinstance(item, (tuple, list)) and len(item) == 2}
    run_precompile(env.get("PROJECT_DIR"), env.get("PIOENV"), defines, env.GetProjectOption("lib_deps", []))
elif __name__ == "__main__":
    run_precompile(Path(__file__).resolve().parents[1], os.environ.get("PIOENV", "usb"), {})
