# RedBoxIO
> A WIP ASIO-based engine for virtual mic output, audio routing, and live processing.

RedBoxIO is a personal audio utility for low-latency input routing and real-time processing using the Steinberg ASIO SDK. It is designed to capture microphone input, apply optional processing, and route the result to a virtual input device for use in applications like Discord, OBS, or DAWs.

### Project Vision

Unlike popular tools like Voicemeeter or Voicemod—which are designed primarily for general-purpose hardware and rely on system-level APIs like WASAPI—RedBoxIO is designed specifically for users of ASIO hardware. Existing solutions often perform poorly with prosumer and professional audio interfaces, introducing latency, instability, or routing limitations.

RedBoxIO is being developed from scratch to provide a clean, low-latency, and stable alternative tailored to the needs of ASIO-based workflows.

Ultimately, planned RedBoxIO functionality includes:
- Capturing audio from ASIO input devices
- Applying optional real-time DSP (gain, EQ, compression)
- Outputting to a virtual input device for use in other software
- Live monitoring with minimal latency
- Simple preset and routing scene management


---
## Required Dependencies: Steinberg ASIO SDK & Focusrite Control

**Due to licensing restrictions, the ASIO SDK and Focusrite Control DLLs are not included in this repository.**

This project uses Steinberg's ASIO SDK to build an ASIO host application. It also depends on an installed Focusrite USB ASIO driver (tested with the Scarlett Solo 3rd Gen).


### 1. Steinberg ASIO SDK
To build this project, download the ASIO SDK from Steinberg's official website:
[https://www.steinberg.net/en/company/developers.html](https://www.steinberg.net/en/company/developers.html)

After downloading the SDK:
1. Copy the required source files from the downloaded SDK into the `external/` directory of the project in your IDE.
2. Required files include:
    - asio.h
    - asiodrvr.h
    - asiodrivers.h
    - asiolist.cpp
    - asiodrivers.cpp
    - asio.cpp
    - asiosys.h
    - ginclude.h
    - iasiodrv.h
3. Once these files are in place, you can build the project using CMake:

```bash
rm -r build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -A x64
cmake --build build --config Release
```

### 2. Focusrite Control + USB ASIO Driver

> ⚠️ **Compatibility Warning:**  
>This project has **only** been tested with the 3rd Gen Focusrite Scarlett Solo and 2i2.  
> Compatibility with other Focusrite models is **not guaranteed**.  
> I am not aware of driver-level differences between models, and I cannot currently devote the time or resources to investigating broader compatibility.

This project assumes that the Focusrite ASIO driver is already installed.  
If it is not already installed, you can download it here:

[https://downloads.focusrite.com/focusrite/scarlett-3rd-gen/scarlett-solo-3rd-gen](https://downloads.focusrite.com/focusrite/scarlett-3rd-gen/scarlett-solo-3rd-gen)

The compiled binary is located at ```build/Release/CustomMixer.exe```.
