#pragma once
#include <arpa/inet.h>
#include <cstdint>
#include <memory.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "packet.h"

namespace Hev {
class TBD {
public:
  TBD(const char *local_addr, const int local_port, const char *peer_ip,
      const int peer_port);
  ~TBD();
  // Await for your peer to connect to you, essentially you invite a peer
  // and await for them acknowledge your invitation
  const int Listen();
  // Join your peer which has sent you an invitation already to join
  // their circle
  const int Connect();
  const int Send(Buffer &buffer, const size_t buffer_len,
                 const uint8_t type = PacketType::MSG);
  Buffer Receive();

private:
  const int Send(Buffer &buffer, const size_t buffer_len, uint32_t sequence,
                 const uint8_t type);
  void AckPacket(uint32_t sequence, uint32_t length);
  const int RetrievePacket(TBPacket &packet, sockaddr_in *received_addr);
  const bool ProcessPacket(TBPacket &received_packet,
                           sockaddr_in &received_addr,
                           Buffer *retrieved_buffer);

private:
  int m_sock;
  sockaddr_in m_local_addr;
  sockaddr_in m_peer_addr;

  uint32_t m_sequence;
  bool m_connected;
  // maximum tries for sending a packet before giving up
  static const uint8_t MAX_TRIES = 10;
};
} // namespace Hev
