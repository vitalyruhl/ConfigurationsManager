# Bounded Root Gzip Response Follow-up Plan

> **For agentic workers:** Use superpowers:executing-plans for the scoped steps below. No commits or uploads are authorized.

**Goal:** Preserve the complete root gzip response during TCP memory pressure while bounding response-owned in-flight bytes.

**Architecture:** Add one AsyncWebServerResponse subclass for static/custom gzip assets, retaining the header until accepted and addressing the immutable body directly. Admit at most 512 bytes per call and 512 total accepted-but-unacknowledged bytes; distinguish AsyncTCP add acceptance from send/flush success.

**Spec:** Parent-approved user scope, 2026-10-15: root browser download/stalled MQTT follow-up. Do not patch dependencies or alter JSON/OTA routes. Preserve all existing runtime, Serena and WiFi changes.

**Tech stack:** Pinned ESPAsyncWebServer-esphome 3.4 and AsyncTCP-esphome 2.1.4; ESP32 memory-mapped PROGMEM; C++17 host tests with explicit mocked platform shell.

## Steps

- [ ] Add focused tests compiling the actual subclass against mocked platform types: zero/short header and body adds, add success plus send failure then poll, partial ACK/zero ACK, 512-byte outstanding bound, complete built-in gzip-byte equality, final-ACK lifecycle.
- [ ] Add `src/web/StaticGzipResponse.h`, then route only custom/embedded root gzip responses through it. Use `add(..., ASYNC_WRITE_FLAG_COPY)` for accounting and `send()` separately; preserve queued ownership across flush failure. After final ACK, set END and close without any later object access.
- [ ] Run focused host tests RED then GREEN and diff check. Update existing unpublished 4.6.0 changelog with the compatible fix; no separate release/version bump within this uncommitted feature branch.
- [ ] Parent independently reviews source and builds SBT USB/OTA consumers against this CM worktree. No redundant CM build, no upload or hardware probing in this subtask.

## Verified cause and scope

Pinned `AsyncAbstractResponse::_ack` advances source/header state without honoring short/zero transport writes. In addition, `AsyncClient::write` returns zero if `send` fails even when `add` already accepted bytes. The replacement must therefore account using `add`, never `write`. Dynamic JSON routes remain outside this bounded follow-up. Actual device recovery and MQTT coexistence remain pending user hardware acceptance.


## Evidence and limits

- Parent approved before implementation. The actual new subclass was compiled
  against explicit mocked platform types and passed exact reassembly of the
  current 59,553-byte generated gzip asset, not a tiny fixture.
- Tests cover zero and short admission in headers/body, queued bytes followed by
  failed flush, repeated poll retry without duplicates, partial/final ACK,
  outstanding and per-admission maximum 512 bytes, and synchronous destruction
  from the final close callback. These tests validate the new response; they do
  not execute or claim a reproduced test of the legacy dependency implementation.
- Pinned AsyncTCP `_close` invokes the disconnect callback synchronously. The
  response sets END before closing and returns without accessing itself/request
  afterward. The webserver does not itself close a completed fixed-length
  response, so advertising Connection: close requires this explicit final close.
- Bounded admission is a correctness/backpressure fix. A 512-byte window may
  reduce throughput under delayed ACK; no transfer-speed or MQTT-recovery claim
  is made without target timing. TCP/RF tuning is outside this scope.
- Parent handles final SBT USB/OTA consumer builds and independent review.
