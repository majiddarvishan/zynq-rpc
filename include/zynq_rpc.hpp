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
