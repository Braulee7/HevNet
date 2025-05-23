// basic structure for the packets that a TBD
// socket will/should be receiving
#pragma once
#include <cstdint>
#include <memory>

namespace Hev {
using Buffer = std::unique_ptr<uint8_t[]>;

struct PacketType {
  static const uint16_t SYN = 0x01;
  static const uint16_t ACK = 0x02;
  static const uint16_t SYNACK = SYN | ACK;
  static const uint16_t PING = 0x04;
  static const uint16_t MSG = 0x08;
};

struct TBHeader {
  uint16_t type;
  uint32_t sequence;
  uint32_t length;
};

struct TBPacket {
  TBHeader header;
  Buffer payload;
};

/* BuildPacket
 * Builds the packet from the given data. Serializes all the data
 * to have it ready to send to the network router.
 * params:
 *  type: the type of message being sent. Preferably should be a
 *    PacketType value
 *  sequence: the sequence number of the packet being sent
 *  payload: the actual data being sent
 *  payload_len: the length of the payload
 * return:
 *  a pair containing the serialised packet in a single buffer
 *  and the size of the buffer so it can send it to the socket
 */
std::pair<Buffer, size_t> BuildPacket(uint32_t type, uint32_t sequence,
                                      Buffer &payload, uint32_t payload_len);

/* RebuildPacket
 * Takes in a serialized packet and deserializes it so that the user
 * can inspect all of the elements of the packet. This relies on the fact
 * that the buffer is a TBPacket, if it is not behavior of this function
 * is undefined
 * params:
 *  buffer: the serialized packet *Note* it takes ownership so use of buffer
 *    after this is undefined
 *  returns:
 *    the rebuilt packet containing all  the information ready for the
 *    host to inspect.
 */
TBPacket RebuildPacket(Buffer buffer);
} // namespace Hev
