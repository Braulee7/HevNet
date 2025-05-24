// Errors specific for the networking library

namespace Hev {
namespace Net {
#define SOCKET_CLOSED 0x3001
#define EMPTY_BUFFER 0x3002
#define BIND_SOCKET_ERROR 0x3003

#define RECEIVE_ERROR 0x3100
#define TIMEOUT 0x3101
#define UNRECOGNIZED_PEER 0x3102

#define RECEIVE_SUCCESS 0x3200
#define RECEIVED_ACK 0x3201
#define RECEIVED_PACKET 0x3202
#define RECEIVED_PING 0x3203
#define RECEIVED_PONG 0x3204
} // namespace Net
} // namespace Hev
