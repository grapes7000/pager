#include "pager_link.h"

#include <WiFi.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <string.h>

namespace {
constexpr uint8_t kBroadcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

bool sameMac(const uint8_t a[6], const uint8_t b[6]) {
  return memcmp(a, b, 6) == 0;
}

void printMac(const uint8_t mac[6]) {
  Serial.printf("%02X:%02X:%02X:%02X:%02X:%02X",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}
}  // namespace

PagerLink* PagerLink::instance_ = nullptr;

bool PagerLink::begin() {
  if (initialized_) return true;

  instance_ = this;
  rawQueue_ = xQueueCreate(kRawQueueCapacity, sizeof(RawFrame));
  if (!rawQueue_) {
    Serial.println("[LINK] ERROR: receive queue allocation failed");
    return false;
  }

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, false);
  delay(20);

  esp_err_t err = esp_wifi_set_channel(PAGER_LINK_CHANNEL, WIFI_SECOND_CHAN_NONE);
  if (err != ESP_OK) {
    Serial.printf("[LINK] ERROR: failed to set Wi-Fi channel (%d)\n", (int)err);
    return false;
  }

  err = esp_now_init();
  if (err != ESP_OK) {
    Serial.printf("[LINK] ERROR: esp_now_init failed (%d)\n", (int)err);
    return false;
  }

  err = esp_now_register_recv_cb(receiveCallback);
  if (err != ESP_OK) {
    Serial.printf("[LINK] ERROR: receive callback registration failed (%d)\n", (int)err);
    return false;
  }

  if (!addEspNowPeer(kBroadcastMac)) {
    Serial.println("[LINK] ERROR: could not add broadcast peer");
    return false;
  }

  sessionId_ = esp_random();
  if (sessionId_ == 0) sessionId_ = 1;
  initialized_ = true;
  lastHelloMs_ = millis() - kHelloIntervalMs;

  Serial.printf("[LINK] ready id=%u name=%s channel=%u mac=%s\n",
                (unsigned)PAGER_DEVICE_ID, PAGER_DEVICE_NAME,
                (unsigned)PAGER_LINK_CHANNEL, WiFi.macAddress().c_str());
  return true;
}

bool PagerLink::peerAvailable() const {
  return initialized_ && peerKnown_ &&
         (uint32_t)(millis() - lastPeerSeenMs_) <= kPeerFreshMs;
}

bool PagerLink::addEspNowPeer(const uint8_t mac[6]) {
  if (esp_now_is_peer_exist(mac)) return true;

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, mac, 6);
  peer.channel = PAGER_LINK_CHANNEL;
  peer.ifidx = WIFI_IF_STA;
  peer.encrypt = false;

  esp_err_t err = esp_now_add_peer(&peer);
  if (err == ESP_OK || err == ESP_ERR_ESPNOW_EXIST) return true;

  Serial.printf("[LINK] esp_now_add_peer failed (%d) for ", (int)err);
  printMac(mac);
  Serial.println();
  return false;
}

bool PagerLink::learnPeer(const uint8_t mac[6]) {
  if (peerKnown_ && sameMac(peerMac_, mac)) {
    lastPeerSeenMs_ = millis();
    return true;
  }

  if (peerKnown_ && esp_now_is_peer_exist(peerMac_)) {
    esp_now_del_peer(peerMac_);
  }

  if (!addEspNowPeer(mac)) return false;

  memcpy(peerMac_, mac, 6);
  peerKnown_ = true;
  lastPeerSeenMs_ = millis();
  Serial.print("[LINK] peer discovered: ");
  printMac(peerMac_);
  Serial.println();
  return true;
}

void PagerLink::receiveCallback(const esp_now_recv_info_t* info,
                                const uint8_t* data, int length) {
  if (!instance_ || !info || !info->src_addr || !data) return;
  instance_->enqueueRaw(info->src_addr, data, length);
}

