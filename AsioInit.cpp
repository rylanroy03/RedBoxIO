// RedBoxIO - Low-latency ASIO audio routing engine
// Copyright (C) 2025 Rylan Markwardt

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published
// by the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU Affero General Public License for more details.

// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

#include "AsioInit.h"
#define IEEE754_64FLOAT 1
#include "asio.h"
#include "asiodrivers.h"
#include <iostream>

static AsioDrivers* asioDrivers = nullptr;

bool initializeAsioDriver(const char* driverName, double& outSampleRate) {
    asioDrivers = new AsioDrivers();

    // Create a mutable copy of the driver name. 
    // Steinberg ASIO requires nonconst char* so we copy here.
    char targetDriver[128];
    std::strncpy(targetDriver, driverName, sizeof(targetDriver));
    targetDriver[sizeof(targetDriver) - 1] = '\0';

    // Loads driver by name specified within main.
    if (!asioDrivers->loadDriver(targetDriver)) {
        std::cerr << "[ASIO] Failed to load driver: " << targetDriver << "\n";
        delete asioDrivers;
        return false;
    }

    ASIODriverInfo info = {};
    info.asioVersion = 2;

    // Initializes selected ASIO driver and prepares for stream.
    if (ASIOInit(&info) != ASE_OK) {
        std::cerr << "[ASIO] ASIOInit failed.\n";
        delete asioDrivers;
        return false;
    }
    
    std::cout << "[ASIO] Successfully initialized driver: " << info.name << "\n";
    
    // Queries sample rate. Mostly diagnostic for now until DSP is incorporated.
    ASIOSampleRate currentRate;
    if (ASIOGetSampleRate(&currentRate) == ASE_OK) {
        outSampleRate = currentRate;
        std::cout << "[ASIO] Sample rate: " << outSampleRate << " Hz\n";
    } else {
        std::cerr << "[ASIO] Failed to assign: Setting to 48000 Hz\n";
        outSampleRate = 48000.0;
    }
    
    return true;
}