//------------------------------------------------------------------------------
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#ifndef ZAP_APPINTEGRATOR_H_
#define ZAP_APPINTEGRATOR_H_

#ifdef BF_STEAM
// These imports need to happen outside namespace Zap
# include <iostream>
# include "steam.grpc.pb.h"
# include <grpcpp/grpcpp.h>
#endif

#include "tnlTypes.h"
#include "tnlVector.h"

#include <string>


//using namespace TNL;
using TNL::U32;
using TNL::S64;
using TNL::S32;
using TNL::S8;
using TNL::Vector;

namespace Zap
{

using namespace std;

class Timer;

// Abstract class for any application integrator
struct AppIntegrator
{
   AppIntegrator();
   virtual ~AppIntegrator();

   // Pure virtual functions only
   virtual void init() = 0;
   virtual void shutdown() = 0;
   virtual void runCallbacks() = 0;

   virtual void updateState(const string &state) = 0;
   virtual void updateDetails(const string &details) = 0;
};


// Class called from Bitfighter code to run all the IntegratedApp stuff
class AppIntegrationController
{
private:
   static Vector<AppIntegrator*> mAppIntegrators;
   static Timer mCallbackTimer;

public:
	AppIntegrationController();
	virtual ~AppIntegrationController();

	static void init();
   static void shutdown();

   static void idle(U32 deltaTime);
   static void runCallbacks();

   // This method should be generic enough to update as much as possible in any
   // of the applications; consider moving to a struct of some sort
   static void updateState(const string &state, const string &details);
};


#ifdef BF_DISCORD

#include "../discord-rpc/include/discord_rpc.h"

// This struct mirrors the
struct PersistentRichPresence
{
   string state;   /* max 128 bytes */
   string details; /* max 128 bytes */
   S64 startTimestamp;
   S64 endTimestamp;
   string largeImageKey;  /* max 32 bytes */
   string largeImageText; /* max 128 bytes */
   string smallImageKey;  /* max 32 bytes */
   string smallImageText; /* max 128 bytes */
   string partyId;        /* max 128 bytes */
   S32 partySize;
   S32 partyMax;
   string matchSecret;    /* max 128 bytes */
   string joinSecret;     /* max 128 bytes */
   string spectateSecret; /* max 128 bytes */
   S8 instance;
};



class DiscordIntegrator : public AppIntegrator
{
private:
   static string discordClientId;

   S64 mStartTime;
   PersistentRichPresence mPersistentPresence;
   DiscordRichPresence   mRichPresence;

   static void handleDiscordReady(const DiscordUser *connectedUser);
   static void handleDiscordDisconnected(int errcode, const char *message);
   static void handleDiscordError(int errcode, const char *message);
   static void handleDiscordJoin(const char *secret);
   static void handleDiscordSpectate(const char *secret);
   static void handleDiscordJoinRequest(const DiscordUser *request);

public:
   DiscordIntegrator();
   virtual ~DiscordIntegrator();

   void init();
   void shutdown();
   void runCallbacks();

   void updatePresence();

   void updateState(const string &state);
   void updateDetails(const string &details);

   void updateBitfighterState();

};
#endif

#ifdef BF_STEAM

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using steam::Steam;
using steam::SteamworksInitRequest;
using steam::SteamworksInitResult;
using steam::StateUpdateMessage;
using steam::StateUpdateAck;
using steam::DetailsUpdateMessage;
using steam::DetailsUpdateAck;


struct SteamState {
   bool is_init = false;
   std::string user_name = "";
};

class SteamClient {
 public:
  SteamClient(std::shared_ptr<Channel> channel)
      : stub_(Steam::NewStub(channel)) {}

  // Assembles the client's payload, sends it and presents the response back
  // from the server.
  void InitSteamworks(SteamState& state) {
    // Data we are sending to the server.
    SteamworksInitRequest request;

    // Container for the data we expect from the server.
    SteamworksInitResult result;

    // Context for the client. It could be used to convey extra information to
    // the server and/or tweak certain RPC behaviors.
    ClientContext context;

    // The actual RPC.
    Status status = stub_->InitSteamworks(&context, request, &result);

    // Act upon its status.
    if (status.ok()) {
      state.is_init = result.succeeded();
      state.user_name = result.user_name();

      if (state.is_init){
         std::cout << "Steamworks initialization succeeded for user " << state.user_name << "." << std::endl;
      } else {
         std::cout << "Steamworks initialization failed for no particular reason." << std::endl;
      }
    } else {
      std::cout << "Steamworks initialization failed with local error code " << status.error_code() << ": " << status.error_message() << std::endl;
    }
  }

  void UpdateGameState(const std::string state_message){
     StateUpdateMessage message;
     message.set_state(state_message);
     StateUpdateAck ack;
     ClientContext context;
     Status status = stub_->UpdateGameState(&context, message, &ack);
  }

  void UpdateGameDetails(const std::string details_message){
     DetailsUpdateMessage message;
     message.set_details(details_message);
     DetailsUpdateAck ack;
     ClientContext context;
     Status status = stub_->UpdateGameDetails(&context, message, &ack);
  }

 private:
  std::unique_ptr<Steam::Stub> stub_;
};



class SteamIntegrator : public AppIntegrator
{
private:
   SteamClient sc;
   SteamState state;

public:
   SteamIntegrator();
   virtual ~SteamIntegrator();

   void init();
   void shutdown();
   void runCallbacks();

   void updateState(const string &state);
   void updateDetails(const string &details);

};



#endif

} /* namespace Zap */

#endif /* ZAP_APPINTEGRATOR_H_ */
