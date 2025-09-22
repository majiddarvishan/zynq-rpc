#include "zmq_rpc.hpp"

namespace zmq_rpc {

// ----------------- SERVER IMPLEMENTATION -----------------
Server::Server(const std::string& bind_addr, int timeout_sec)
    : context_(1), router_(context_, zmq::socket_type::router),
      timeout_(std::chrono::seconds(timeout_sec)), running_(true)
{
    router_.bind(bind_addr);
    worker_ = std::thread([this]{ poll_loop(); });
}

Server::~Server() {
    running_ = false;
    if (worker_.joinable()) worker_.join();
}

std::future<std::string> Server::send_request(const std::string& request_id, const std::string& payload) {
    std::lock_guard<std::mutex> lock(mutex_);
    router_.send(zmq::message_t(), zmq::send_flags::sndmore); // empty frame
    router_.send(zmq::buffer(request_id), zmq::send_flags::sndmore);
    router_.send(zmq::buffer(payload), zmq::send_flags::none);

    std::promise<std::string> prom;
    auto fut = prom.get_future();
    pending_[request_id] = {payload, std::chrono::steady_clock::now(), std::move(prom)};
    return fut;
}

void Server::poll_loop() {
    while (running_) {
        zmq::pollitem_t items[] = {{static_cast<void*>(router_), 0, ZMQ_POLLIN, 0}};
        zmq::poll(items, 1, 100);

        if (items[0].revents & ZMQ_POLLIN) {
            zmq::message_t identity, empty, req_id, data;
            router_.recv(identity);
            router_.recv(empty);
            router_.recv(req_id);
            router_.recv(data);

            std::string client_id(static_cast<char*>(identity.data()), identity.size());
            std::string resp_id(static_cast<char*>(req_id.data()), req_id.size());
            std::string resp_val(static_cast<char*>(data.data()), data.size());

            std::lock_guard<std::mutex> lock(mutex_);
            auto it = pending_.find(resp_id);
            if (it != pending_.end()) {
                it->second.promise.set_value(resp_val);
                pending_.erase(it);
                std::cout << "[Server] Response from " << client_id
                          << " for " << resp_id << ": " << resp_val << std::endl;
            }
        }

        check_timeouts();
    }
}

void Server::check_timeouts() {
    auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto it = pending_.begin(); it != pending_.end();) {
        if (now - it->second.timestamp > timeout_) {
            it->second.promise.set_exception(
                std::make_exception_ptr(TimeoutException("Request " + it->first + " timed out"))
            );
            std::cout << "[Server] ❌ Request " << it->first
                      << " timed out (payload=" << it->second.payload << ")\n";
            it = pending_.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace zmq_rpc
