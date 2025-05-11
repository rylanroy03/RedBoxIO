#include <iostream>
#include "asio.h"
#include "asiodrivers.h"

static AsioDrivers* asioDrivers = nullptr;

int main() {
    asioDrivers = new AsioDrivers();

    char targetDriver[] = "Focusrite USB ASIO";

    if (!asioDrivers->loadDriver(targetDriver)) {
        std::cerr << "Failed to load ASIO driver: " << targetDriver << "\n";
        delete asioDrivers;
        return 1;
    }

    ASIODriverInfo info = {};
    info.asioVersion = 2;

    if (ASIOInit(&info) == ASE_OK) {
        std::cout << "Successfully initialized ASIO driver: " << info.name << "\n";
    } else {
        std::cerr << "ASIOInit failed.\n";
    }

    ASIOExit(); // Always cleanup
    delete asioDrivers;
    return 0;
}
