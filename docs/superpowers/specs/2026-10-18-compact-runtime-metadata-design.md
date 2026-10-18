# Compact runtime metadata design

Status: Owner-approved, 2026-10-18. The owner explicitly requested richer
restart context followed by implementation. Authorization boundaries and the
current phase are recorded in the companion handoff.

## Goal and scope

Add small, purpose-specific runtime metadata types to ConfigurationsManager
while preserving existing applications using RuntimeFieldMeta. Keep one library,
one WebUI schema and shared layout/serialization behavior. This is an additive
minor feature, proposed target 4.7.0 after the current 4.6.1 fixes are preserved.

This design is CM-only. SolarInverterLimiter and SolarBatteryTelemetry receive
separate migration plans after CM implementation, validation and owner acceptance.
Do not edit their source, bindings, settings or versions as part of this work.

## Verified starting point

- Current branch: feature/hybrid-json-output; local checkpoint e578c91.
- Current uncommitted product delta: sequential MQTT System-Info lifetimes;
  its focused host tests and actual SBT consumer build passed.
- Previous branch: feature/jbd-bms-transport at dea1bf9. It is an ancestor of
  the current branch (0 previous-only, 2 current-only commits at planning time).
- Target ABI probe: String=16 bytes, RuntimeFieldMeta=244 bytes,
  RuntimeFieldStyle=12 bytes. Sixty-four records alone occupy 15,616 bytes;
  long-text storage, styles and spare vector capacity are additional.
- RuntimeManager.h stores one vector<RuntimeFieldMeta>. Existing mutable
  pointer lookup, mutation callbacks and predicates expose the legacy type.
- Output already uses bounded count/write serialization; do not restore
  document trees or complete metadata-copy snapshots for the new path.

## Proposed data model

Use composition and separate typed storage, without a polymorphic allocation
per field, a vector of largest-alternative variants, or inline optional large
extension objects.

Names below are proposed public names to approve with this design:

| Type | Type-specific retained data beyond RuntimeFieldBase |
| --- | --- |
| RuntimeFloatMeta | Numeric unit reference and decimal precision. |
| RuntimeIntMeta | Numeric unit reference; decimal precision is derived as 0. |
| RuntimeBoolMeta | Existing boolean display kind, derived from the type; no engineering unit, numeric limits or decimal precision. |
| RuntimeTextMeta | No numeric or boolean payload. |
| RuntimeDividerMeta | No numeric or boolean payload. |
| RuntimeButtonMeta | Button kind, derived from the type; no numeric payload. |
| RuntimeCheckboxMeta | Checkbox kind, derived from the type; current state remains in the getter/provider. |
| RuntimeStateButtonMeta | State-button-specific initial state only. |
| RuntimeMomentaryButtonMeta | Momentary-button kind, derived from the type; optional state labels live separately. |
| RuntimeIntSliderMeta / RuntimeIntInputMeta | Integer min/max/initial value and numeric unit reference. |
| RuntimeFloatSliderMeta / RuntimeFloatInputMeta | Float min/max/initial value, decimal precision and numeric unit reference. |

Public RuntimeFieldBase is a registration/update definition owning String key,
label, group, sourceGroup, page and card, plus order. Numeric public definitions
own their unit String. Each public definition contains only its kind-specific
members and no inline alarm/style extension. Register temporary/scoped definitions;
an application retaining its own definition also retains that separate memory.

The manager's internal StoredRuntimeFieldBase owns only key and label Strings,
order and IDs for group, sourceGroup, page and card. Stored numeric records use
a unit ID. Store repeated routing text and units in a manager-owned text table,
indexed by 32-bit IDs. Empty text has a defined ID; no borrowed application pointer
is retained. The storage conversion is private: apps never need to construct
internal text IDs. New public definitions are not the retained storage layout. Interning compares length and bytes, preserving UTF-8 and embedded NUL
behavior of existing JSON output. Resolve IDs while holding the metadata lock;
never expose pointers into a growing text table. Unique text and table capacity
must be included in memory accounting, not counted as free deduplication.

Keep separate vectors for each record kind in a lazily created compact store.
A RuntimeFieldId is a 32-bit opaque value; IDs identify compact fields and survive
vector growth. No removal API is introduced in this feature. Optional data is
stored in separate sparse vectors keyed by field ID:

- RuntimeNumericAlarm: alarmMin, alarmMax, warnMin, warnMax and alarm enable state.
- RuntimeBoolAlarm: boolean alarm enable/trigger state, without numeric limits.
- RuntimeStateLabels: optional on/off labels for boolean/control displays.
- Existing RuntimeFieldStyle, only for fields with nonempty rules.
- Optional static display text, without a String member on every field.

Style visibility and alarm logic remain existing capabilities. This change
alters metadata storage, not actuator behavior or alarm evaluation policy.
Existing public behavior is the reference for control-specific flags/defaults.
No new Bool display enum, inversion feature or WebUI widget is introduced;
existing style rules and dedicated control kinds provide current presentation.

## Registration and access

Add typed overloads to ConfigManagerRuntime::addRuntimeMeta; return a
RuntimeFieldId for the new types. Preserve the existing void overload accepting
const RuntimeFieldMeta&. Legacy calls keep real legacy records and signatures.

