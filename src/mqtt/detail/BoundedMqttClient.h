#pragma once

#include <WiFiClient.h>
#include <lwip/sockets.h>
#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include "BoundedMqttStats.h"

namespace cm {
namespace detail {

// PubSubClient sees a packet only after the complete bounded frame is validated.
// TCP reads/writes bypass WiFiClient's allocating RX buffer and retrying write().
class BoundedMqttClient : public Client {
public:
  static constexpr uint16_t MaxFrameBytes = 2304;
  static constexpr uint16_t PumpByteBudget = 256;
  static constexpr uint32_t PumpTimeBudgetUs = 1000;
  static constexpr uint32_t FrameLifetimeMs = 30000;
  static constexpr uint16_t TxCapacity = 2 * MaxFrameBytes;
  static constexpr uint16_t MaxTxCapacity = 16384;
  // No positive TCP send for five seconds expires pending bytes. This is not a
  // packet-age or broker-delivery deadline; enqueue and transient errors do not renew it.
  static constexpr uint32_t TxLifetimeMs = 5000;
  static constexpr uint16_t TxCallBudget = 64;
  static constexpr uint32_t TxTimeBudgetUs = 1000;
  static constexpr uint8_t MaxTxSegments = 64;
  static constexpr uint16_t TxControlReserve = 64;

  BoundedMqttClient() = default;
  BoundedMqttClient(const BoundedMqttClient&) = delete;
  BoundedMqttClient& operator=(const BoundedMqttClient&) = delete;
  ~BoundedMqttClient() override { std::free(txExtra_); }

  uint16_t txCapacity() const { return txCapacity_; }
  uint16_t txUsed() const { return txUsed_; }
  uint16_t txFree() const { return txCapacity_ - txUsed_; }
  void noteRejectedPublish() { ++stats_.txRejectedPublishes; }
  void setDropOldestQueuedPublishes(bool enabled) {
    if (enabled == dropOldest_)
      return;
    dropOldest_ = enabled;
    txSegmentCount_ = 0;
    if (enabled && txUsed_) {
      txSegments_[0] = {txUsed_, false, true};
      txSegmentCount_ = 1;
    }
  }
  void beginApplicationPublish() { applicationPublish_ = true; }
  void endApplicationPublish() { applicationPublish_ = false; }
  bool canQueuePublish(size_t topicBytes, size_t payloadBytes, size_t bufferBytes) const {
    constexpr size_t maxHeader = 5;
    if (!topicBytes || topicBytes > 65535 || bufferBytes < maxHeader + 2 ||
        topicBytes > bufferBytes - maxHeader - 2 ||
        payloadBytes > bufferBytes - maxHeader - 2 - topicBytes ||
        payloadBytes > 268435455u - 2 - topicBytes)
      return false;
    const size_t remaining = 2 + topicBytes + payloadBytes;
    size_t varintBytes = 1;
    for (size_t n = remaining; n >= 128; n /= 128)
      ++varintBytes;
    const size_t packetBytes = 1 + varintBytes + remaining;
    if (packetBytes > MaxTxCapacity || packetBytes + TxControlReserve > txCapacity_)
      return false;
    if (!dropOldest_)
      return txFree() >= TxControlReserve && packetBytes <= txFree() - TxControlReserve;
    bool selected[MaxTxSegments]{};
    return selectDrops_(packetBytes, selected);
  }
  bool setTxCapacity(uint16_t bytes) {
    if (bytes < TxCapacity || bytes > MaxTxCapacity)
      return false;
    if (bytes == txCapacity_)
      return true;
    if (txUsed_)
      return false;
    uint8_t* replacement = nullptr;
    if (bytes != TxCapacity) {
      replacement = static_cast<uint8_t*>(std::malloc(bytes));
      if (!replacement)
        return false;
    }
    std::free(txExtra_);
    txExtra_ = replacement;
    txCapacity_ = bytes;
    txHead_ = 0;
    return true;
  }
  void setTxStallTimeoutMs(uint32_t ms) {
    txStallTimeoutMs_ = ms == 0 ? TxLifetimeMs : std::max<uint32_t>(2000, std::min<uint32_t>(ms, 120000));
  }

