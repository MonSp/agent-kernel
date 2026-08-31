#include "ipc/AgentKernelBridge.h"
#include <csignal>
#include <cstdio>
#include <cstring>
#include <string>

static IPC::AgentKernelBridge* g_bridge = nullptr;

static void signalHandler(int /*sig*/) {
    if (g_bridge) {
        printf("\nShutting down agent-kernel-daemon...\n");
        g_bridge->stop();
    }
}

int main(int argc, char* argv[]) {
    std::string socketPath = "/tmp/agent-kernel.sock";

    for (int i = 1; i < argc; ++i) {
        if ((strcmp(argv[i], "--socket") == 0 || strcmp(argv[i], "-s") == 0) && i + 1 < argc) {
            socketPath = argv[++i];
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("Usage: %s [--socket <path>]\n", argv[0]);
            printf("  --socket, -s  Unix socket path (default: /tmp/agent-kernel.sock)\n");
            return 0;
        }
    }

    IPC::AgentKernelBridge bridge(socketPath);
    g_bridge = &bridge;

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signalHandler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT,  &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);

    if (!bridge.start()) {
        fprintf(stderr, "Failed to start agent-kernel bridge on %s\n", socketPath.c_str());
        return 1;
    }

    printf("agent-kernel-daemon listening on %s\n", socketPath.c_str());

    // Block until stopped by signal
    while (bridge.isRunning()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    printf("agent-kernel-daemon stopped.\n");
    return 0;
}
