#!/usr/bin/env python3
"""
Setup script for ESP32 Configuration Manager precompile dependencies.
Run this script to install required Python packages.
"""

import subprocess
import sys
import os

def run_command(command, description):
    """Run a command and handle errors."""
    print(f"🔄 {description}...")
    try:
        result = subprocess.run(command, shell=True, check=True, capture_output=True, text=True)
        print(f"✅ {description} completed successfully")
        return True
    except subprocess.CalledProcessError as e:
        print(f"❌ {description} failed:")
        print(f"   Error: {e.stderr.strip()}")
        return False

def check_python_version():
    """Check if Python version is 3.7+"""
    version = sys.version_info
    if version.major >= 3 and version.minor >= 7:
        print(f"✅ Python {version.major}.{version.minor}.{version.micro} is compatible")
        return True
    else:
        print(f"❌ Python {version.major}.{version.minor}.{version.micro} is too old. Please install Python 3.7+")
        return False

def check_node_npm():
    """Check if Node.js and npm are available"""
    node_ok = run_command("node --version", "Checking Node.js")
    npm_ok = run_command("npm --version", "Checking npm")
    return node_ok and npm_ok

def install_python_packages():
    """Install required Python packages"""
    packages = [
        "intelhex"
    ]
    
    success = True
    for package in packages:
        if not run_command(f"pip install {package}", f"Installing {package}"):
            success = False
    
    return success

def main():
    print("🚀 ESP32 Configuration Manager - Dependency Setup")
    print("=" * 50)
    
    success = True
    
    # Check Python version
    if not check_python_version():
        success = False
    
    # Check Node.js and npm
    print("\n📦 Checking Node.js and npm...")
    if not check_node_npm():
        print("⚠️  Node.js and/or npm not found. Please install Node.js from https://nodejs.org/")
        success = False
    
    # Install Python packages
    print("\n🐍 Installing Python packages...")
    if not install_python_packages():
        success = False
    
    print("\n" + "=" * 50)
    if success:
        print("✅ All dependencies installed successfully!")
        print("   You can now use the precompile wrapper with PlatformIO.")
    else:
        print("❌ Some dependencies failed to install.")
        print("   Please check the errors above and install manually.")
    
    return 0 if success else 1

if __name__ == "__main__":
    sys.exit(main())