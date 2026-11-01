"""Exercise the real package delegation boundary with [MOCKED!] CM packages."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class PackagePrecompileTests(unittest.TestCase):
    def test_only_selected_package_is_executed_with_live_enabled(self):
        base = ROOT / ".pio" / "wrapper-tests"
        base.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=base) as temp:
            project = Path(temp)
            package = project / ".pio/libdeps/bench/ESP32 Configuration Manager"
            (package / "tools").mkdir(parents=True)
            marker = project / "result.json"
            (package / "tools/preCompile_script.py").write_text(
                "import os, json, pathlib\n"
                "pathlib.Path(os.environ['PROJECT_DIR'], 'result.json').write_text("
                "json.dumps({'cwd': os.getcwd(), 'live': os.environ['CM_ENABLE_LIVE_UI'], "
                "'vite_live': os.environ['VITE_CM_ENABLE_LIVE_UI'], "
                "'pioenv': os.environ['PIOENV']}))\n",
                encoding="utf-8",
            )
            spec = importlib.util.spec_from_file_location("package_wrapper", ROOT / "tools/precompile_wrapper.py")
            module = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(module)
            module.run_precompile(project, "bench", {"CM_ENABLE_LIVE_UI": "1"})
            result = json.loads(marker.read_text())
            self.assertEqual(Path(result["cwd"]), package)
            self.assertEqual(result["live"], "1")
            self.assertEqual(result["vite_live"], "1")
            self.assertEqual(result["pioenv"], "bench")

    def test_declared_symlink_runs_original_source_hook(self):
        # [MOCKED!] The dependency source stays in its original project directory.
        base = ROOT / ".pio" / "wrapper-tests"
        base.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=base) as temp:
            project = Path(temp)
            package = project / "original-cm"
            (package / "tools").mkdir(parents=True)
            marker = project / "original-hook-ran.txt"
            (package / "tools/preCompile_script.py").write_text(
                "from pathlib import Path\n"
                "Path('../original-hook-ran.txt').write_text('selected')\n",
                encoding="utf-8",
            )
            spec = importlib.util.spec_from_file_location("package_wrapper", ROOT / "tools/precompile_wrapper.py")
            module = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(module)
            module.run_precompile(project, "usb", {}, ["symlink://original-cm"])
            self.assertEqual(marker.read_text(), "selected")

    def test_missing_selected_package_fails_closed(self):
        spec = importlib.util.spec_from_file_location("package_wrapper", ROOT / "tools/precompile_wrapper.py")
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        with self.assertRaises(FileNotFoundError):
            module.run_precompile(ROOT / ".pio/nonexistent-package-fixture", "bench", {})


if __name__ == "__main__":
    unittest.main()
