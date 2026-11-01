"""Run the example's portable protocol, control-gate and provisioning checks."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default=os.environ.get("CXX"))
    args = parser.parse_args()
    compiler = args.compiler or shutil.which("clang++") or shutil.which("g++")
    if not compiler and os.name == "nt":
        candidate = Path("C:/Program Files/LLVM/bin/clang++.exe")
        if candidate.is_file():
            compiler = str(candidate)
    if not compiler:
        parser.error("C++17 compiler missing; set CXX or pass --compiler")
    project = Path(__file__).resolve().parents[2]
    output = project / ".pio" / "native-tests"
    output.mkdir(parents=True, exist_ok=True)
    for case in ("jbd_protocol_test", "bms_command_policy_test", "smoke_policy_test"):
        executable = output / (case + (".exe" if os.name == "nt" else ""))
        subprocess.run([compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror",
                        "-I", str(project / "src"),
                        str(project / "tests" / "native" / (case + ".cpp")),
                        "-o", str(executable)], check=True)
        subprocess.run([str(executable)], check=True)
    print("[I] Three native test executables passed")


if __name__ == "__main__":
    main()
