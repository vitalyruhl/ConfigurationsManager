# Release And Version Rules

- Status: canonical summary; reread governance before release or version work.
- Always reread `.github/AGENTS.md` and `.github/agents/workflow.agent.md` before version,
  release, or integration work.
- `library.json` is the only canonical ConfigurationsManager project/library version source.
  Known mirrors must be scanned and synchronized when that version changes.
- Examples other than the minimal example may have independent app/firmware versions; do
  not change them automatically.
- Version, dependency, PlatformIO, firmware, and build-output changes follow the repository
  version policy. Governance-only and documentation-only changes do not require a version
  bump.
- User-visible, release-relevant, dependency, build, or version changes require a changelog
  decision and documentation impact check.
- `main` is published/released and receives changes through PRs by default. `release/*`
  branches are runnable snapshots; do not assume one exists.
- Never stage, commit, push, merge, create/update release branches, or clean branches unless
  explicitly requested or covered by a named governed workflow.
- No rule requires a patch bump merely when `workflow.begin` or refactor work starts.
- Current verified version state: canonical `library.json`, `src/ConfigManager.h`,
  `webui/package.json`, and `webui/package-lock.json` are `4.2.1`; README still says
  `4.2.0`.

Sources: `.github/AGENTS.md`, `.github/agents/workflow.agent.md`,
`.github/agents/refactor.agent.md`, `library.json`, `src/ConfigManager.h`,
`webui/package.json`, `webui/package-lock.json`, `README.md`.
