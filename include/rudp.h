#pragma once
#include <arpa/inet.h>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory.h>
#include <netinet/in.h>
#include <ratio>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>

#include "packet.h"
#include "tsqueue.h"

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
  Buffer Receive(std::chrono::milliseconds ms);

private:
  const int Send(Buffer &buffer, const size_t buffer_len, uint32_t sequence,
                 const uint8_t type);
  const int SendConstructed(const Buffer &packet, const size_t packet_len);
  const int SendAndWait(Buffer &buffer, const size_t buffer_len, uint8_t type);
  void QueueAck(uint32_t sequence, uint32_t length);
  void AckPacket(uint32_t sequence, uint32_t length);
  const int RetrievePacket(TBPacket &packet, sockaddr_in *received_addr);
  const uint32_t ProcessPacket(TBPacket &received_packet,
                               sockaddr_in &received_addr,
                               Buffer *retrieved_buffer);

  std::thread SetupSenderThread();
  std::thread SetupReceiverThread();

private:
  struct QueuePacket {
    Buffer buffer;
    size_t buffer_len;
    uint32_t sequence;

    QueuePacket() = default;
    QueuePacket(Buffer _buffer, size_t _buffer_len, uint32_t _sequence)
        : buffer(std::move(_buffer)), buffer_len(_buffer_len),
          sequence(_sequence) {}
  };

private:
  int m_sock;
  sockaddr_in m_local_addr;
  sockaddr_in m_peer_addr;

  uint32_t m_sequence;
  std::atomic<bool> m_connected;

  // queues to put send and received packets
  TSQueue<QueuePacket> m_send_queue;
  TSQueue<Buffer> m_received_queues;

  // thread ids of the running threads
  std::thread m_sender_thread;
  std::thread m_receiver_thread;

  // maximum tries for sending a packet before giving up
  static const uint8_t MAX_TRIES = 10;
};
} // namespace Hev
