#include "SteamIntegrator.h"

#ifdef BF_STEAM

////////////////////////////////////////
////////////////////////////////////////
// Steam
namespace Zap {
    SteamIntegrator::SteamIntegrator()
    : sc(grpc::CreateChannel(
        "localhost:50051", grpc::InsecureChannelCredentials()))
    {
    std::cout << "Steam: Constructed." << std::endl;
    }

    SteamIntegrator::~SteamIntegrator()
    {
    std::cout << "Steam: Destructed." << std::endl;
    }

    void SteamIntegrator::init()
    {
    sc.InitSteamworks(state);
    }

    void SteamIntegrator::shutdown()
    {
    std::cout << "Steam: Shut down." << std::endl;
    }

    void SteamIntegrator::runCallbacks()
    {
    // Do nothing.
    }

    void SteamIntegrator::updateState(const string &state)
    {
    sc.UpdateGameState(state);
    std::cout << "Steam: State updated- " << state << std::endl;
    }

    void SteamIntegrator::updateDetails(const string &details)
    {
    sc.UpdateGameDetails(details);
    std::cout << "Steam: Details updated- " << details << std::endl;
    }

    std::string SteamIntegrator::getNickname(){
        std::cout << "Steam: Game requested nickname." << std::endl;
        if (state.is_init){
            std::cout << "Steam: Provided user " << state.user_name << "." <<std::endl;
            return state.user_name;
        }
        std::cerr << "Steam: Initialization not completed, can't provide a username." << std::endl;
        return "";
    }
}
#endif // BF_STEAM