void PagerLink::enqueueRaw(const uint8_t source[6], const uint8_t* data,
                           int length) {
  if (!rawQueue_ || length < (int)kHeaderSize ||
      length > (int)sizeof(Packet)) {
    return;
  }

  RawFrame frame = {};
  memcpy(frame.source, source, 6);
  frame.length = (uint16_t)length;
  memcpy(frame.data, data, (size_t)length);
  xQueueSend(rawQueue_, &frame, 0);
}

bool PagerLink::validPacket(const Packet& packet, size_t length) const {
  if (packet.magic != kMagic || packet.version != kProtocolVersion) return false;
  if (packet.senderId != PAGER_PEER_ID) return false;
  if (packet.recipientId != PAGER_DEVICE_ID &&
      packet.recipientId != kBroadcastRecipient) {
    return false;
  }
  if (packet.payloadLength > kMaxMessageLength) return false;
  if (length != kHeaderSize + packet.payloadLength) return false;

  PacketType type = (PacketType)packet.type;
  if (type != PacketType::Hello && type != PacketType::Message &&
      type != PacketType::Ack) {
    return false;
  }
  if (type == PacketType::Message && packet.recipientId != PAGER_DEVICE_ID) {
    return false;
  }
  if (type == PacketType::Ack && packet.recipientId != PAGER_DEVICE_ID) {
    return false;
  }
  if (type != PacketType::Message && packet.payloadLength != 0) return false;
  return true;
}

void PagerLink::processRawFrames() {
  if (!rawQueue_) return;
  RawFrame frame;
  while (xQueueReceive(rawQueue_, &frame, 0) == pdTRUE) {
    processPacket(frame);
  }
}

void PagerLink::processPacket(const RawFrame& frame) {
  Packet packet = {};
  memcpy(&packet, frame.data, frame.length);
  if (!validPacket(packet, frame.length)) return;

  if (!learnPeer(frame.source)) return;

  PacketType type = (PacketType)packet.type;
  if (type == PacketType::Hello) return;

  if (type == PacketType::Ack) {
    acknowledgePending(packet.sessionId, packet.messageId);
    return;
  }

  // ACK every valid message, including duplicates. This lets a sender recover
  // when the original ACK was lost without displaying the text twice.
  sendAck(packet.sessionId, packet.messageId);

  if (wasRecentlyReceived(packet.sessionId, packet.messageId)) {
    Serial.printf("[LINK] duplicate message %lu ignored\n",
                  (unsigned long)packet.messageId);
    return;
  }

  rememberReceived(packet.sessionId, packet.messageId);
  pushIncoming(packet.messageId, packet.payload, packet.payloadLength);
  Serial.printf("[LINK] received message %lu (%u bytes)\n",
                (unsigned long)packet.messageId,
                (unsigned)packet.payloadLength);
}

bool PagerLink::sendPacket(const uint8_t mac[6], const Packet& packet) {
  size_t length = kHeaderSize + packet.payloadLength;
  esp_err_t err = esp_now_send(mac, reinterpret_cast<const uint8_t*>(&packet),
                               length);
  if (err == ESP_OK) return true;

  Serial.printf("[LINK] send enqueue failed (%d)\n", (int)err);
  return false;
}

void PagerLink::sendHello() {
  Packet packet = {};
  packet.magic = kMagic;
  packet.version = kProtocolVersion;
  packet.type = (uint8_t)PacketType::Hello;
  packet.senderId = PAGER_DEVICE_ID;
  packet.recipientId = kBroadcastRecipient;
  packet.sessionId = sessionId_;
  sendPacket(kBroadcastMac, packet);
  lastHelloMs_ = millis();
}

void PagerLink::sendAck(uint32_t sessionId, uint32_t messageId) {
  if (!peerKnown_) return;

  Packet packet = {};
  packet.magic = kMagic;
  packet.version = kProtocolVersion;
  packet.type = (uint8_t)PacketType::Ack;
  packet.senderId = PAGER_DEVICE_ID;
  packet.recipientId = PAGER_PEER_ID;
  packet.sessionId = sessionId;
  packet.messageId = messageId;
  sendPacket(peerMac_, packet);
}

