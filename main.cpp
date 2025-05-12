#include <iostream>
#define IEEE754_64FLOAT 1
#include "asio.h"
#include "asiodrivers.h"
#include <windows.h>
#define DEBUG_MODE 0

static AsioDrivers* asioDrivers = nullptr;
ASIOBufferInfo* g_buffers = nullptr;
long g_bufferSize = 0;

ASIOTime* bufferSwitchTimeInfo(ASIOTime* timeInfo, long bufferIndex, ASIOBool processNow) {
    float* input = static_cast<float*>(g_buffers[0].buffers[bufferIndex]); // MONO input, channel 0
    float* outputL = static_cast<float*>(g_buffers[1].buffers[bufferIndex]); // MONO left out
    float* outputR = static_cast<float*>(g_buffers[2].buffers[bufferIndex]); // MONO right out

    if (!input || !outputL || !outputR) return nullptr; // error-catch bailout
    // Very simple passthrough. Copies input to left and right output.
    for (long i = 0; i < g_bufferSize; ++i) {
        outputL[i] = input[i]; // L in->out
        outputR[i] = input[i];  // R in->out

    }
    return nullptr;
}



// Turns out we have to emulate a DAW just to initialize the ASIO driver and establish a stream.
// The debug section can be safely ignored but is useful for development.
#if DEBUG_MODE
long asioMessage(long selector, long value, void* message, double* opt) {
    std::cout << "[asioMessage] selector: " << selector 
              << ", value: " << value 
              << ", message: " << (message ? "(non-null)" : "null") 
              << ", opt: " << (opt ? *opt : 0.0) << "\n";

    switch (selector) {
        case 1: std::cout << "  -> kAsioSelector\n"; break;
        case 2: std::cout << "  -> kAsioEngineVersion\n"; return 2;
        case 3: std::cout << "  -> kAsioSupportsInputMonitor\n"; return 0;
        case 4: std::cout << "  -> kAsioResetRequest\n"; return 1;
        case 5: std::cout << "  -> kAsioBufferSizeChange\n"; return 1;
        case 6: std::cout << "  -> kAsioResyncRequest\n"; return 1;
        case 7: std::cout << "  -> kAsioLatenciesChanged\n"; return 1;
        case 8: std::cout << "  -> kAsioSupportsTimeInfo\n"; return 1;
        case 9: std::cout << "  -> kAsioSupportsTimeCode\n"; return 0;
        default: std::cout << "  -> Unknown selector\n";
    }
    return 0;
}
#else
long asioMessage(long selector, long value, void* message, double* opt) {
    switch (selector) {
        case 2: return 2;
        case 3: return 0;
        case 4: return 1;
        case 5: return 1;
        case 6: return 1;
        case 7: return 1;
        case 8: return 1;
        case 9: return 0;
        default: return 0;
    }
}
#endif

/* ###########
Main code loop
########### */

int main() {

    /* This section is dedicated to debugging present channels */
    #if DEBUG_MODE
    long numInputs = 0;
    long numOutputs = 0;
    // This should scan the channels for input/output
    if (ASIOGetChannels(&numInputs, &numOutputs) == ASE_OK) {
    std::cout << "Input channels: " << numInputs << std::endl;
    std::cout << "Output channels: " << numOutputs << std::endl;
    } else {
    std::cerr << "Failed to get ASIO channel information." << std::endl;
    }

    ASIOChannelInfo chInfo = {};

    std::cout << "\n--- Input Channels ---\n";
    for (long i = 0; i < numInputs; ++i) {
        chInfo.channel = i;
        chInfo.isInput = ASIOTrue;
        if (ASIOGetChannelInfo(&chInfo) == ASE_OK) {
            std::cout << "[" << i << "] Input: " << chInfo.name << std::endl;
        } else {
            std::cerr << "Failed to get info for input channel " << i << std::endl;
        }
    }

    std::cout << "\n--- Output Channels ---\n";
    for (long i = 0; i < numOutputs; ++i) {
        chInfo.channel = i;
        chInfo.isInput = ASIOFalse;
        if (ASIOGetChannelInfo(&chInfo) == ASE_OK) {
            std::cout << "[" << i << "] Output: " << chInfo.name << std::endl;
        } else {
            std::cerr << "Failed to get info for output channel " << i << std::endl;
        }
    }
    #endif

    asioDrivers = new AsioDrivers();
    char targetDriver[] = "Focusrite USB ASIO";

    if (!asioDrivers->loadDriver(targetDriver)) {
        std::cerr << "Failed to load ASIO driver: " << targetDriver << "\n";
        delete asioDrivers;
        return 1;
    }

    ASIODriverInfo info = {}; // This initializes the ASIO struct
    info.asioVersion = 2;

    if (ASIOInit(&info) == ASE_OK) { // Checks ASIO struct status (ASE_OK=0)
        std::cout << "Successfully initialized ASIO driver: " << info.name << "\n";
    } else {
        std::cerr << "ASIOInit failed.\n";
    }
    ASIOSampleRate desiredRate = 48000.0;
    if (ASIOSetSampleRate(desiredRate) != ASE_OK) {
        std::cerr << "ASIOSetSampleRate failed." << std::endl;
    }

    // Yes, this is techinically debug info, but useful for development...
    long minSize, maxSize, preferredSize, granularity;
    if (ASIOGetBufferSize(&minSize, &maxSize, &preferredSize, &granularity) == ASE_OK) {
        std::cout << "Buffer size range: " << minSize << " to " << maxSize << std::endl;
        std::cout << "Preferred size: " << preferredSize << std::endl; // ...and currently queries device preferredSize.
        std::cout << "Granularity: " << granularity << std::endl;
    } else {
        std::cerr << "Failed to query buffer size range." << std::endl;
    }
    
    // Here we're defining the channel to be used. Plan to implement channel selection later.
    ASIOBufferInfo bufferInfos[3] = { 
        { ASIOTrue,  0, nullptr },  
        { ASIOFalse, 0, nullptr },
        { ASIOFalse, 1, nullptr }
    };

    g_buffers = bufferInfos;
    g_bufferSize = preferredSize;

    ASIOCallbacks callbacks = {
        nullptr,
        nullptr,
        asioMessage,    // message handler to wake up the interface
        bufferSwitchTimeInfo // active buffer callback
    };


    // Initializes the buffers used in 
    if (ASIOCreateBuffers(bufferInfos, 3, preferredSize, &callbacks) != ASE_OK) {
        std::cerr << "Failed to create ASIO buffers." << std::endl;
        ASIOExit();
        delete asioDrivers;
        return 1;
    }
    std::cout << "Buffers created." << std::endl;

    if (ASIOStart() != ASE_OK) { // Attempts to initiate ASIO stream
        std::cerr << "Failed to start ASIO stream." << std::endl;
        ASIOExit();
        delete asioDrivers;
        return 1;
    }
    std::cout << "ASIO stream started. Press ENTER to stop..." << std::endl;
    std::cin.get();



    ASIOStop();
    ASIODisposeBuffers();
    ASIOExit(); // Always cleanup
    delete asioDrivers;
    return 0;
}