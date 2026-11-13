#ifndef _EMSCRIPTEN_SOCKET_H_
#define _EMSCRIPTEN_SOCKET_H_

// Browser stand-in for Socket::sendto / recvfrom. Compiled only for Emscripten.
// When a relay URL is configured (?relay=ws://host:port or Module.relay), UDP
// datagrams are framed and carried on one WebSocket. With no URL the POSIX
// path stays in place and a local game still uses connectLocal.

#include "tnlUDP.h"

namespace TNL
{

bool emscriptenRelayEnabled();
bool emscriptenRelayBind(S32 &outFd, U16 port);
void emscriptenRelayClose(S32 fd);
bool emscriptenRelayOwns(S32 fd);
NetError emscriptenRelaySendTo(S32 fd, const Address &address, const U8 *buffer, S32 bufferSize);
NetError emscriptenRelayRecvFrom(S32 fd, Address *address, U8 *buffer, S32 bufferSize, S32 *outSize);

}

#endif
