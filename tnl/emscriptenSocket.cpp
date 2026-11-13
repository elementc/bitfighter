#include "emscriptenSocket.h"

#include "tnlLog.h"

#include <emscripten/websocket.h>

#include <cstdint>
#include <cstring>
#include <deque>
#include <map>
#include <string>
#include <vector>

using namespace TNL;

namespace
{

const uint8_t BIND = 1;
const uint8_t SEND = 2;
const uint8_t RECV = 3;
const S32 MAX_PAYLOAD = 8192;

struct Packet
{
   U32 ip;
   U16 port;
   std::vector<U8> data;
};

struct RelaySock
{
   U32 id;
   bool bound;
   std::deque<Packet> inbox;
   std::deque<std::vector<U8>> waiting;
};

std::string gUrl;
bool gUrlReady = false;
EMSCRIPTEN_WEBSOCKET_T gWs = 0;
bool gOpen = false;
std::map<U32, RelaySock> gSocks;
std::deque<std::vector<U8>> gEarly;
U32 gNextId = 1;

void writeU32(U8 *p, U32 v)
{
   p[0] = U8(v >> 24);
   p[1] = U8(v >> 16);
   p[2] = U8(v >> 8);
   p[3] = U8(v);
}

void writeU16(U8 *p, U16 v)
{
   p[0] = U8(v >> 8);
   p[1] = U8(v);
}

U32 readU32(const U8 *p)
{
   return (U32(p[0]) << 24) | (U32(p[1]) << 16) | (U32(p[2]) << 8) | U32(p[3]);
}

U16 readU16(const U8 *p)
{
   return U16((U16(p[0]) << 8) | U16(p[1]));
}

void sendFrame(const std::vector<U8> &frame)
{
   if(!gOpen)
   {
      gEarly.push_back(frame);
      return;
   }
   emscripten_websocket_send_binary(gWs, (void *)frame.data(), frame.size());
}

void flushEarly()
{
   while(!gEarly.empty())
   {
      std::vector<U8> frame = gEarly.front();
      gEarly.pop_front();
      emscripten_websocket_send_binary(gWs, frame.data(), frame.size());
   }
}

void handleFrame(const U8 *data, U32 len)
{
   if(len < 1)
      return;
   if(data[0] == BIND && len >= 7)
   {
      U32 id = readU32(data + 1);
      U16 port = readU16(data + 5);
      std::map<U32, RelaySock>::iterator it = gSocks.find(id);
      if(it == gSocks.end())
         return;
      it->second.bound = true;
      logprintf(LogConsumer::LogUDP, "Relay bound socket %u on UDP port %u", id, port);
      while(!it->second.waiting.empty())
      {
         sendFrame(it->second.waiting.front());
         it->second.waiting.pop_front();
      }
   }
   else if(data[0] == RECV && len >= 13)
   {
      U32 id = readU32(data + 1);
      U32 ip = readU32(data + 5);
      U16 port = readU16(data + 9);
      U16 payload = readU16(data + 11);
      if(U32(13 + payload) > len)
         return;
      std::map<U32, RelaySock>::iterator it = gSocks.find(id);
      if(it == gSocks.end())
         return;
      Packet packet;
      packet.ip = ip;
      packet.port = port;
      packet.data.assign(data + 13, data + 13 + payload);
      it->second.inbox.push_back(packet);
   }
}

EM_BOOL onOpen(int, const EmscriptenWebSocketOpenEvent *, void *)
{
   gOpen = true;
   logprintf(LogConsumer::LogUDP, "UDP relay websocket open: %s", gUrl.c_str());
   flushEarly();
   return EM_TRUE;
}

EM_BOOL onClose(int, const EmscriptenWebSocketCloseEvent *, void *)
{
   gOpen = false;
   logprintf(LogConsumer::LogUDP, "UDP relay websocket closed");
   return EM_TRUE;
}

EM_BOOL onError(int, const EmscriptenWebSocketErrorEvent *, void *)
{
   logprintf(LogConsumer::LogError, "UDP relay websocket error (%s)", gUrl.c_str());
   return EM_TRUE;
}

EM_BOOL onMessage(int, const EmscriptenWebSocketMessageEvent *event, void *)
{
   if(event->isText)
      return EM_TRUE;
   handleFrame(event->data, event->numBytes);
   return EM_TRUE;
}

void ensureSocket()
{
   if(gWs)
      return;
   EmscriptenWebSocketCreateAttributes attr;
   emscripten_websocket_init_create_attributes(&attr);
   attr.url = gUrl.c_str();
   gWs = emscripten_websocket_new(&attr);
   emscripten_websocket_set_onopen_callback(gWs, 0, onOpen);
   emscripten_websocket_set_onclose_callback(gWs, 0, onClose);
   emscripten_websocket_set_onerror_callback(gWs, 0, onError);
   emscripten_websocket_set_onmessage_callback(gWs, 0, onMessage);
}

std::string readRelayUrl()
{
   char buf[512];
   buf[0] = 0;
   EM_ASM({
      var q = '';
      try {
         q = new URLSearchParams(window.location.search).get('relay') || '';
      } catch (e) {}
      if (!q && typeof Module !== 'undefined' && Module['relay'])
         q = Module['relay'];
      stringToUTF8(q, $0, $1);
   }, buf, (int)sizeof(buf));
   return std::string(buf);
}

}

