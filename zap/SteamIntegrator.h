#pragma once

#ifdef BF_STEAM

#include <iostream>
#include <string>
#include "steam.grpc.pb.h"
#include <grpcpp/grpcpp.h>

#include "AppIntegrator.h"

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

namespace Zap {
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
                    std::cout << "Steamworks initialization failed within the launcher." << std::endl;
                }
            } else {
                std::cout << "Steamworks initialization failed with error code " << status.error_code() << ": " << status.error_message() << std::endl;
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

    class SteamIntegrator : public AppIntegrator {
        private:
            SteamClient sc;
            SteamState state;

        public:
            SteamIntegrator();
            virtual ~SteamIntegrator();

            void init();
            void shutdown();
            void runCallbacks();

            void updateState(const std::string &state);
            void updateDetails(const std::string &details);
            std::string getNickname();

    };
} // namespace Zap
#endif