  void beginLoop() {
    budget_ = PumpByteBudget;
    timeBudgetUs_ = PumpTimeBudgetUs;
    beginTxBudget_();
  }

  void setFrameCapacity(uint16_t bytes) {
    capacity_ = std::max<uint16_t>(8, std::min<uint16_t>(bytes, MaxFrameBytes));
    if (used_ > capacity_ || total_ > capacity_)
      reject_(stats_.rxOversize);
  }

  const MqttBoundedIoStats& stats() const {
    return stats_;
  }
  uint16_t pendingBytes() const {
    return used_;
  }
  void rejectHost() {
    reject_(stats_.rejectedHosts);
  }

  int connect(IPAddress ip, uint16_t port) override {
    stop();
    beginLoop();
    const int result = socket_.connect(ip, port, 100);
    awaitingConnack_ = result != 0;
    if (!result)
      ++stats_.connectFailures;
    return result;
  }

  int connect(const char* host, uint16_t port) override {
    IPAddress ip;
    if (!host || !ip.fromString(host)) {
      rejectHost();
      return 0;
    }
    return connect(ip, port);
  }

  void pump() {
    pumpTx_();
    const uint32_t startedUs = micros();
    pump_(startedUs);
    const uint32_t elapsed = static_cast<uint32_t>(micros() - startedUs);
    timeBudgetUs_ = elapsed >= timeBudgetUs_ ? 0 : timeBudgetUs_ - elapsed;
  }

private:
  void pump_(uint32_t startedUs) {
    if (ready_)
      return;
    if (used_ && static_cast<uint32_t>(millis() - startedMs_) >= FrameLifetimeMs) {
      reject_(stats_.rxTimeouts);
      return;
    }
    const int fd = socket_.fd();
    if (fd < 0)
      return;
    while (budget_ && static_cast<uint32_t>(micros() - startedUs) < timeBudgetUs_) {
      const uint16_t wanted = total_ ? static_cast<uint16_t>(total_ - used_) : 1;
      const uint16_t count = std::min<uint16_t>(budget_, wanted);
      const int received = ::recv(fd, frame_ + used_, count, MSG_DONTWAIT);
      if (received < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
          stop();
        return;
      }
      if (received == 0) {
        stop();
        return;
      }
      if (used_ && static_cast<uint32_t>(millis() - startedMs_) >= FrameLifetimeMs) {
        reject_(stats_.rxTimeouts);
        return;
      }
      if (used_ == 0)
        startedMs_ = millis();
      budget_ -= static_cast<uint16_t>(received);
      used_ += static_cast<uint16_t>(received);
      if (!parse_())
        return;
      if (total_ && used_ == total_) {
        if (!validBody_()) {
          reject_(stats_.rxMalformed);
          return;
        }
        ready_ = true;
        awaitingConnack_ = false;
        ++stats_.rxFrames;
        return;
      }
    }
  }

public:
  int available() override {
    // PubSubClient waits synchronously here (socket timeout: 1 second), so no
    // outer CM loop can replenish the budget for a delayed/fragmented CONNACK.
    // Only this four-byte handshake frame may replenish its per-poll budget.
    if (awaitingConnack_) {
      budget_ = PumpByteBudget;
      timeBudgetUs_ = PumpTimeBudgetUs;
      // TX retries share one real-time epoch; polling cannot manufacture budget.
      if (static_cast<uint32_t>(micros() - txEpochUs_) >= TxTimeBudgetUs)
        beginTxBudget_();
    }
    pump();
    return ready_ ? used_ - cursor_ : 0;
  }
  int peek() override {
    return ready_ ? frame_[cursor_] : -1;
  }
  int read() override {
    if (!ready_)
      return -1;
    const uint8_t value = frame_[cursor_++];
    if (cursor_ == used_)
      clearFrame_();
    return value;
  }
  int read(uint8_t* output, size_t bytes) override {
    if (!ready_ || !output)
      return 0;
    const size_t count = std::min<size_t>(bytes, used_ - cursor_);
    std::memcpy(output, frame_ + cursor_, count);
    cursor_ += static_cast<uint16_t>(count);
    if (cursor_ == used_)
      clearFrame_();
    return static_cast<int>(count);
  }
  size_t write(uint8_t byte) override {
    return write(&byte, 1);
  }
  size_t write(const uint8_t* bytes, size_t count) override {
    if (!count)
      return 0;
    if (!bytes || socket_.fd() < 0)
      return 0;
    pumpTx_();
    if (socket_.fd() < 0)
      return 0;
    const bool appPacket = applicationPublish_;
    if (dropOldest_ && appPacket) {
      // A marked write must contain precisely one complete QoS 0 PUBLISH.
      if (!isCompletePublish_(bytes, count))
        return 0;
      bool selected[MaxTxSegments]{};
      if (!selectDrops_(count, selected)) {
        ++stats_.txRejectedPublishes;
        return 0;
      }
      compactDropped_(selected);
    }
    if (count > static_cast<size_t>(txCapacity_ - txUsed_)) {
      ++stats_.txOverflows;
      reject_(stats_.txFailures);
      return 0;
    }
    if (!txUsed_)
      txProgressMs_ = millis();
    const uint16_t tail = static_cast<uint16_t>((txHead_ + txUsed_) % txCapacity_);
    const size_t first = std::min<size_t>(count, txCapacity_ - tail);
    uint8_t* tx = txExtra_ ? txExtra_ : tx_;
    std::memcpy(tx + tail, bytes, first);
    std::memcpy(tx, bytes + first, count - first);
    if (dropOldest_)
      appendSegment_(static_cast<uint16_t>(count), appPacket);
    txUsed_ += static_cast<uint16_t>(count);
    pumpTx_();
    // Success means ownership of the complete chunk, not a broker acknowledgement.
    return socket_.fd() >= 0 ? count : 0;
  }
  // PubSubClient calls flush() before stop(); never wait for pending TCP data.
  void flush() override {}
  void stop() override {
    socket_.stop();
    clearFrame_();
    txUsed_ = txHead_ = 0;
    txProgressMs_ = 0;
    txBlocked_ = false;
    awaitingConnack_ = false;
    txSegmentCount_ = 0;
    applicationPublish_ = false;
  }
  uint8_t connected() override {
    return socket_.connected();
  }
  operator bool() override {
    return connected();
  }

private:
  WiFiClient socket_;
  uint8_t frame_[MaxFrameBytes]{};
  uint8_t tx_[TxCapacity]{};
  uint8_t* txExtra_ = nullptr;
  uint16_t txCapacity_ = TxCapacity;
  uint32_t txStallTimeoutMs_ = TxLifetimeMs;
  struct TxSegment {
    uint16_t bytes;
    bool publish;
    bool started;
  };
  TxSegment txSegments_[MaxTxSegments]{};
  uint8_t txSegmentCount_ = 0;
  bool dropOldest_ = false;
  bool applicationPublish_ = false;
  uint16_t txHead_ = 0;
  uint16_t txUsed_ = 0;
  uint16_t txCallsLeft_ = TxCallBudget;
  uint32_t txTimeLeftUs_ = TxTimeBudgetUs;
  uint32_t txEpochUs_ = 0;
  uint32_t txProgressMs_ = 0;
  bool txBlocked_ = false;
  MqttBoundedIoStats stats_;
  uint32_t startedMs_ = 0;
  uint32_t remaining_ = 0;
  uint32_t timeBudgetUs_ = PumpTimeBudgetUs;
  uint16_t capacity_ = 1024;
  uint16_t used_ = 0;
  uint16_t cursor_ = 0;
  uint16_t total_ = 0;
  uint16_t budget_ = PumpByteBudget;
  uint8_t headerBytes_ = 0;
  bool ready_ = false;
  bool awaitingConnack_ = false;

