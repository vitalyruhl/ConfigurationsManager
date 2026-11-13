# Memory ownership and measurement

This reference explains the memory categories used by ConfigurationsManager.
For supported APIs, use the guides under `docs/`; this document records the
ownership principles needed to interpret measurements and review changes.

## Separate memory categories

- **Static storage:** retained objects and inline arrays exist for the object's
  lifetime. Emptying a fixed queue does not release its array storage.
- **Retained heap:** vectors, owned strings, transport state and worker stacks
  remain allocated while their owners need them. Logical disable/pause does not
  imply destruction or memory reclamation.
- **Transient heap:** JSON documents, serialized output and response objects can
  overlap while a request is being built and delivered.
- **Task stack:** local buffers use the current task's stack. A worker stack
  allocated with `xTaskCreate` is retained heap from the system's perspective.
  On ESP-IDF, its stack depth and high-water values are measured in bytes.
- **Flash:** an embedded asset does not consume its complete linked size as live
  heap merely because it is included in the firmware.

These quantities cannot be added blindly. Record which allocations coexist,
which task owns them and when each owner releases them. Moving a temporary
buffer onto a task stack changes its budget; it does not increase free heap by
that amount if the stack was already allocated.

## Ownership boundaries

Runtime metadata and layout registrations remain owned by their registries.
Selecting fewer visible runtime groups avoids unrelated provider work but does
not delete registrations or shrink retained capacity. Compact metadata uses
owned text and sparse optional data; compatibility promotion to legacy mutable
metadata can allocate. See [runtime metadata](../docs/runtime-metadata.md).

A response must retain its payload through delivery. Moving a `String` can avoid
a copy while preserving the payload allocation itself. Weak metadata-cache
ownership reuses a snapshot while another strong owner keeps it alive; it does
not retain a full snapshot permanently on its own. Bounded TCP admission limits
in-flight bytes, separately from the complete generated response buffer.

WebSocket transport ownership can end after bytes are copied into TCP, before
ACK. General event/log traffic has separate behavior from runtime delivery.
See [runtime delivery](../docs/GUI-Runtime.md#delivery-and-backpressure).

Bluetooth has fixed command/event queues and a retained worker. Ordinary
pause/disable does not deinitialize the radio or reclaim its controller memory.
Terminal shutdown for restart is a distinct lifecycle and is not a general
memory-recovery operation. See [Bluetooth](../docs/BLUETOOTH.md).

## Interpret measurements within their scope

Total free heap, the largest eligible free block and allocation capability are
different constraints. An allocation can fail with free bytes remaining if no
suitable contiguous block exists. ArduinoJson compatibility names such as
`StaticJsonDocument<N>` must be interpreted against the installed ArduinoJson
version; the name alone does not establish an N-byte stack pool.

Host object sizes and allocation fixtures measure their host ABI and stubs.
Compiler layout checks measure target record layout, not actual device free
heap. Firmware flash/RAM reports, live heap samples, stack high-water marks and
long-term device observations answer different questions. Record source/build
identity, flags, measurement phase and included/excluded storage when presenting
numbers. Neither a short run nor a lower host fixture total proves that an
unclassified device panic has been repaired.
