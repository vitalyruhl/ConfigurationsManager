# Version Impact

Use before a change that may affect library output, public behavior, dependency
resolution, PlatformIO configuration, package metadata, release notes, or
version mirrors.

## Inputs

- changed or proposed paths and behavior
- canonical version from `library.json`
- version scan results

## Procedure

1. Classify the change as no bump, patch, minor, or major under canonical
   governance.
2. Scan `library.json`, `src/ConfigManager.h`, WebUI package files, README,
   changelog, minimal example references, and all discovered current/target
   version occurrences.
3. Identify required mirrors, changelog impact, and any pre-existing mismatch.

## Output

Report classification, rationale, canonical source, required mirrors, scan
commands, and blockers.

## Stop

Do not change a version when the target is ambiguous or mirrors disagree without
an explained resolution. Governance-only and documentation-only work normally
has no version impact.
