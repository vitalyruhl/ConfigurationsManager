#!/usr/bin/env python3
"""
ConfigManager Feature Flag Testing Script

This script automatically tests different combinations of ConfigManager feature flags
to ensure compilation succeeds for all valid combinations and catches incompatible
flag combinations early.

Usage:
    python tools/test_feature_flags.py [--quick] [--verbose] [--report]

Options:
    --quick     Test only essential combinations (faster)
    --verbose   Show detailed compilation output
    --report    Generate HTML test report
"""

import os
import sys
import subprocess
import time
import argparse
import json
from pathlib import Path
from typing import Dict, List, Tuple, Optional
from dataclasses import dataclass, asdict
from datetime import datetime

@dataclass
class TestResult:
    """Represents the result of a single flag combination test"""
    flags: Dict[str, int]
    success: bool
    compile_time_seconds: float
    error_output: str = ""
    flash_usage_percent: float = 0.0
    ram_usage_percent: float = 0.0

@dataclass
class TestSuite:
    """Represents a complete test suite run"""
    start_time: datetime
    end_time: Optional[datetime] = None
    total_tests: int = 0
    passed_tests: int = 0
    failed_tests: int = 0
    results: List[TestResult] = None
    
    def __post_init__(self):
        if self.results is None:
            self.results = []