  void beginTxBudget_() {
    txCallsLeft_ = TxCallBudget;
    txTimeLeftUs_ = TxTimeBudgetUs;
    txEpochUs_ = micros();
    txBlocked_ = false;
  }

  static bool isCompletePublish_(const uint8_t* bytes, size_t count) {
    if (count < 2 || (bytes[0] & 0xfe) != 0x30)
      return false;
    size_t remaining = 0;
    size_t multiplier = 1;
    for (size_t i = 1; i < count && i <= 4; ++i) {
      const uint8_t digit = bytes[i];
      remaining += static_cast<size_t>(digit & 127) * multiplier;
      if (!(digit & 128))
        return remaining == count - i - 1 && remaining >= 3;
      multiplier *= 128;
    }
    return false;
  }

  bool selectDrops_(size_t bytes, bool* selected) const {
    if (bytes > txCapacity_ || bytes + TxControlReserve > txCapacity_)
      return false;
    size_t available = txFree();
    unsigned removed = 0;
    const size_t required = bytes + TxControlReserve;
    if (available >= required && txSegmentCount_ < MaxTxSegments)
      return true;
    for (uint8_t i = 0; i < txSegmentCount_; ++i) {
      const TxSegment& segment = txSegments_[i];
      if (segment.publish && !segment.started) {
        selected[i] = true;
        available += segment.bytes;
        ++removed;
        if (available >= required && txSegmentCount_ - removed < MaxTxSegments)
          return true;
      }
    }
    return false;
  }

