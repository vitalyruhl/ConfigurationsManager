# Contributing

ConfigurationsManager is a C++17 ESP32/Arduino library with PlatformIO examples
and an embedded Vue Web UI. Keep changes focused and include enough validation
for a reviewer to assess the affected behavior.

## Repository layout

- `src/`: library implementation and public headers.
- `examples/`: standalone PlatformIO applications.
- `webui/`: frontend source, build configuration and browser tests.
- `test/`: firmware and host test fixtures.
- `tools/`: public build, test and CI helpers.
- `docs/`: maintained user/contributor guides and the changelog.
- `dev-info/`: curated technical reference and settled implementation principles.
- `.github/`: workflows, dependency updates and collaboration templates.

Use the [README documentation index](../README.md#documentation-index) to find
an existing guide before adding another. Keep task plans, raw investigations,
review reports and session notes in the repository's ignored local working area;
they are not public documentation or build inputs. Public tools and CI must work
in a clean clone without local AI configuration.

## Proposing a change

Use the [issue tracker](https://github.com/vitalyruhl/ConfigurationsManager/issues)
for bugs and feature requests. Describe the observed behavior, expected behavior,
reproduction steps, relevant board/dependency details and any useful logs. Remove
credentials and private device data from reports.

Develop on a topic branch such as `fix/<topic>`, `feature/<topic>` or
`docs/<topic>`, then open a pull request against `main`. Release branches are
optional snapshots; no particular release branch is required for development.
Keep unrelated edits out of the change and explain any public API or saved-setting
compatibility impact. A merge requires the configured checks and applicable
validation to pass.

## Code conventions

Write code comments, diagnostic messages, documentation and contribution text in
English. Use clear names consistent with neighboring public APIs; update all
affected call sites, examples and documentation when changing an API. Preserve
compatibility unless a breaking change is explicitly intended and documented.

Prefer RAII and clear ownership. Use `std::string_view` for borrowed text only
when its lifetime is guaranteed; `data()` is not necessarily null-terminated.
Use owned strings when text must outlive the caller. Avoid allocating or blocking
work in callbacks and interrupt handlers. Keep callbacks short and forward work
to the application loop or the owning worker.

Use concise log severity tags `[E]`, `[W]`, `[I]`, `[D]` and `[T]`. Use wrap-safe
elapsed-time comparisons for `millis()`/`micros()` deadlines and document timeout
units at API boundaries.

## Documentation

Keep the root README an introduction with practical entry points. Explain
feature behavior and limitations in the relevant maintained guide. API guides
should include a method table where it helps readers find supported overloads;
non-API documents do not need placeholder method tables.

Verify commands, flags, defaults, local links and examples against source.
Document confirmed behavior and distinguish host tests, embedded compilation
and device observations. Do not describe unresolved failures as repaired based
only on a build or a short observation. The changelog records user-relevant
product changes; documentation-only cleanup does not require a version bump or
a new release entry. Preserve published release history.

## Focused validation

Run checks that cover the changed behavior and its affected integration points.
Documentation-only changes need link/content checks rather than firmware builds.
A C++ change needs at least the relevant PlatformIO build and applicable tests:

```sh
pio run -e usb
```

For an example, select its actual directory and environment:

```sh
pio run -d examples/minimal -e usb
```

The scripts under `tools/test/` provide focused host checks. For example:

```powershell
pwsh -NoProfile -File tools/test/run-compact-runtime-meta-host-tests.ps1
```

For frontend changes, use the applicable Node/browser tests under `webui/test/`
and the build instructions in [the Web UI guide](../webui/README.md).
Embedded compilation and host mocks do not exercise real RF, flash callbacks,
device timing or long-term stability. Record device checks separately when they
are necessary; build commands never authorize an upload.

## Complete repository gate

Cross-cutting changes and final integration may require the complete public
gate. It discovers PlatformIO targets, runs the automated checks and records
results without uploading firmware:

```powershell
pwsh -NoProfile -File tools/build/run-full-repository-gate.ps1
```

It requires PowerShell 7, clang-format 18.1.3, and the toolchains used by the
selected checks. The format check selects `clang-format-18` first, then the
optional Windows executable at `.Temp/ci-clang-format/clang-format.exe`, then
`clang-format` on PATH. Other clang-format versions are rejected to match CI.
Inspect supported targets without running the full matrix:

```powershell
pwsh -NoProfile -File tools/build/run-full-repository-gate.ps1 -DiscoverOnly
```

Describe the checks actually run in the pull request, their results and any
remaining verification gaps. Do not claim device acceptance from compilation
or reuse a test result after changing the behavior it covers.
