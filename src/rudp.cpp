#include "rudp.h"
#include "errors.h"
#include "packet.h"
#include <bits/types/struct_timeval.h>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <sys/select.h>
#include <thread>

#define MAX_BUFFER_LEN 2048
namespace Hev {
TBD::TBD(const char *local_addr, const int local_port)
    : m_sequence(0), m_connected(false) {
  // set up local socket info where we'll be listening from
  m_sock = socket(AF_INET, SOCK_DGRAM, 0);
  m_local_addr.sin_addr.s_addr = INADDR_ANY;
  m_local_addr.sin_port = htons(local_port);
  m_local_addr.sin_family = AF_INET;
}

TBD::TBD(TBD &&other) {
  if (this == &other)
    return;

  this->m_sock = other.m_sock;
  other.m_sock = -1;
  this->m_local_addr = other.m_local_addr;
  this->m_sequence = other.m_sequence;
  this->m_connected = other.m_connected.load();

  // if it is running we want to kill the other thread so
  // we can start it on this object instead
  other.m_connected = false;
  if (other.m_receiver_thread.joinable()) {
    // stop other threads
    other.m_sender_thread.join();
    other.m_receiver_thread.join();
    other.m_ping_thread.join();
    // move over any pending messages
    this->m_send_queue = std::move(other.m_send_queue);
    this->m_received_queues = std::move(other.m_received_queues);
    // set up this threads
    m_receiver_thread = SetupReceiverThread();
    m_sender_thread = SetupSenderThread();
    m_ping_thread = SetupPingThread();
  }
}

TBD::~TBD() {
  // signal the threads to close
  m_connected.store(false);
  // we detach here to non block the user
  // valgrind throws error for this
  if (m_sender_thread.joinable())
    m_sender_thread.detach();
  if (m_receiver_thread.joinable())
    m_receiver_thread.detach();
  if (m_ping_thread.joinable())
    m_ping_thread.detach();
  // shouldn't overwrite the standard fds
  if (m_sock > 2)
    close(m_sock);
}

const int TBD::SetUpPeerInfo(const char *peer_ip, const int peer_port) {
  // peer addr info where we'll be sending to
  m_peer_addr.sin_port = htons(peer_port);
  m_peer_addr.sin_family = AF_INET;
  inet_pton(AF_INET, peer_ip, &m_peer_addr.sin_addr);
  return 0;
}

TBD TBD::Bind(const char *local_addr, const int local_port) {
  TBD socket(local_addr, local_port);

  // wait for a syn then send a a syn back
  if (bind(socket.m_sock, (const sockaddr *)&socket.m_local_addr,
           sizeof(socket.m_local_addr)) < 0) {
    throw 0;
  }

  return socket;
}

const int TBD::Listen(const char *peer_ip, const int peer_port) {
  if (SetUpPeerInfo(peer_ip, peer_port) != 0)
    return INVALID_PEER;

  bool acked = false;
  int times = 6;
  while (!acked && times > 0) {
    TBPacket received_packet = {};
    sockaddr_in received_addr = {};
    int status = 0;
    Buffer empty_buffer;
    times--;
    if ((status = RetrievePacket(received_packet, &received_addr)) != 0) {
      continue;
    }
    if (ProcessPacket(received_packet, received_addr, nullptr) ==
        RECEIVED_ACK) {
      m_sequence = received_packet.header.sequence;
      SendAndWait(empty_buffer, 0, PacketType::SYNACK);
      acked = true;
    }
  }
  if (acked) {
    m_connected.store(true);
    m_receiver_thread = SetupReceiverThread();
    m_sender_thread = SetupSenderThread();
    m_ping_thread = SetupPingThread();
    return 0;
  } else {
    return HANDSHAKE_FAIL;
  }
}
// bind socket, wait for SYN then send a SYN back and wait for the ACK
const int TBD::Connect(const char *peer_ip, const int peer_port) {
  if (SetUpPeerInfo(peer_ip, peer_port) != 0)
    return INVALID_PEER;

  int status = 0;
  m_sequence = 1;
  Buffer empty_buffer;
  // send takes care of the SYNACK
  if ((status = SendAndWait(empty_buffer, 0, PacketType::SYN)) < 0) {
    return status;
  }
  AckPacket(1, 1);
  m_connected.store(true);
  m_receiver_thread = SetupReceiverThread();
  m_sender_thread = SetupSenderThread();
  m_ping_thread = SetupPingThread();
  return 0;
}

std::pair<Buffer, size_t> TBD::BuildAndUpdatePacket(Buffer &buffer,
                                                    const size_t buffer_len,
                                                    uint8_t type) {
  auto packet_and_len = BuildPacket(type, m_sequence, buffer, buffer_len);
  m_sequence += buffer_len;
  return packet_and_len;
}

const int TBD::Send(Buffer &buffer, const size_t buffer_len, uint8_t type) {
  // not connected to a peer
  if (!m_connected)
    return SOCKET_CLOSED;
  return QueueSend(buffer, buffer_len, type);
}

const int TBD::QueueSend(std::unique_ptr<uint8_t[]> &buffer,
                         const size_t buffer_len, const uint8_t type) {
  return QueuePacket(buffer, buffer_len, type, m_sequence);
  return 0;
}

const int TBD::QueueRetransmit(SharedBuffer &buffer, const size_t buffer_len,
                               const uint32_t sequence) {
  m_send_queue.emplace(buffer, buffer_len, sequence);
  return 0;
}

void TBD::QueueAck(uint32_t sequence, uint32_t length) {
  Buffer empty_load;
  auto [packet, packet_len] =
      BuildPacket(PacketType::ACK, sequence + length, empty_load, 0);
  m_send_queue.emplace(std::move(packet), packet_len, sequence + length);
}

const int TBD::QueuePacket(Buffer &buffer, const size_t buffer_len,
                           const uint8_t type, const uint32_t sequence) {
  auto [packet, packet_len] = BuildAndUpdatePacket(buffer, buffer_len, type);
  m_send_queue.emplace(std::move(packet), packet_len, sequence);
  return 0;
}

const int TBD::SendConstructed(const Buffer &packet, const size_t packet_len) {
  return SendConstructed(packet.get(), packet_len);
}

const int TBD::SendConstructed(const SharedBuffer &packet,
                               const size_t packet_len) {
  return SendConstructed(packet.get(), packet_len);
}

const int TBD::SendConstructed(const uint8_t *packet, const size_t packet_len) {
  // setup the timeout

  fd_set write_fds;
  int select_ret = 0;
  timeval tv;
  tv.tv_sec = 2;
  tv.tv_usec = 0;
  FD_ZERO(&write_fds);
  FD_SET(m_sock, &write_fds);

  select_ret = select(m_sock + 1, NULL, &write_fds, NULL, &tv);

  // timeout or error
  if (select_ret == 0 || select_ret < 1) {
    return -1;
  }
  return sendto(m_sock, packet, packet_len, 0, (const sockaddr *)&m_peer_addr,
                sizeof(m_peer_addr));
}

const int TBD::SendAndWait(Buffer &buffer, const size_t buffer_len,
                           uint8_t type) {
  auto [packet, packet_len] = BuildPacket(type, m_sequence, buffer, buffer_len);
  int status = -1;
  uint8_t total_tries = 0;
  bool acked = false;
  while (!acked && ++total_tries < MAX_TRIES) {
    status = SendConstructed(packet, packet_len);
    if (status > 0) {
      total_tries += MAX_TRIES;
    }

    // wait for an ack
    TBPacket retrieved_pack = {};
    sockaddr_in received_addr;
    if (RetrievePacket(retrieved_pack, &received_addr) != 0) {
      continue;
    }
    if (ProcessPacket(retrieved_pack, received_addr, nullptr) == RECEIVED_ACK) {
      acked = true;
    }
  }

  return status;
}

const int TBD::Receive(Buffer *buffer) {
  // not connected to a peer
  if (!m_connected)
    return SOCKET_CLOSED;
  Buffer payload;
  if (!m_received_queues.pop_wait(&payload))
    return RECEIVE_ERROR;
  if (!buffer)
    return INVALID_PARAM;
  *buffer = std::move(payload);
  return 0;
}

const int TBD::Receive(Buffer *buffer, std::chrono::milliseconds ms) {

  // not connected to a peer
  if (!m_connected)
    return SOCKET_CLOSED;
  Buffer payload;
  if (!m_received_queues.pop_wait_till(ms, &payload))
    return RECEIVE_ERROR;
  if (!buffer)
    return INVALID_PARAM;
  *buffer = std::move(payload);
  return 0;
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
  tv.tv_sec = 2;
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
    return RECEIVE_ERROR;
  }
  packet = RebuildPacket(std::move(buffer));
  if (_received_addr) {
    *(_received_addr) = received_addr;
  }
  return 0;
}

