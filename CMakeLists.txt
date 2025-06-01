cmake_minimum_required(VERSION 3.10)
project(CustomMixer)

# We have to define this or else ASIOSampleRate will not be defined as a double
add_definitions(-DIEEE754_64FLOAT)

set(CMAKE_CXX_STANDARD 17)

# Tell the compiler where to find asio.h and friends
include_directories("external")

# TELL CMAKE TO SHUT UP ABOUT THE STRCPY WARNING
add_compile_definitions(_CRT_SECURE_NO_WARNINGS)

# This tells Cmake to ignore size_t warnings because we use 64 bit now and the world is healing
set_source_files_properties(
    external/asiolist.cpp
    PROPERTIES COMPILE_FLAGS "/wd4267"
)

# Source files to compile
set(ASIO_SRC
    "external/asio.cpp"
    "external/asiodrivers.cpp"
    "external/asiolist.cpp"
    "AsioInit.cpp"
)

# Main app
add_executable(CustomMixer main.cpp ${ASIO_SRC})

# Link Win32 COM libs
target_link_libraries(CustomMixer PRIVATE ole32 uuid winmm)