  void compactDropped_(const bool* selected) {
    bool any = false;
    for (uint8_t i = 0; i < txSegmentCount_; ++i)
      any = any || selected[i];
    if (!any)
      return;
    uint8_t* tx = txExtra_ ? txExtra_ : tx_;
    uint16_t writeOffset = 0;
    uint16_t readIndex = txHead_;
    uint16_t writeIndex = txHead_;
    uint8_t output = 0;
    for (uint8_t i = 0; i < txSegmentCount_; ++i) {
      const TxSegment segment = txSegments_[i];
      if (selected[i]) {
        ++stats_.txDroppedPublishes;
        stats_.txDroppedBytes += segment.bytes;
        readIndex = static_cast<uint16_t>((readIndex + segment.bytes) % txCapacity_);
        continue;
      }
      for (uint16_t j = 0; j < segment.bytes; ++j) {
        if (readIndex != writeIndex)
          tx[writeIndex] = tx[readIndex];
        ++writeOffset;
        if (++readIndex == txCapacity_)
          readIndex = 0;
        if (++writeIndex == txCapacity_)
          writeIndex = 0;
      }
      txSegments_[output++] = segment;
    }
    txUsed_ = writeOffset;
    txSegmentCount_ = output;
  }

  void appendSegment_(uint16_t bytes, bool publish) {
    if (!publish && txSegmentCount_ && !txSegments_[txSegmentCount_ - 1].publish) {
      txSegments_[txSegmentCount_ - 1].bytes += bytes;
      return;
    }
    if (txSegmentCount_ == MaxTxSegments) {
      // Control traffic cannot be dropped. Conservatively protect the entire
      // existing stream if its exact descriptors cannot be retained.
      txSegments_[0] = {txUsed_, false, true};
      txSegmentCount_ = txUsed_ ? 1 : 0;
      if (!publish && txSegmentCount_) {
        txSegments_[0].bytes += bytes;
        return;
      }
    }
    txSegments_[txSegmentCount_++] = {bytes, publish, false};
  }

  void consumeSegments_(uint16_t bytes) {
    while (bytes && txSegmentCount_) {
      TxSegment& head = txSegments_[0];
      if (bytes < head.bytes) {
        head.bytes -= bytes;
        head.started = true;
        return;
      }
      bytes -= head.bytes;
      for (uint8_t i = 1; i < txSegmentCount_; ++i)
        txSegments_[i - 1] = txSegments_[i];
      --txSegmentCount_;
    }
  }

  bool expireTx_() {
    if (txUsed_ && static_cast<uint32_t>(millis() - txProgressMs_) >= txStallTimeoutMs_) {
      ++stats_.txTimeouts;
      reject_(stats_.txFailures);
      return true;
    }
    return false;
  }

