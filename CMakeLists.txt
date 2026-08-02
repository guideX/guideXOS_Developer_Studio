cmake_minimum_required(VERSION 3.16)

project(guideXOSDeveloperStudio LANGUAGES CXX)

enable_testing()

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(developer_studio_models STATIC
    src/developer_studio_find.cpp
    src/developer_studio_syntax.cpp
    src/developer_studio_output.cpp
    src/developer_studio_build.cpp
    src/developer_studio_models.cpp
    src/developer_studio_projects.cpp
    src/developer_studio_run.cpp
    src/developer_studio_workspace.cpp
    src/developer_studio_project_search.cpp
    src/developer_studio_symbols.cpp
    src/developer_studio_navigation.cpp
    src/developer_studio_references.cpp
    src/developer_studio_rename.cpp
    src/developer_studio_completion.cpp
    src/developer_studio_signature.cpp
    src/developer_studio_include_graph.cpp
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

add_executable(developer_studio_run_test
    tests/run_test.cpp
)
target_link_libraries(developer_studio_run_test PRIVATE developer_studio_models)
set_target_properties(developer_studio_run_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
add_test(NAME developer_studio_run_test COMMAND developer_studio_run_test)

add_executable(developer_studio_output_test
    tests/output_test.cpp
)
target_link_libraries(developer_studio_output_test PRIVATE developer_studio_models)
set_target_properties(developer_studio_output_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
add_test(NAME developer_studio_output_test COMMAND developer_studio_output_test)

add_executable(developer_studio_navigation_test
    tests/navigation_test.cpp
)
target_link_libraries(developer_studio_navigation_test PRIVATE developer_studio_models)
set_target_properties(developer_studio_navigation_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
add_test(NAME developer_studio_navigation_test COMMAND developer_studio_navigation_test)

add_executable(developer_studio_syntax_test
    tests/syntax_test.cpp
)
target_link_libraries(developer_studio_syntax_test PRIVATE developer_studio_models)
set_target_properties(developer_studio_syntax_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
add_test(NAME developer_studio_syntax_test COMMAND developer_studio_syntax_test)

add_executable(developer_studio_find_test
    tests/find_test.cpp
)
target_link_libraries(developer_studio_find_test PRIVATE developer_studio_models)
set_target_properties(developer_studio_find_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
add_test(NAME developer_studio_find_test COMMAND developer_studio_find_test)

add_executable(developer_studio_project_search_test
    tests/project_search_test.cpp
)
target_link_libraries(developer_studio_project_search_test PRIVATE developer_studio_models)
set_target_properties(developer_studio_project_search_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
add_test(NAME developer_studio_project_search_test COMMAND developer_studio_project_search_test)

add_executable(developer_studio_symbol_test
    tests/symbol_test.cpp
)
target_link_libraries(developer_studio_symbol_test PRIVATE developer_studio_models)
set_target_properties(developer_studio_symbol_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
add_test(NAME developer_studio_symbol_test COMMAND developer_studio_symbol_test)

add_executable(developer_studio_navigation_definition_test
    tests/navigation_definition_test.cpp
)
target_link_libraries(developer_studio_navigation_definition_test PRIVATE developer_studio_models)
set_target_properties(developer_studio_navigation_definition_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
add_test(NAME developer_studio_navigation_definition_test COMMAND developer_studio_navigation_definition_test)

add_executable(developer_studio_reference_test
    tests/reference_test.cpp
)
target_link_libraries(developer_studio_reference_test PRIVATE developer_studio_models)
set_target_properties(developer_studio_reference_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
add_test(NAME developer_studio_reference_test COMMAND developer_studio_reference_test)

add_executable(developer_studio_rename_test
    tests/rename_test.cpp
)
target_link_libraries(developer_studio_rename_test PRIVATE developer_studio_models)
set_target_properties(developer_studio_rename_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
add_test(NAME developer_studio_rename_test COMMAND developer_studio_rename_test)

add_executable(developer_studio_completion_test
    tests/completion_test.cpp
)
target_link_libraries(developer_studio_completion_test PRIVATE developer_studio_models)
set_target_properties(developer_studio_completion_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
add_test(NAME developer_studio_completion_test COMMAND developer_studio_completion_test)

add_executable(developer_studio_signature_test
    tests/signature_test.cpp
)
target_link_libraries(developer_studio_signature_test PRIVATE developer_studio_models)
set_target_properties(developer_studio_signature_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
add_test(NAME developer_studio_signature_test COMMAND developer_studio_signature_test)

add_executable(developer_studio_include_graph_test
    tests/include_graph_test.cpp
)
target_link_libraries(developer_studio_include_graph_test PRIVATE developer_studio_models)
set_target_properties(developer_studio_include_graph_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
add_test(NAME developer_studio_include_graph_test COMMAND developer_studio_include_graph_test)

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
        src/developer_studio_find.cpp
        src/developer_studio_syntax.cpp
        src/developer_studio_output.cpp
        src/developer_studio_build.cpp
        src/developer_studio_models.cpp
        src/developer_studio_projects.cpp
        src/developer_studio_run.cpp
        src/developer_studio_workspace.cpp
        src/developer_studio_project_search.cpp
        src/developer_studio_symbols.cpp
        src/developer_studio_navigation.cpp
        src/developer_studio_references.cpp
        src/developer_studio_rename.cpp
        src/developer_studio_completion.cpp
        src/developer_studio_signature.cpp
        src/developer_studio_include_graph.cpp
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
