#pragma once
#include <arpa/inet.h>
#include <cstdint>
#include <memory.h>
#include <memory>
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
  const int Connect();
  const int Send(std::unique_ptr<uint8_t[]> &buffer, const size_t buffer_len,
                 const uint8_t type = PacketType::MSG);
  std::unique_ptr<uint8_t[]> Receive();

private:
  int m_sock;
  sockaddr_in m_local_addr;
  sockaddr_in m_peer_addr;

  uint32_t m_sequence;
};
} // namespace Hev
