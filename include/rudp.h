#pragma once
#include <arpa/inet.h>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory.h>
#include <netinet/in.h>
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
  /* Await for your peer to connect to you, essentially you invite a peer
   * and await for them acknowledge your invitation
   * Listen:
   * Makes the peer wait passively until another peer initiates a
   * connection. Once a peer reaches out with a SYN the two peers
   * establish a connection through a three way handshake.
   * Returns: status of connection 0 if it is successful else otherwise
   */
  const int Listen();
  /* Join your peer which has sent you an invitation already to join
   * their circle
   * Connect:
   * Connects to a listening peer. Establishes a connection through a
   * three way handshake. Waits for until a connection is established
   * or returns. Unblocking call.
   * Returns: status of the connection 0 if successful, else otherwise
   */
  const int Connect();
  /* Send:
   * Sends a message to the peer connected to. Unblocking call and instead
   * queues the message to be sent whenever the peer and socket are ready.
   * Messages will not be guaranteed in order but are guaranteed to be
   * received.
   * Send before a connection is established is not guaranteed to be received
   * params
   *  buffer: The payload to send to the peer
   *  buffer_len: the length of the buffer to send
   *  type: type for the header of the packet. MSG by default since externally
   *    that makes the most sense but it's not restrictive.
   * Return: integer indicating status of the send
   */
  const int Send(Buffer &buffer, const size_t buffer_len,
                 const uint8_t type = PacketType::MSG);

  /* Receive:
   * Gets a message from the peer address. This is a blocking function
   * that won't return until a message is received. If the connection
   * has not been set up prior to this call then the call will block
   * indefenitely.
   * Returns: uint8_t[] containing the received payload
   */
  Buffer Receive();
  /* Receive:
   * param: ms - milliseconds to wait for a packet to be received.
   * Works just like Receive() but is a non blocking call. The call
   * will only block for ms milliseconds.
   * Return: uint8_t contianing the payload if a message was received
   * or nullptr if nothing was received.
   */
  Buffer Receive(std::chrono::milliseconds ms);

private:
  /*
   * BuildAndUpdatePacket
   * builds the packet with the payload and updates the sequence
   * number for this peer.
   * params:
   *  buffer: the payload to send to the peer
   *  buffer_len: the length of the payload
   *  type: the type of packet being sent
   * Returns: A pair with the serialized packet and the length of
   *  the packet to send.
   */
  std::pair<Buffer, size_t> BuildAndUpdatePacket(Buffer &buffer,
                                                 const size_t buffer_len,
                                                 const uint8_t type);
  /* QueueSend
   * Queues up a packet to send to the peer.
   * params:
   *  buffer: the payload to send to the peer. THis is the unbuilt packet
   *    just the payload
   *  buffer_len: the length of the payload
   *  type: the type of packet to send
   * return: status of the queue. Currently always returns 0
   */
  const int QueueSend(Buffer &buffer, const size_t buffer_len,
                      const uint8_t type);
  /* SendConstructed
   * Immediately sends a constructed packet to the socket.
   * params:
   *  packet: the built packet to send
   *  packet_len: the length of the packet
   * returns: an integer indicating the success of the send
   */
  const int SendConstructed(const Buffer &packet, const size_t packet_len);
  /* SendAndWait
   * Sends a packet and waits until an ack is received. This waits for
   * a small ammount of time and currently breaks the multi-threaded set up.
   * Only meant to be used to complete the three way handshake
   * params:
   *  buffer: the payload to send to the peer
   *  buffer_len: size of the payload
   *  type: type of packet to send
   */
  const int SendAndWait(Buffer &buffer, const size_t buffer_len, uint8_t type);
  /*
   * QueueAck
   * Queues up an acknowledgment to send to the peer. Nonblocking
   * params:
   *  sequence: sequence that's getting acknowledged
   *  length: length of the payload that was received
   */
  void QueueAck(uint32_t sequence, uint32_t length);
  /* AckPacket
   * immediately sends an ack to the peer. Breaks the multithreaded
   * design of the socket and should just be used in the threeway
   * handshake to initiate handshake
   * params:
   *  sequence: sequence that's getting acknowledged
   *  length: length of the payload that was received
   */
  void AckPacket(uint32_t sequence, uint32_t length);
  /* RetrievePacket
   * Waits for a packet to be received. This blocks for two seconds then
   * returns whether a packet was received or not.
   * params:
   *  packet: out - the packet that was received if any.
   *  received_addr: out + optional - the address of the peer that sent the
   *  packet if it was received
   * returns:
   *  status of the received. 0 if a packet was received. otherwise an
   *  error code is returned. the packet and address are returned through
   *  the parameters
   */
  const int RetrievePacket(TBPacket &packet, sockaddr_in *received_addr);
  /* ProcessPacket
   * Takes in a packet and parses the header to determine what to do.
   * if the address is not from our connected peer we discard the message.
   * An ack (SYNACK too) just gets noted, nothing more is done. Otherwise
   * an ack is sent back to the peer and the payload is returned to the
   * caller.
   * params:
   *  received_packet: in - the packet to process
   *  received_addr: in - the address that sent the packet
   *  retrieved_buffer: out + optional - the payload retrieved if any
   * returns:
   *  A status code is returned depending on the packet that was received
   *  the payload if one was received is returned through the parameter
   */
  const uint32_t ProcessPacket(TBPacket &received_packet,
                               sockaddr_in &received_addr,
                               Buffer *retrieved_buffer);
  /* SetupSenderThread
   * Creates the thread that will continuously send the the messages
   * that are queued up. Uses a nameless function in order to capture
   * the shared variables encapsulated in this class
   * returns:
   *  the thread object containing the id of the created thread
   */
  std::thread SetupSenderThread();
  /* SetupReceiverThread
   * Creates the thread that will received and process the messages
   * sent to this peer. It will queue up acks on our behalf and
   * queue up a payload if any are given.
   * returns:
   *  the thread object containing the id of the created thread
   */
  std::thread SetupReceiverThread();

private:
  /* SendPacket
   * internal struct that will be used to queue up the packets
   * that are ready to be sent. Contains the serialized buffer,
   * size of the buffer and the sequence number for this
   * specific packet.
   */
  struct SendPacket {
    Buffer buffer;
    size_t buffer_len;
    uint32_t sequence;

    SendPacket() = default;
    SendPacket(Buffer _buffer, size_t _buffer_len, uint32_t _sequence)
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
  TSQueue<SendPacket> m_send_queue;
  TSQueue<Buffer> m_received_queues;

  // thread ids of the running threads
  std::thread m_sender_thread;
  std::thread m_receiver_thread;

  // maximum tries for sending a packet before giving up
  static const uint8_t MAX_TRIES = 10;
};
} // namespace Hev
