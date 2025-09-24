#include "zynq_rpc.hpp"
#include <deque>

namespace zynq_rpc {

class ServerImpl {
public:
    std::deque<std::string> clients; // round-robin queue
    size_t rr_index = 0;
};

Server::Server(const std::string& bind_addr, int timeout_sec)
        : context_(1),
          router_(context_, zmq::socket_type::router),
          timeout_(std::chrono::seconds(timeout_sec)),
          running_(true)
        //   , rr_index_(0)
    {
        router_.bind(bind_addr);
        worker_ = std::thread([this]{ poll_loop(); });
    }

Server::~Server() {
    running_ = false;
    if (worker_.joinable()) worker_.join();
}

std::string Server::pick_client() {
    if (client_list_.empty()) {
        throw std::runtime_error("No clients connected");
    }
    std::string client_id = client_list_[rr_index_ % client_list_.size()];
    rr_index_++;
    return client_id;
}

// std::future<std::string> Server::send_request(const std::string& request_id,
//                                           const std::string& payload) {
//         std::lock_guard<std::mutex> lock(mutex_);
//         if (clients_.empty()) {
//             throw std::runtime_error("No clients connected");
//         }

//         // round-robin client choice
//         std::string client_id = clients_[rr_index_];
//         rr_index_ = (rr_index_ + 1) % clients_.size();

//         // ROUTER frames: [identity][empty][req_id][payload]
//         router_.send(zmq::buffer(client_id), zmq::send_flags::sndmore);
//         router_.send(zmq::message_t(), zmq::send_flags::sndmore);
//         router_.send(zmq::buffer(request_id), zmq::send_flags::sndmore);
//         router_.send(zmq::buffer(payload), zmq::send_flags::none);

//         std::promise<std::string> prom;
//         auto fut = prom.get_future();
//         pending_[request_id] = {payload, std::chrono::steady_clock::now(), std::move(prom)};
//         return fut;
//     }
std::future<std::string> Server::send_request(const std::string& request_id,
                                              const std::string& payload) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string client_id = pick_client();

    router_.send(zmq::buffer(client_id), zmq::send_flags::sndmore);
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
            zmq::message_t identity, empty, first, second;
            router_.recv(identity);
            router_.recv(empty);
            router_.recv(first);
            router_.recv(second);

            std::string client_id(static_cast<char*>(identity.data()), identity.size());
            std::string frame1(static_cast<char*>(first.data()), first.size());
            std::string frame2(static_cast<char*>(second.data()), second.size());

            std::lock_guard<std::mutex> lock(mutex_);

            // if (frame1 == std::string("HELLO")) {
            if (strncmp(frame1.c_str(), "HELLO", 5) == 0) {
                if (clients_.insert(client_id).second) {
                    client_list_.push_back(client_id);
                    std::cout << "[Server] 🎉 New client registered: " << client_id << std::endl;
                }
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
    }
}

// void Server::poll_loop() {
//         while (running_) {
//             zmq::pollitem_t items[] = {{static_cast<void*>(router_), 0, ZMQ_POLLIN, 0}};
//             zmq::poll(items, 1, 100);

//             if (items[0].revents & ZMQ_POLLIN) {
//                 zmq::message_t identity, empty, req_id, data;
//                 router_.recv(identity);
//                 router_.recv(empty);
//                 router_.recv(req_id);
//                 router_.recv(data);

//                 std::string client_id(static_cast<char*>(identity.data()), identity.size());
//                 std::string resp_id(static_cast<char*>(req_id.data()), req_id.size());
//                 std::string resp_val(static_cast<char*>(data.data()), data.size());

//                 {
//                     std::lock_guard<std::mutex> lock(mutex_);
//                     // register new client
//                     if (std::find(clients_.begin(), clients_.end(), client_id) == clients_.end()) {
//                         clients_.push_back(client_id);
//                         std::cout << "[Server] Registered new client: " << client_id << std::endl;
//                     }

//                     auto it = pending_.find(resp_id);
//                     if (it != pending_.end()) {
//                         it->second.promise.set_value(resp_val);
//                         pending_.erase(it);
//                         std::cout << "[Server] Response from " << client_id
//                                   << " for " << resp_id << ": " << resp_val << std::endl;
//                     }
//                 }
//             }

//             check_timeouts();
//         }
//     }

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

// void Server::check_timeouts() {
//         auto now = std::chrono::steady_clock::now();
//         std::lock_guard<std::mutex> lock(mutex_);
//         for (auto it = pending_.begin(); it != pending_.end();) {
//             if (now - it->second.timestamp > timeout_) {
//                 it->second.promise.set_exception(
//                     std::make_exception_ptr(TimeoutException("Request " + it->first + " timed out"))
//                 );
//                 std::cout << "[Server] ❌ Request " << it->first
//                           << " timed out (payload=" << it->second.payload << ")\n";
//                 it = pending_.erase(it);
//             } else {
//                 ++it;
//             }
//         }
//     }

} // namespace zync_rpc
