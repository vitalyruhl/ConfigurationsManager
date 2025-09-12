import os
import subprocess

# Absolute path to npm and node
npm_cmd = r'C:\Program Files\nodejs\npm.cmd'
node_exe = r'C:\Program Files\nodejs\node.exe'

# Note: If "vite" is not found, run "npm install" in the webui folder first!
# Automatic installation if node_modules is missing:
webui_path = 'webui'
subprocess.run([npm_cmd, 'install'], cwd=webui_path, check=True)

# 1. Build Vue app
subprocess.run([npm_cmd, 'run', 'build'], cwd='webui', check=True)
# 2. Run Node.js script
subprocess.run([node_exe, 'webui_to_header.js'], cwd='.', check=True)
