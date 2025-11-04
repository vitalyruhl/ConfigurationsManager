#!/usr/bin/env python3
"""
Wrapper script to call the precompile script from the ESP32 Configuration Manager library.
This ensures the script runs during PlatformIO build process.
"""

import os
import sys
import subprocess
from pathlib import Path

# Try to access PlatformIO/SCons build environment 
try:
    from SCons.Script import Import  # type: ignore
    Import('env')  # provides "env" from PlatformIO
    print("[precompile_wrapper] Starting ESP32 Configuration Manager precompile...")
    SCONS_AVAILABLE = True
except Exception:
    env = None
    print("[precompile_wrapper] SCons environment not available, running in standalone mode")
    SCONS_AVAILABLE = False

def main():
    if not SCONS_AVAILABLE:
        print("[precompile_wrapper] Starting precompile wrapper...")
    
    # Get the project directory
    project_dir = Path(__file__).parent.parent
    
    if not SCONS_AVAILABLE:
        print(f"[precompile_wrapper] Project directory: {project_dir}")
    
    # Determine active PlatformIO environment (fallback to 'usb' if undetermined)
    try:
        active_env = (env.get('PIOENV') if SCONS_AVAILABLE else None) or os.environ.get('PIOENV') or os.environ.get('PLATFORMIO_ENV') or 'usb'
    except Exception:
        active_env = os.environ.get('PIOENV') or os.environ.get('PLATFORMIO_ENV') or 'usb'

    # Find the ESP32 Configuration Manager library directory
    lib_path = project_dir / ".pio" / "libdeps" / active_env / "ESP32 Configuration Manager"
    
    if not lib_path.exists():
        print(f"[precompile_wrapper] Library not found at: {lib_path}")
        print("[precompile_wrapper] Skipping precompile step...")
        return
    
    # Path to the actual precompile script
    precompile_script = lib_path / "tools" / "preCompile_script.py"
    
    if not precompile_script.exists():
        print(f"[precompile_wrapper] Precompile script not found at: {precompile_script}")
        print("[precompile_wrapper] Skipping precompile step...")
        return
    
    print(f"[precompile_wrapper] Running ESP32 Configuration Manager precompile script for env '{active_env}'...")
    print(f"[precompile_wrapper] Library path: {lib_path}")
    print(f"[precompile_wrapper] Script path: {precompile_script}")
    
    # Run the library script from the library directory (it expects its own relative paths)
    # but propagate PlatformIO build flags explicitly so it can enable features correctly.
    original_cwd = os.getcwd()
    try:
        # Set environment variables that the script might need
        env_vars = os.environ.copy()
        env_vars['PIOENV'] = active_env
        env_vars['PLATFORMIO_ENV'] = active_env
        env_vars['PROJECT_DIR'] = str(project_dir)
        print(f"[precompile_wrapper] Original CWD: {original_cwd}")
        
        # Parse build_flags from platformio.ini and pass them via PLATFORMIO_BUILD_FLAGS for the precompiler
        def _collect_build_flags(ini_path: Path, env_name: str):
            if not ini_path.exists():
                return []
            content = ini_path.read_text(encoding='utf-8', errors='ignore').splitlines()
            in_env = False
            collecting = False
            tokens = []
            for line in content:
                s = line.strip()
                if s.startswith('[') and s.endswith(']'):
                    in_env = (s == f'[env:{env_name}]')
                    collecting = False
                    continue
                if not in_env:
                    continue
                if s.startswith('build_flags'):
                    collecting = True
                    parts = line.split('=', 1)
                    if len(parts) == 2:
                        rhs = parts[1]
                        tokens.extend(rhs.strip().split())
                    continue
                if collecting and s and not line.startswith(('\t', ' ', ';', '#')) and '=' in line:
                    collecting = False
                if collecting:
                    if s and not s.startswith((';', '#')):
                        tokens.extend(s.split())
            return tokens

        flags_tokens = _collect_build_flags(project_dir / 'platformio.ini', active_env)
        if flags_tokens:
            env_vars['PLATFORMIO_BUILD_FLAGS'] = ' '.join(flags_tokens)
            # Also keep legacy var name for robustness
            env_vars['BUILD_FLAGS'] = env_vars['PLATFORMIO_BUILD_FLAGS']
            print(f"[precompile_wrapper] Propagating {len(flags_tokens)} build flags")
        else:
            print(f"[precompile_wrapper] WARNING: No build flags found in platformio.ini")

        # Change working directory to the library so its relative paths resolve
        os.chdir(str(lib_path))
        print(f"[precompile_wrapper] Changed CWD to: {os.getcwd()}")
        print(f"[precompile_wrapper] Running: {sys.executable} {precompile_script}")

        result = subprocess.run([sys.executable, str(precompile_script)], 
                                capture_output=False, 
                                text=True,
                                env=env_vars)
        if result.returncode != 0:
            print(f"[precompile_wrapper] Precompile script failed with return code: {result.returncode}")
            sys.exit(result.returncode)
        else:
            print("[precompile_wrapper] ESP32 Configuration Manager precompile completed successfully")
    except Exception as e:
        print(f"[precompile_wrapper] Error running precompile script: {e}")
        sys.exit(1)
    finally:
        os.chdir(original_cwd)

if __name__ == "__main__":
    main()