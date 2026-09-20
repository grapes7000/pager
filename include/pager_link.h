#pragma once

#include <Arduino.h>
#include <esp_now.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#ifndef PAGER_DEVICE_ID
#define PAGER_DEVICE_ID 1
#endif

#ifndef PAGER_PEER_ID
#define PAGER_PEER_ID 2
#endif

#ifndef PAGER_DEVICE_NAME
#define PAGER_DEVICE_NAME "Pager1"
#endif

#ifndef PAGER_PEER_NAME
#define PAGER_PEER_NAME "Pager2"
#endif

#ifndef PAGER_LINK_CHANNEL
#define PAGER_LINK_CHANNEL 6
#endif

struct PagerIncomingMessage {
  uint32_t id = 0;
  String body;
};

class PagerLink {
 public:
  static constexpr size_t kMaxMessageLength = 160;

  bool begin();
  void update();

  // Queues a message even when the other pager is offline. The oldest queued
  // message is retried until an application-level ACK is received.
  bool queueMessage(uint32_t id, const String& body);
  bool popReceived(PagerIncomingMessage& message);

  bool ready() const { return initialized_; }
  bool peerAvailable() const;
  size_t pendingCount() const { return outgoingCount_; }
  const char* deviceName() const { return PAGER_DEVICE_NAME; }
  const char* peerName() const { return PAGER_PEER_NAME; }

 private:
  enum class PacketType : uint8_t {
    Hello = 1,
    Message = 2,
    Ack = 3,
  };

  static constexpr uint16_t kMagic = 0x5047;  // "PG"
  static constexpr uint8_t kProtocolVersion = 1;
  static constexpr uint8_t kBroadcastRecipient = 0xFF;
  static constexpr uint32_t kHelloIntervalMs = 2000;
  static constexpr uint32_t kRetryIntervalMs = 1200;
  static constexpr uint32_t kPeerFreshMs = 6500;
  static constexpr size_t kRawQueueCapacity = 8;
  static constexpr size_t kIncomingCapacity = 8;
  static constexpr size_t kOutgoingCapacity = 8;
  static constexpr size_t kRecentCapacity = 8;

  struct __attribute__((packed)) Packet {
    uint16_t magic;
    uint8_t version;
    uint8_t type;
    uint8_t senderId;
    uint8_t recipientId;
    uint32_t sessionId;
    uint32_t messageId;
    uint16_t payloadLength;
    char payload[kMaxMessageLength];
  };

  static constexpr size_t kHeaderSize = offsetof(Packet, payload);
  static_assert(sizeof(Packet) <= ESP_NOW_MAX_DATA_LEN,
                "Pager ESP-NOW packet must fit in an ESP-NOW v1 packet");

  struct RawFrame {
    uint8_t source[6];
    uint16_t length;
    uint8_t data[sizeof(Packet)];
  };

  struct OutgoingMessage {
    uint32_t sessionId = 0;
    uint32_t id = 0;
    uint16_t length = 0;
    char body[kMaxMessageLength + 1] = {};
    uint32_t lastAttemptMs = 0;
    bool awaitingAck = false;
  };

  struct RecentMessage {
    uint32_t sessionId = 0;
    uint32_t id = 0;
    bool used = false;
  };

  bool initialized_ = false;
  bool peerKnown_ = false;
  uint8_t peerMac_[6] = {};
  uint32_t sessionId_ = 0;
  uint32_t lastHelloMs_ = 0;
  uint32_t lastPeerSeenMs_ = 0;

  QueueHandle_t rawQueue_ = nullptr;

  PagerIncomingMessage incoming_[kIncomingCapacity];
  size_t incomingHead_ = 0;
  size_t incomingTail_ = 0;
  size_t incomingCount_ = 0;

  OutgoingMessage outgoing_[kOutgoingCapacity];
  size_t outgoingHead_ = 0;
  size_t outgoingCount_ = 0;

  RecentMessage recent_[kRecentCapacity];
  size_t recentNext_ = 0;

  static PagerLink* instance_;
  static void receiveCallback(const esp_now_recv_info_t* info,
                              const uint8_t* data, int length);

  void enqueueRaw(const uint8_t source[6], const uint8_t* data, int length);
  void processRawFrames();
  void processPacket(const RawFrame& frame);
  bool validPacket(const Packet& packet, size_t length) const;

  bool addEspNowPeer(const uint8_t mac[6]);
  bool learnPeer(const uint8_t mac[6]);
  bool sendPacket(const uint8_t mac[6], const Packet& packet);
  void sendHello();
  void sendAck(uint32_t sessionId, uint32_t messageId);
  void trySendPending();

  bool wasRecentlyReceived(uint32_t sessionId, uint32_t id) const;
  void rememberReceived(uint32_t sessionId, uint32_t id);
  void pushIncoming(uint32_t id, const char* body, size_t length);
  void acknowledgePending(uint32_t sessionId, uint32_t id);
};