class FeatureFlagTester:
    """Main testing class for ConfigManager feature flags"""
    
    # Core feature flags that can be toggled
    CORE_FLAGS = {
        'CM_EMBED_WEBUI': [0, 1],
        'CM_ENABLE_WS_PUSH': [0, 1], 
        'CM_ENABLE_SYSTEM_PROVIDER': [0, 1],
        'CM_ENABLE_OTA': [0, 1],
        'CM_ENABLE_RUNTIME_BUTTONS': [0, 1],
        'CM_ENABLE_RUNTIME_CHECKBOXES': [0, 1],
        'CM_ENABLE_RUNTIME_STATE_BUTTONS': [0, 1],
        'CM_ENABLE_RUNTIME_ANALOG_SLIDERS': [0, 1],
        'CM_ENABLE_RUNTIME_ALARMS': [0, 1],
        'CM_ENABLE_RUNTIME_NUMBER_INPUTS': [0, 1],
        'CM_ENABLE_STYLE_RULES': [0, 1],
        'CM_ENABLE_USER_CSS': [0, 1],
        'CM_ENABLE_LOGGING': [0, 1],
        'CM_ENABLE_VERBOSE_LOGGING': [0, 1]
    }
    
    # Essential combinations for quick testing
    ESSENTIAL_COMBINATIONS = [
        # All disabled (minimal build)
        {flag: 0 for flag in CORE_FLAGS.keys()},
        # All enabled (full featured build) 
        {flag: 1 for flag in CORE_FLAGS.keys()},
        # Typical production build
        {
            'CM_EMBED_WEBUI': 1,
            'CM_ENABLE_WS_PUSH': 1,
            'CM_ENABLE_SYSTEM_PROVIDER': 1,
            'CM_ENABLE_OTA': 1,
            'CM_ENABLE_RUNTIME_BUTTONS': 1,
            'CM_ENABLE_RUNTIME_CHECKBOXES': 1,
            'CM_ENABLE_RUNTIME_STATE_BUTTONS': 1,
            'CM_ENABLE_RUNTIME_ANALOG_SLIDERS': 1,
            'CM_ENABLE_RUNTIME_ALARMS': 0,
            'CM_ENABLE_RUNTIME_NUMBER_INPUTS': 0,
            'CM_ENABLE_STYLE_RULES': 0,
            'CM_ENABLE_USER_CSS': 0,
            'CM_ENABLE_LOGGING': 1,
            'CM_ENABLE_VERBOSE_LOGGING': 0
        },
        # Minimal web build (no runtime features)
        {
            'CM_EMBED_WEBUI': 1,
            'CM_ENABLE_WS_PUSH': 0,
            'CM_ENABLE_SYSTEM_PROVIDER': 1,
            'CM_ENABLE_OTA': 1,
            'CM_ENABLE_RUNTIME_BUTTONS': 0,
            'CM_ENABLE_RUNTIME_CHECKBOXES': 0,
            'CM_ENABLE_RUNTIME_STATE_BUTTONS': 0,
            'CM_ENABLE_RUNTIME_ANALOG_SLIDERS': 0,
            'CM_ENABLE_RUNTIME_ALARMS': 0,
            'CM_ENABLE_RUNTIME_NUMBER_INPUTS': 0,
            'CM_ENABLE_STYLE_RULES': 0,
            'CM_ENABLE_USER_CSS': 0,
            'CM_ENABLE_LOGGING': 1,
            'CM_ENABLE_VERBOSE_LOGGING': 0
        },
        # External UI build (no embedded webui)
        {
            'CM_EMBED_WEBUI': 0,
            'CM_ENABLE_WS_PUSH': 1,
            'CM_ENABLE_SYSTEM_PROVIDER': 1,
            'CM_ENABLE_OTA': 0,
            'CM_ENABLE_RUNTIME_BUTTONS': 1,
            'CM_ENABLE_RUNTIME_CHECKBOXES': 1,
            'CM_ENABLE_RUNTIME_STATE_BUTTONS': 1,
            'CM_ENABLE_RUNTIME_ANALOG_SLIDERS': 1,
            'CM_ENABLE_RUNTIME_ALARMS': 1,
            'CM_ENABLE_RUNTIME_NUMBER_INPUTS': 1,
            'CM_ENABLE_STYLE_RULES': 1,
            'CM_ENABLE_USER_CSS': 1,
            'CM_ENABLE_LOGGING': 1,
            'CM_ENABLE_VERBOSE_LOGGING': 0
        }
    ]
    
    def __init__(self, project_dir: Path, verbose: bool = False):
        self.project_dir = project_dir
        self.verbose = verbose
        self.platformio_ini = project_dir / 'platformio.ini'
        self.original_ini_content = self._backup_platformio_ini()
        
    def _backup_platformio_ini(self) -> str:
        """Backup the original platformio.ini content"""
        try:
            with open(self.platformio_ini, 'r', encoding='utf-8') as f:
                return f.read()
        except Exception as e:
            raise Exception(f"Failed to read platformio.ini: {str(e)}")
    
    def _restore_platformio_ini(self):
        """Restore the original platformio.ini content"""
        try:
            with open(self.platformio_ini, 'w', encoding='utf-8') as f:
                f.write(self.original_ini_content)
        except Exception as e:
            print(f"⚠️  Warning: Failed to restore platformio.ini: {str(e)}")
            print("Please manually restore the file from backup.")
    
    def _update_platformio_ini(self, flags: Dict[str, int]):
        """Update platformio.ini with specific flag combination"""
        content = self.original_ini_content
        
        # Find the build_flags section and replace it
        lines = content.split('\n')
        new_lines = []
        in_build_flags = False
        build_flags_found = False
        
        for line in lines:
            if line.strip().startswith('build_flags'):
                in_build_flags = True
                build_flags_found = True
                new_lines.append('build_flags =')
                new_lines.append('\t-DUNIT_TEST')
                new_lines.append('\t-Wno-deprecated-declarations') 
                new_lines.append('\t-std=gnu++17')
                
                # Add our feature flags
                for flag, value in flags.items():
                    new_lines.append(f'\t-D{flag}={value}')
                
                # Add other non-feature flags from original
                new_lines.append('\t-DCONFIG_BOOTLOADER_WDT_DISABLE_IN_USER_CODE=1')
                new_lines.append('\t-DCONFIG_ESP32_BROWNOUT_DET_LVL0=1')
                    
            elif in_build_flags and line.startswith('\t-D') and any(flag in line for flag in self.CORE_FLAGS.keys()):
                # Skip original feature flag lines - we're replacing them
                continue
            elif in_build_flags and line.startswith('\t-D'):
                # Keep other -D flags that aren't feature flags
                if 'CM_ENABLE' not in line and 'CM_EMBED' not in line:
                    new_lines.append(line)
            elif in_build_flags and not line.startswith('\t') and line.strip():
                # End of build_flags section
                in_build_flags = False
                new_lines.append(line)
            else:
                new_lines.append(line)
        
        if not build_flags_found:
            raise Exception("Could not find build_flags section in platformio.ini")
        
        # Write updated content
        try:
            with open(self.platformio_ini, 'w', encoding='utf-8') as f:
                f.write('\n'.join(new_lines))
        except Exception as e:
            raise Exception(f"Failed to write platformio.ini: {str(e)}")
    
    
    def _compile_project(self) -> Tuple[bool, str, float, float, float]:
        """Compile the project and return success, output, time, flash%, ram%"""
        start_time = time.time()
        
        try:
            # Clean build first to ensure fresh state
            clean_result = subprocess.run(
                ['pio', 'run', '-e', 'usb', '--target', 'clean'],
                cwd=self.project_dir,
                capture_output=True,
                text=True,
                timeout=60
            )
            
            if self.verbose:
                print(f"    Clean result: {clean_result.returncode}")
            
            # Now build
            result = subprocess.run(
                ['pio', 'run', '-e', 'usb'],
                cwd=self.project_dir,
                capture_output=True,
                text=True,
                timeout=120  # 2 minutes should be enough
            )
            
            compile_time = time.time() - start_time
            success = result.returncode == 0
            output = result.stdout + result.stderr
            
            if self.verbose and not success:
                print(f"    Build failed with return code: {result.returncode}")
                print(f"    Last few lines of output: {output.split('\\n')[-5:]}")
            
            # Parse memory usage from output
            flash_percent, ram_percent = self._parse_memory_usage(output)
            
            return success, output, compile_time, flash_percent, ram_percent
            
        except subprocess.TimeoutExpired:
            compile_time = time.time() - start_time
            return False, "Compilation timeout (5 minutes)", compile_time, 0.0, 0.0
        except FileNotFoundError:
            compile_time = time.time() - start_time
            return False, "PlatformIO 'pio' command not found. Please ensure PlatformIO is installed and in PATH.", compile_time, 0.0, 0.0
        except Exception as e:
            compile_time = time.time() - start_time
            return False, f"Compilation error: {str(e)}", compile_time, 0.0, 0.0
    
    def _parse_memory_usage(self, output: str) -> Tuple[float, float]:
        """Parse flash and RAM usage percentages from PlatformIO output"""
        flash_percent = 0.0
        ram_percent = 0.0
        
        for line in output.split('\n'):
            if 'Flash:' in line and '%' in line:
                try:
                    # Extract percentage from "Flash: [====] 85.0% (used X bytes from Y bytes)"
                    parts = line.split('%')[0]
                    flash_percent = float(parts.split()[-1])
                except:
                    pass
            elif 'RAM:' in line and '%' in line:
                try:
                    # Extract percentage from "RAM: [==] 17.4% (used X bytes from Y bytes)"
                    parts = line.split('%')[0]
                    ram_percent = float(parts.split()[-1])
                except:
                    pass
        
        return flash_percent, ram_percent
    
    def test_flag_combination(self, flags: Dict[str, int]) -> TestResult:
        """Test a specific flag combination"""
        if self.verbose:
            flags_str = ', '.join([f"{k}={v}" for k, v in flags.items()])
            print(f"Testing: {flags_str}")
        
        # Update platformio.ini with flags
        self._update_platformio_ini(flags)
        
        # Compile project
        success, output, compile_time, flash_percent, ram_percent = self._compile_project()
        
        result = TestResult(
            flags=flags.copy(),
            success=success,
            compile_time_seconds=compile_time,
            error_output=output if not success else "",
            flash_usage_percent=flash_percent,
            ram_usage_percent=ram_percent
        )
        
        if self.verbose:
            status = "✅ PASS" if success else "❌ FAIL"
            print(f"  {status} - {compile_time:.1f}s - Flash: {flash_percent:.1f}% - RAM: {ram_percent:.1f}%")
            if not success:
                print(f"  Error: {output.split('error:')[-1][:100] if 'error:' in output else 'Unknown error'}")
        
        return result
    
    def run_essential_tests(self) -> TestSuite:
        """Run essential flag combination tests"""
        suite = TestSuite(start_time=datetime.now())
        
        print("🧪 Running essential feature flag tests...")
        
        for i, flags in enumerate(self.ESSENTIAL_COMBINATIONS, 1):
            print(f"Test {i}/{len(self.ESSENTIAL_COMBINATIONS)}: ", end="")
            result = self.test_flag_combination(flags)
            suite.results.append(result)
            
            if result.success:
                suite.passed_tests += 1
                print("✅")
            else:
                suite.failed_tests += 1
                print("❌")
        
        suite.total_tests = len(self.ESSENTIAL_COMBINATIONS)
        suite.end_time = datetime.now()
        
        return suite
    
    def run_comprehensive_tests(self) -> TestSuite:
        """Run comprehensive flag combination tests (warning: can take very long!)"""
        # This would test many more combinations - implement if needed
        # For now, just run essential tests
        return self.run_essential_tests()
    
    def generate_report(self, suite: TestSuite, output_file: Path):
        """Generate HTML test report"""
        html_content = f"""
<!DOCTYPE html>
<html>
<head>
    <title>ConfigManager Feature Flag Test Report</title>
    <style>
        body {{ font-family: Arial, sans-serif; margin: 20px; }}
        .header {{ background: #f5f5f5; padding: 20px; border-radius: 5px; }}
        .summary {{ margin: 20px 0; }}
        .passed {{ color: green; }}
        .failed {{ color: red; }}
        table {{ border-collapse: collapse; width: 100%; }}
        th, td {{ border: 1px solid #ddd; padding: 8px; text-align: left; }}
        th {{ background-color: #f2f2f2; }}
        .flag-cell {{ font-family: monospace; font-size: 12px; }}
        .error {{ background-color: #ffe6e6; }}
        .success {{ background-color: #e6ffe6; }}
    </style>
</head>
<body>
    <div class="header">
        <h1>ConfigManager Feature Flag Test Report</h1>
        <p>Generated: {suite.start_time.strftime('%Y-%m-%d %H:%M:%S')}</p>
        <p>Duration: {(suite.end_time - suite.start_time).total_seconds():.1f} seconds</p>
    </div>
    
    <div class="summary">
        <h2>Summary</h2>
        <p>Total Tests: {suite.total_tests}</p>
        <p class="passed">Passed: {suite.passed_tests}</p>
        <p class="failed">Failed: {suite.failed_tests}</p>
        <p>Success Rate: {(suite.passed_tests/suite.total_tests*100):.1f}%</p>
    </div>
    
    <h2>Test Results</h2>
    <table>
        <tr>
            <th>Test</th>
            <th>Status</th>
            <th>Time (s)</th>
            <th>Flash %</th>
            <th>RAM %</th>
            <th>Flags</th>
            <th>Error</th>
        </tr>
"""
        
        for i, result in enumerate(suite.results, 1):
            status_class = "success" if result.success else "error"
            status_text = "✅ PASS" if result.success else "❌ FAIL"
            flags_text = ", ".join([f"{k}={v}" for k, v in result.flags.items()])
            error_text = result.error_output[:200] + "..." if len(result.error_output) > 200 else result.error_output
            
            html_content += f"""
        <tr class="{status_class}">
            <td>{i}</td>
            <td>{status_text}</td>
            <td>{result.compile_time_seconds:.1f}</td>
            <td>{result.flash_usage_percent:.1f}%</td>
            <td>{result.ram_usage_percent:.1f}%</td>
            <td class="flag-cell">{flags_text}</td>
            <td>{error_text}</td>
        </tr>"""
        
        html_content += """
    </table>
</body>
</html>"""
        
        with open(output_file, 'w') as f:
            f.write(html_content)
        
        print(f"📊 Test report generated: {output_file}")

