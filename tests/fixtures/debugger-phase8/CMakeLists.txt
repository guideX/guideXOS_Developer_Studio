cmake_minimum_required(VERSION 3.16)
project(DebuggerPhase8Fixture LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(GUIDEXOS_SERVER_ROOT "" CACHE PATH "guideXOS Server checkout")
if(NOT GUIDEXOS_SERVER_ROOT)
    message(FATAL_ERROR "Pass -DGUIDEXOS_SERVER_ROOT=<server-checkout>")
endif()

add_executable(debugger-phase8.elf src/main.cpp src/freestanding_memory.cpp)
target_include_directories(debugger-phase8.elf PRIVATE "${GUIDEXOS_SERVER_ROOT}/sdk/include")
target_compile_options(debugger-phase8.elf PRIVATE
    --target=x86_64-unknown-elf
    -ffreestanding
    -fno-exceptions
    -fno-rtti
    -fno-stack-protector
    -fno-omit-frame-pointer
    -fno-unwind-tables
    -fno-asynchronous-unwind-tables
)
target_link_options(debugger-phase8.elf PRIVATE
    --target=x86_64-unknown-elf
    -nostdlib
    -static
    -fuse-ld=lld
    -Wl,-e,gx_main
    -Wl,--image-base=0x20000000
)
set_target_properties(debugger-phase8.elf PROPERTIES OUTPUT_NAME debugger-phase8 SUFFIX ".elf")
