# CLI Chat App
This is a CLI chat app I wrote for a networking class based on raw TCP sockets and a custom protocol.

This comes as a client and server, the server supports having multiple channels, and users can join and leave channels, and switch the current channel.

The `distributed` channel contains an implementation for a distributed server system where a number of servers can be started and linked together. Users can then join separate servers and chat with anyone on any server linked to their server.