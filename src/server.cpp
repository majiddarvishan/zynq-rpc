#include "zynq_rpc.hpp"

namespace zynq_rpc {

Server::Server(const std::string& bind_addr, int timeout_sec)
    : context_(1), router_(context_, zmq::socket_type::router),
      timeout_(std::chrono::seconds(timeout_sec)), running_(true), rr_index_(0)
{
    router_.bind(bind_addr);
    worker_ = std::thread([this]{ poll_loop(); });
}

Server::~Server() {
    running_ = false;
    if (worker_.joinable()) worker_.join();
}

std::string Server::pick_client() {
    if (clients_.empty()) {
        throw std::runtime_error("No clients connected");
    }
    std::string client_id = clients_[rr_index_ % clients_.size()];
    rr_index_++;
    return client_id;
}

std::future<std::string> Server::send_request(const std::string& request_id,
                                              const std::string& payload) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string client_id = pick_client();

    router_.send(zmq::buffer(client_id), zmq::send_flags::sndmore);
    router_.send(zmq::message_t(), zmq::send_flags::sndmore);
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
            zmq::message_t identity, empty, first, second;
            router_.recv(identity);
            router_.recv(empty);
            router_.recv(first);
            router_.recv(second);

            std::string client_id(static_cast<char*>(identity.data()), identity.size());
            std::string frame1(static_cast<char*>(first.data()), first.size());
            std::string frame2(static_cast<char*>(second.data()), second.size());

            std::lock_guard<std::mutex> lock(mutex_);

            last_seen_[client_id] = std::chrono::steady_clock::now();

            // if (frame1 == "HELLO") {
            if (strncmp(frame1.c_str(), "HELLO", 5) == 0) {
                if (std::find(clients_.begin(), clients_.end(), client_id) == clients_.end()) {
                    clients_.push_back(client_id);
                    std::cout << "[Server] 🎉 New client registered: " << client_id << std::endl;
                }
            } else if (frame1 == "PING") {
                std::cout << "[Server] 🔄 Heartbeat from " << client_id << std::endl;
            } else {
                // normal response
                std::string resp_id = frame1;
                std::string resp_val = frame2;

                auto it = pending_.find(resp_id);
                if (it != pending_.end()) {
                    it->second.promise.set_value(resp_val);
                    pending_.erase(it);
                    std::cout << "[Server] ✅ Response from " << client_id
                              << " for " << resp_id << ": " << resp_val << std::endl;
                }
            }
        }

        check_timeouts();
        cleanup_clients();
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

void Server::cleanup_clients() {
    auto now = std::chrono::steady_clock::now();
    auto cutoff = std::chrono::seconds(10);

    for (auto it = clients_.begin(); it != clients_.end();) {
        const std::string& cid = *it;
        if (last_seen_.find(cid) != last_seen_.end() &&
            now - last_seen_[cid] > cutoff) {
            std::cout << "[Server] ⚠ Removing inactive client: " << cid << std::endl;
            last_seen_.erase(cid);
            it = clients_.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace zynq_rpc