  void pumpTx_() {
    if (expireTx_() || !txUsed_ || txBlocked_)
      return;
    const uint32_t startedUs = micros();
    while (txUsed_ && txCallsLeft_ && static_cast<uint32_t>(micros() - startedUs) < txTimeLeftUs_) {
      if (expireTx_())
        break;
      const uint16_t count = std::min<uint16_t>(txUsed_, txCapacity_ - txHead_);
      --txCallsLeft_;
      const uint8_t* tx = txExtra_ ? txExtra_ : tx_;
      const int sent = ::send(socket_.fd(), tx + txHead_, count, MSG_DONTWAIT);
      if (sent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
        ++stats_.txBackpressure;
        txBlocked_ = true;
        break;
      }
      if (sent <= 0 || sent > count) {
        reject_(stats_.txFailures);
        break;
      }
      if (sent < count)
        ++stats_.txPartialWrites;
      if (dropOldest_)
        consumeSegments_(static_cast<uint16_t>(sent));
      txHead_ = static_cast<uint16_t>((txHead_ + sent) % txCapacity_);
      txUsed_ -= static_cast<uint16_t>(sent);
      txProgressMs_ = millis();
      if (!txUsed_) {
        txHead_ = 0;
        txProgressMs_ = 0;
      }
    }
    const uint32_t elapsed = static_cast<uint32_t>(micros() - startedUs);
    txTimeLeftUs_ = elapsed >= txTimeLeftUs_ ? 0 : txTimeLeftUs_ - elapsed;
  }

  void clearFrame_() {
    used_ = cursor_ = total_ = 0;
    remaining_ = 0;
    headerBytes_ = 0;
    startedMs_ = 0;
    ready_ = false;
  }
  void reject_(uint32_t& counter) {
    ++counter;
    stop();
  }

  bool parse_() {
    const uint8_t type = frame_[0] >> 4;
    const uint8_t flags = frame_[0] & 15;
    if (used_ == 1) {
      if (awaitingConnack_ && frame_[0] != 0x20) {
        reject_(stats_.rxMalformed);
        return false;
      }
      // Server-to-client MQTT 3.1.1 packets supported by PubSubClient 2.8.
      const bool allowed = type == 2 || type == 3 || (type >= 4 && type <= 7) ||
                           type == 9 || (type >= 11 && type <= 14);
      const bool flagsOk = type == 3 ? ((flags & 6) <= 2) : flags == (type == 6 ? 2 : 0);
      if (!allowed || !flagsOk) {
        reject_(stats_.rxMalformed);
        return false;
      }
      return true;
    }
    if (!headerBytes_) {
      const uint8_t digit = frame_[used_ - 1];
      remaining_ += static_cast<uint32_t>(digit & 127) << ((used_ - 2) * 7);
      if (digit & 128) {
        if (used_ == 5) {
          reject_(stats_.rxMalformed);
          return false;
        }
        return true;
      }
      if (used_ > 2 && digit == 0) {
        reject_(stats_.rxMalformed);
        return false;
      }
      headerBytes_ = static_cast<uint8_t>(used_);
      if (remaining_ + headerBytes_ > capacity_) {
        reject_(stats_.rxOversize);
        return false;
      }
      total_ = static_cast<uint16_t>(remaining_ + headerBytes_);
      const bool lengthOk = type == 3 ? remaining_ >= 3 : type == 9  ? remaining_ >= 3
                                                        : type >= 12 ? remaining_ == 0
                                                                     : remaining_ == 2;
      if (!lengthOk) {
        reject_(stats_.rxMalformed);
        return false;
      }
    }
    return true;
  }

  bool validBody_() const {
    if ((frame_[0] >> 4) == 2) {
      return frame_[headerBytes_] <= 1 && frame_[headerBytes_ + 1] <= 5;
    }
    if ((frame_[0] >> 4) != 3)
      return true;
    const uint16_t topicBytes = (static_cast<uint16_t>(frame_[headerBytes_]) << 8) |
                                frame_[headerBytes_ + 1];
    const uint8_t qosBytes = (frame_[0] & 6) ? 2 : 0;
    if (!topicBytes || static_cast<uint32_t>(topicBytes) + 2 + qosBytes > remaining_)
      return false;
    for (uint16_t i = 0; i < topicBytes; ++i) {
      const uint8_t c = frame_[headerBytes_ + 2 + i];
      if (!c || c == '#' || c == '+')
        return false;
    }
    if (qosBytes) {
      const uint16_t offset = headerBytes_ + 2 + topicBytes;
      if (!frame_[offset] && !frame_[offset + 1])
        return false;
    }
    return true;
  }
};

} // namespace detail
} // namespace cm
