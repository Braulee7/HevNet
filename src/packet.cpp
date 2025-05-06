#include "packet.h"
#include <arpa/inet.h>
#include <cstring>

namespace Hev {

std::pair<std::unique_ptr<uint8_t[]>, size_t>
BuildPacket(uint32_t type, uint32_t sequence,
            std::unique_ptr<uint8_t[]> &payload, uint32_t payload_len) {

  size_t total_length = sizeof(TBHeader) + payload_len;
  // get byte order correct
  TBPacket packet = {.header = {.type = htons(type),
                                .sequence = htonl(sequence),
                                .length = htonl(payload_len)},
                     .payload = nullptr};
  packet.payload.swap(payload);

  // copy into buffer
  size_t offset = 0;
  std::unique_ptr<uint8_t[]> buffer(
      std::make_unique<uint8_t[]>(sizeof(TBHeader) + payload_len));

  std::memcpy(buffer.get(), &packet.header, sizeof(TBHeader));
  offset += sizeof(TBHeader);
  std::memcpy(buffer.get() + offset, packet.payload.get(), payload_len);
  offset += payload_len;

  return std::make_pair(std::move(buffer), total_length);
}

TBPacket RebuildPacket(std::unique_ptr<uint8_t[]> buffer) {
  TBPacket packet = {};
  std::memcpy(&packet.header, buffer.get(), sizeof(TBHeader));
  // convert to host byte order
  packet.header = {.type = ntohs(packet.header.type),
                   .sequence = ntohl(packet.header.sequence),
                   .length = ntohl(packet.header.length)};
  // check if there's a payload to copy
  if (packet.header.length > 0) {
    packet.payload = std::make_unique<uint8_t[]>(packet.header.length);
    std::memcpy(packet.payload.get(), buffer.get() + sizeof(TBHeader),
                packet.header.length);
  }
  return packet;
}

} // namespace Hev
