#include <CommonAPI/CommonAPI.hpp>
#include "OtaUpdateServiceImpl.hpp"
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    std::cout << "[OTA] Starting OTA Update Service...\n";

    auto runtime = CommonAPI::Runtime::get();

    auto service = std::make_shared<OtaUpdateServiceImpl>();

    // Domain / instance must match InstanceId in the .fdepl
    const std::string domain   = "local";
    const std::string instance = "com.myapp.ota.OtaUpdate";

    bool registered = runtime->registerService(domain, instance, service,
                                               "OtaUpdateService");
    if (!registered) {
        std::cerr << "[OTA] Failed to register service!\n";
        return 1;
    }

    std::cout << "[OTA] Service registered. Waiting for laptop...\n";

    while (true)
        std::this_thread::sleep_for(std::chrono::seconds(5));

    return 0;
}
