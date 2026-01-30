Import("env")

import os
import shutil
import stat


def _rmtree_onerror(func, path, exc_info):
    try:
        os.chmod(path, stat.S_IWRITE)
        func(path)
    except Exception as e:
        print("[WARNING] Failed to remove path during cleanup: {} ({})".format(path, e))


def _maybe_remove_cached_library_folder():
    if os.environ.get("CM_PIO_NO_LIB_REFRESH", "").strip() in ("1", "true", "TRUE", "yes", "YES"):
        print("[INFO] Skipping local library refresh (CM_PIO_NO_LIB_REFRESH=1)")
        return

    lib_name = "ESP32 Configuration Manager"
    project_libdeps_dir = env.subst("$PROJECT_LIBDEPS_DIR")
    pio_env_name = env.subst("$PIOENV")

    if not project_libdeps_dir or not pio_env_name:
        return

    cached_lib_folder = os.path.join(project_libdeps_dir, pio_env_name, lib_name)

    if not os.path.isdir(cached_lib_folder):
        return

    print("[INFO] Removing cached local library folder to force refresh: {}".format(cached_lib_folder))
    shutil.rmtree(cached_lib_folder, onerror=_rmtree_onerror)


_maybe_remove_cached_library_folder()

