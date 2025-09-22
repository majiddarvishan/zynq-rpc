#include "zmq_rpc.hpp"
#include <thread>
#include <chrono>

int main() {
    zmq_rpc::Client client("tcp://localhost:5555");

    client.set_request_handler([](const std::string& payload) -> std::string {
        std::this_thread::sleep_for(std::chrono::milliseconds(500)); // simulate work
        return "Handled(" + payload + ")";
    });

    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}