bool PagerLink::queueMessage(uint32_t id, const String& body) {
  if (!initialized_ || body.isEmpty() || body.length() > kMaxMessageLength ||
      outgoingCount_ >= kOutgoingCapacity) {
    return false;
  }

  size_t slot = (outgoingHead_ + outgoingCount_) % kOutgoingCapacity;
  OutgoingMessage& message = outgoing_[slot];
  message = OutgoingMessage{};
  message.sessionId = sessionId_;
  message.id = id;
  message.length = (uint16_t)body.length();
  memcpy(message.body, body.c_str(), message.length);
  message.body[message.length] = '\0';
  ++outgoingCount_;

  Serial.printf("[LINK] queued message %lu; pending=%u\n",
                (unsigned long)id, (unsigned)outgoingCount_);
  return true;
}

void PagerLink::trySendPending() {
  if (!peerKnown_ || outgoingCount_ == 0) return;

  OutgoingMessage& pending = outgoing_[outgoingHead_];
  uint32_t now = millis();
  if (pending.awaitingAck &&
      (uint32_t)(now - pending.lastAttemptMs) < kRetryIntervalMs) {
    return;
  }

  Packet packet = {};
  packet.magic = kMagic;
  packet.version = kProtocolVersion;
  packet.type = (uint8_t)PacketType::Message;
  packet.senderId = PAGER_DEVICE_ID;
  packet.recipientId = PAGER_PEER_ID;
  packet.sessionId = pending.sessionId;
  packet.messageId = pending.id;
  packet.payloadLength = pending.length;
  memcpy(packet.payload, pending.body, pending.length);

  pending.lastAttemptMs = now;
  pending.awaitingAck = true;
  if (sendPacket(peerMac_, packet)) {
    Serial.printf("[LINK] sent message %lu; awaiting ACK\n",
                  (unsigned long)pending.id);
  }
}

void PagerLink::acknowledgePending(uint32_t sessionId, uint32_t id) {
  if (outgoingCount_ == 0) return;

  OutgoingMessage& pending = outgoing_[outgoingHead_];
  if (pending.sessionId != sessionId || pending.id != id) return;

  Serial.printf("[LINK] delivered message %lu\n", (unsigned long)id);
  pending = OutgoingMessage{};
  outgoingHead_ = (outgoingHead_ + 1) % kOutgoingCapacity;
  --outgoingCount_;
}

bool PagerLink::wasRecentlyReceived(uint32_t sessionId, uint32_t id) const {
  for (size_t i = 0; i < kRecentCapacity; ++i) {
    if (recent_[i].used && recent_[i].sessionId == sessionId &&
        recent_[i].id == id) {
      return true;
    }
  }
  return false;
}

void PagerLink::rememberReceived(uint32_t sessionId, uint32_t id) {
  recent_[recentNext_] = {sessionId, id, true};
  recentNext_ = (recentNext_ + 1) % kRecentCapacity;
}

void PagerLink::pushIncoming(uint32_t id, const char* body, size_t length) {
  if (incomingCount_ >= kIncomingCapacity) {
    // Keep the newest traffic if the UI falls behind.
    incoming_[incomingHead_] = PagerIncomingMessage{};
    incomingHead_ = (incomingHead_ + 1) % kIncomingCapacity;
    --incomingCount_;
  }

  PagerIncomingMessage& message = incoming_[incomingTail_];
  message.id = id;
  message.body = String();
  message.body.reserve(length);
  for (size_t i = 0; i < length; ++i) message.body += body[i];

  incomingTail_ = (incomingTail_ + 1) % kIncomingCapacity;
  ++incomingCount_;
}

bool PagerLink::popReceived(PagerIncomingMessage& message) {
  if (incomingCount_ == 0) return false;
  message = incoming_[incomingHead_];
  incoming_[incomingHead_] = PagerIncomingMessage{};
  incomingHead_ = (incomingHead_ + 1) % kIncomingCapacity;
  --incomingCount_;
  return true;
}

void PagerLink::update() {
  if (!initialized_) return;

  processRawFrames();

  uint32_t now = millis();
  if ((uint32_t)(now - lastHelloMs_) >= kHelloIntervalMs) sendHello();

  trySendPending();
}
