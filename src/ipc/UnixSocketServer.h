#pragma once

#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <memory>
#include <vector>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <cstdio>

namespace IPC {

// Minimal Unix domain socket server using SOCK_STREAM.
// Reads newline-delimited JSON from each client.
// Passes each complete line to the request handler callback and writes
// the returned string back (caller is responsible for appending '\n').

class UnixSocketServer {
public:
    using RequestHandler = std::function<std::string(const std::string& rawRequest)>;

    explicit UnixSocketServer(const std::string& socketPath)
        : socketPath_(socketPath), serverFd_(-1), running_(false) {}

    ~UnixSocketServer() { stop(); }

    void setRequestHandler(RequestHandler handler) {
        handler_ = std::move(handler);
    }

    bool start() {
        serverFd_ = socket(AF_UNIX, SOCK_STREAM, 0);
        if (serverFd_ < 0) {
            perror("socket");
            return false;
        }

        unlink(socketPath_.c_str());

        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, socketPath_.c_str(), sizeof(addr.sun_path) - 1);

        if (bind(serverFd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            perror("bind");
            close(serverFd_);
            serverFd_ = -1;
            return false;
        }

        if (listen(serverFd_, 8) < 0) {
            perror("listen");
            close(serverFd_);
            unlink(socketPath_.c_str());
            serverFd_ = -1;
            return false;
        }

        running_ = true;
        acceptThread_ = std::make_unique<std::thread>([this]() { acceptLoop(); });
        return true;
    }

    void stop() {
        running_ = false;
        if (serverFd_ >= 0) {
            ::shutdown(serverFd_, SHUT_RDWR);
            close(serverFd_);
            serverFd_ = -1;
        }
        if (acceptThread_ && acceptThread_->joinable()) {
            acceptThread_->join();
        }
        unlink(socketPath_.c_str());
    }

    bool isRunning() const { return running_.load(); }

private:
    void acceptLoop() {
        while (running_) {
            int clientFd = accept(serverFd_, nullptr, nullptr);
            if (clientFd < 0) {
                if (!running_) break;
                continue;
            }
            // Detached thread per client
            std::thread([this, clientFd]() { handleClient(clientFd); }).detach();
        }
    }

    void handleClient(int clientFd) {
        std::string buffer;
        char chunk[4096];

        while (running_) {
            ssize_t n = recv(clientFd, chunk, sizeof(chunk), 0);
            if (n <= 0) break; // client disconnected or error

            buffer.append(chunk, static_cast<size_t>(n));

            // Process newline-delimited messages
            size_t pos;
            while ((pos = buffer.find('\n')) != std::string::npos) {
                std::string line = buffer.substr(0, pos);
                buffer.erase(0, pos + 1);

                // Trim trailing \r if present
                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }

                if (line.empty()) continue;

                std::string response;
                if (handler_) {
                    response = handler_(line);
                } else {
                    response = "{\"ok\":false,\"error\":\"no handler\"}";
                }
                response += '\n';

                ssize_t sent = 0;
                size_t total = response.size();
                while (sent < static_cast<ssize_t>(total)) {
                    ssize_t w = send(clientFd, response.data() + sent, total - static_cast<size_t>(sent), MSG_NOSIGNAL);
                    if (w <= 0) goto done;
                    sent += w;
                }
            }
        }
    done:
        close(clientFd);
    }

    std::string socketPath_;
    int serverFd_;
    std::atomic<bool> running_;
    std::unique_ptr<std::thread> acceptThread_;
    RequestHandler handler_;
};

} // namespace IPC
