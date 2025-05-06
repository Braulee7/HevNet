#include "rudp.h"
#include <cstdint>
#include <cstring>
#include <memory>
#include <netinet/in.h>
#include <sys/socket.h>

#define MAX_BUFFER_LEN 2048
namespace Hev {
TBD::TBD(const char *local_addr, const int local_port, const char *peer_ip,
         const int peer_port)
    : m_sequence(0) {
  // set up local socket info where we'll be listening from
  m_sock = socket(AF_INET, SOCK_DGRAM, 0);
  m_local_addr.sin_addr.s_addr = INADDR_ANY;
  m_local_addr.sin_port = htons(local_port);
  m_local_addr.sin_family = AF_INET;
  // peer addr info where we'll be sending to
  m_peer_addr.sin_port = htons(peer_port);
  m_peer_addr.sin_family = AF_INET;
  inet_pton(AF_INET, peer_ip, &m_peer_addr.sin_addr);

  // TODO: set up sequencing variables
}

TBD::~TBD() {
  // shouldn't overwrite the standard fds
  if (m_sock > 2)
    close(m_sock);
}

const int TBD::Connect() {
  if (bind(m_sock, (const sockaddr *)&m_local_addr, sizeof(m_local_addr)) < 0) {
    return -1;
  }
  return 0;
}

const int TBD::Send(std::unique_ptr<uint8_t[]> &buffer, const size_t buffer_len,
                    const uint8_t type) {
  auto [packet, packet_len] = BuildPacket(type, m_sequence, buffer, buffer_len);
  const int status =
      sendto(m_sock, packet.get(), packet_len, 0,
             (const sockaddr *)&m_peer_addr, sizeof(m_peer_addr));
  return status;
}

std::unique_ptr<uint8_t[]> TBD::Receive() {
  size_t received_len = 0;
  std::unique_ptr<uint8_t[]> buffer =
      std::make_unique<uint8_t[]>(MAX_BUFFER_LEN);
  sockaddr_in received_addr;
  socklen_t received_addr_len = sizeof(received_addr);
  TBPacket received_packet = {};

  received_len = recvfrom(m_sock, (uint8_t *)buffer.get(), MAX_BUFFER_LEN, 0,
                          (sockaddr *)&received_addr, &received_addr_len);
  // got nothing
  if (received_len < 0) {
    return nullptr;
  }
  received_packet = RebuildPacket(std::move(buffer));

  // make sure received address is from whom we expect
  if (received_addr.sin_addr.s_addr != m_peer_addr.sin_addr.s_addr) {
    // disregard
    return nullptr;
  }

  // TODO: process the payload according to type
  return std::move(received_packet.payload);
}
} // namespace Hev
