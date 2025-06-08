# Hev Net
This is a basic networking library meant to be used with multiplayer games.
The goal is to have this library be extensive and be cross platform but currently
only supports POSIX sockets and is VERY EARLY in development

# Protocols
All protocols implemented will be documented here
## Turn Based Datagram
This is a reliable UDP protocol which allows users to connect to other users
using UDP. It currently suppoorts ACKs, sequencing, and retransmissions. It's designed
to be a non-blocking public API so users can either send and forget and receive either 
immediately or wait until a message arrives. It does so by using a multi-threaded 
design with queues to to send a receive packets from the connected peer.

Users must first `Bind` an IP and port to the socket via the `Bind` function call then they 
can initiate a connection to a peer. 

In order to connect to a peer, one must initiate a connection via the `Listen` function
call then a peer can connect with the `Connect` function and the peer IP and port.
Users can then start messaging back and forth by creating Buffers of the message they wish to send.
Connections are currently maintained throughout the life time of the socket object and 
disconnect automatically via RAII.

This is still very early in development and hope to get more detailed documentation and protocols
developed in the future. 
