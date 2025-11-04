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
        # Ensure devDependencies (vite, @vitejs/plugin-vue) are installed
        env_vars['NODE_ENV'] = 'development'
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

        def _collect_local_lib_paths(ini_path: Path):
            """Return a list of candidate local library paths which exist on disk.
            Sources:
              - Environment variables: CM_LOCAL_LIB, ESP32_CONFIG_MANAGER_LOCAL, ESP32_CONFIG_LIB_PATH
              - Absolute paths present (even commented) under lib_deps section
              - Common defaults near this workspace
            """
            paths = []

            # 1) Environment variables
            for var in ('CM_LOCAL_LIB', 'ESP32_CONFIG_MANAGER_LOCAL', 'ESP32_CONFIG_LIB_PATH'):
                v = os.environ.get(var)
                if v:
                    p = Path(v)
                    if p.exists() and p.is_dir():
                        paths.append(p)

            # 2) Absolute paths present in platformio.ini (tolerate commented lines)
            if ini_path.exists():
                lines = ini_path.read_text(encoding='utf-8', errors='ignore').splitlines()
                capture = False
                for line in lines:
                    raw = line.rstrip('\n')
                    s = raw.strip()
                    if s.startswith('lib_deps'):
                        capture = True
                        continue
                    if capture:
                        # Stop on next config key
                        if s and not raw.startswith(('\t', ' ', ';', '#')) and '=' in raw:
                            capture = False
                        # Collect even commented absolute paths for convenience
                        cand = s.lstrip(';').strip()
                        if cand and (':' in cand or cand.startswith('\\\\')):
                            p = Path(cand)
                            if p.exists() and p.is_dir():
                                paths.append(p)

            # 3) Common defaults (user-specific, but helpful)
            default_candidates = [
                Path('C:/Daten/_Codding/ESP32/ConfigurationsManager'),
                project_dir.parent / 'ConfigurationsManager',
            ]
            for p in default_candidates:
                if p.exists() and p.is_dir():
                    paths.append(p)

            # De-duplicate preserving order
            seen = set()
            uniq = []
            for p in paths:
                if str(p) not in seen:
                    seen.add(str(p))
                    uniq.append(p)
            return uniq

        def _ensure_webui_sources(libdir: Path) -> bool:
            """Ensure libdir/webui contains index.html. Try to copy from any local lib path if missing."""
            wdir = libdir / 'webui'
            idx = wdir / 'index.html'
            if idx.exists():
                return True
            # Try to find a local library path with full webui
            local_paths = _collect_local_lib_paths(project_dir / 'platformio.ini')
            for lp in local_paths:
                src_wui = lp / 'webui'
                if (src_wui / 'index.html').exists():
                    print(f"[precompile_wrapper] webui sources missing in libdeps, copying from local library: {src_wui}")
                    # copy selected items (avoid node_modules heavy copy when possible)
                    import shutil
                    wdir.mkdir(parents=True, exist_ok=True)
                    # items to copy
                    for name in ['index.html', 'src', 'public', 'vite.config.mjs', 'package.json', 'package-lock.json', 'db.json', 'db.live.json', 'runtime_meta_min.json', 'logo.svg']:
                        s = src_wui / name
                        d = wdir / name
                        try:
                            if s.is_dir():
                                if d.exists():
                                    shutil.rmtree(d)
                                shutil.copytree(s, d)
                            elif s.is_file():
                                shutil.copy2(s, d)
                        except Exception as ce:
                            print(f"[precompile_wrapper] Warning: failed copying {s} -> {d}: {ce}")
                    
                    # Ensure vite CLI is available. If npm install later still fails because vite bin missing,
                    # proactively copy node_modules from local lib as a fallback.
                    src_nm = src_wui / 'node_modules'
                    dst_nm = wdir / 'node_modules'
                    try:
                        need_nm_copy = False
                        if not dst_nm.exists():
                            need_nm_copy = True
                        else:
                            if not (dst_nm / 'vite' / 'bin' / 'vite.js').exists():
                                need_nm_copy = True
                        if need_nm_copy and src_nm.exists():
                            print("[precompile_wrapper] Hydrating node_modules from local library (vite CLI)")
                            if dst_nm.exists():
                                shutil.rmtree(dst_nm)
                            shutil.copytree(src_nm, dst_nm)
                    except Exception as ce:
                        print(f"[precompile_wrapper] Note: could not copy node_modules: {ce}")
                    return (wdir / 'index.html').exists()
            print("[precompile_wrapper] WARNING: webui sources not found in library; skipping Vue rebuild and keeping packaged header.")
            return False

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
        # If published package lacks webui sources, try to hydrate from a local lib path
        try:
            has_sources = _ensure_webui_sources(lib_path)
        except Exception as e_copy:
            print(f"[precompile_wrapper] Note: could not ensure webui sources: {e_copy}")
            has_sources = False

        if not has_sources:
            # Do not invoke precompile script; keep packaged header as-is
            print("[precompile_wrapper] Skipping precompile since webui sources are unavailable.")
            return

        # Ensure header generator is available where library expects it (lib/tools)
        try:
            lib_tools = lib_path / 'tools'
            gen_src = project_dir / 'tools' / 'webui_to_header.js'
            gen_dst = lib_tools / 'webui_to_header.js'
            if gen_src.exists():
                lib_tools.mkdir(parents=True, exist_ok=True)
                with open(gen_src, 'rb') as fsrc, open(gen_dst, 'wb') as fdst:
                    fdst.write(fsrc.read())
                print(f"[precompile_wrapper] Provided header generator to library: {gen_dst}")
        except Exception as e:
            print(f"[precompile_wrapper] Warning: could not place header generator: {e}")

        # Force fresh npm install if vite binary is missing
        try:
            wdir = lib_path / 'webui'
            vite_bin = wdir / 'node_modules' / 'vite' / 'bin' / 'vite.js'
            if not vite_bin.exists():
                print("[precompile_wrapper] vite CLI missing; purging node_modules and package-lock.json to force reinstall")
                import shutil
                nm = wdir / 'node_modules'
                if nm.exists():
                    shutil.rmtree(nm)
                plock = wdir / 'package-lock.json'
                if plock.exists():
                    try:
                        plock.unlink()
                    except Exception:
                        pass
                # Hint npm to force install
                env_vars['npm_config_force'] = 'true'
        except Exception as e:
            print(f"[precompile_wrapper] Warning: could not purge node_modules: {e}")

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