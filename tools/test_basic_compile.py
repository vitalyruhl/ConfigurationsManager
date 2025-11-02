#!/usr/bin/env python3
"""
Simple test script to debug feature flag testing issues
"""

import os
import sys
import subprocess
from pathlib import Path

def test_basic_compilation():
    """Test basic compilation with current settings"""
    print("🧪 Testing basic compilation...")
    
    project_dir = Path(__file__).parent.parent
    print(f"📁 Project directory: {project_dir}")
    
    # Check if platformio.ini exists
    platformio_ini = project_dir / 'platformio.ini'
    if not platformio_ini.exists():
        print("❌ platformio.ini not found!")
        return False
    
    print("✅ platformio.ini found")
    
    # Test PlatformIO command
    try:
        result = subprocess.run(
            ['pio', '--version'],
            capture_output=True,
            text=True,
            timeout=30
        )
        print(f"✅ PlatformIO version: {result.stdout.strip()}")
    except Exception as e:
        print(f"❌ PlatformIO not available: {e}")
        return False
    
    # Test compilation
    print("🔨 Testing compilation...")
    try:
        result = subprocess.run(
            ['pio', 'run', '-e', 'usb', '--target', 'clean'],
            cwd=project_dir,
            capture_output=True,
            text=True,
            timeout=60
        )
        print(f"🧹 Clean result: {result.returncode}")
        
        result = subprocess.run(
            ['pio', 'run', '-e', 'usb'],
            cwd=project_dir,
            capture_output=True,
            text=True,
            timeout=120  # 2 minutes should be enough
        )
        
        if result.returncode == 0:
            print("✅ Compilation successful!")
            
            # Look for memory usage
            output = result.stdout + result.stderr
            for line in output.split('\n'):
                if 'Flash:' in line or 'RAM:' in line:
                    print(f"📊 {line.strip()}")
            
            return True
        else:
            print(f"❌ Compilation failed with code: {result.returncode}")
            print("Last few lines of output:")
            output_lines = (result.stdout + result.stderr).split('\n')
            for line in output_lines[-10:]:
                if line.strip():
                    print(f"   {line}")
            return False
            
    except subprocess.TimeoutExpired:
        print("❌ Compilation timeout!")
        return False
    except Exception as e:
        print(f"❌ Compilation error: {e}")
        return False

if __name__ == '__main__':
    print("🔍 Simple Compilation Test")
    print("=" * 40)
    
    success = test_basic_compilation()
    
    if success:
        print("\n✅ Basic compilation test passed!")
        print("You can now try the full feature flag testing script.")
    else:
        print("\n❌ Basic compilation test failed!")
        print("Please fix the compilation issues before running feature flag tests.")
        sys.exit(1)