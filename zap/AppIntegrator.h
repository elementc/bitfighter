//------------------------------------------------------------------------------
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#ifndef ZAP_APPINTEGRATOR_H_
#define ZAP_APPINTEGRATOR_H_

#include "tnlTypes.h"
#include "tnlVector.h"

#include <string>


// Because some integrations might pull in libraries that have class naming conflicts on Windows,
// we can't get away with using namespace TNL; here.
// For example, Visual Studio will complain about TNL::ByteBuffer colliding with grpc::ByteBuffer.
// Linux and MacOS builds work fine, actually, so, seems to be a Windows quirk.
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

      // This should return an empty string if the integration does not provide a user's nickname.
      virtual string getNickname() = 0;
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

      static string getNickname();
   };

} /* namespace Zap */

#endif /* ZAP_APPINTEGRATOR_H_ */
