# Compact runtime metadata handoff and execution record

Updated: 2026-10-18. This file is the restart entry point. Read it before
redispatching work; then read the linked design and implementation plan.

## Owner request and authority

The owner approved the design/plan in conversation, requested enough context
for a different lead agent to resume, and explicitly instructed implementation
after that context is saved. This authorizes the Level C CM metadata change,
the planned local fix checkpoint, fast-forward to the previous feature branch
and creation of feature/compact-runtime-metadata. Use parent-led integration,
with a bounded storage specialist if useful and an independent final review.
Do not ask again for these already approved steps.

No push, main merge, release, branch deletion, upload, serial monitor, device
control or automatic final implementation commit is authorized. SIL and SBT
source/bindings are out of scope. Their separate migration plans come after CM
completion and owner acceptance. Preserve all pre-existing consumer edits.

## Read order and documents

1. Root AGENTS.md, origin/main:.github/AGENTS.md and the role needed now.
2. [Design](../specs/2026-10-18-compact-runtime-metadata-design.md).
3. [Implementation plan](2026-10-18-compact-runtime-metadata.md).
4. [Heap inventory](../../heap-memory-inventory.md) for category/estimate context.
5. [Crash investigation](../../todo_http_delivery_panic.md) only when needed.

Chat is informal German; repository code/comments/records are English. Never
edit the mirrored ChatGPT project's sources directory. This is the real repo:
C:/Daten/_Codding/ESP32/ConfigurationsManager.

## Why this work exists

Opening/reloading CM pages intermittently exhausts DEFAULT-capable heap on SBT;
SIL has similar owner-reported symptoms without serial proof. Integrity samples
were OK; failed small response-header allocations and 1536-byte TCP allocations
were decoded against exact ELF hashes. INTERNAL free heap overstated usable
DEFAULT memory. This feature reduces retained metadata costs; it does not by
itself establish that browser-load panics are fixed.

The prior fix batch contains direct CM-owned JSON serialization, bounded
responses, app-name recovery, static CSS delivery, serialized browser startup
and verbose-only heap diagnostics. SBT diagnostics remain app-owned. ArduinoJson
parsing/provider APIs stay supported. MQTT System-Info was separately changed to
release each document before publishing and ESP payload/topic before WiFi build.
Do not reimplement or discard these fixes when creating the new feature branch.

Target ABI evidence: String16, legacy RuntimeFieldMeta244, style-vector handle12
bytes. At64 fields:15616 record bytes before text/styles/spare capacity. ESP32
ArduinoJson default pool1024; host64 pool4096. Never present host memory savings
as measured device improvement. MQTT host JSON peak8401->4227,10 allocations
unchanged; this excludes String/TCP allocations.

## Decisions already accepted

- Keep legacy RuntimeFieldMeta source API and real records; add typed metadata.
- Public typed definitions own their inputs. Private stored records keep key/
  label Strings and intern repeated routing/unit text by ID.
- Separate typed pools plus sparse numeric alarms, bool alarms, state labels,
  styles and static text. No per-field polymorphic allocation, largest-union
  record or inline large optional payload.
- One view-based serializer/layout path; no normal compact-to-legacy full copy.
- Old mutation callbacks may use one temporary legacy record and convert back.
  Unsupported combinations promote one field; raw mutable-pointer lookup also
  deliberately promotes. No permanent dual mirror. Promoted typed updates fail.
- Preserve actual duplicate matching, source/page identity, output schema/defaults,
  cache invalidation, locks, development override and control callbacks.
- New registration/control metadata does not register callbacks automatically;
  existing callback registration is reused by group/key.
- Target4.7.0 is a minor addition. Require>=25% retained metadata saving in the
  64-Float fixture including capacities/text/index overhead; old-only apps must
  allocate no compact store and gain<=16 target object bytes.
- Preserve absence of UI/protocol/NVS/OTA/build-pipeline changes.

## Git state at approval; reverify before any mutation

- feature/hybrid-json-output HEAD:e578c9184b9d7146df005a5e953e63b550262bd1.
- origin/feature/hybrid-json-output:cf68f0477d90882c555c26d39ae62b3fbca51a1d.
- Previous feature/jbd-bms-transport:dea1bf931d7dcacb4d774127a612574599033b1f.
- Previous is ancestor (0 previous-only,2 current-only at approval).
- main is not an implementation/integration target in this task.
- Separate ConfigurationsManager-demand-runtime checkout is detached554869d;
  do not edit/reset/archive it.
