cmake_minimum_required(VERSION 3.16)

project(guideXOSDeveloperStudio LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(developer_studio_models STATIC
    src/developer_studio_models.cpp
    src/developer_studio_workspace.cpp
)
target_include_directories(developer_studio_models PUBLIC src)

add_executable(developer_studio_model_test
    tests/model_test.cpp
)
target_link_libraries(developer_studio_model_test PRIVATE developer_studio_models)

# The production Native ELF link is intentionally driven by build.ps1. That
# script follows the Server SDK's direct clang/ld.lld convention and writes the
# package into the Server checkout rather than creating a second SDK copy here.
option(GUIDEXOS_BUILD_NATIVE_ELF "Build the Native ELF shell with CMake" OFF)
set(GUIDEXOS_SERVER_ROOT "" CACHE PATH "guideXOS Server checkout")

if(GUIDEXOS_BUILD_NATIVE_ELF)
    if(NOT GUIDEXOS_SERVER_ROOT)
        message(FATAL_ERROR "GUIDEXOS_SERVER_ROOT is required for the Native ELF target")
    endif()

    add_executable(developerstudio.elf
        src/developer_studio_models.cpp
        src/developer_studio_workspace.cpp
        src/main.cpp
    )
    target_include_directories(developerstudio.elf PRIVATE
        src
        "${GUIDEXOS_SERVER_ROOT}/sdk/include"
    )
    target_compile_options(developerstudio.elf PRIVATE
        --target=x86_64-unknown-elf
        -ffreestanding
        -fno-exceptions
        -fno-rtti
        -fno-stack-protector
        -fno-unwind-tables
        -fno-asynchronous-unwind-tables
    )
    target_link_options(developerstudio.elf PRIVATE
        --target=x86_64-unknown-elf
        -nostdlib
        -static
        -fuse-ld=lld
        -Wl,-e,gx_main
    )
    set_target_properties(developerstudio.elf PROPERTIES
        OUTPUT_NAME developerstudio
        SUFFIX .elf
        RUNTIME_OUTPUT_DIRECTORY "${GUIDEXOS_SERVER_ROOT}/Apps/DeveloperStudio/bin/amd64"
    )
endif()
