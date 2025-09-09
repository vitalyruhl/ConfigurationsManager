import subprocess

# Absoluter Pfad zu npm und node
npm_cmd = r'C:\Program Files\nodejs\npm.cmd'
node_exe = r'C:\Program Files\nodejs\node.exe'

# 1. Vue-App bauen
subprocess.run([npm_cmd, 'run', 'build'], cwd='webui', check=True)
# 2. Node.js-Skript ausführen
subprocess.run([node_exe, 'webui_to_header.js'], cwd='.', check=True)