const uint32_t TBD::ProcessPacket(TBPacket &received_packet,
                                  sockaddr_in &received_addr,
                                  Buffer *retrieved_buffer) {
  const uint32_t received_seq = received_packet.header.sequence;
  const uint16_t packet_type = received_packet.header.type;

  // make sure received address is from whom we expect
  if (received_addr.sin_addr.s_addr != m_peer_addr.sin_addr.s_addr) {
    // disregard
    return UNRECOGNIZED_PEER;
  }

  if (packet_type & PacketType::SYNACK) {
    // make sure the packet sequence is the same as current
    // sequence
    if (received_seq < m_sequence) {
      std::cout << "Retransmitting\n";
      // retransmit
      std::vector<SendPacket> packets_to_retransmit(
          m_unacked_packets.GetGreaterThan(received_seq));
      for (auto &packet : packets_to_retransmit) {
        QueueRetransmit(packet.buffer, packet.buffer_len, packet.sequence);
      }
    } else {
    }
    return RECEIVED_ACK;
  }
  if (packet_type & PacketType::PING) {
    Buffer empty_load;
    QueueSend(empty_load, 0, PacketType::PONG);
    return RECEIVED_PING;
  }
  if (packet_type & PacketType::PONG) {
    m_ponged = true;
    return RECEIVED_PONG;
  }
  // any other message we acknowledge it and return teh payload
  QueueAck(received_seq, received_packet.header.length);
  if (retrieved_buffer)
    *retrieved_buffer = std::move(received_packet.payload);
  return RECEIVED_PACKET;
}

