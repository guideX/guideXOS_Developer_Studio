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
    src/developer_studio_cpp_lexer.cpp
    src/developer_studio_task_list.cpp
    src/developer_studio_document_outline.cpp
    src/developer_studio_replace_preview.cpp
    src/developer_studio_tools.cpp
    src/developer_studio_symbols.cpp
    src/developer_studio_navigation.cpp
    src/developer_studio_references.cpp
    src/developer_studio_rename.cpp
    src/developer_studio_completion.cpp
    src/developer_studio_signature.cpp
    src/developer_studio_include_graph.cpp
    src/developer_studio_relationships.cpp
    src/developer_studio_ownership.cpp
    src/developer_studio_types.cpp
    src/developer_studio_debugger.cpp
    src/developer_studio_debugger_stack.cpp
    src/developer_studio_debug_symbols.cpp
    src/developer_studio_debug_variables.cpp
    src/developer_studio_debug_watches.cpp
    src/developer_studio_debug_tips.cpp
    src/developer_studio_debugger_hosted.cpp
    src/developer_studio_debug_editor.cpp
    src/developer_studio_debugger_workspace.cpp
    src/developer_studio_startup.cpp
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

add_executable(developer_studio_startup_test
    tests/startup_test.cpp
)
target_link_libraries(developer_studio_startup_test PRIVATE developer_studio_models)
add_test(NAME developer_studio_startup_test COMMAND developer_studio_startup_test)

add_executable(developer_studio_sentinel_test
    tests/sentinel_test.cpp
)
target_link_libraries(developer_studio_sentinel_test PRIVATE developer_studio_models)
add_test(NAME developer_studio_sentinel_test COMMAND developer_studio_sentinel_test)

