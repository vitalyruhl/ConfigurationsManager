# Compact runtime metadata handoff and execution record

Current disposition: 2026-10-22. CM sources are consolidated on
`feature/ble-compact-runtime` at `4c2daf2238ca3fbb6ea9a5ecb8a802b09a8f1072`,
version 4.8.0 Unreleased. The operator accepted normal SIL/SBT web access as
faster and without observed crashes; the current HTTP/WebSocket iteration is
complete. See [the current acceptance and historical evidence](../../todo_http_delivery_panic.md).
Earlier versions, branches, permissions and pending gates below describe their
dated checkpoints. Overload research remains optional and deferred; historical
failed cohorts and artifact identities are preserved. No main integration or
release is implied.

Historical handoff updated: 2026-10-18. Read the linked design and implementation
plan when investigating the original compact-metadata work.

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


## Authorized follow-up: built-in System metadata and SBT migration

The owner requested an intermediate commit followed by internal System-field
migration. Checkpoint `fc02a11025593e6c93a1e7fead7940cd1b95e646` contains the
previous compact feature and BME example. It is local on the same feature branch;
no push occurred. The earlier final-status statement about uncommitted base work
is superseded by this checkpoint. Follow-up changes remain uncommitted.

Bounded design: extract hardware-independent System metadata registration into
`runtime/detail/BuiltinSystemMetadata.h`; use Int/Bool/Text for new fields, keep
existing matching fields and their override precedence. Preserve complete wire
output. Ruling: historical nonnumeric precision zero is kept in spare handle
padding, not in public Bool/Text definitions. Handle size remains eight bytes.
Legacy callback changes to precision 0 or 2 stay compact; unsupported values
still promote. The ordinary typed API defaults remain unchanged. Stored settings,
NVS keys, sampled values and transport are outside this CM follow-up.

RED evidence: `system-metadata-red.log` rejects promotion for zero precision;
`system-registration-red.log` rejects legacy-only built-in registration.
GREEN: `system-metadata-green.log` verifies exact JSON with time on/off, no retained
legacy records, repeat-upsert stability, custom unit/style/alarm/placement
preservation and normal typed updates/promotion. Host retained totals:
legacy 7,968 bytes; compact 2,824 without time or 3,256 with time.

The owner subsequently authorized SBT migration to new types and COM7 upload
once BME validation shows no new regression/crash. This supersedes the earlier
deferral of the SBT implementation, but not SIL. Prepare a separate SBT migration
record and preserve its existing logging changes. Keep existing test-ESP settings,
verify active app partition before app-only flashing, and leave the open HTTP
transport finding explicit. Owner will perform the subsequent behavior test.

Independent follow-up review identified one important failure-path issue:
a two-stage Bool/Text add could retain precision 2 if its later update failed,
and an update failure could be mistaken for absence and append a duplicate.
Fixed in one pass: initialize compatibility precision before publishing the
compact record and check existence with a borrowed view after rejected updates.
The internal registration helper is a friend; no new public application API or
retained manager state was added. `system-review-red.log` reproduces precision
mismatch; `system-review-green.log` passes 200 injected single-reserve failures
across initial registration and repeated upserts, with no duplicate keys,
incorrect precision or legacy promotion. Storage suite also passes.
Final BME build after the correction: success, 39.13 seconds.

BME follow-up hardware gate (COM7, 2026-10-18): app-only upload at verified
0x10000, 1,246,256 bytes; NVS/table/OTA bytes identical immediately after upload.
Browser showed CM-BME280-Temp-Sensor 4.7.0, four sensor fields and WebSocket mode
before and after one explicit reload. HTTP: appinfo complete, runtime metadata
3,238/3,238 bytes with 18 System fields and precision zero, runtime complete,
root 61,595/61,595 bytes in 9.12 seconds. Serial observation 70 seconds: zero
abort/backtrace/Guru/brownout/assert markers, one initial POWERON_RESET/setup,
sensor ready and one loop report. The initial boot marker is not an app crash.

Raw NVS bytes differed after runtime. CRC-checked semantic comparison of all
197 active NVS keys found no missing/added keys and only one changed value:
`nvs.net80211/bssid.set`, an ESP WiFi driver cache/selection flag. All application
settings, including stored credentials/IP, stayed identical. Partition table and
OTA selection remained byte-identical. Preserve this distinction: do not claim
whole-NVS byte identity for this follow-up runtime. Evidence is private under
`.local/compact-runtime-metadata/system-device-COM7/` (http/serial summaries,
metadata snapshots, and nvs-semantic-comparison.json). No new crash/regression
was observed in this short gate; the pre-existing intermittent HTTP issue
remains open. Owner-authorized SBT migration/upload can proceed.

SBT follow-up software/build/review and COM7 app-only upload are complete.
SBT 1.0.2 keeps 64 compact application fields. Its first browser load produced
an observed Core 0 lwIP LoadProhibited panic (sys_sem_signal/tcpip_thread), recorded
in docs/todo_http_delivery_panic.md and the separate SBT handoff plan. Subsequent
identity/64-field HTTP responses and browser WebSocket UI succeeded after reset.
Do not conflate this with stable acceptance or the earlier Core 1 abort.
All app settings survived; only SDK WiFi cache entries differed. Owner will test.
Both follow-up implementations remain uncommitted; the approved CM checkpoint
is fc02a11025593e6c93a1e7fead7940cd1b95e646. No push or final merge occurred.

## Current device and build state after SIL comparison