void TBD::AckPacket(uint32_t sequence, uint32_t length) {
  Buffer empty_load;
  auto [packet, packet_len] =
      BuildPacket(PacketType::ACK, sequence + length, empty_load, 0);

  sendto(m_sock, packet.get(), packet_len, 0, (const sockaddr *)&m_peer_addr,
         sizeof(m_peer_addr));
}

std::thread TBD::SetupSenderThread() {
  return std::thread([this]() {
    while (this->m_connected || !this->m_send_queue.empty()) {
      SendPacket packet_struct;
      if (!this->m_send_queue.pop_wait_till(std::chrono::milliseconds(2000),
                                            &packet_struct))
        continue;
      int total_tries = 0;
      int status = 0;
      while (++total_tries < MAX_TRIES) {
        status =
            SendConstructed(packet_struct.buffer, packet_struct.buffer_len);
        if (status > 0) {
          // add packet to the ack map
          this->m_unacked_packets.insert(packet_struct.sequence, packet_struct);
          total_tries += MAX_TRIES;
        }
      }
    }
  });
}

std::thread TBD::SetupReceiverThread() {

  return std::thread([this]() {
    while (this->m_connected) {
      TBPacket received_packet{};
      sockaddr_in received_addr;
      Buffer received_buffer;

      if (RetrievePacket(received_packet, &received_addr) != 0)
        continue;

      if (ProcessPacket(received_packet, received_addr, &received_buffer) !=
          RECEIVED_PACKET)
        continue;

      this->m_received_queues.emplace(std::move(received_buffer));
    }
  });
}

std::thread TBD::SetupPingThread() {
  return std::thread([this]() {
    auto start = std::chrono::high_resolution_clock::now();
    std::chrono::seconds timeout(60);
    while (this->m_connected) {
      if (this->m_ponged.load()) {
        auto now = std::chrono::high_resolution_clock::now();
        auto diff = now - start;
        this->m_ponged = false;
      } else {
        auto now = std::chrono::high_resolution_clock::now();
        auto diff = now - start;
        if (now - start > timeout) {
          // lost connection
          this->m_connected = false;
          this->m_received_queues.release_all_blocks();
        }
      }

      // send a ping
      Buffer empty_load;
      this->QueueSend(empty_load, 0, PacketType::PING);
      // wait to check
      std::this_thread::sleep_for(std::chrono::seconds(15));
    }
  });
}

} // namespace Hev
