#include "zmq_rpc.hpp"
#include <iostream>
#include <thread>
#include <chrono>

namespace zmq_rpc {

// ----------------- CLIENT IMPLEMENTATION -----------------
Client::Client(const std::string& server_addr)
    : context_(1), dealer_(context_, zmq::socket_type::dealer), running_(true)
{
    identity_ = "client-" + std::to_string(rand() % 10000);
    dealer_.set(zmq::sockopt::routing_id, identity_);
    dealer_.connect(server_addr);
    worker_ = std::thread([this]{ poll_loop(); });
}

Client::~Client() {
    running_ = false;
    if (worker_.joinable()) worker_.join();
}

void Client::set_request_handler(std::function<std::string(const std::string&)> handler) {
    handler_ = handler;
}

void Client::poll_loop() {
    while (running_) {
        zmq::message_t empty, req_id, payload;
        dealer_.recv(empty);
        dealer_.recv(req_id);
        dealer_.recv(payload);

        std::string req_id_str(static_cast<char*>(req_id.data()), req_id.size());
        std::string payload_str(static_cast<char*>(payload.data()), payload.size());

        std::string result;
        if (handler_) {
            result = handler_(payload_str);
        } else {
            result = "Processed(" + payload_str + ")";
        }

        dealer_.send(req_id, zmq::send_flags::sndmore);
        dealer_.send(zmq::buffer(result), zmq::send_flags::none);

        std::cout << "[" << identity_ << "] Responded id=" << req_id_str
                  << " result=" << result << std::endl;
    }
}

} // namespace zmq_rpc
