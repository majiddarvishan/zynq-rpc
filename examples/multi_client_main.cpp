#include "zmq_rpc.hpp"
#include <thread>
#include <chrono>
#include <vector>
#include <iostream>

// ANSI colors
const std::vector<std::string> COLORS = {
    "\033[31m", // red
    "\033[32m", // green
    "\033[33m", // yellow
    "\033[34m", // blue
    "\033[35m", // magenta
    "\033[36m", // cyan
    "\033[37m"  // white
};
const std::string COLOR_RESET = "\033[0m";

void run_client(int id) {
    zmq_rpc::Client client("tcp://localhost:5555");
    std::string color = COLORS[id % COLORS.size()];

    client.set_request_handler([id, color](const std::string& payload) -> std::string {
        std::this_thread::sleep_for(std::chrono::milliseconds(500 + id*100));
        std::string result = "Client" + std::to_string(id) + "_Handled(" + payload + ")";
        std::cout << color << "[" << "Client" << id << "] Responded: " << result << COLOR_RESET << std::endl;
        return result;
    });

    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

int main() {
    const int NUM_CLIENTS = 5; // spawn 5 clients
    std::vector<std::thread> threads;

    for (int i = 0; i < NUM_CLIENTS; ++i) {
        threads.emplace_back(run_client, i);
    }

    for (auto& t : threads) {
        t.join();
    }
}
