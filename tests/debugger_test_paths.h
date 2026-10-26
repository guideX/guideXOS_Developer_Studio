#ifndef GUIDEXOS_DEVELOPER_STUDIO_DEBUGGER_TEST_PATHS_H
#define GUIDEXOS_DEVELOPER_STUDIO_DEBUGGER_TEST_PATHS_H

#include <string>

#if defined(_WIN32)
#include <direct.h>
#else
#include <unistd.h>
#endif

inline std::string debuggerTestFixtureRoot(const char* fixtureName) {
    char currentDirectory[4096] = {};
#if defined(_WIN32)
    if (!_getcwd(currentDirectory, sizeof(currentDirectory))) return std::string();
#else
    if (!getcwd(currentDirectory, sizeof(currentDirectory))) return std::string();
#endif
    return std::string(currentDirectory) + "/tests/fixtures/" + fixtureName;
}

#endif
