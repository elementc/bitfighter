//------------------------------------------------------------------------------
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#include "AppIntegrator.h"

#include "Timer.h"
#include "Intervals.h"

#include "tnlLog.h"

#include <cstdio>
#include <cstring>

#include "DiscordIntegrator.h"
#include "SteamIntegrator.h"


namespace Zap
{

using namespace std;


////////////////////////////////////////
////////////////////////////////////////

AppIntegrator::AppIntegrator()
{
   // Do nothing
}


AppIntegrator::~AppIntegrator()
{
   // Do nothing
}


////////////////////////////////////////
////////////////////////////////////////

Vector<AppIntegrator*> AppIntegrationController::mAppIntegrators = Vector<AppIntegrator*>();
Timer AppIntegrationController::mCallbackTimer = Timer();

AppIntegrationController::AppIntegrationController()
{
   // Do nothing
}


AppIntegrationController::~AppIntegrationController()
{
   // Do nothing
}


void AppIntegrationController::init()
{
   // Instantiate and add any integrators here.
   // Order matters: if two integrators provide the same service,
   // some hooks like getNickname will return the first integration that is registered.

   #ifdef BF_STEAM
      mAppIntegrators.push_back(new SteamIntegrator());
   #endif
   #ifdef BF_DISCORD
      mAppIntegrators.push_back(new DiscordIntegrator());
   #endif

   // Run init() on all the integrators
   for(int i = 0; i < mAppIntegrators.size(); i++)
      mAppIntegrators[i]->init();

   // Start callback timer
   mCallbackTimer.reset(TWO_SECONDS);
}


void AppIntegrationController::idle(U32 deltaTime)
{
   if(mCallbackTimer.update(deltaTime))
   {
      // Check for 3rd party app callbacks
      runCallbacks();

      // Restart timer
      mCallbackTimer.reset();
   }
}


void AppIntegrationController::shutdown()
{
   // Run shutdown() on all the integrators
   for(int i = 0; i < mAppIntegrators.size(); i++)
      mAppIntegrators[i]->shutdown();

   // Clean up
   for(int i = 0; i < mAppIntegrators.size(); i++)
   {
      AppIntegrator *app = mAppIntegrators[i];

      if(app != NULL)
         delete app;
   }
}


void AppIntegrationController::runCallbacks()
{
   for(int i = 0; i < mAppIntegrators.size(); i++)
      mAppIntegrators[i]->runCallbacks();
}


void AppIntegrationController::updateState(const string &state, const string &details)
{

   for(int i = 0; i < mAppIntegrators.size(); i++)
   {
      if(state != "")
         mAppIntegrators[i]->updateState(state);

      if(details != "")
         mAppIntegrators[i]->updateDetails(details);
   }
}

string AppIntegrationController::getNickname(){
   // Return the first nickname that an integration provides.
   for (int i = 0; i < mAppIntegrators.size(); i++){
      string integrationNickname = mAppIntegrators[i]->getNickname();
      if (integrationNickname != ""){
         return integrationNickname;
      }
   }

   // No integrations provide nicknames, return an empty string.
   return "";
}

} /* namespace Zap */