Add typed updates using a handle and callback, for example:

- bool updateRuntimeMeta(RuntimeFieldId, const std::function<void(RuntimeFloatMeta&)>&)
  and corresponding overloads for each approved kind.
- bool setRuntimeNumericAlarm(RuntimeFieldId, const RuntimeNumericAlarm&).
- bool setRuntimeBoolAlarm(RuntimeFieldId, const RuntimeBoolAlarm&).
- bool setRuntimeStateLabels(RuntimeFieldId, const RuntimeStateLabels&).
- bool setRuntimeFieldStyle(RuntimeFieldId, const RuntimeFieldStyle&).
- bool setRuntimeStaticText(RuntimeFieldId, const String&).

A wrong kind/invalid ID or invalid extension returns false without mutation.
Alarm setters accept only matching field kinds. An empty style or static text removes its
sparse entry. Typed update callbacks receive an owned, temporary public
definition and commit a validated compact conversion; no retained mutable
reference to text-table storage escapes. No new callbacks or stored copies of provider values are added.
Existing provider and control callback registration APIs remain usable; typed
control metadata connects through the same group/key and existing handlers.
No new runtime value transport or device-control API is part of this feature.

## Backward compatibility and deliberate promotion

Old applications retain their source API, stored records, mutable pointer
behavior, filter callbacks and wire output. No automatic legacy-to-compact
migration or deprecation is introduced.

Mixed old/new registrations must work. Internal reads/layout/filtering use a
non-owning RuntimeMetaView resolving either representation. One shared metadata
serializer consumes that view and preserves legacy omission/order/escaping,
page selection, revision invalidation, weak snapshot ownership and locking.
Integer precision=0 and boolean/default wire fields are synthesized during
serialization; absence from stored data must not remove required JSON fields.

The legacy updateRuntimeMeta(group,key,callback) searches both stores. A compact
match is presented as one temporary RuntimeFieldMeta to the callback. If the
result fits its type/extensions, commit it back compactly. If it uses legacy-only
combinations, promote that field to one real legacy record. Preserve duplicate
matching behavior using a characterized common ordering, not an accidental
legacy-first search. Retain existing group/sourceGroup matching rules.

The raw findRuntimeMeta(group,key) API must return a real mutable object. If its
match is compact, deliberately promote that field once and return the legacy
pointer. Delete its compact record payload/extensions logically and release
owned resources; spare vector capacity may remain. Keep the RuntimeFieldId
mapped to the promoted record, so new handles do not silently alias another
field. Subsequent typed updates on a promoted ID return false; legacy access
continues to work. Do not keep a second permanent mirror or return a temporary
pointer. Document this promotion as a memory trade-off, not a free operation.

Existing predicate APIs can materialize one temporary legacy record when called
by external legacy predicates. Internal page selection uses the view directly.
Temporary materialization may allocate long-text copies; measure it separately.
Normal compact serialization performs no legacy materialization.

Preserve development overrides and the original legacy-only path. When only
legacy registration is used, the compact store stays unallocated; added manager
object storage must be measured and limited to at most 16 bytes on ESP32.

## Ownership, ordering and failure

Mutations preserve the established cache-mutex then data-mutex order. The count
and write passes observe the same locked field and text state. Use small field
references for mixed sorting rather than copied full records. Account for this
sorting scratch memory and keep the established group/order/label comparator.

Registration, extension changes, interning and promotion must not leave an
incomplete visible field, a stale text ID or a stale revision on failure.
Respect the actual ESP32 exception configuration: if an atomic failure path
cannot be supported with the chosen container allocation strategy, report the
constraint before implementation proceeds. Do not claim std::vector growth is
OOM-safe merely because the public API has a failure result. Existing legacy
behavior need not acquire a new general allocator framework in this feature.

## Acceptance

- Unchanged legacy fixtures compile and retain exact JSON/control behavior.
- Mixed registrations support layout, selected pages, cache invalidation,
  alarms, styles, callbacks, legacy updates and deliberate pointer promotion.
- New plain Bool records contain no unit, numeric alarm or decimal-precision
  storage; Int records contain no stored decimal precision; sparse extensions
  allocate no per-field payload when absent.
- Allocation-accounted 64-Float reference fixture saves at least 25% retained
  metadata memory against equivalent legacy registration, including routing
  text, vector capacities, handles/indexes and extension-store overhead.
- Report Float-only, Bool-only, mixed and legacy-only results, target ABI sizes,
  serialization peak/scratch and flash separately. Host figures are not device
  heap measurements. Hardware acceptance remains separately authorized.
- No NVS, settings schema, HTTP/WebSocket protocol, OTA, dependency or build
  pipeline redesign. Public API remains additive; no SIL/SBT migration here.

## Branch and approval boundary

The accompanying plan defines preservation of current fixes, return to the
previous feature branch and creation of feature/compact-runtime-metadata. Plan
approval precedes every such mutation. Push, main integration, branch deletion,
release and device actions are not included. Approval must cover this design,
particularly deliberate legacy-pointer promotion and its memory cost.
