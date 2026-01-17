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
    
    # Run the library script from the library directory (it expects its own relative paths).
    original_cwd = os.getcwd()
    try:
        # Set environment variables that the script might need
        env_vars = os.environ.copy()
        env_vars['PIOENV'] = active_env
        env_vars['PLATFORMIO_ENV'] = active_env
        env_vars['PROJECT_DIR'] = str(project_dir)
        print(f"[precompile_wrapper] Original CWD: {original_cwd}")
        
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