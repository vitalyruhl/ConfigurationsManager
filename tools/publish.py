#!/usr/bin/env python3
"""
Automated publish script for ESP32 Configuration Manager
Reads version and changelog from library.json and README.md, then:
1. Git add all changes
2. Git commit with version changelog message
3. Create git tag
4. Push to remote
5. Publish to PlatformIO registry
"""

import json
import re
import subprocess
import sys
from pathlib import Path


def run_command(cmd, check=True, capture_output=False):
    """Run a shell command and handle errors"""
    print(f"Running: {cmd}")
    result = subprocess.run(
        cmd,
        shell=True,
        check=check,
        capture_output=capture_output,
        text=True
    )
    if capture_output:
        return result.stdout.strip()
    return result.returncode == 0


def get_library_version():
    """Read version from library.json"""
    library_json = Path("library.json")
    if not library_json.exists():
        print("[ERROR] library.json not found!")
        sys.exit(1)
    
    with open(library_json, 'r', encoding='utf-8') as f:
        data = json.load(f)
    
    version = data.get("version")
    if not version:
        print("[ERROR] No version found in library.json!")
        sys.exit(1)
    
    return version


def get_latest_changelog():
    """Extract the latest version changelog from README.md"""
    readme = Path("README.md")
    if not readme.exists():
        print("[ERROR] README.md not found!")
        sys.exit(1)
    
    with open(readme, 'r', encoding='utf-8') as f:
        content = f.read()
    
    # Find the Version History section
    version_section = re.search(r'## Version History\n\n(.+?)(?=\n##|\Z)', content, re.DOTALL)
    if not version_section:
        print("[ERROR] Version History section not found in README.md!")
        sys.exit(1)
    
    history_text = version_section.group(1)
    
    # Find ALL version entries
    # Pattern: - **X.X.X**: changelog text
    all_entries = re.findall(r'^- \*\*(\d+\.\d+\.\d+)\*\*:\s*(.+?)(?=\n- \*\*\d+\.\d+\.\d+\*\*:|\n\n##|\Z)', 
                            history_text, re.MULTILINE | re.DOTALL)
    
    if not all_entries:
        print("[ERROR] No version entries found in Version History!")
        sys.exit(1)
    
    # The LAST entry in the list is the most recent (newest at bottom of list in README)
    version, changelog = all_entries[-1]
    changelog = changelog.strip()
    
    return version, changelog


def check_git_status():
    """Check if there are uncommitted changes"""
    result = run_command("git status --porcelain", capture_output=True)
    return bool(result)


def main():
    print("=" * 80)
    print("ESP32 Configuration Manager - Automated Publish Script")
    print("=" * 80)
    print()
    
    # Get version from library.json
    lib_version = get_library_version()
    print(f"[INFO] Library version: {lib_version}")
    
    # Get latest changelog from README
    readme_version, changelog = get_latest_changelog()
    print(f"[INFO] README latest version: {readme_version}")
    print(f"[INFO] Changelog: {changelog[:100]}..." if len(changelog) > 100 else f"[INFO] Changelog: {changelog}")
    print()
    
    # Verify versions match
    if lib_version != readme_version:
        print("[WARNING] Version mismatch!")
        print(f"   library.json: {lib_version}")
        print(f"   README.md:    {readme_version}")
        response = input("Continue anyway? (y/N): ")
        if response.lower() != 'y':
            print("[ERROR] Aborted by user")
            sys.exit(1)
    
    # Check for uncommitted changes
    has_changes = check_git_status()
    if not has_changes:
        print("[INFO] No uncommitted changes detected")
        response = input("Continue with git operations anyway? (y/N): ")
        if response.lower() != 'y':
            print("[INFO] Skipping git operations, proceeding to publish...")
            skip_git = True
        else:
            skip_git = False
    else:
        print("[INFO] Uncommitted changes detected")
        skip_git = False
    
    # Prepare commit message
    commit_message = f"Release v{lib_version}: {changelog}"
    print()
    print("[INFO] Commit message:")
    print(f"   {commit_message}")
    print()
    
    # Confirm before proceeding
    print("[INFO] Ready to publish:")
    print(f"   - Version: {lib_version}")
    print(f"   - Git tag: v{lib_version}")
    if not skip_git:
        print(f"   - Commit & push changes")
    print(f"   - Publish to PlatformIO registry")
    print()
    response = input("Proceed? (y/N): ")
    if response.lower() != 'y':
        print("[ERROR] Aborted by user")
        sys.exit(1)
    
    print()
    print("=" * 80)
    print("Starting publish process...")
    print("=" * 80)
    print()
    
    # Git operations
    if not skip_git:
        # Step 1: Git add
        print("[INFO] Step 1: Adding all changes to git...")
        if not run_command("git add ."):
            print("[ERROR] Git add failed!")
            sys.exit(1)
        print("[SUCCESS] Changes added\n")
        
        # Step 2: Git commit
        print("[INFO] Step 2: Committing changes...")
        # Escape quotes in commit message
        safe_message = commit_message.replace('"', '\\"')
        if not run_command(f'git commit -m "{safe_message}"', check=False):
            print("[WARNING] Commit failed (may be no changes to commit)")
        else:
            print("[SUCCESS] Changes committed\n")
        
        # Step 3: Create git tag
        print(f"[INFO] Step 3: Creating git tag v{lib_version}...")
        if not run_command(f'git tag -a v{lib_version} -m "Version {lib_version}"', check=False):
            print("[WARNING] Tag creation failed (may already exist)")
        else:
            print("[SUCCESS] Tag created\n")
        
        # Step 4: Push to remote
        print("[INFO] Step 4: Pushing to remote repository...")
        if not run_command("git push", check=False):
            print("[WARNING] Push failed")
        else:
            print("[SUCCESS] Pushed to remote\n")
        
        print("[INFO] Step 5: Pushing tags...")
        if not run_command("git push --tags", check=False):
            print("[WARNING] Tag push failed")
        else:
            print("[SUCCESS] Tags pushed\n")
    
    # Step 6: Publish to PlatformIO
    print("[INFO] Step 6: Publishing to PlatformIO registry...")
    print("[INFO] This may take a while depending on your connection and package size...")
    print()
    
    if not run_command("pio pkg publish ."):
        print("[ERROR] PlatformIO publish failed!")
        sys.exit(1)
    
    print()
    print("=" * 80)
    print("[SUCCESS] PUBLISH COMPLETE!")
    print("=" * 80)
    print()
    print(f"[SUCCESS] Version {lib_version} has been published successfully!")
    print()
    print("Next steps:")
    print("  - Check your email for PlatformIO registry processing confirmation")
    print("  - Verify the package at: https://registry.platformio.org/libraries/vitaly.ruhl/ESP32%20Configuration%20Manager")
    print("  - Update any dependent projects to use the new version")
    print()


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\n\n[ERROR] Aborted by user (Ctrl+C)")
        sys.exit(1)
    except Exception as e:
        print(f"\n\n[ERROR] Unexpected error: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)
