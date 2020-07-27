#include "DiscordIntegrator.h"

#include "GameManager.h"
#include "gameType.h"
#include "ClientGame.h"
#include "UIManager.h"
#include "UIGame.h"
#include "UIEditor.h"

#ifdef BF_DISCORD

////////////////////////////////////////
////////////////////////////////////////
// Discord

// Statics
// This is Bitfighter's discord ID
string DiscordIntegrator::discordClientId = "620788608743243776";


DiscordIntegrator::DiscordIntegrator()
{
   // Do nothing
   mStartTime = 0;
   mPersistentPresence = PersistentRichPresence();
}

DiscordIntegrator::~DiscordIntegrator()
{
   // Do nothing
}

void DiscordIntegrator::init()
{
   mStartTime = time(0);

   // Guarantee memory (sort of). internal const char* values can lose info so
   // We'll use our PersistentRichPresence object to hold onto it and reset the
   // values in this
   memset(&mRichPresence, 0, sizeof(mRichPresence));

   // Set function handlers for various discord actions
   DiscordEventHandlers handlers;
   memset(&handlers, 0, sizeof(handlers));

   handlers.ready = handleDiscordReady;
   handlers.disconnected = handleDiscordDisconnected;
   handlers.errored = handleDiscordError;
   handlers.joinGame = handleDiscordJoin;
   handlers.spectateGame = handleDiscordSpectate;
   handlers.joinRequest = handleDiscordJoinRequest;

   Discord_Initialize(discordClientId.c_str(), &handlers, 1, NULL);


   // Now set our persistent presence to be in game!
   mPersistentPresence.state = "";
   mPersistentPresence.details = "";

   mPersistentPresence.startTimestamp = mStartTime;
//   mDiscordPresence.endTimestamp = time(0) + 5 * 60;
   mPersistentPresence.largeImageKey = "ship_blue";
   mPersistentPresence.largeImageText = "BITFIGHTER";
//   mDiscordPresence.smallImageKey = "ship_blue";
//   mDiscordPresence.smallImageText = "ptb-small";

   // There are other members you can set, see DiscordRichPresence struct in
   // discord_rpc.h

   // Always call this if changing anything about the rich-presence
   updatePresence();
}

void DiscordIntegrator::shutdown()
{
   Discord_Shutdown();
}

void DiscordIntegrator::runCallbacks()
{
   updateBitfighterState();

   // Actually run the callbacks
   Discord_RunCallbacks();
}


void DiscordIntegrator::updateBitfighterState()
{
   const Vector<ClientGame *> *clientGames = GameManager::getClientGames();

   // No client game?
   if(clientGames->size() == 0)
      return;

   ClientGame *clientGame = clientGames->get(0);
   UIManager *uiManager = clientGame->getUIManager();

   // Let's update the rich presence based on what UI we're in
   static UserInterface *currentImportantUI = NULL;
   static UserInterface *newImportantUI = NULL;

   string state = mPersistentPresence.state;
   string details = mPersistentPresence.details;
   string largeImage = mPersistentPresence.largeImageKey;
   if(uiManager->isCurrentUI<EditorUserInterface>() || uiManager->cameFrom<EditorUserInterface>())
   {
      newImportantUI = uiManager->getUI<EditorUserInterface>();

      state = "In editor";
      details = "";
      largeImage = "ship_green";
   }
   else if(uiManager->isCurrentUI<GameUserInterface>() || uiManager->cameFrom<GameUserInterface>())
   {
      newImportantUI = uiManager->getUI<GameUserInterface>();

      // state/details set when changing levels
//      state = "In battle";
      largeImage = "ship_red";
   }
   else
   {
      newImportantUI = uiManager->getUI<MainMenuUserInterface>();

      state = "In menus";
      details = "";
      largeImage = "ship_blue";
   }

   // Update state only on UI change
   if(newImportantUI != currentImportantUI)
   {
      currentImportantUI = newImportantUI;

      mPersistentPresence.state = state;
      mPersistentPresence.details = details;
      mPersistentPresence.largeImageKey = largeImage;

      // Reset time
      mPersistentPresence.startTimestamp = time(0);

      updatePresence();
   }
}


// Always call this function instead of Discord_UpdatePresence() directly
// This will re-fill out a DiscordRichPresence object from our persistent
// object so memory errors will not occur.
void DiscordIntegrator::updatePresence()
{
   // Refresh the DiscordRichPresence with memory-safe values
   mRichPresence.state            = mPersistentPresence.state.c_str();
   mRichPresence.details          = mPersistentPresence.details.c_str();
   mRichPresence.startTimestamp   = mPersistentPresence.startTimestamp;
   mRichPresence.endTimestamp     = mPersistentPresence.endTimestamp;
   mRichPresence.largeImageKey    = mPersistentPresence.largeImageKey.c_str();
   mRichPresence.largeImageText   = mPersistentPresence.largeImageText.c_str();
   mRichPresence.smallImageKey    = mPersistentPresence.smallImageKey.c_str();
   mRichPresence.smallImageText   = mPersistentPresence.smallImageText.c_str();
   mRichPresence.partyId          = mPersistentPresence.partyId.c_str();
   mRichPresence.partySize        = mPersistentPresence.partySize;
   mRichPresence.partyMax         = mPersistentPresence.partyMax;
   mRichPresence.matchSecret      = mPersistentPresence.matchSecret.c_str();
   mRichPresence.joinSecret       = mPersistentPresence.joinSecret.c_str();
   mRichPresence.spectateSecret   = mPersistentPresence.spectateSecret.c_str();
   mRichPresence.instance         = mPersistentPresence.instance;

   // Update the presence
   Discord_UpdatePresence(&mRichPresence);
}


void DiscordIntegrator::updateState(const string &state)
{
   mPersistentPresence.state = state;
   updatePresence();
}


void DiscordIntegrator::updateDetails(const string &details)
{
   mPersistentPresence.details = details;
   updatePresence();
}

string DiscordIntegrator::getNickname()
{
    // Not supported.
    return "";
}


// Discord Callbacks
void DiscordIntegrator::handleDiscordReady(const DiscordUser *connectedUser)
{
   logprintf("Discord: connected to user %s#%s - %s", connectedUser->username,
         connectedUser->discriminator, connectedUser->userId);
}

void DiscordIntegrator::handleDiscordDisconnected(int errcode, const char *message)
{
//   logprintf("Discord: disconnected (%d: %s)", errcode, message);
}

void DiscordIntegrator::handleDiscordError(int errcode, const char *message)
{
//   logprintf("Discord: error (%d: %s)", errcode, message);
}

void DiscordIntegrator::handleDiscordJoin(const char *secret)
{
//   logprintf("Discord: join (%s)", secret);
}

void DiscordIntegrator::handleDiscordSpectate(const char *secret)
{
//   logprintf("Discord: spectate (%s)", secret);
}

void DiscordIntegrator::handleDiscordJoinRequest(const DiscordUser *request)
{
//   logprintf("Discord: join request from %s#%s - %s", request->username,
//         request->discriminator, request->userId);
}

#endif  // BF_DISCORD