add_executable(developer_studio_build_test
    tests/build_test.cpp
)
target_link_libraries(developer_studio_build_test PRIVATE developer_studio_models)
set_target_properties(developer_studio_build_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
add_test(NAME developer_studio_build_test COMMAND developer_studio_build_test)

add_executable(developer_studio_source_enumeration_test
    tests/source_enumeration_test.cpp
)
target_link_libraries(developer_studio_source_enumeration_test PRIVATE developer_studio_models)
set_target_properties(developer_studio_source_enumeration_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
add_test(NAME developer_studio_source_enumeration_test COMMAND developer_studio_source_enumeration_test)

add_executable(developer_studio_file_workflow_test
    tests/workspace_file_workflow_test.cpp
)
target_link_libraries(developer_studio_file_workflow_test PRIVATE developer_studio_models)
set_target_properties(developer_studio_file_workflow_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
add_test(NAME developer_studio_file_workflow_test COMMAND developer_studio_file_workflow_test)

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

add_executable(developer_studio_project_navigation_inventory_test
    tests/project_navigation_inventory_test.cpp
)
target_link_libraries(developer_studio_project_navigation_inventory_test PRIVATE developer_studio_models)
set_target_properties(developer_studio_project_navigation_inventory_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
set_target_properties(developer_studio_navigation_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
add_test(NAME developer_studio_navigation_test COMMAND developer_studio_navigation_test)
add_test(NAME developer_studio_project_navigation_inventory_test COMMAND developer_studio_project_navigation_inventory_test)

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

add_executable(developer_studio_replace_preview_test
    tests/replace_preview_test.cpp
)
target_link_libraries(developer_studio_replace_preview_test PRIVATE developer_studio_models)
set_target_properties(developer_studio_replace_preview_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
add_test(NAME developer_studio_replace_preview_test COMMAND developer_studio_replace_preview_test)

add_executable(developer_studio_tools_test
    tests/tools_test.cpp
)
target_link_libraries(developer_studio_tools_test PRIVATE developer_studio_models)
add_test(NAME developer_studio_tools_test COMMAND developer_studio_tools_test)

add_executable(developer_studio_task_list_test
    tests/task_list_test.cpp
)
target_link_libraries(developer_studio_task_list_test PRIVATE developer_studio_models)
set_target_properties(developer_studio_task_list_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
add_test(NAME developer_studio_task_list_test COMMAND developer_studio_task_list_test)

add_executable(developer_studio_document_outline_test
    tests/document_outline_test.cpp
)
target_link_libraries(developer_studio_document_outline_test PRIVATE developer_studio_models)
set_target_properties(developer_studio_document_outline_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
add_test(NAME developer_studio_document_outline_test COMMAND developer_studio_document_outline_test)

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

add_executable(developer_studio_relationship_test
    tests/relationship_test.cpp
)
target_link_libraries(developer_studio_relationship_test PRIVATE developer_studio_models)
set_target_properties(developer_studio_relationship_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
add_test(NAME developer_studio_relationship_test COMMAND developer_studio_relationship_test)

add_executable(developer_studio_ownership_test
    tests/ownership_test.cpp
)
target_link_libraries(developer_studio_ownership_test PRIVATE developer_studio_models)
set_target_properties(developer_studio_ownership_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
add_test(NAME developer_studio_ownership_test COMMAND developer_studio_ownership_test)

add_executable(developer_studio_types_test
    tests/types_test.cpp
)
target_link_libraries(developer_studio_types_test PRIVATE developer_studio_models)
set_target_properties(developer_studio_types_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
add_test(NAME developer_studio_types_test COMMAND developer_studio_types_test)

add_executable(developer_studio_debugger_test
    tests/debugger_test.cpp
)
target_link_libraries(developer_studio_debugger_test PRIVATE developer_studio_models)
set_target_properties(developer_studio_debugger_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)

add_executable(developer_studio_debug_symbols_test
    tests/debug_symbols_test.cpp
)
target_link_libraries(developer_studio_debug_symbols_test PRIVATE developer_studio_models)
set_target_properties(developer_studio_debug_symbols_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
add_test(NAME developer_studio_debug_symbols_test COMMAND developer_studio_debug_symbols_test)
add_test(NAME developer_studio_debugger_test COMMAND developer_studio_debugger_test)

add_executable(developer_studio_debugger_step_test
    tests/debugger_step_test.cpp
)
target_link_libraries(developer_studio_debugger_step_test PRIVATE developer_studio_models)
set_target_properties(developer_studio_debugger_step_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
add_test(NAME developer_studio_debugger_step_test COMMAND developer_studio_debugger_step_test)

add_executable(developer_studio_debugger_stack_test
    tests/debugger_stack_test.cpp
)
target_link_libraries(developer_studio_debugger_stack_test PRIVATE developer_studio_models)
set_target_properties(developer_studio_debugger_stack_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
add_test(NAME developer_studio_debugger_stack_test COMMAND developer_studio_debugger_stack_test)

add_executable(developer_studio_debugger_variables_test
    tests/debugger_variables_test.cpp
)
target_link_libraries(developer_studio_debugger_variables_test PRIVATE developer_studio_models)
set_target_properties(developer_studio_debugger_variables_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)

add_executable(developer_studio_debugger_structured_variables_test
    tests/debugger_structured_variables_test.cpp
)
target_link_libraries(developer_studio_debugger_structured_variables_test PRIVATE developer_studio_models)
set_target_properties(developer_studio_debugger_structured_variables_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
add_test(NAME developer_studio_debugger_structured_variables_test COMMAND developer_studio_debugger_structured_variables_test)
add_test(NAME developer_studio_debugger_variables_test COMMAND developer_studio_debugger_variables_test)

add_executable(developer_studio_debugger_watches_test
    tests/debugger_watches_test.cpp
)
target_link_libraries(developer_studio_debugger_watches_test PRIVATE developer_studio_models)
set_target_properties(developer_studio_debugger_watches_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
add_test(NAME developer_studio_debugger_watches_test COMMAND developer_studio_debugger_watches_test)

add_executable(developer_studio_debugger_conditional_breakpoints_test
    tests/debugger_conditional_breakpoints_test.cpp
)
target_link_libraries(developer_studio_debugger_conditional_breakpoints_test PRIVATE developer_studio_models)
set_target_properties(developer_studio_debugger_conditional_breakpoints_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
add_test(NAME developer_studio_debugger_conditional_breakpoints_test COMMAND developer_studio_debugger_conditional_breakpoints_test)

add_executable(developer_studio_debug_editor_test
    tests/debug_editor_test.cpp
)
target_link_libraries(developer_studio_debug_editor_test PRIVATE developer_studio_models)
set_target_properties(developer_studio_debug_editor_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
add_test(NAME developer_studio_debug_editor_test COMMAND developer_studio_debug_editor_test)

add_executable(developer_studio_debugger_workspace_test
    tests/debugger_workspace_test.cpp
)
target_link_libraries(developer_studio_debugger_workspace_test PRIVATE developer_studio_models)
set_target_properties(developer_studio_debugger_workspace_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
add_test(NAME developer_studio_debugger_workspace_test COMMAND developer_studio_debugger_workspace_test)

add_executable(developer_studio_debug_tips_test
    tests/debugger_data_tips_test.cpp
)
target_link_libraries(developer_studio_debug_tips_test PRIVATE developer_studio_models)
set_target_properties(developer_studio_debug_tips_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
add_test(NAME developer_studio_debug_tips_test COMMAND developer_studio_debug_tips_test)
set_tests_properties(
    developer_studio_debugger_structured_variables_test
    developer_studio_debugger_variables_test
    developer_studio_debugger_watches_test
    developer_studio_debugger_conditional_breakpoints_test
    PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# CMake defines NDEBUG for Release builds. These model tests use assert() for
# checks and fixture setup, so keep assertions enabled in test executables
# without changing the optimized Release configuration of the model library.
get_property(developer_studio_directory_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
if(MSVC)
    set(developer_studio_undefine_ndebug /UNDEBUG)
else()
    set(developer_studio_undefine_ndebug -UNDEBUG)
endif()
foreach(developer_studio_test_target IN LISTS developer_studio_directory_targets)
    if(developer_studio_test_target MATCHES "^developer_studio_.*_test$")
        target_compile_options(${developer_studio_test_target} PRIVATE
            ${developer_studio_undefine_ndebug})
    endif()
endforeach()

# The production Native ELF link is normally driven by build.ps1. The CMake
# target mirrors its freestanding bounds and fixed-address ELF contract, and
# can direct its output to an isolated staging directory for qualification.
option(GUIDEXOS_BUILD_NATIVE_ELF "Build the Native ELF shell with CMake" OFF)
set(GUIDEXOS_SERVER_ROOT "" CACHE PATH "guideXOS Server checkout")
set(GUIDEXOS_TARGET_ARCHITECTURE "amd64" CACHE STRING "Native ELF target architecture")
set_property(CACHE GUIDEXOS_TARGET_ARCHITECTURE PROPERTY STRINGS amd64 arm64)
set(GUIDEXOS_NATIVE_ELF_OUTPUT_DIR "" CACHE PATH
    "Optional override for the Native ELF output directory (useful for isolated staging)")

if(GUIDEXOS_TARGET_ARCHITECTURE STREQUAL "amd64")
    set(GUIDEXOS_COMPILER_TRIPLE "x86_64-unknown-elf")
    set(GUIDEXOS_LINK_MACHINE "elf_x86_64")
elseif(GUIDEXOS_TARGET_ARCHITECTURE STREQUAL "arm64")
    set(GUIDEXOS_COMPILER_TRIPLE "aarch64-none-elf")
    set(GUIDEXOS_LINK_MACHINE "aarch64elf")
    add_compile_definitions(GXOS_DEVELOPER_STUDIO_AARCH64)
else()
    message(FATAL_ERROR "GUIDEXOS_TARGET_ARCHITECTURE must be amd64 or arm64")
endif()

option(GUIDEXOS_ENABLE_TASK_LIST_HOSTED_UI_TEST
    "Enable the real hosted Server UI interaction test for the Task List" OFF)
set(GUIDEXOS_TASK_LIST_UI_SERVER_ROOT "" CACHE PATH
    "Server checkout containing guideXOSServer.experimental.exe")
set(GUIDEXOS_TASK_LIST_UI_APP_PACKAGE "" CACHE PATH
    "Isolated AMD64 Developer Studio package directory for the hosted UI test")

if(GUIDEXOS_ENABLE_TASK_LIST_HOSTED_UI_TEST)
    if(NOT WIN32)
        message(FATAL_ERROR "The hosted Task List UI test requires the Windows Server UI runtime")
    endif()
    if(NOT GUIDEXOS_TASK_LIST_UI_SERVER_ROOT OR NOT GUIDEXOS_TASK_LIST_UI_APP_PACKAGE)
        message(FATAL_ERROR
            "Set GUIDEXOS_TASK_LIST_UI_SERVER_ROOT and GUIDEXOS_TASK_LIST_UI_APP_PACKAGE to enable the hosted UI test")
    endif()
    find_program(GUIDEXOS_TASK_LIST_UI_POWERSHELL NAMES pwsh powershell)
    if(NOT GUIDEXOS_TASK_LIST_UI_POWERSHELL)
        message(FATAL_ERROR "PowerShell is required to run the hosted Task List UI test")
    endif()
    add_test(NAME developer_studio_task_list_hosted_ui_test
        COMMAND "${GUIDEXOS_TASK_LIST_UI_POWERSHELL}"
            -NoProfile
            -ExecutionPolicy Bypass
            -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/smoke-developer-studio-task-list.ps1"
            -ServerRoot "${GUIDEXOS_TASK_LIST_UI_SERVER_ROOT}"
            -AppPackageRoot "${GUIDEXOS_TASK_LIST_UI_APP_PACKAGE}"
            -EvidenceRoot "${CMAKE_CURRENT_BINARY_DIR}/tools7-hosted-ui-evidence")
    set_tests_properties(developer_studio_task_list_hosted_ui_test PROPERTIES
        LABELS "hosted-ui;integration"
        RUN_SERIAL TRUE
        TIMEOUT 300)
endif()

option(GUIDEXOS_ENABLE_DOCUMENT_OUTLINE_HOSTED_UI_TEST
    "Enable the real hosted Server UI interaction test for Document Outline" OFF)
set(GUIDEXOS_DOCUMENT_OUTLINE_UI_SERVER_ROOT "" CACHE PATH
    "Server checkout containing guideXOSServer.experimental.exe")
set(GUIDEXOS_DOCUMENT_OUTLINE_UI_APP_PACKAGE "" CACHE PATH
    "Isolated AMD64 Developer Studio package directory for the hosted UI test")

if(GUIDEXOS_ENABLE_DOCUMENT_OUTLINE_HOSTED_UI_TEST)
    if(NOT WIN32)
        message(FATAL_ERROR "The hosted Document Outline UI test requires the Windows Server UI runtime")
    endif()
    if(NOT GUIDEXOS_DOCUMENT_OUTLINE_UI_SERVER_ROOT OR NOT GUIDEXOS_DOCUMENT_OUTLINE_UI_APP_PACKAGE)
        message(FATAL_ERROR
            "Set GUIDEXOS_DOCUMENT_OUTLINE_UI_SERVER_ROOT and GUIDEXOS_DOCUMENT_OUTLINE_UI_APP_PACKAGE to enable the hosted UI test")
    endif()
    find_program(GUIDEXOS_DOCUMENT_OUTLINE_UI_POWERSHELL NAMES pwsh powershell)
    if(NOT GUIDEXOS_DOCUMENT_OUTLINE_UI_POWERSHELL)
        message(FATAL_ERROR "PowerShell is required to run the hosted Document Outline UI test")
    endif()
    add_test(NAME developer_studio_document_outline_hosted_ui_test
        COMMAND "${GUIDEXOS_DOCUMENT_OUTLINE_UI_POWERSHELL}"
            -NoProfile
            -ExecutionPolicy Bypass
            -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/smoke-developer-studio-document-outline.ps1"
            -ServerRoot "${GUIDEXOS_DOCUMENT_OUTLINE_UI_SERVER_ROOT}"
            -AppPackageRoot "${GUIDEXOS_DOCUMENT_OUTLINE_UI_APP_PACKAGE}"
            -EvidenceRoot "${CMAKE_CURRENT_BINARY_DIR}/tools9-hosted-ui-evidence")
    set_tests_properties(developer_studio_document_outline_hosted_ui_test PROPERTIES
        LABELS "hosted-ui;integration"
        RUN_SERIAL TRUE
        TIMEOUT 300)
endif()

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
        src/developer_studio_cpp_lexer.cpp
        src/developer_studio_task_list.cpp
        src/developer_studio_document_outline.cpp
        src/developer_studio_replace_preview.cpp
        src/developer_studio_symbols.cpp
        src/developer_studio_navigation.cpp
        src/developer_studio_references.cpp
        src/developer_studio_rename.cpp
        src/developer_studio_completion.cpp
        src/developer_studio_signature.cpp
        src/developer_studio_include_graph.cpp
        src/developer_studio_relationships.cpp
        src/developer_studio_ownership.cpp
        src/developer_studio_types.cpp
        src/developer_studio_debugger.cpp
        src/developer_studio_debugger_stack.cpp
        src/developer_studio_debug_symbols.cpp
        src/developer_studio_debug_variables.cpp
        src/developer_studio_debug_watches.cpp
        src/developer_studio_debugger_hosted.cpp
        src/developer_studio_debugger_workspace.cpp
        src/developer_studio_startup.cpp
        src/freestanding_memory.cpp
        src/main.cpp
    )
    target_include_directories(developerstudio.elf PRIVATE
        src
        "${GUIDEXOS_SERVER_ROOT}/sdk/include"
    )
    target_compile_definitions(developerstudio.elf PRIVATE GXOS_DEVELOPER_STUDIO_BARE_METAL)
    target_compile_options(developerstudio.elf PRIVATE
        --target=${GUIDEXOS_COMPILER_TRIPLE}
        -ffreestanding
        $<$<STREQUAL:${GUIDEXOS_TARGET_ARCHITECTURE},amd64>:-mno-red-zone>
        -fno-exceptions
        -fno-rtti
        -fno-stack-protector
        -fno-unwind-tables
        -fno-asynchronous-unwind-tables
    )
    target_link_options(developerstudio.elf PRIVATE
        --target=${GUIDEXOS_COMPILER_TRIPLE}
        -nostdlib
        -static
        -fuse-ld=lld
        -Wl,-e,gx_main
        -Wl,-m,${GUIDEXOS_LINK_MACHINE}
        -Wl,--image-base=0x50000000
        -Wl,-z,max-page-size=0x1000
    )
    if(GUIDEXOS_NATIVE_ELF_OUTPUT_DIR)
        set(_developer_studio_native_output_dir "${GUIDEXOS_NATIVE_ELF_OUTPUT_DIR}")
    else()
        set(_developer_studio_native_output_dir
            "${GUIDEXOS_SERVER_ROOT}/Apps/DeveloperStudio/bin/${GUIDEXOS_TARGET_ARCHITECTURE}")
    endif()
    set_target_properties(developerstudio.elf PROPERTIES
        OUTPUT_NAME developerstudio
        SUFFIX .elf
        RUNTIME_OUTPUT_DIRECTORY "${_developer_studio_native_output_dir}"
    )
endif()
