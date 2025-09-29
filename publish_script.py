import os
import shutil
from pathlib import Path

VERBOSE = True

# PlatformIO runs extra scripts via SCons which may not define __file__.
# Fallback to cwd if __file__ is not available.
try:
    THIS_FILE = Path(__file__)
except NameError:
    THIS_FILE = Path(os.getcwd()) / 'publish_script.py'

# This script prepares the project for the 'publish' environment.
# Actions:
# 1. If src/main.cpp exists, move it to examples/ as main_example.cpp_disable (preserving if already moved)
# 2. Create a minimal src/main.cpp stub that just includes Arduino and halts (so library build succeeds)
# 3. Leaves existing generated html header generation to extra_script.py (configured separately)

# Try smarter root detection: walk upwards until platformio.ini is found
def find_project_root(start: Path) -> Path:
    cur = start
    for _ in range(10):  # limit ascent
        if (cur / 'platformio.ini').exists():
            return cur
        if cur.parent == cur:
            break
        cur = cur.parent
    return start  # fallback

ROOT = find_project_root(THIS_FILE.parent if THIS_FILE.exists() else Path(os.getcwd()))
SRC = ROOT / 'src'
EXAMPLES = ROOT / 'examples'
TARGET_NAME = 'main.cpp'
RENAMED_NAME = 'main_example.cpp_disable'

SRC_MAIN = SRC / TARGET_NAME
EXAMPLE_DEST = EXAMPLES / RENAMED_NAME
STUB_PATH = SRC / TARGET_NAME  # after moving original we'll recreate this stub

STUB_CODE = '''#include <Arduino.h>
#include "ConfigManager.h"
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

// Auto-generated stub for publish environment.
// Original example moved to examples/main_example.cpp_disable
AsyncWebServer server(80);
ConfigManagerClass ConfigManager; // minimal instance

void setup(){ /* library publish stub */ }
void loop(){ /* no-op */ }
'''.strip() + '\n'

def log(msg: str):
    if VERBOSE:
        print(f"[publish_script] {msg}")

def ensure_examples():
    EXAMPLES.mkdir(exist_ok=True)
    log(f"examples dir: {EXAMPLES}")


def move_main(force: bool = True):
    if SRC_MAIN.exists():
        if EXAMPLE_DEST.exists() and not force:
            log(f"Destination already exists and force is False; skip move.")
            return
        if EXAMPLE_DEST.exists() and force:
            log(f"Force overwrite: removing old {EXAMPLE_DEST}")
            EXAMPLE_DEST.unlink()
        log(f"Moving {SRC_MAIN} -> {EXAMPLE_DEST}")
        shutil.move(str(SRC_MAIN), str(EXAMPLE_DEST))
    else:
        log(f"No {SRC_MAIN} to move (maybe already moved).")


def write_stub():
    with open(SRC_MAIN, 'w', encoding='utf-8') as f:
        f.write(STUB_CODE)
    log(f"Stub written to {SRC_MAIN}")


def main():
    log(f"Detected ROOT={ROOT}")
    log(f"SRC_MAIN exists? {SRC_MAIN.exists()}")
    ensure_examples()
    move_main(force=True)
    write_stub()
    log("Publish preparation complete.")

if __name__ == '__main__':
    main()