- Pending authorized checkpoint paths: MQTTManager.h, new mqtt/detail/SystemInfoJson.h,
  new MQTT host test/runner, CHANGELOG, inventory, hybrid work record and this
  metadata design/plan/handoff package. Stage named reviewed paths only.
- .serena/project.yml is intentionally local/ignored, removed from tracking in
  cf68f04. Keep it and .local/tool state out of commits.

## Validation evidence inherited from the fix batch

MQTT five host fixtures passed and independent scoped review found no substantive
issues. Actual SBT USB build passed using real Arduino String, isolated output
.pio/http-diagnostics-check:31.77s,staticRAM85248,flash1386369 bytes, ELF SHA256
D62E6CF34ADA803C1CE10710C163C0EB15E8B29989F8B99E7311C1356799E0CF.
The earlier SBT ELF6E402C02... is preserved under ignored .local/panic-*.
UI31 unit tests, mocked mounted-browser startup/retry/OTA checks and embedded
page checks passed before the MQTT-only change. No target upload/acceptance.
These are historical results: reuse only if corresponding sources are unchanged.

## Tools and focused commands

Windows PowerShell; use login:false. The external repo needs the provided
require_escalated file/read/edit/test permission mechanism with narrow purpose.
Default sandbox PowerShell has stalled with InitializeDefaultDrives failures.
No automatic approval rejection occurred. rg works; use directory plus -g,
not a Windows wildcard filename as a literal path.

- PIO:C:/Users/Vital/.platformio/penv/Scripts/pio.exe
- Python:C:/Users/Vital/.platformio/penv/Scripts/python.exe
- Host compiler:C:/Program Files/LLVM/bin/clang++.exe
- ESP32 compiler:C:/Users/Vital/.platformio/packages/toolchain-xtensa-esp32/bin/
- ArduinoJson:.pio/libdeps/usb/ArduinoJson/src (pinned~7.4.3)
- Host JSON baseline: tools/test/run-runtime-meta-json-host-tests.ps1
- MQTT baseline: tools/test/run-mqtt-system-info-json-host-tests.ps1
- Embedded compile-only: pio test -e runtime-meta-test --without-uploading --without-testing -v
  (SKIPPED runtime is not a passing hardware test).
- Consumer build, only if separately in scope: set PLATFORMIO_BUILD_DIR to the
  project's .pio/http-diagnostics-check, never delete/clean the owner's caches.
- Logs/evidence:.local/compact-runtime-metadata (ignored); keep essential progress
  and decisions in this handoff so a copied Git tree remains understandable.

Atlas previously failed its graph projection/index; use narrow direct source
reads if still unavailable. Do not rebuild the index to do this task. No
network browsing or secret/device/config reads are needed for metadata work.

## Execution ledger

| Phase/task | State | Evidence / next step |
| --- | --- | --- |
| Context and owner approval | Complete | Owner approved and instructed implementation; this restart record added. |
| Phase0 checkpoint/return/new branch | Pending | Verify clean named scope, commit fixes/context, ff-only previous branch, create new branch. |
| Task1 compatibility/baselines | Pending | Characterize actual legacy behavior before storage integration. |
| Task2 compact types/storage | Pending | Test-first, then scoped store implementation; define shared view contract. |
| Task3 integration/bridges | Pending | Parent integrates shared serializer/layout, pointer promotion and revision locking. |
| Task4 memory/docs/build/review | Pending | Memory threshold,4.7.0 mirrors,focused ESP32 build,independent review. |

A resumed agent must inspect git status/log and this ledger before starting.
Finished rows must not be rerun wholesale. Record file ownership and active
agent work here before delegation. No active implementation agent at this snapshot.

## Preflight decisions and dependencies

| Producer -> consumer | Shared interface | Review |
| --- | --- | --- |
| Task1 -> Tasks2/3 | Legacy fixture schema/matching and baseline memory accounting | Characterize before adapting. |
| Task2 -> Task3 | Typed definitions, compact store, locked read view, opaque IDs | Agree interfaces before concurrent integration edits. |
| Task3 -> Task4 | Output parity,promotion,allocation/capacity stats | Preserve fixture identity; label host vs target. |

No open product question is required before starting. Allocation failure behavior
must be inspected under actual exception flags; do not promise new std::vector
OOM recovery. If a new allocator/public-contract decision is necessary, present
the concrete constraint before implementing that affected part.
