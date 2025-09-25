#include "zynq_rpc.hpp"
#include <iostream>
#include <thread>
#include <chrono>

namespace zynq_rpc {

Client::Client(const std::string& server_addr)
    : context_(1), dealer_(context_, zmq::socket_type::dealer), running_(true)
{
    identity_ = "client-" + std::to_string(rand() % 10000);
    dealer_.set(zmq::sockopt::routing_id, identity_);
    dealer_.connect(server_addr);

    // Register with HELLO
    dealer_.send(zmq::message_t(), zmq::send_flags::sndmore);
    dealer_.send(zmq::buffer("HELLO"), zmq::send_flags::sndmore);
    dealer_.send(zmq::buffer(identity_), zmq::send_flags::none);

    // Start threads
    worker_ = std::thread([this]{ poll_loop(); });
    heartbeat_thread_ = std::thread([this]{ heartbeat_loop(); });
}

Client::~Client() {
    unbind();  // ensure we send BYE on destruction

    running_ = false;
    if (worker_.joinable()) worker_.join();
    if (heartbeat_thread_.joinable()) heartbeat_thread_.join();
}

void Client::unbind() {
    if (running_) {
        std::cout << "[" << identity_ << "] 👋 Sending BYE" << std::endl;
        dealer_.send(zmq::message_t(), zmq::send_flags::sndmore);
        dealer_.send(zmq::buffer("BYE"), zmq::send_flags::sndmore);
        dealer_.send(zmq::buffer(identity_), zmq::send_flags::none);
    }
}

void Client::set_request_handler(std::function<std::string(const std::string&)> handler) {
    handler_ = handler;
}

void Client::poll_loop() {
    while (running_) {
        zmq::message_t empty, req_id, payload;
        if (!dealer_.recv(empty, zmq::recv_flags::dontwait)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

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

        dealer_.send(zmq::buffer(req_id_str), zmq::send_flags::sndmore);
        dealer_.send(zmq::buffer(result), zmq::send_flags::none);

        std::cout << "[" << identity_ << "] Responded id=" << req_id_str
                  << " result=" << result << std::endl;
    }
}

void Client::heartbeat_loop() {
    while (running_) {
        std::this_thread::sleep_for(std::chrono::seconds(3));

        dealer_.send(zmq::message_t(), zmq::send_flags::sndmore);
        dealer_.send(zmq::buffer("PING"), zmq::send_flags::sndmore);
        dealer_.send(zmq::buffer(identity_), zmq::send_flags::none);

        std::cout << "[" << identity_ << "] ❤️ Sent heartbeat" << std::endl;
    }
}

} // namespace zynq_rpc