Owner explicitly paused SBT debugging for a SIL-without-BLE comparison, authorized
its compact metadata migration/COM7 upload, and then requested SIL's shared PIO
options in [env]. SIL 5.0.2 is now installed app-only on COM7; no additional NVS
checks were performed. Existing test profile mocks RS485 output and protects the
stored OTA password. No BLE module/NimBLE dependency or active BLE code is linked.
Short test: 70.1 seconds, no panic/software reset; 80 metadata fields delivered,
reported freeHeap 113,988 bytes; full 61,595-byte root delivered in 7.78 seconds.
First browser connection reset without an ESP panic; cause remains unproven.
Owner reports UI subjectively about three times faster and is actively testing.
Do not interrupt that test with additional upload/browser/monitor operations.

SIL PIO was subsequently corrected after owner feedback: common dependencies now
use symlink://../ConfigurationsManager rather than an older published CM pin;
unused OLED packages removed. Normal USB build also passes. No further device
upload occurred after this build-configuration correction. Separate full SIL
handoff: C:/Daten/_Codding/ESP32/SolarInverterLimiter/docs/superpowers/plans/2026-10-18-compact-runtime-metadata.md.

Current branches: CM feature/compact-runtime-metadata (local checkpoint fc02a11
plus uncommitted System follow-up); SBT feature/move-sil-planning (uncommitted
1.0.2 migration); SIL feature/battery-energy-control (uncommitted 5.0.2 migration
and PIO cleanup). No push/final commit/merge occurred. Preserve the SBT NULL-lwIP-
semaphore findings in its handoff; SIL smoke success is not proof of a BLE bug.

## Consolidated CM branch (2026-10-18)

The owner requested one shared CM feature branch beside main for BLE and the
compact runtime field types. The active branch is now
`feature/ble-compact-runtime`, created from `fc02a11`. BLE transport completion
`554869d`, `feature/jbd-bms-transport` at `cc627b1`, and
`feature/hybrid-json-output` at `cc627b1` are already ancestors of this branch;
no content merge or conflict resolution was needed. The checkpoint on this
branch also includes the built-in System compact-metadata follow-up and its
failure-path regression coverage described above. This supersedes earlier
statements that the CM System follow-up is uncommitted.

`main` and `origin/main` remain at
`fc171e07a2081dcfff722e527e144ddf4d92fe5b`. Previous feature branches and the
detached demand-runtime worktree are preserved. CM remains 4.7.0 Unreleased.
No push, release, main integration or branch deletion is authorized by this
consolidation request or performed as part of it.

Fresh consolidation checks: compact storage host tests and production-manager
compatibility host tests pass, including exact System JSON with time on/off,
200 injected reserve failures and legacy compatibility. The focused ESP32
compile-only result is recorded in the checkpoint validation below. Private
build log: `.local/compact-runtime-metadata/consolidation-embedded-build.log`.

SBT and SIL changes remain in their separate repositories/branches as previously
documented. SIL remains installed on COM7 and under owner testing; this task
performs no device, browser, serial-monitor or NVS operations. The intermittent
HTTP/lwIP panic investigation remains open; consolidation is not a crash fix.
Checkpoint validation: `pio test -e runtime-meta-test --without-uploading
--without-testing` exited 0 after 58.56 seconds. Its build stage completed;
PlatformIO reports the execution stage as SKIPPED, with zero device test cases
run. Both host suites exited 0. Product sources were unchanged after these checks.
## Approved BLE memory / SBT GUI follow-up (2026-10-18)

CM on feature/ble-compact-runtime now has an uncommitted payload-free BLE
configuration / bounded IO split and optional CM_BLE_TX_MAX_BYTES (default272).
ESP32 layout/host contracts verify config48, IO288/default or24/TX9, RX320;
SBT's TX9 saves3360 retained bytes without changing RX272 or queue8/16. The
checkbox request acknowledgment toast is neutral with five execution tests;
all36 WebUI Node tests passed. The generated HTML asset was deliberately rebuilt.
CM version4.7.0 Unreleased remains synchronized; no commit/push/main integration.

SBT1.1.0 has50 GUI fields/24 telemetry channels and recovers4768 staticRAM bytes
in the normal USB build. Its installed diagnostic image passed180.1s of Chrome
reload/page navigation with zero panic/NULL/allocator-failure markers after a
previous240.1s clean first revision. BMS/Victron sources were unavailable on this
test ESP: do not infer real-acquisition/long-run acceptance or a proven BLE cause.
The exact final SBT ELF/firmware hashes and all validation/owner scope are in
SolarBatteryTelemetry/docs/plans/2026-10-18-compact-runtime-metadata.md. No NVS
readback or device control was performed. Original crash investigation stays open.
## Owner-authorized intermediate commit and test-target update

The owner explicitly requested an intermediate commit of the reviewed GUI/heap
work. This checkpoint commits the CM follow-up above on the existing shared
branch; prior uncommitted statements describe the earlier development stage.
All recorded source/build/test results still apply: no product code changed
since the successful final builds and device checks. No additional hardware
operation is needed for this Git checkpoint. No push or main merge is authorized.

The owner now reports an OTA update of the remote SBT at192.168.2.21, removal of
COM7, and an already authenticated MQTT Explorer available for further tests.
Treat the remote installation as owner-reported until verified; do not use COM7
or infer new upload/control authority from the observation. Fresh acquisition
and long-run acceptance remain open. Preserve the matching normal and diagnostic
ELFs described in the SBT handoff.