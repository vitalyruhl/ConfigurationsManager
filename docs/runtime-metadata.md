# Runtime metadata: legacy and compact fields

Since 4.7.0, applications can opt into compact field metadata. Existing
RuntimeFieldMeta, live builders, providers, controls and JSON consumers remain
supported. Compact metadata describes fields; providers/getters still supply
values. Existing applications do not need conversion.

## Register a compact measurement

```cpp
#include <ConfigManager.h>

auto& runtime = ConfigManager.getRuntime();
runtime.addRuntimeProvider("sensors", [](JsonObject& values) {
  values["temperature"] = 21.5f;
}, 10);

RuntimeFloatMeta temperature;
temperature.base.key = "temperature";
temperature.base.label = "Temperature";
temperature.base.sourceGroup = "sensors";
temperature.base.page = "Sensors";
temperature.base.card = "Environment";
temperature.unit = "C";
temperature.precision = 1;
RuntimeFieldId temperatureId = runtime.addRuntimeMeta(temperature);
if (!temperatureId) {
  // [E] Registration failed; do not attach extensions to an invalid handle.
}
```

Definitions own their String inputs and may go out of scope after registration.
The manager stores an independent compact representation. If an application
keeps its definition objects, that application-owned memory remains additional.
Group/sourceGroup normalization follows the legacy registration convention.
Create explicit pages/cards/groups when specific layout order is required;
see the BME280-Temp-Sensor example for a complete setup.

## Types and optional data

All types use `.base` for key, label, group, sourceGroup, page, card and order.

| Type | Additional definition members |
| --- | --- |
| RuntimeFloatMeta | unit, precision |
| RuntimeIntMeta | unit; precision is derived as zero |
| RuntimeBoolMeta | None; boolean type is derived, no numeric unit/precision |
| RuntimeTextMeta / RuntimeDividerMeta / RuntimeButtonMeta / RuntimeCheckboxMeta / RuntimeMomentaryButtonMeta | None |
| RuntimeStateButtonMeta | initialState |
| RuntimeIntSliderMeta / RuntimeIntInputMeta | unit, intMin, intMax, intInit |
| RuntimeFloatSliderMeta / RuntimeFloatInputMeta | unit, floatMin, floatMax, floatInit, precision |

Alarms, state labels, styles and static text have separate sparse storage:

```cpp
RuntimeNumericAlarm limits;
limits.hasAlarm = true;
limits.alarmMax = 80.0f;
runtime.setRuntimeNumericAlarm(temperatureId, limits);

RuntimeFieldStyle style;
style.rule("values").set("color", "red");
runtime.setRuntimeFieldStyle(temperatureId, style);

RuntimeBoolMeta link;
link.base.key = "connected";
link.base.label = "Connected";
link.base.sourceGroup = "sensors";
RuntimeFieldId linkId = runtime.addRuntimeMeta(link);
runtime.setRuntimeStateLabels(linkId, RuntimeStateLabels{"Online", "Offline"});
RuntimeBoolAlarm alarm;
alarm.hasAlarm = true;
alarm.alarmWhenTrue = true;
alarm.boolAlarmValue = true;
runtime.setRuntimeBoolAlarm(linkId, alarm);
```

The boolean example describes alarm presentation; application alarm evaluation
and hardware control policy are unchanged. A provider must supply `connected`.
Each setter returns bool; production applications should handle false.
Invalid/promoted handles and incompatible extension kinds are rejected. Passing
an empty style, empty static text, or two empty state labels removes that sparse
payload. Field IDs are opaque, 1-based, and are scoped to their runtime manager.

## Typed updates and controls

```cpp
runtime.updateRuntimeMeta(temperatureId, [](RuntimeFloatMeta& definition) {
  definition.precision = 2;
});

RuntimeIntSliderMeta slider;
slider.base.key = "level";
slider.base.label = "Level";
slider.base.sourceGroup = "controls";
slider.intMin = 0;
slider.intMax = 100;
slider.intInit = 10;
RuntimeFieldId sliderId = runtime.addRuntimeMeta(slider);
runtime.registerRuntimeIntSlider("controls", "level", readLevel, writeLevel, 0, 100);
```

`readLevel`/`writeLevel` are application callbacks with the existing getter/setter
signatures. Metadata registration alone does not install callbacks. Wrong-kind
updates return false. Typed definitions are temporary owned update values;
references to them must not escape the callback. Control range validation applies
to new typed definitions; arbitrary legacy flag combinations remain supported.

## Legacy access and promotion

Old registrations keep real RuntimeFieldMeta records and their API. Mixing old
and compact registrations is supported. Normal compact output/page selection
uses borrowed locked views, without temporary JsonDocument trees or complete
legacy record copies.

The old updateRuntimeMeta(group,key,callback) API can edit a compact record via
one temporary legacy value. Representable changes remain compact; changes using
legacy-only combinations promote that one record. The old findRuntimeMeta()
returns a mutable pointer and therefore promotes a compact match to one real
legacy record. The compact payload is released, with spare pool capacity allowed
to remain; there is no second permanent metadata mirror.

An ID mapped to a promoted field stays associated with that field, but typed
updates/extensions on that promoted ID return false. Use the legacy API afterward.
Legacy pointer lifetime remains limited to the next metadata mutation. Legacy
predicate callbacks may copy one record/long strings; internal selected-page
filtering does not need that compatibility copy. Do not assume raw pointer
mutation automatically adds cache invalidation beyond the existing contract.

## Memory and failure boundaries

The compact store is lazy: old-only applications allocate none. Retained compact
storage uses checked local buffers and owned text; repeated routing and unit
text is shared. Empty numeric-alarm/style options are not embedded in every
field. Registration failure returns an invalid ID; rejected updates do not
commit partial visible fields. Caller-owned definitions and callback code are
outside these internal allocation guarantees.

Legacy compatibility conversions can still allocate String/style/vector memory.
They preserve legacy allocation limitations; no global allocator or exception
configuration is changed. This feature does not make every existing CM/network
allocation OOM-safe or prove that an app's browser-load panic is resolved.

Target ABI checks for the installed ESP32 toolchain: legacy field244 bytes,
stored Float60, stored Bool56, runtime manager192->200 bytes. These record sizes
exclude text-table, handles, spare capacities and sparse payloads. The full
actual-manager 64-Float host fixture accounts for these retained capacities:
legacy43200->compact15184 bytes (64.85% less). Host ABI/String behavior differs
from ESP32 and allocator bookkeeping is excluded. No device heap/fragmentation
saving is claimed from those host numbers. See heap-memory-inventory.md.

## Focused validation

```powershell
tools/test/run-compact-runtime-meta-host-tests.ps1
tools/test/run-runtime-meta-compatibility-host-tests.ps1
tools/test/run-runtime-meta-json-host-tests.ps1
pio test -e runtime-meta-test --without-uploading --without-testing -v
pio run -d examples/BME280-Temp-Sensor -e usb
```

Native compatibility tests exercise the real registry/cache implementation but
stub target placement/logging hooks. Embedded compilation is not hardware test
execution. Only BME280-Temp-Sensor is migrated in this feature; SIL, SBT and
other examples retain their current APIs until separate plans are approved.
