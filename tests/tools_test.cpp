#include "developer_studio_tools.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <iostream>

using namespace guidexos::developer_studio;

struct ToolLifecycle {
    uint32_t activateCount;
    uint32_t closeCount;
    bool activationSucceeds;
    bool closeSucceeds;
};

static bool activateFindInFiles(void* userData) {
    ToolLifecycle* lifecycle = static_cast<ToolLifecycle*>(userData);
    ++lifecycle->activateCount;
    return lifecycle->activationSucceeds;
}

static bool closeFindInFiles(void* userData) {
    ToolLifecycle* lifecycle = static_cast<ToolLifecycle*>(userData);
    ++lifecycle->closeCount;
    return lifecycle->closeSucceeds;
}

int main() {
    DeveloperToolRegistry registry = {};
    DeveloperToolRegistryInit(&registry);
    ToolLifecycle lifecycle = { 0, 0, true, true };
    const DeveloperToolCommand findInFiles = {
        DeveloperToolId::FindInFiles, "Find in Files", "Ctrl+Shift+F",
        activateFindInFiles, closeFindInFiles
    };
    const DeveloperToolCommand goToFile = {
        DeveloperToolId::GoToFile, "Go to File", "Ctrl+P",
        activateFindInFiles, closeFindInFiles
    };
    const DeveloperToolCommand goToLine = {
        DeveloperToolId::GoToLine, "Go to Line", "Ctrl+G",
        activateFindInFiles, closeFindInFiles
    };
    const DeveloperToolCommand replaceInFiles = {
        DeveloperToolId::ReplaceInFiles, "Replace in Files", "Ctrl+Shift+H",
        activateFindInFiles, closeFindInFiles
    };

    assert(DeveloperToolRegistryRegister(&registry, &goToFile));
    assert(DeveloperToolRegistryRegister(&registry, &goToLine));
    assert(DeveloperToolRegistryRegister(&registry, &findInFiles));
    assert(DeveloperToolRegistryRegister(&registry, &replaceInFiles));
    assert(!DeveloperToolRegistryRegister(&registry, &findInFiles));
    assert(DeveloperToolRegistryAt(&registry, 0) != nullptr);
    assert(DeveloperToolRegistryAt(&registry, 3)->id == DeveloperToolId::ReplaceInFiles);
    assert(DeveloperToolRegistryAt(&registry, 4) == nullptr);
    assert(DeveloperToolRegistryAt(&registry, 0)->id == DeveloperToolId::GoToFile);
    assert(DeveloperToolRegistryAt(&registry, 1)->id == DeveloperToolId::GoToLine);
    assert(DeveloperToolRegistryAt(&registry, 2)->id == DeveloperToolId::FindInFiles);
    assert(std::strcmp(DeveloperToolRegistryAt(&registry, 0)->shortcut, "Ctrl+P") == 0);
    assert(std::strcmp(DeveloperToolRegistryAt(&registry, 1)->shortcut, "Ctrl+G") == 0);
    assert(DeveloperToolIdFromShortcut(80, true, false, false) == DeveloperToolId::GoToFile);
    assert(DeveloperToolIdFromShortcut(112, true, false, false) == DeveloperToolId::GoToFile);
    assert(DeveloperToolIdFromShortcut(71, true, false, false) == DeveloperToolId::GoToLine);
    assert(DeveloperToolIdFromShortcut(103, true, false, false) == DeveloperToolId::GoToLine);
    assert(DeveloperToolIdFromShortcut(70, true, true, false) == DeveloperToolId::FindInFiles);
    assert(DeveloperToolIdFromShortcut(72, true, true, false) == DeveloperToolId::ReplaceInFiles);
    assert(DeveloperToolIdFromShortcut(72, true, true, true) == DeveloperToolId::None);
    assert(DeveloperToolIdFromShortcut(80, false, false, false) == DeveloperToolId::None);
    assert(DeveloperToolIdFromShortcut(80, true, true, false) == DeveloperToolId::None);
    assert(DeveloperToolIdFromShortcut(71, true, false, true) == DeveloperToolId::None);
    const DeveloperToolCommand* registered =
        DeveloperToolRegistryFind(&registry, DeveloperToolId::FindInFiles);
    assert(registered && std::strcmp(registered->label, "Find in Files") == 0);
    assert(std::strcmp(registered->shortcut, "Ctrl+Shift+F") == 0);

    // The Tools menu and Ctrl+Shift+F both dispatch through this same command
    // activation path. Re-activation preserves an already-open utility.
    assert(DeveloperToolRegistryActivate(&registry, DeveloperToolId::FindInFiles, &lifecycle));
    assert(DeveloperToolRegistryActivate(&registry, DeveloperToolId::FindInFiles, &lifecycle));
    assert(lifecycle.activateCount == 2);
    assert(DeveloperToolRegistryIsActive(&registry, DeveloperToolId::FindInFiles));

    lifecycle.closeSucceeds = false;
    assert(!DeveloperToolRegistryClose(&registry, DeveloperToolId::FindInFiles, &lifecycle));
    assert(DeveloperToolRegistryIsActive(&registry, DeveloperToolId::FindInFiles));
    lifecycle.closeSucceeds = true;
    assert(DeveloperToolRegistryCloseActive(&registry, &lifecycle));
    assert(!DeveloperToolRegistryIsActive(&registry, DeveloperToolId::FindInFiles));
    const uint32_t closesAfterActive = lifecycle.closeCount;
    assert(DeveloperToolRegistryClose(&registry, DeveloperToolId::FindInFiles, &lifecycle));
    assert(lifecycle.closeCount == closesAfterActive);

    lifecycle.activationSucceeds = false;
    assert(!DeveloperToolRegistryActivate(&registry, DeveloperToolId::FindInFiles, &lifecycle));
    assert(!DeveloperToolRegistryIsActive(&registry, DeveloperToolId::FindInFiles));

    const char* filePaths[] = {
        "C:/project/src/main.cpp",
        "C:/project/tests/main_test.cpp",
        "C:/project/src/nested/main.h",
        "C:/project/src/utility.cpp",
        "C:\\project\\src\\main.cpp",
        "C:/elsewhere/main.cpp",
        "C:/project/../outside.cpp",
        "C:/project/include/src_only.h"
    };
    DeveloperToolFileSearchResults fileResults = {};
    assert(DeveloperToolBuildFileSearchResults("C:/project", filePaths,
                                               sizeof(filePaths) / sizeof(filePaths[0]),
                                               "MAIN", &fileResults));
    assert(fileResults.count == 3);
    assert(fileResults.totalMatches == 3);
    assert(!fileResults.truncated);
    assert(std::strcmp(fileResults.results[0].relativePath, "src/main.cpp") == 0);
    assert(std::strcmp(fileResults.results[1].relativePath, "src/nested/main.h") == 0);
    assert(std::strcmp(fileResults.results[2].relativePath, "tests/main_test.cpp") == 0);
    assert(fileResults.results[0].fileNameMatch);

    assert(DeveloperToolBuildFileSearchResults("C:/project", filePaths,
                                               sizeof(filePaths) / sizeof(filePaths[0]),
                                               "include", &fileResults));
    assert(fileResults.count == 1);
    assert(std::strcmp(fileResults.results[0].relativePath, "include/src_only.h") == 0);
    assert(!fileResults.results[0].fileNameMatch);

    const char* filenameFirstPaths[] = {
        "C:/project/z/src-folder/document.txt",
        "C:/project/a/src.txt",
        "C:/project/a/src.txt"
    };
    assert(DeveloperToolBuildFileSearchResults("C:/project", filenameFirstPaths, 3,
                                               "src", &fileResults));
    assert(fileResults.count == 2 && fileResults.totalMatches == 2);
    assert(fileResults.results[0].fileNameMatch);
    assert(std::strcmp(fileResults.results[0].relativePath, "a/src.txt") == 0);
    assert(!fileResults.results[1].fileNameMatch);
    assert(std::strcmp(fileResults.results[1].relativePath, "z/src-folder/document.txt") == 0);

    assert(DeveloperToolBuildFileSearchResults("C:/project", filePaths,
                                               sizeof(filePaths) / sizeof(filePaths[0]),
                                               "not-present", &fileResults));
    assert(fileResults.count == 0 && fileResults.totalMatches == 0);

    char oversizedQuery[kDeveloperToolMaxFileQueryBytes + 2] = {};
    std::memset(oversizedQuery, 'x', sizeof(oversizedQuery) - 1);
    assert(!DeveloperToolBuildFileSearchResults("C:/project", filePaths,
                                                sizeof(filePaths) / sizeof(filePaths[0]),
                                                oversizedQuery, &fileResults));
    assert(fileResults.count == 0);

    char resultPaths[80][64] = {};
    const char* resultPathPointers[80] = {};
    for (uint32_t i = 0; i < 80; ++i) {
        std::snprintf(resultPaths[i], sizeof(resultPaths[i]), "C:/project/src/file%03u.cpp", i);
        resultPathPointers[i] = resultPaths[i];
    }
    assert(DeveloperToolBuildFileSearchResults("C:/project", resultPathPointers, 80, "",
                                               &fileResults));
    assert(fileResults.count == kDeveloperToolMaxFileResults);
    assert(fileResults.totalMatches == 80);
    assert(fileResults.truncated);
    assert(std::strcmp(fileResults.results[0].relativePath, "src/file000.cpp") == 0);

    assert(DeveloperToolProjectIdentityMatches("com.example.app", 7,
                                               "COM.EXAMPLE.APP", 7));
    assert(!DeveloperToolProjectIdentityMatches("com.example.app", 7,
                                                "com.other.app", 7));
    assert(!DeveloperToolProjectIdentityMatches("com.example.app", 7,
                                                "com.example.app", 8));

    uint32_t line = 0;
    assert(DeveloperToolParsePositiveLineNumber("1", &line) && line == 1);
    assert(DeveloperToolParsePositiveLineNumber("4294967295", &line) && line == 0xFFFFFFFFu);
    assert(!DeveloperToolParsePositiveLineNumber("", &line));
    assert(!DeveloperToolParsePositiveLineNumber("0", &line));
    assert(!DeveloperToolParsePositiveLineNumber("-1", &line));
    assert(!DeveloperToolParsePositiveLineNumber("12x", &line));
    assert(!DeveloperToolParsePositiveLineNumber("4294967296", &line));
    assert(!DeveloperToolParsePositiveLineNumber("12345678901", &line));

    const char lfText[] = "first\nmiddle\nlast";
    uint32_t caret = 99;
    uint32_t anchor = 44;
    bool selectionActive = true;
    assert(DeveloperToolPositionAtLine(lfText, sizeof(lfText) - 1, 1,
                                       &caret, &anchor, &selectionActive));
    assert(caret == 0 && anchor == 0 && !selectionActive);
    caret = 4; anchor = 2; selectionActive = true;
    assert(DeveloperToolPositionAtLine(lfText, sizeof(lfText) - 1, 2,
                                       &caret, &anchor, &selectionActive));
    assert(caret == 6 && anchor == 6 && !selectionActive);
    assert(DeveloperToolPositionAtLine(lfText, sizeof(lfText) - 1, 3,
                                       &caret, &anchor, &selectionActive));
    assert(caret == 13);
    caret = 55; anchor = 22; selectionActive = true;
    assert(!DeveloperToolPositionAtLine(lfText, sizeof(lfText) - 1, 4,
                                        &caret, &anchor, &selectionActive));
    assert(caret == 55 && anchor == 22 && selectionActive);

    const char crlfText[] = "first\r\nmiddle\r\nlast";
    assert(DeveloperToolPositionAtLine(crlfText, sizeof(crlfText) - 1, 2,
                                       &caret, &anchor, &selectionActive));
    assert(caret == 7 && anchor == 7 && !selectionActive);
    assert(DeveloperToolPositionAtLine(crlfText, sizeof(crlfText) - 1, 3,
                                       &caret, &anchor, &selectionActive));
    assert(caret == 15);
    assert(DeveloperToolPositionAtLine(nullptr, 0, 1,
                                       &caret, &anchor, &selectionActive));
    assert(caret == 0 && anchor == 0 && !selectionActive);
    std::cout << "Developer Studio static tools registry PASS\n";
    return 0;
}
