#include "zynq_rpc.hpp"

#include <thread>
#include <chrono>
#include <iostream>

int main() {
    zynq_rpc::Client client("tcp://localhost:5555", std::chrono::seconds(10));

    client.set_request_handler([](const std::string& payload) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        return "Handled(" + payload + ")";
    });

    std::this_thread::sleep_for(std::chrono::seconds(10));

    // Graceful disconnect
    client.unbind();
    std::cout << "Client exiting..." << std::endl;

    return 0;
}
