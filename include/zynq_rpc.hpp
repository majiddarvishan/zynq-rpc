#pragma once
#include <zmq.hpp>
#include <string>
#include <unordered_map>
#include <chrono>
#include <iostream>
#include <thread>
#include <mutex>
#include <future>
#include <functional>
#include <stdexcept>
#include <deque>
#include <memory>
#include <algorithm>

namespace zynq_rpc {

enum class ControlType : uint8_t {
    HELLO = 1,
    PING  = 2,
    BYE   = 3
};

struct ControlPacket {
    ControlType type;
    std::string identity;

    // Serialize to zmq frames
    std::vector<zmq::message_t> to_frames() const {
        std::vector<zmq::message_t> frames;
        frames.emplace_back(&type, sizeof(type));
        frames.emplace_back(identity.data(), identity.size());
        return frames;
    }

    // Parse from zmq frames
    static ControlPacket from_frames(const zmq::message_t& f1, const zmq::message_t& f2) {
        if (f1.size() != sizeof(ControlType))
            throw std::runtime_error("Invalid control frame size");
        ControlType t;
        std::memcpy(&t, f1.data(), sizeof(t));
        std::string id(static_cast<const char*>(f2.data()), f2.size());
        return {t, id};
    }
};

struct TimeoutException : public std::runtime_error {
    TimeoutException(const std::string& msg) : std::runtime_error(msg) {}
};

// ==================================================
// SERVER
// ==================================================
class Server {
public:
    Server(const std::string& bind_addr, int timeout_sec = 3);
    ~Server();

    std::future<std::string> send_request(const std::string& request_id,
                                          const std::string& payload);

        size_t active_client_count() {
        std::lock_guard<std::mutex> lock(mutex_);
        return clients_.size();
    }

private:
    struct RequestInfo {
        std::string payload;
        std::chrono::steady_clock::time_point timestamp;
        std::promise<std::string> promise;
    };

    void poll_loop();
    void check_timeouts();
    void cleanup_clients();
    std::string pick_client();
    void monitor_loop();


    zmq::context_t context_;
    zmq::socket_t router_;

    zmq::socket_t monitor_socket_;
    std::thread monitor_thread_;

    std::unordered_map<std::string, RequestInfo> pending_;
    std::deque<std::string> clients_;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> last_seen_;
    std::chrono::seconds timeout_;
    bool running_;
    std::thread worker_;
    std::mutex mutex_;
    size_t rr_index_;
};

// ==================================================
// CLIENT
// ==================================================
class Client {
public:
    Client(const std::string& server_addr);
    ~Client();

    void set_request_handler(std::function<std::string(const std::string&)> handler);

    // Graceful disconnect
    void unbind();

private:
    void poll_loop();
    void heartbeat_loop();

    zmq::context_t context_;
    zmq::socket_t dealer_;
    std::string identity_;
    bool running_;
    std::thread worker_;
    std::thread heartbeat_thread_;
    std::function<std::string(const std::string&)> handler_;
};

} // namespace zynq_rpc
