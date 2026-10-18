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
| Phase0 checkpoint/return/new branch | Complete | cc627b1e0096f83a359e1d88554f509ea49af966 committed; previous branch ff-merged; feature/compact-runtime-metadata created at same SHA; no push. |
| Task1 compatibility/baselines | Complete | Actual RuntimeMetadata.cpp native baseline and legacy normalization/lookup/cache/override/concurrency fixtures pass. |
| Task2 compact types/storage | Complete | All13 types, checked pools/interner/sparse data, rollback, promotion and actual String release covered; normal/no-exception host tests pass. |
| Task3 integration/bridges | Complete | Actual native all-kind JSON parity, mixed order/legacy bridges/cache/concurrency and retained-memory gate pass; target integration compiles. |
| Task4 memory/docs/build/review | Software complete; bounded device test complete with limitation | 4.7.0 guide/mirrors, final CM/BME builds, independent review; COM7 image verified, no serial crashes, settings byte-identical. HTTP timeout/truncation remains open. |

A resumed agent must inspect git status/log and this ledger before starting.
Finished rows must not be rerun wholesale. Record file ownership and active
agent work here before delegation. Active ownership is listed below; completed rows must not be blindly redispatched.

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

## Approved scope additions and live ownership

The owner subsequently authorized migrating exactly one nonminimal example,
preferably BME280-Temp-Sensor, then uploading that example over COM7 and briefly
checking crashes/browser loading. Existing test-ESP IP:http://192.168.2.126/.
The owner explicitly requires preserving all existing settings, especially the
shared WiFi/NVS values. No other example migration is authorized.

Before upload: save the current partition table/NVS (prefer full flash backup)
under ignored local evidence, compare offsets/sizes, and do not erase flash or
rewrite/relocate data partitions. Prefer app-only flashing with preserved OTA
selection when partition tables differ; verify active app partition first.
Example startup must not seed/overwrite WiFi or OTA defaults. Never print saved
passwords/secret values; backups are local ignored device data.

- compact_store: RuntimeFieldTypes.h, CompactRuntimeMetaStore.h, RuntimeMetaView.h,
  CompactHostString.h, compact storage host tests/runner. Public composition
  member is .base; field IDs are 1-based, zero invalid.
- metadata_integration: RuntimeManager.h/.cpp, RuntimeMetadata.cpp,
  ConfigManager.h/Layout, actual-core compatibility test/runner and relevant
  embedded regression. Actual hardware hooks are stubbed only in native tests.
- bme_example: completed only examples/BME280-Temp-Sensor/src/main.cpp and README.
  Four compact floats + one provider; legacy alarm retained. Automatic network/
  OTA default-writing paths removed. No build/device work by that agent.
- parent: ownership coordination, docs/version after integration, final builds,
  independent review, backup/COM7 upload and bounded live check.

Ruling: metadata methods are extracted into RuntimeMetadata.cpp behind small
placement/logging hooks to test actual registry/cache implementation natively.
This is a scoped file split, not a second metadata implementation. Target build
must validate hooks. Ruling: retained compact storage uses checked local buffers
and checked String copies; existing legacy escape paths retain documented
allocation limitations and conditional bad_alloc handling. Actual installed
Arduino target enables -fexceptions; never equate SDK configuration alone with
a guarantee of OOM recovery. String/allocator accounting separates capacities
and allocator bookkeeping; host and target numbers remain separate.

## Final software validation and device state (completion in progress)

All implementation workers finished; no source writer remains active. Parent
owns documentation/final hardware evidence only. Independent fresh-context
metadata_review found no concrete correctness/compatibility defect. Its limits:
native tests stub placement hooks; embedded actual-page regression compiled but
not run; no new hardware control-handler actuation. Handler policy stayed intact.

Builds: CM compile-only runtime-meta-test exit0,27.56s, runtimeSKIPPED;
BME280 USB example exit0,29.01s, staticRAM55736,flash1237321. Full regression
matrix not requested. Real String behavior, conditional exceptions and target
ABI validated. Runtime manager192->200; legacyMeta244,storedFloat60,Bool56.
Full manager host64Float retained43200->15184;64Bool15064;64mixed14032.
All counts include capacities/owned payloads, exclude allocator bookkeeping;
no measured ESP32 heap improvement inferred from hostfigures.

