// main.cpp
#include "StardustBridge.hpp"
#include "OverteClient.hpp"
#include "SceneSync.Hpp"
#include "InputHandler.hpp"

#include <iostream>
#include <thread>
#include <chrono>

int main(int argc, char** argv) {
    // Simple CLI: --socket=/path/to.sock or --abstract=name
    std::string socketOverride;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        const std::string so = "--socket=";
        const std::string ab = "--abstract=";
        if (arg.rfind(so, 0) == 0) socketOverride = arg.substr(so.size());
        else if (arg.rfind(ab, 0) == 0) socketOverride = '@' + arg.substr(ab.size());
    }
    StardustBridge stardust;
    if (!stardust.connect(socketOverride)) {
        std::cerr << "Failed to connect to StardustXR compositor.\n";
        return 1;
    }

    // Overte localhost default assumption (can override via OVERTE_URL env or --overte=ws://host:port)
    std::string overteUrl = "ws://127.0.0.1:40102";
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        const std::string ov = "--overte=";
        if (arg.rfind(ov, 0) == 0) overteUrl = arg.substr(ov.size());
    }
    if (const char* envOv = std::getenv("OVERTE_URL")) {
        overteUrl = envOv;
    }
    OverteClient overte(overteUrl);
    // Overte is optional; warn if unreachable but continue in offline mode.
    if (!overte.connect()) {
        std::cerr << "[Overte] Domain unreachable; running in offline mode.\n";
    }

    InputHandler input(stardust, overte);

    // Main loop
    while (stardust.running()) {
    overte.poll();
        stardust.poll();

        // Sync avatars/entities
        SceneSync::update(stardust, overte);

        // Simple input mapping
        input.update(1.0f / 90.0f);

        // Small sleep to avoid busy-spin in the stub
        std::this_thread::sleep_for(std::chrono::milliseconds(11));
    }

    return 0;
}

