cmake_minimum_required(VERSION 3.16)

project(guideXOSDeveloperStudio LANGUAGES CXX)

enable_testing()

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(developer_studio_models STATIC
    src/developer_studio_build.cpp
    src/developer_studio_models.cpp
    src/developer_studio_projects.cpp
    src/developer_studio_workspace.cpp
)
target_include_directories(developer_studio_models PUBLIC src)

add_executable(developer_studio_model_test
    tests/model_test.cpp
)
target_link_libraries(developer_studio_model_test PRIVATE developer_studio_models)

add_executable(developer_studio_workflow_test
    tests/workflow_test.cpp
)
target_link_libraries(developer_studio_workflow_test PRIVATE developer_studio_models)
set_target_properties(developer_studio_workflow_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)

add_executable(developer_studio_project_test
    tests/project_test.cpp
)
target_link_libraries(developer_studio_project_test PRIVATE developer_studio_models)
set_target_properties(developer_studio_project_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
add_test(NAME developer_studio_model_test COMMAND developer_studio_model_test)
add_test(NAME developer_studio_project_test COMMAND developer_studio_project_test)

add_executable(developer_studio_build_test
    tests/build_test.cpp
)
target_link_libraries(developer_studio_build_test PRIVATE developer_studio_models)
set_target_properties(developer_studio_build_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
add_test(NAME developer_studio_build_test COMMAND developer_studio_build_test)

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
        src/developer_studio_projects.cpp
        src/developer_studio_workspace.cpp
        src/freestanding_memory.cpp
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
