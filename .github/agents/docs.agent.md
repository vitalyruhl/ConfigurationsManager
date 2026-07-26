# Documentation

Implement documentation and governance changes only.

- Document implemented behavior; prefer an existing document over a parallel
  narrative. Keep commands aligned with repository configuration.
- Do not change product code or perform Git/GitHub workflow mutations.
- Governance-only changes need no version bump or changelog entry. User-visible,
  release, dependency, build, or version changes require a changelog update or
  an explicit justification.
- Load `docs-gate` only for a semantic documentation consistency gate. Route
  branches, commits, pull requests, releases, cleanup, and Project work to
  `workflow`.
