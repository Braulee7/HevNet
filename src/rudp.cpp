#include "rudp.h"
#include "errors.h"
#include "packet.h"
#include <bits/types/struct_timeval.h>
#include <cstdint>
#include <cstring>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>

#define MAX_BUFFER_LEN 2048
namespace Hev {
TBD::TBD(const char *local_addr, const int local_port, const char *peer_ip,
         const int peer_port)
    : m_sequence(0), m_connected(false) {
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

const int TBD::Listen() {
  // wait for a syn then send a a syn back
  if (bind(m_sock, (const sockaddr *)&m_local_addr, sizeof(m_local_addr)) < 0) {
    return -1;
  }
  Buffer empty_buffer;
  bool acked = false;
  int times = 6;
  while (!acked && times > 0) {
    TBPacket received_packet = {};
    sockaddr_in received_addr = {};
    int status = 0;
    times--;
    if ((status = RetrievePacket(received_packet, &received_addr)) != 0) {
      continue;
    }
    if (ProcessPacket(received_packet, received_addr, nullptr)) {
      m_sequence = received_packet.header.sequence;
      Send(empty_buffer, 0, PacketType::SYNACK);
      acked = true;
    }
  }
  if (acked) {
    m_connected = true;
    return 0;
  } else {
    return -1;
  }
}
// bind socket, wait for SYN then send a SYN back and wait for the ACK
const int TBD::Connect() {

  if (bind(m_sock, (const sockaddr *)&m_local_addr, sizeof(m_local_addr)) < 0) {
    return -1;
  }
  Buffer empty_buffer;
  int status = 0;
  m_sequence = 1;
  // send takes care of the SYNACK
  if ((status = Send(empty_buffer, 0, PacketType::SYN)) < 0) {
    return status;
  }
  AckPacket(0, 1);
  m_connected = true;
  return 0;
}

const int TBD::Send(Buffer &buffer, const size_t buffer_len, uint8_t type) {
  return Send(buffer, buffer_len, m_sequence, type);
}

const int TBD::Send(std::unique_ptr<uint8_t[]> &buffer, const size_t buffer_len,
                    uint32_t sequence, const uint8_t type) {
  auto [packet, packet_len] = BuildPacket(type, sequence, buffer, buffer_len);
  bool acked = false;
  int status = -1;
  uint8_t total_tries = 0;
  while (!acked && ++total_tries < MAX_TRIES) {
    status = sendto(m_sock, packet.get(), packet_len, 0,
                    (const sockaddr *)&m_peer_addr, sizeof(m_peer_addr));
    // wait for an ack
    TBPacket retrieved_pack = {};
    sockaddr_in received_addr;
    if (RetrievePacket(retrieved_pack, &received_addr) != 0) {
      continue;
    }
    if (ProcessPacket(retrieved_pack, received_addr, nullptr)) {
      acked = true;
    }
  }
  return status;
}

Buffer TBD::Receive() {
  Buffer payload = nullptr;
  uint8_t total_tries = 0;
  while (!payload && ++total_tries < MAX_TRIES) {
    TBPacket received_packet = {};
    sockaddr_in received_addr = {};
    if (RetrievePacket(received_packet, &received_addr) != 0) {
      return nullptr;
    }
    bool status = ProcessPacket(received_packet, received_addr, &payload);
  }
  return std::move(payload);
}

const int TBD::RetrievePacket(TBPacket &packet, sockaddr_in *_received_addr) {
  TBPacket received_packet = {};
  Buffer buffer = std::make_unique<uint8_t[]>(MAX_BUFFER_LEN);
  size_t received_len = 0;
  sockaddr_in received_addr;
  socklen_t received_addr_len = sizeof(received_addr);
  // setup the timeout
  fd_set read_fds;
  int select_ret = 0;
  timeval tv;
  tv.tv_sec = 15;
  tv.tv_usec = 0;
  FD_ZERO(&read_fds);
  FD_SET(m_sock, &read_fds);

  select_ret = select(m_sock + 1, &read_fds, NULL, NULL, &tv);

  if (select_ret == 0) {
    return TIMEOUT;
  } else if (select_ret < 1) {
    return -1;
  }
  received_len = recvfrom(m_sock, (uint8_t *)buffer.get(), MAX_BUFFER_LEN, 0,
                          (sockaddr *)&received_addr, &received_addr_len);
  // got nothing
  if (received_len < 0) {
    return -1;
  }
  packet = RebuildPacket(std::move(buffer));
  if (_received_addr) {
    *(_received_addr) = received_addr;
  }
  return 0;
}

const bool TBD::ProcessPacket(TBPacket &received_packet,
                              sockaddr_in &received_addr,
                              Buffer *retrieved_buffer) {
  const uint32_t received_seq = received_packet.header.sequence;
  const uint16_t packet_type = received_packet.header.type;

  // make sure received address is from whom we expect
  if (received_addr.sin_addr.s_addr != m_peer_addr.sin_addr.s_addr) {
    // disregard
    return false;
  }

  if (packet_type & PacketType::SYNACK) {
    return true;
  }
  // any other message we acknowledge it and return teh payload
  AckPacket(received_seq, received_packet.header.length);
  if (retrieved_buffer)
    *retrieved_buffer = std::move(received_packet.payload);
  return true;
}

void TBD::AckPacket(uint32_t sequence, uint32_t length) {
  m_sequence = sequence;
  Buffer empty_load;
  auto [packet, packet_len] =
      BuildPacket(PacketType::ACK, sequence + length, empty_load, 0);

  sendto(m_sock, packet.get(), packet_len, 0, (const sockaddr *)&m_peer_addr,
         sizeof(m_peer_addr));
}

} // namespace Hev