Device COM7 flash backup4194304 bytes saved locally under
.local/compact-runtime-metadata/device-COM7/flash-before.bin. NVS20480 bytes
saved separately. Active partitionapp0 offset0x10000,size0x1c0000 determined
from the actual saved partition table and valid OTA selection CRC. BME firmware
1243904 bytes, SHA256 AC01FA679F9995ECD22C6516EC96315C18B45196972FE069108C11F818FCD377,
written ONLY to activeapp0; esptool verified image hash. No table/NVS/otadata/
bootloader upload. Readback0x8000..0x10000 was BYTE-IDENTICAL before app startup.

Browser visibly rendered CM-BME280-Temp-Sensor4.7.0 with Temperature/Humidity/
Pressure/Dewpoint and Condensation Risk in WebSocket mode. BME sensor initialized.
A repeated browser-automation batch exceeded its control timeout and debugger
binding was lost, so it is NOT counted as10verifiedbrowserreloads. Serial180s
reported0crashmarkers,1setup,2loopreports, WiFi3 and heap173720/172844.
Supplementary HTTP10x6route smoke aborted on an8s read timeout and is not
a passing60-request run. Further serial90s finished with0crashmarkers,0new
setup completions,2loopreports. Post-runtime readback of table,NVS and
OTA selection was BYTE-IDENTICAL to the original full backup. Preserve private logs/backups ignored and never
publish settings/password values.

Ruling: retain an honest bounded validation scope. Retained RAM is allocation-
accounted and passes the approval threshold; absolute per-phase allocator peaks
are not comprehensively benchmarked. Report this limitation rather than relabel
record sizes or observed free heap as a complete allocation census.

## Final handoff status

Implementation is reviewable and uncommitted on `feature/compact-runtime-metadata`
(base `cc627b1`). Do not redo branch setup or reimplement completed tasks. No push,
main merge, final commit, consumer migration or other-example conversion occurred.
User guide: `docs/runtime-metadata.md`. All version mirrors are `4.7.0`.

The final example build succeeded in 32.48 seconds after whitespace cleanup.
Its firmware binary is byte-identical to the image already uploaded to COM7.
Static RAM: 55,736 bytes. Flash: 1,237,321 bytes.

COM7 smoke findings: BME initialized and the browser rendered four sensor values
plus the legacy warning in WebSocket mode. Two serial observation windows covered
270 seconds: zero abort/Backtrace/Guru/Brownout/assert markers, one initial setup,
and zero unexpected setups in the later window. Expected esptool verification
resets occurred outside those windows. NVS, partition table and OTA selection
remained byte-identical after runtime, as well as immediately after upload.
Private backup files and logs are under ignored `.local/compact-runtime-metadata`.

HTTP limit: read-only appinfo, metadata, runtime, page index and layout endpoints
returned complete HTTP 200 bodies in an endpoint diagnostic, but some took
5.23 or 10.58 seconds. One static-page response closed after 3,072 of 61,595 bytes
(about 13 seconds). After the intentional settings-verification reboot, one full
root response delivered all 61,595 bytes in 9.74 seconds. This is intermittent
HTTP delivery evidence; do not claim all reloads pass. The eight-second timeout
was insufficient for the successful 9.74-second transfer, but does not explain
the separately observed premature EOF. Exact records: `device-http-diagnostic.json`
and `device-root-retry.json`. No proxy was configured.

Independent narrow source review found server NODELAY is already active; do not
add another `setNoDelay` as a supposed fix. An ACK stall followed by AsyncTCP's
five-second ACK timeout is a candidate, not proven without close-reason and ACK
instrumentation. `BoundedResponse.h`, `StaticGzipResponse.h` and `WebServer.cpp`
are unchanged from `cc627b1`, so a new metadata regression has not been established.
Preserve this finding in `docs/todo_http_delivery_panic.md`. Do not silently expand
this feature into an HTTP transport rewrite or mark the original SBT/SIL panic fixed.

Remaining owner acceptance: review the new metadata API/results and this delivery
limitation. SIL and SBT migration plans remain deferred until CM is accepted.
No hardware control-handler actuation or full embedded test execution was done.
