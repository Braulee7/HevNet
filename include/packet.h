// basic structure for the packets that a TBD
// socket will/should be receiving
#pragma once
#include <cstdint>
#include <memory>

namespace Hev {
struct PacketType {
  static const uint16_t ACK = 0x01;
  static const uint16_t PING = 0x02;
  static const uint16_t MSG = 0x03;
};

struct TBHeader {
  uint16_t type;
  uint32_t sequence;
  uint32_t length;
};

struct TBPacket {
  TBHeader header;
  std::unique_ptr<uint8_t[]> payload;
};

std::pair<std::unique_ptr<uint8_t[]>, size_t>
BuildPacket(uint32_t type, uint32_t sequence,
            std::unique_ptr<uint8_t[]> &payload, uint32_t payload_len);

TBPacket RebuildPacket(std::unique_ptr<uint8_t[]> buffer);
} // namespace Hev
