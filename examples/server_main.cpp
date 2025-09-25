#include "zynq_rpc.hpp"
#include <thread>
#include <chrono>
#include <iostream>
#include <vector>
#include <atomic>

std::vector<std::string> COLORS = {
    "\033[31m","\033[32m","\033[33m","\033[34m","\033[35m","\033[36m","\033[37m"
};
const std::string RESET = "\033[0m";

int main() {
    zynq_rpc::Server server("tcp://*:5555", 5);
    int counter = 0;
    std::atomic<bool> running{true};

    // Monitoring thread
    std::thread monitor([&]() {
        while (running) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            // Just print how many active clients exist
            std::cout << "[Server] 👥 Active clients: "
                      << server.active_client_count() << std::endl;
        }
    });

    while (true) {
        std::string req_id = "req-" + std::to_string(counter);
        std::string payload = "JobData-" + std::to_string(counter);

        try {
            auto fut = server.send_request(req_id, payload);

            std::thread([req_id, fut=std::move(fut), counter]() mutable {
                try {
                    std::string result = fut.get();
                    std::string color = COLORS[counter % COLORS.size()];
                    std::cout << color << "[Async] Request " << req_id
                              << " result: " << result << RESET << std::endl;
                } catch (const std::exception& e) {
                    std::cout << "[Async] Request " << req_id
                              << " failed: " << e.what() << std::endl;
                }
            }).detach();
        } catch (const std::exception& e) {
            std::cout << "[Server] ⚠ No clients available, skipping request "
                      << req_id << std::endl;
        }

        counter++;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    running = false;
    if (monitor.joinable()) monitor.join();
}
