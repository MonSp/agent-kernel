#pragma once
// EventStreamServer — Unix socket server that pushes kernel events to subscribers.
// Thread-safe: tracks all client threads, joins on stop.

#include "EventJournal.h"
#include "AgentMailbox.h"
#include <string>
#include <vector>
#include <mutex>
#include <memory>
#include <thread>
#include <atomic>
#include <algorithm>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>
#include <cstdio>

namespace Systems {

class EventStreamServer {
public:
    explicit EventStreamServer(const std::string& socketPath,
                                EventJournal& journal,
                                AgentMailbox& mailbox)
        : socketPath_(socketPath), serverFd_(-1), running_(false) {

        journal.addListener([this](const JournalEvent& e) {
            pushEvent(journalEventToJson(e));
        });

        mailbox.addListener([this](const MailboxMessage& m) {
            pushEvent(mailboxMessageToJson(m));
        });
    }

    ~EventStreamServer() { stop(); }

    bool start() {
        serverFd_ = socket(AF_UNIX, SOCK_STREAM, 0);
        if (serverFd_ < 0) return false;

        unlink(socketPath_.c_str());

        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, socketPath_.c_str(), sizeof(addr.sun_path) - 1);

        if (bind(serverFd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            close(serverFd_);
            serverFd_ = -1;
            return false;
        }

        if (listen(serverFd_, 8) < 0) {
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

        // Close server socket to unblock accept()
        if (serverFd_ >= 0) {
            ::shutdown(serverFd_, SHUT_RDWR);
            close(serverFd_);
            serverFd_ = -1;
        }

        // Join accept thread
        if (acceptThread_ && acceptThread_->joinable()) {
            acceptThread_->join();
        }

        // Close all subscriber sockets (unblocks recv in client threads)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (int fd : subscribers_) {
                ::shutdown(fd, SHUT_RDWR);
                close(fd);
            }
            subscribers_.clear();
        }

        // Join all client threads (C3 fix: no more detached threads)
        {
            std::lock_guard<std::mutex> lock(threadsMutex_);
            for (auto& t : clientThreads_) {
                if (t.joinable()) {
                    t.join();
                }
            }
            clientThreads_.clear();
        }

        unlink(socketPath_.c_str());
    }

    int subscriberCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return static_cast<int>(subscribers_.size());
    }

private:
    void acceptLoop() {
        while (running_) {
            int clientFd = accept(serverFd_, nullptr, nullptr);
            if (clientFd < 0) {
                if (!running_) break;
                continue;
            }
            {
                std::lock_guard<std::mutex> lock(mutex_);
                subscribers_.push_back(clientFd);
            }
            // Track client thread instead of detaching (C3 fix)
            {
                std::lock_guard<std::mutex> lock(threadsMutex_);
                // Clean up finished threads before adding new ones
                clientThreads_.erase(
                    std::remove_if(clientThreads_.begin(), clientThreads_.end(),
                        [](const std::thread& t) { return !t.joinable(); }),
                    clientThreads_.end());
                clientThreads_.emplace_back([this, clientFd]() { handleClient(clientFd); });
            }
        }
    }

    void handleClient(int clientFd) {
        char buf[64];
        while (running_) {
            ssize_t n = recv(clientFd, buf, sizeof(buf), 0);
            if (n <= 0) break;
        }
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = std::find(subscribers_.begin(), subscribers_.end(), clientFd);
            if (it != subscribers_.end()) {
                subscribers_.erase(it);
            }
        }
        close(clientFd);
    }

    void pushEvent(const std::string& jsonLine) {
        std::string data = jsonLine + "\n";
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<int> dead;
        for (int fd : subscribers_) {
            ssize_t n = send(fd, data.c_str(), data.size(), MSG_NOSIGNAL);
            if (n <= 0) {
                dead.push_back(fd);
            }
        }
        for (int fd : dead) {
            auto it = std::find(subscribers_.begin(), subscribers_.end(), fd);
            if (it != subscribers_.end()) {
                subscribers_.erase(it);
            }
            close(fd);
        }
    }

    static std::string journalEventToJson(const JournalEvent& e) {
        std::string json = "{\"type\":\"journal_event\"";
        json += ",\"id\":" + std::to_string(e.id);
        json += ",\"timestamp\":" + std::to_string(e.timestamp);
        json += ",\"entityId\":" + std::to_string(e.entity_id);
        json += ",\"eventType\":\"" + escapeStr(e.event_type) + "\"";
        json += ",\"payload\":" + embedPayload(e.payload) + "}";
        return json;
    }

    static std::string mailboxMessageToJson(const MailboxMessage& m) {
        std::string json = "{\"type\":\"message_received\"";
        json += ",\"id\":" + std::to_string(m.id);
        json += ",\"from\":" + std::to_string(m.from);
        json += ",\"to\":" + std::to_string(m.to);
        json += ",\"payload\":" + embedPayload(m.payload);
        json += ",\"timestamp\":" + std::to_string(m.timestamp) + "}";
        return json;
    }

    static std::string embedPayload(const std::string& payload) {
        if (payload.empty()) return "\"\"";
        if (payload[0] == '{' || payload[0] == '[') {
            std::string cleaned;
            bool escaped = false;
            for (char c : payload) {
                if (c == '\\' && !escaped) {
                    escaped = true;
                    continue;
                }
                if (escaped) {
                    escaped = false;
                    if (c == '"') { cleaned += '"'; continue; }
                    if (c == '\\') { cleaned += '\\'; continue; }
                    if (c == 'n') { cleaned += '\n'; continue; }
                    if (c == 'r') { cleaned += '\r'; continue; }
                    if (c == 't') { cleaned += '\t'; continue; }
                    cleaned += '\\';
                    cleaned += c;
                    continue;
                }
                cleaned += c;
            }
            return cleaned;
        }
        return "\"" + escapeStr(payload) + "\"";
    }

    static std::string escapeStr(const std::string& s) {
        std::string out;
        for (char c : s) {
            switch (c) {
                case '"':  out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n";  break;
                case '\r': out += "\\r";  break;
                case '\t': out += "\\t";  break;  // I6 fix
                default:   out += c;      break;
            }
        }
        return out;
    }

    std::string socketPath_;
    int serverFd_;
    std::atomic<bool> running_;
    std::unique_ptr<std::thread> acceptThread_;
    std::vector<int> subscribers_;
    mutable std::mutex mutex_;
    std::vector<std::thread> clientThreads_;  // C3 fix: track instead of detach
    std::mutex threadsMutex_;
};

} // namespace Systems
