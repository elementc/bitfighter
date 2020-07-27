#pragma once

#ifdef BF_DISCORD

#include <string>
#include "tnlTypes.h"
using TNL::U32;
using TNL::S64;
using TNL::S32;
using TNL::S8;
using std::string;

#include "AppIntegrator.h"
#include "../discord-rpc/include/discord_rpc.h"

namespace Zap {

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

            string getNickname();

            void updateBitfighterState();

    };
}
#endif
