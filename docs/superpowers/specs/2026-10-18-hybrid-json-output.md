# Hybrid JSON Output Design

Owner approval: 2026-10-18 conversation, implement the previously recommended hybrid approach on a separate branch after checkpoint and push.

## Scope
Replace temporary ArduinoJson document trees in CM-owned live page index, live layout and runtime metadata generation with a bounded count/write sink. Retain ArduinoJson parsing and public JsonObject provider callbacks. Preserve JSON schema, field omission, ordering, numeric values, duplicate-key last-write semantics, page selection, metadata locking and response ownership. Do not change HTTP transport, OTA, device settings or consumer dependency bindings.

Count and write use the same serializer over stable input; reserve the output once. The sink performs checked writes, JSON string escaping including control bytes, and scalar formatting without a JsonDocument. Reuse the installed ArduinoJson numeric formatter behind one adapter if necessary to preserve its float wire representation without introducing document allocations. Pin that internal dependency with compatibility tests. Preserve strings until serialization completes; never borrow changing provider strings across locks.

## Acceptance
Host tests compare output to ArduinoJson reference documents, verify count/write agreement, reserve and short-write failure, control characters, UTF-8, integer boundaries, floating-point special values, and duplicate metadata style properties. Allocation evidence distinguishes serializer allocations from the output buffer and existing layout sorting/normalization. Relevant ESP32 build and compile-only device tests must pass. Hardware validation remains pending and requires a separate authorized window; this change alone does not establish the panic root cause or fix.

## Workflow
Checkpoint dea1bf9 is pushed on feature/jbd-bms-transport. Branch feature/hybrid-json-output starts at cf68f04, which removes only .serena/project.yml from tracking and preserves its local file. Recreate local Serena configuration with the installed version using cpp, markdown, json, yaml and read_only: true if needed. No automatic final commit, push, merge or upload.