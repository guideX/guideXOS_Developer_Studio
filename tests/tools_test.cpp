#include "developer_studio_tools.h"
#include "test_check.h"

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
    const DeveloperToolCommand taskList = {
        DeveloperToolId::TaskList, "Task List", "",
        activateFindInFiles, closeFindInFiles
    };
    const DeveloperToolCommand documentOutline = {
        DeveloperToolId::DocumentOutline, "Document Outline", "",
        activateFindInFiles, closeFindInFiles
    };
    const DeveloperToolCommand projectSymbols = {
        DeveloperToolId::ProjectSymbols, "Project Symbols", "",
        activateFindInFiles, closeFindInFiles
    };

    TEST_CHECK(DeveloperToolRegistryRegister(&registry, &goToFile));
    TEST_CHECK(DeveloperToolRegistryRegister(&registry, &goToLine));
    TEST_CHECK(DeveloperToolRegistryRegister(&registry, &findInFiles));
    TEST_CHECK(DeveloperToolRegistryRegister(&registry, &replaceInFiles));
    TEST_CHECK(DeveloperToolRegistryRegister(&registry, &taskList));
    TEST_CHECK(DeveloperToolRegistryRegister(&registry, &documentOutline));
    TEST_CHECK(DeveloperToolRegistryRegister(&registry, &projectSymbols));
    TEST_CHECK(!DeveloperToolRegistryRegister(&registry, &findInFiles));
    TEST_CHECK(DeveloperToolRegistryAt(&registry, 0) != nullptr);
    TEST_CHECK(DeveloperToolRegistryAt(&registry, 3)->id == DeveloperToolId::ReplaceInFiles);
    TEST_CHECK(DeveloperToolRegistryAt(&registry, 4)->id == DeveloperToolId::TaskList);
    TEST_CHECK(DeveloperToolRegistryAt(&registry, 5)->id == DeveloperToolId::DocumentOutline);
    TEST_CHECK(DeveloperToolRegistryAt(&registry, 6)->id == DeveloperToolId::ProjectSymbols);
    TEST_CHECK(DeveloperToolRegistryAt(&registry, 7) == nullptr);
    TEST_CHECK(DeveloperToolRegistryAt(&registry, 0)->id == DeveloperToolId::GoToFile);
    TEST_CHECK(DeveloperToolRegistryAt(&registry, 1)->id == DeveloperToolId::GoToLine);
    TEST_CHECK(DeveloperToolRegistryAt(&registry, 2)->id == DeveloperToolId::FindInFiles);
    TEST_CHECK(std::strcmp(DeveloperToolRegistryAt(&registry, 0)->shortcut, "Ctrl+P") == 0);
    TEST_CHECK(std::strcmp(DeveloperToolRegistryAt(&registry, 1)->shortcut, "Ctrl+G") == 0);
    TEST_CHECK(DeveloperToolIdFromShortcut(80, true, false, false) == DeveloperToolId::GoToFile);
    TEST_CHECK(DeveloperToolIdFromShortcut(112, true, false, false) == DeveloperToolId::GoToFile);
    TEST_CHECK(DeveloperToolIdFromShortcut(71, true, false, false) == DeveloperToolId::GoToLine);
    TEST_CHECK(DeveloperToolIdFromShortcut(103, true, false, false) == DeveloperToolId::GoToLine);
    TEST_CHECK(DeveloperToolIdFromShortcut(70, true, true, false) == DeveloperToolId::FindInFiles);
    TEST_CHECK(DeveloperToolIdFromShortcut(72, true, true, false) == DeveloperToolId::ReplaceInFiles);
    TEST_CHECK(DeveloperToolIdFromShortcut(72, true, true, true) == DeveloperToolId::None);
    TEST_CHECK(DeveloperToolIdFromShortcut(80, false, false, false) == DeveloperToolId::None);
    TEST_CHECK(DeveloperToolIdFromShortcut(80, true, true, false) == DeveloperToolId::None);
    TEST_CHECK(DeveloperToolIdFromShortcut(71, true, false, true) == DeveloperToolId::None);
    const DeveloperToolCommand* registered =
        DeveloperToolRegistryFind(&registry, DeveloperToolId::FindInFiles);
    TEST_CHECK(registered && std::strcmp(registered->label, "Find in Files") == 0);
    TEST_CHECK(std::strcmp(registered->shortcut, "Ctrl+Shift+F") == 0);

    // The Tools menu and Ctrl+Shift+F both dispatch through this same command
    // activation path. Re-activation preserves an already-open utility.
    TEST_CHECK(DeveloperToolRegistryActivate(&registry, DeveloperToolId::FindInFiles, &lifecycle));
    TEST_CHECK(DeveloperToolRegistryActivate(&registry, DeveloperToolId::FindInFiles, &lifecycle));
    TEST_CHECK(lifecycle.activateCount == 2);
    TEST_CHECK(DeveloperToolRegistryIsActive(&registry, DeveloperToolId::FindInFiles));

    lifecycle.closeSucceeds = false;
    TEST_CHECK(!DeveloperToolRegistryClose(&registry, DeveloperToolId::FindInFiles, &lifecycle));
    TEST_CHECK(DeveloperToolRegistryIsActive(&registry, DeveloperToolId::FindInFiles));
    lifecycle.closeSucceeds = true;
    TEST_CHECK(DeveloperToolRegistryCloseActive(&registry, &lifecycle));
    TEST_CHECK(!DeveloperToolRegistryIsActive(&registry, DeveloperToolId::FindInFiles));
    const uint32_t closesAfterActive = lifecycle.closeCount;
    TEST_CHECK(DeveloperToolRegistryClose(&registry, DeveloperToolId::FindInFiles, &lifecycle));
    TEST_CHECK(lifecycle.closeCount == closesAfterActive);

    lifecycle.activationSucceeds = false;
    TEST_CHECK(!DeveloperToolRegistryActivate(&registry, DeveloperToolId::FindInFiles, &lifecycle));
    TEST_CHECK(!DeveloperToolRegistryIsActive(&registry, DeveloperToolId::FindInFiles));

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
    TEST_CHECK(DeveloperToolBuildFileSearchResults("C:/project", filePaths,
                                               sizeof(filePaths) / sizeof(filePaths[0]),
                                               "MAIN", &fileResults));
    TEST_CHECK(fileResults.count == 3);
    TEST_CHECK(fileResults.totalMatches == 3);
    TEST_CHECK(!fileResults.truncated);
    TEST_CHECK(std::strcmp(fileResults.results[0].relativePath, "src/main.cpp") == 0);
    TEST_CHECK(std::strcmp(fileResults.results[1].relativePath, "src/nested/main.h") == 0);
    TEST_CHECK(std::strcmp(fileResults.results[2].relativePath, "tests/main_test.cpp") == 0);
    TEST_CHECK(fileResults.results[0].fileNameMatch);

    TEST_CHECK(DeveloperToolBuildFileSearchResults("C:/project", filePaths,
                                               sizeof(filePaths) / sizeof(filePaths[0]),
                                               "include", &fileResults));
    TEST_CHECK(fileResults.count == 1);
    TEST_CHECK(std::strcmp(fileResults.results[0].relativePath, "include/src_only.h") == 0);
    TEST_CHECK(!fileResults.results[0].fileNameMatch);

    const char* filenameFirstPaths[] = {
        "C:/project/z/src-folder/document.txt",
        "C:/project/a/src.txt",
        "C:/project/a/src.txt"
    };
    TEST_CHECK(DeveloperToolBuildFileSearchResults("C:/project", filenameFirstPaths, 3,
                                               "src", &fileResults));
    TEST_CHECK(fileResults.count == 2 && fileResults.totalMatches == 2);
    TEST_CHECK(fileResults.results[0].fileNameMatch);
    TEST_CHECK(std::strcmp(fileResults.results[0].relativePath, "a/src.txt") == 0);
    TEST_CHECK(!fileResults.results[1].fileNameMatch);
    TEST_CHECK(std::strcmp(fileResults.results[1].relativePath, "z/src-folder/document.txt") == 0);

    TEST_CHECK(DeveloperToolBuildFileSearchResults("C:/project", filePaths,
                                               sizeof(filePaths) / sizeof(filePaths[0]),
                                               "not-present", &fileResults));
    TEST_CHECK(fileResults.count == 0 && fileResults.totalMatches == 0);

    char oversizedQuery[kDeveloperToolMaxFileQueryBytes + 2] = {};
    std::memset(oversizedQuery, 'x', sizeof(oversizedQuery) - 1);
    TEST_CHECK(!DeveloperToolBuildFileSearchResults("C:/project", filePaths,
                                                sizeof(filePaths) / sizeof(filePaths[0]),
                                                oversizedQuery, &fileResults));
    TEST_CHECK(fileResults.count == 0);

    char resultPaths[80][64] = {};
    const char* resultPathPointers[80] = {};
    for (uint32_t i = 0; i < 80; ++i) {
        std::snprintf(resultPaths[i], sizeof(resultPaths[i]), "C:/project/src/file%03u.cpp", i);
        resultPathPointers[i] = resultPaths[i];
    }
    TEST_CHECK(DeveloperToolBuildFileSearchResults("C:/project", resultPathPointers, 80, "",
                                               &fileResults));
    TEST_CHECK(fileResults.count == kDeveloperToolMaxFileResults);
    TEST_CHECK(fileResults.totalMatches == 80);
    TEST_CHECK(fileResults.truncated);
    TEST_CHECK(std::strcmp(fileResults.results[0].relativePath, "src/file000.cpp") == 0);

    TEST_CHECK(DeveloperToolProjectIdentityMatches("com.example.app", 7,
                                               "COM.EXAMPLE.APP", 7));
    TEST_CHECK(!DeveloperToolProjectIdentityMatches("com.example.app", 7,
                                                "com.other.app", 7));
    TEST_CHECK(!DeveloperToolProjectIdentityMatches("com.example.app", 7,
                                                "com.example.app", 8));

    uint32_t line = 0;
    TEST_CHECK(DeveloperToolParsePositiveLineNumber("1", &line) && line == 1);
    TEST_CHECK(DeveloperToolParsePositiveLineNumber("4294967295", &line) && line == 0xFFFFFFFFu);
    TEST_CHECK(!DeveloperToolParsePositiveLineNumber("", &line));
    TEST_CHECK(!DeveloperToolParsePositiveLineNumber("0", &line));
    TEST_CHECK(!DeveloperToolParsePositiveLineNumber("-1", &line));
    TEST_CHECK(!DeveloperToolParsePositiveLineNumber("12x", &line));
    TEST_CHECK(!DeveloperToolParsePositiveLineNumber("4294967296", &line));
    TEST_CHECK(!DeveloperToolParsePositiveLineNumber("12345678901", &line));

    const char lfText[] = "first\nmiddle\nlast";
    uint32_t caret = 99;
    uint32_t anchor = 44;
    bool selectionActive = true;
    TEST_CHECK(DeveloperToolPositionAtLine(lfText, sizeof(lfText) - 1, 1,
                                       &caret, &anchor, &selectionActive));
    TEST_CHECK(caret == 0 && anchor == 0 && !selectionActive);
    caret = 4; anchor = 2; selectionActive = true;
    TEST_CHECK(DeveloperToolPositionAtLine(lfText, sizeof(lfText) - 1, 2,
                                       &caret, &anchor, &selectionActive));
    TEST_CHECK(caret == 6 && anchor == 6 && !selectionActive);
    TEST_CHECK(DeveloperToolPositionAtLine(lfText, sizeof(lfText) - 1, 3,
                                       &caret, &anchor, &selectionActive));
    TEST_CHECK(caret == 13);
    caret = 55; anchor = 22; selectionActive = true;
    TEST_CHECK(!DeveloperToolPositionAtLine(lfText, sizeof(lfText) - 1, 4,
                                        &caret, &anchor, &selectionActive));
    TEST_CHECK(caret == 55 && anchor == 22 && selectionActive);

    const char crlfText[] = "first\r\nmiddle\r\nlast";
    TEST_CHECK(DeveloperToolPositionAtLine(crlfText, sizeof(crlfText) - 1, 2,
                                       &caret, &anchor, &selectionActive));
    TEST_CHECK(caret == 7 && anchor == 7 && !selectionActive);
    TEST_CHECK(DeveloperToolPositionAtLine(crlfText, sizeof(crlfText) - 1, 3,
                                       &caret, &anchor, &selectionActive));
    TEST_CHECK(caret == 15);
    TEST_CHECK(DeveloperToolPositionAtLine(nullptr, 0, 1,
                                       &caret, &anchor, &selectionActive));
    TEST_CHECK(caret == 0 && anchor == 0 && !selectionActive);
    std::cout << "Developer Studio static tools registry PASS\n";
    return 0;
}