bool TNL::emscriptenRelayEnabled()
{
   if(!gUrlReady)
   {
      gUrl = readRelayUrl();
      gUrlReady = true;
      if(!gUrl.empty())
         logprintf(LogConsumer::LogUDP, "UDP relay configured: %s", gUrl.c_str());
   }
   return !gUrl.empty();
}

bool TNL::emscriptenRelayBind(S32 &outFd, U16 port)
{
   if(!emscriptenRelayEnabled())
      return false;
   ensureSocket();
   RelaySock sock;
   sock.id = gNextId++;
   sock.bound = false;
   gSocks[sock.id] = sock;

   std::vector<U8> frame(7);
   frame[0] = BIND;
   writeU32(&frame[1], sock.id);
   writeU16(&frame[5], port);
   sendFrame(frame);

   outFd = S32(sock.id);
   return true;
}

void TNL::emscriptenRelayClose(S32 fd)
{
   gSocks.erase(U32(fd));
}

bool TNL::emscriptenRelayOwns(S32 fd)
{
   return fd > 0 && gSocks.find(U32(fd)) != gSocks.end();
}

NetError TNL::emscriptenRelaySendTo(S32 fd, const Address &address, const U8 *buffer, S32 bufferSize)
{
   std::map<U32, RelaySock>::iterator it = gSocks.find(U32(fd));
   if(it == gSocks.end())
      return InvalidPacketProtocol;
   if(bufferSize < 0 || bufferSize > MAX_PAYLOAD)
      return InvalidPacketProtocol;

   std::vector<U8> frame(13 + bufferSize);
   frame[0] = SEND;
   writeU32(&frame[1], it->second.id);
   writeU32(&frame[5], address.netNum[0]);
   writeU16(&frame[9], address.port);
   writeU16(&frame[11], U16(bufferSize));
   if(bufferSize)
      memcpy(&frame[13], buffer, bufferSize);

   if(!it->second.bound)
      it->second.waiting.push_back(frame);
   else
      sendFrame(frame);
   return NoError;
}

NetError TNL::emscriptenRelayRecvFrom(S32 fd, Address *address, U8 *buffer, S32 bufferSize, S32 *outSize)
{
   std::map<U32, RelaySock>::iterator it = gSocks.find(U32(fd));
   if(it == gSocks.end() || it->second.inbox.empty())
      return WouldBlock;
   Packet packet = it->second.inbox.front();
   it->second.inbox.pop_front();
   if(S32(packet.data.size()) > bufferSize)
      return InvalidPacketProtocol;
   address->transport = IPProtocol;
   address->port = packet.port;
   address->netNum[0] = packet.ip;
   address->netNum[1] = 0;
   address->netNum[2] = 0;
   address->netNum[3] = 0;
   if(!packet.data.empty())
      memcpy(buffer, &packet.data[0], packet.data.size());
   *outSize = S32(packet.data.size());
   return NoError;
}
