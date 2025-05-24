#include <iostream>
#define IEEE754_64FLOAT 1
#include "asio.h"
#include "asiodrivers.h"
#include <windows.h>
#include <cmath>

static AsioDrivers* asioDrivers = nullptr;
ASIOBufferInfo* g_buffers = nullptr;
long g_bufferSize = 0;
static double g_sampleRate = 48000.0;

/* NOTE REGARDING AUDIO BUFFER STRUCTURE:
Audio from the Focusrite is 24-bit packed into 32-bit words. The driver delivers
32-bit words where only bits [8-31] are valid audio. The 8 LSB bits are zeroed padding.
In the bufferswitch function, this array is decoded by bit shift, division and sign-extension.
When returned to the driver, the audio is re-encoded into 32-bit integer form.
It took me too long to figure this shit out so hopefully this is helpful. :) 
*/
ASIOTime* bufferSwitchTimeInfo(ASIOTime* timeInfo, long bufferIndex, ASIOBool processNow) {
    int32_t* input = static_cast<int32_t*>(g_buffers[0].buffers[bufferIndex]);   // MONO input, 24-bit packed
    int32_t* outputL = static_cast<int32_t*>(g_buffers[1].buffers[bufferIndex]); // Left output
    int32_t* outputR = static_cast<int32_t*>(g_buffers[2].buffers[bufferIndex]); // Right output

    if (!input || !outputL || !outputR) return nullptr; // error-catch bail out

    for (long i = 0; i < g_bufferSize; ++i) {
        int32_t raw = input[i] >> 8;    // Shift right to remove padding
        if (raw & 0x800000) raw |= ~0xFFFFFF;   // sign-extend negatives (0xFF padding to 24-31)
        float sample = static_cast<float>(raw) / 8388608.0f;    // Normalizing to float with range [-1.0, 1.0]

        // Future processing hook for routing to external DSP pipeline.
        float output = sample;  // e.g., applyGain(sample), tremolo(sample), etc.
        
        int32_t encoded = static_cast<int32_t>(output * 8388608.0f) << 8;   // This recasts for output
        outputL[i] = encoded;
        outputR[i] = encoded;
    }

    return nullptr;
}


/* ASIOMESSAGE HANDLER:
This function is a required callback for the ASIO driver to communicate with the host application.
It handles driver-specific messages as described in the switch cases below. These are acknowledged
with simple return values to keep the driver happy. In a full DAW this function would manage more 
detailed driver integration, but that is not necessary for this program (for now...).
My implementation is dumb but quick and it works.
*/
long asioMessage(long selector, long value, void* message, double* opt) {
    switch (selector) {
        case 2: return 2; // kAsioEngineVersion
        case 3: return 0; // kAsioSupportsInputMonitor
        case 4: return 1; // kAsioResetRequest
        case 5: return 1; // kAsioBufferSizeChange
        case 6: return 1; // kAsioResyncRequest
        case 7: return 1; // kAsioLatenciesChanged
        case 8: return 1; // kAsioSupportsTimeInfo
        case 9: return 0; // kAsioSupportsTimeCode
        default: return 0;
    }
}


/* MAIN FUNCTION:
This function initializes the ASIO driver, sets up the input/output buffers, 
and starts the real-time audio stream. It currently uses a hardcoded target (Focusrite USB ASIO), 
retrieves sample rate, buffer format, and enters a passthrough loop using the bufferSwitchTimeInfo callback.
Execution blocks until the user presses ENTER, at which point the driver is stopped and cleaned up.
*/

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

    if (ASIOInit(&info) != ASE_OK) {
        std::cerr << "ASIOInit failed.\n";
        delete asioDrivers;
        return 1;
    }

    std::cout << "Successfully initialized ASIO driver: " << info.name << "\n";
    
    // This section queries sample rate
    ASIOSampleRate currentRate;
    if (ASIOGetSampleRate(&currentRate) == ASE_OK) {
        g_sampleRate = currentRate;
        std::cout << "Sample rate: " << g_sampleRate;
    } else {
        std::cerr << "WARN: Using fallback sample rate of 48 kHz\n";
        g_sampleRate = 48000.0;
    }

    // ASIOGetBufferSize is now written to break loop if buffer range is unreported or invalid.
    long minSize, maxSize, preferredSize, granularity;
    if (ASIOGetBufferSize(&minSize, &maxSize, &preferredSize, &granularity) != ASE_OK) {
        std::cerr << "Failed to query buffer size range.\n";
        ASIOExit();
        delete asioDrivers;
        return 1;
    }

    std::cout << "Buffer size range: " << minSize << " to " << maxSize << "\n";
    std::cout << "Preferred size: " << preferredSize << "\n";
    
    // Here we're defining the channel to be used.
    // The plan is to implement channel selection later.
    ASIOBufferInfo bufferInfos[3] = { 
        { ASIOTrue,  0, nullptr },  
        { ASIOFalse, 0, nullptr },
        { ASIOFalse, 1, nullptr }
    };

    g_buffers = bufferInfos;
    g_bufferSize = preferredSize;

    ASIOCallbacks callbacks = {
        nullptr,                // deprecated bufferSwitch
        nullptr,                // sampleRateDidChange
        asioMessage,            // message handler to wake up the interface
        bufferSwitchTimeInfo    // active buffer callback
    };

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
    ASIOExit();
    delete asioDrivers;
    return 0;
}