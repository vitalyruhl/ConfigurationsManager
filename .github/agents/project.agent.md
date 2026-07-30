---
name: Project Profile
description: Provides repository-specific facts for roles that need them.
model: GPT-5.6 Terra
tools: []
agents: []
user-invocable: false
disable-model-invocation: true
handoffs: []
---

# Project Profile

Context only, not a work agent. Load only when repository-specific facts are
required.

```yaml
repo: ConfigurationsManager
type: C++ ESP32 configuration manager library
build: PlatformIO with Arduino framework
version_source: library.json
github_project: ConfigurationsManager (#5)
default_env: usb
```

## Paths

- configuration: `platformio.ini`
- source: `src/`; public headers: `include/` when present
- Web UI: `webui/`; examples: `examples/`; tests: `test/`
- documentation: `README.md`, `docs/`; tooling: `tools/`; Serena: `.serena/`
- generated local governance output: `.Temp/` (ignored and never committed)

## Validation

- focused root build: `pio run -e usb`
- focused tests: `pio test -e usb --without-uploading --without-testing`
- WebUI tests: `node --test webui/test/*.test.mjs` when present
- full gate: `pwsh -NoProfile -File tools/build/run-full-repository-gate.ps1`
- uploads and serial monitor require explicit user authority

## Version and Risk

- `library.json` is canonical. Mirrors include `src/ConfigManager.h`, WebUI
  package files, README text, and minimal-example version references.
- Level C: storage/NVS, OTA, security, PlatformIO/build pipelines, and large
  refactors.
- Serena memories are context only; they never override repository files,
  user instructions, or canonical governance.
