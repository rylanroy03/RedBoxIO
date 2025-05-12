#include <iostream>
#include "asio.h"
#include "asiodrivers.h"
#include <windows.h>
#define DEBUG_MODE 0

static AsioDrivers* asioDrivers = nullptr;
ASIOBufferInfo* g_buffers = nullptr;
long g_bufferSize = 0;

ASIOTime* bufferSwitchTimeInfo(ASIOTime* timeInfo, long bufferIndex, ASIOBool processNow) {
    float* input = static_cast<float*>(g_buffers[0].buffers[bufferIndex]); // MONO input, channel 0
    float* outputL = static_cast<float*>(g_buffers[1].buffers[bufferIndex]); // MONO output, left
    float* outputR = static_cast<float*>(g_buffers[1].buffers[1 - bufferIndex]); // MONO output, right

    if (!input || !outputL || !outputR) return nullptr;

    // Copy input to output
    for (long i = 0; i < g_bufferSize; ++i) {
        outputL[i] = input[i]; // transparent passthrough
        outputR[i] = input[i];  // Right copy

    }

    return nullptr;
}



// Turns out we have to emulate a DAW just to initialize the ASIO driver and establish a stream.
long asioMessage(long selector, long value, void* message, double* opt) {
    std::cout << "[asioMessage] selector: " << selector 
              << ", value: " << value 
              << ", message: " << (message ? "(non-null)" : "null") 
              << ", opt: " << (opt ? *opt : 0.0) << "\n";

    switch (selector) { // Tell the dumb box that the smart box is here and ready.
        case 1: std::cout << "  -> kAsioSelector\n"; break;
        case 2: std::cout << "  -> kAsioEngineVersion\n"; return 2;
        case 3: std::cout << "  -> kAsioSupportsInputMonitor\n"; return 0;
        case 4: std::cout << "  -> kAsioResetRequest\n"; return 1;
        case 5: std::cout << "  -> kAsioBufferSizeChange\n"; return 1;
        case 6: std::cout << "  -> kAsioResyncRequest\n"; return 1;
        case 7: std::cout << "  -> kAsioLatenciesChanged\n"; return 1;
        case 8: std::cout << "  -> kAsioSupportsTimeInfo\n"; return 1;
        case 9: std::cout << "  -> kAsioSupportsTimeCode\n"; return 0;
        default:
            std::cout << "  -> Unknown selector\n";
    }

    return 0;
}

/* ###########
Main code loop
########### */

int main() {

    /* This section is dedicated to debugging */
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
    ASIOSampleRate desiredRate = static_cast<ASIOSampleRate>(48000.0);
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
    ASIOBufferInfo bufferInfos[2] = { 
        { ASIOTrue,  0, nullptr },  
        { ASIOFalse, 0, nullptr }   
    };

    g_buffers = bufferInfos;
    g_bufferSize = preferredSize;

    ASIOCallbacks callbacks = {
        nullptr,
        nullptr,
        asioMessage,    // message handler to wake up the interface
        bufferSwitchTimeInfo // active buffer callback
    };



    if (ASIOCreateBuffers(bufferInfos, 2, preferredSize, &callbacks) != ASE_OK) {
        std::cerr << "Failed to create ASIO buffers." << std::endl;
        ASIOExit();
        delete asioDrivers;
        return 1;
    }


    // Assuming stereo, bufferInfos[1] is the output
    float* outputBuffer = static_cast<float*>(bufferInfos[1].buffers[0]);
    std::cout << "Buffers created." << std::endl;
    // Fill output buffer with silence
    if (outputBuffer) {
        std::cout << "Output buffer acquired. Writing silence..." << std::endl;
        for (long i = 0; i < preferredSize; ++i) outputBuffer[i] = 0.0f;
    } else {
        std::cerr << "Output buffer is NULL!" << std::endl;
    }

    ASIOOutputReady();
    std::cout << "Called ASIOOutputReady()." << std::endl;

    if (ASIOStart() != ASE_OK) {
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