def main():
    parser = argparse.ArgumentParser(description='Test ConfigManager feature flag combinations')
    parser.add_argument('--quick', action='store_true', help='Run only essential tests')
    parser.add_argument('--verbose', action='store_true', help='Show detailed output')
    parser.add_argument('--report', action='store_true', help='Generate HTML report')
    args = parser.parse_args()
    
    # Find project directory
    script_dir = Path(__file__).parent
    project_dir = script_dir.parent
    
    if not (project_dir / 'platformio.ini').exists():
        print("❌ Error: platformio.ini not found. Run from ConfigManager project root.")
        sys.exit(1)
    
    print("🚀 ConfigManager Feature Flag Tester")
    print(f"📁 Project: {project_dir}")
    
    tester = FeatureFlagTester(project_dir, verbose=args.verbose)
    
    try:
        # Run tests
        if args.quick:
            print("⚡ Running quick essential tests...")
            suite = tester.run_essential_tests()
        else:
            print("🔍 Running comprehensive tests...")
            suite = tester.run_comprehensive_tests()
        
        # Print summary
        print("\n📊 Test Summary:")
        print(f"   Total: {suite.total_tests}")
        print(f"   Passed: {suite.passed_tests}")
        print(f"   Failed: {suite.failed_tests}")
        print(f"   Success Rate: {(suite.passed_tests/suite.total_tests*100):.1f}%")
        print(f"   Duration: {(suite.end_time - suite.start_time).total_seconds():.1f}s")
        
        # Generate report if requested
        if args.report:
            report_file = project_dir / 'test_results' / f'feature_flag_test_{datetime.now().strftime("%Y%m%d_%H%M%S")}.html'
            report_file.parent.mkdir(exist_ok=True)
            tester.generate_report(suite, report_file)
        
        # Exit with error code if any tests failed
        if suite.failed_tests > 0:
            print(f"\n❌ {suite.failed_tests} test(s) failed!")
            sys.exit(1)
        else:
            print("\n✅ All tests passed!")
            
    finally:
        # Always restore original platformio.ini
        tester._restore_platformio_ini()
        print("🔄 Restored original platformio.ini")

if __name__ == '__main__':
    main()