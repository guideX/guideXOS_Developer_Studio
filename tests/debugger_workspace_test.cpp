#include "developer_studio_debugger_workspace.h"

#include <cassert>
#include <cstring>
#include <iostream>
#include <string>

using namespace guidexos::developer_studio;

static void assertRejected(const std::string& json, DebuggerWorkspaceErrorCode expected) {
    DebuggerWorkspace output = {};
    output.breakpointCount = 99;
    output.watchCount = 99;
    DebuggerWorkspaceErrorCode error = DebuggerWorkspaceErrorCode::None;
    assert(!ParseDebuggerWorkspace(json.c_str(), static_cast<uint32_t>(json.size()), &output, &error));
    assert(error == expected);
    assert(output.breakpointCount == 0);
    assert(output.watchCount == 0);
    assert(output.breakpoints[0].sourcePath[0] == '\0');
    assert(output.watches[0][0] == '\0');
}

struct FakeFile {
    char path[kMaxPathBytes];
    char bytes[kDebuggerWorkspaceMaxFileBytes + 1];
    uint32_t size;
    bool used;
};

struct FakeFileSystem {
    FakeFile files[8];
    char writes[4][kMaxPathBytes];
    uint32_t writeCount;
    const char* failWritePath;
    const char* shortWritePath;
    const char* failReadPath;
};

static FakeFile* fakeFile(FakeFileSystem* fileSystem, const char* path, bool create) {
    for (uint32_t index = 0; index < 8; ++index) {
        if (fileSystem->files[index].used && std::strcmp(fileSystem->files[index].path, path) == 0)
            return &fileSystem->files[index];
    }
    if (!create) return nullptr;
    for (uint32_t index = 0; index < 8; ++index) {
        if (!fileSystem->files[index].used) {
            fileSystem->files[index].used = true;
            std::strcpy(fileSystem->files[index].path, path);
            return &fileSystem->files[index];
        }
    }
    return nullptr;
}

static bool fakeStat(void* userData, const char* path, FileInfo* outInfo) {
    FakeFile* file = fakeFile(static_cast<FakeFileSystem*>(userData), path, false);
    if (!file || !outInfo) return false;
    outInfo->kind = FileInfoKind::RegularFile;
    outInfo->size = file->size;
    return true;
}

static bool fakeRead(void* userData, const char* path, char* buffer, uint32_t capacity, uint32_t* outBytes) {
    FakeFileSystem* fileSystem = static_cast<FakeFileSystem*>(userData);
    FakeFile* file = fakeFile(fileSystem, path, false);
    if (!file || !buffer || !outBytes || file->size > capacity ||
        (fileSystem->failReadPath && std::strcmp(fileSystem->failReadPath, path) == 0)) return false;
    std::memcpy(buffer, file->bytes, file->size);
    *outBytes = file->size;
    return true;
}

static bool fakeWrite(void* userData, const char* path, const char* buffer, uint32_t bytes, uint32_t* outBytes) {
    FakeFileSystem* fileSystem = static_cast<FakeFileSystem*>(userData);
    if (!buffer || !outBytes || bytes > kDebuggerWorkspaceMaxFileBytes ||
        (fileSystem->failWritePath && std::strcmp(fileSystem->failWritePath, path) == 0)) return false;
    FakeFile* file = fakeFile(fileSystem, path, true);
    if (!file || fileSystem->writeCount >= 4) return false;
    std::strcpy(fileSystem->writes[fileSystem->writeCount++], path);
    std::memcpy(file->bytes, buffer, bytes);
    file->size = bytes;
    *outBytes = fileSystem->shortWritePath && std::strcmp(fileSystem->shortWritePath, path) == 0
        ? bytes - 1 : bytes;
    return true;
}

static WorkspaceFileSystem fakeWorkspaceFileSystem(FakeFileSystem* fileSystem) {
    WorkspaceFileSystem result = {};
    result.userData = fileSystem;
    result.stat = fakeStat;
    result.read = fakeRead;
    result.write = fakeWrite;
    return result;
}

static void fakeSetFile(FakeFileSystem* fileSystem, const char* path, const char* bytes) {
    FakeFile* file = fakeFile(fileSystem, path, true);
    assert(file);
    file->size = static_cast<uint32_t>(std::strlen(bytes));
    std::memcpy(file->bytes, bytes, file->size);
}

static void assertStorageBehavior(const DebuggerWorkspace& workspace, const char* serialized,
                                  uint32_t serializedBytes) {
    FakeFileSystem fake = {};
    WorkspaceFileSystem fileSystem = fakeWorkspaceFileSystem(&fake);
    DebuggerWorkspace stored = workspace;
    char status[96] = {};
    assert(DebuggerWorkspaceStorageSave(fileSystem, "D:/projects/a", "project-a", &stored,
                                        status, sizeof(status)));
    assert(std::strcmp(fake.writes[0], "d:/projects/a/guidexos.debugger.json.bak") == 0);
    assert(std::strcmp(fake.writes[1], "d:/projects/a/guidexos.debugger.json") == 0);

    DebuggerWorkspace beforeFailure = stored;
    fake.failWritePath = "d:/projects/a/guidexos.debugger.json";
    assert(!DebuggerWorkspaceStorageSave(fileSystem, "D:/projects/a", "project-a", &stored,
                                         status, sizeof(status)));
    assert(std::memcmp(&stored, &beforeFailure, sizeof(stored)) == 0);
    assert(std::strcmp(status, "save callback failure") == 0);

    fake.failWritePath = nullptr;
    fake.shortWritePath = "d:/projects/a/guidexos.debugger.json.bak";
    assert(!DebuggerWorkspaceStorageSave(fileSystem, "D:/projects/a", "project-a", &stored,
                                         status, sizeof(status)));
    assert(std::memcmp(&stored, &beforeFailure, sizeof(stored)) == 0);
    assert(std::strcmp(status, "save short write") == 0);

    fakeSetFile(&fake, "d:/projects/a/guidexos.debugger.json", "{");
    fakeSetFile(&fake, "d:/projects/a/guidexos.debugger.json.bak", serialized);
    DebuggerWorkspace loaded = {};
    assert(DebuggerWorkspaceStorageLoad(fileSystem, "D:/projects/a", "project-a", &loaded,
                                        status, sizeof(status)));
    assert(loaded.breakpointCount == workspace.breakpointCount);
    assert(loaded.watchCount == workspace.watchCount);

    fakeSetFile(&fake, "d:/projects/a/guidexos.debugger.json", serialized);
    fake.failReadPath = "d:/projects/a/guidexos.debugger.json";
    DebuggerWorkspace callbackFailure = {};
    assert(!DebuggerWorkspaceStorageLoad(fileSystem, "D:/projects/a", "project-a", &callbackFailure,
                                         status, sizeof(status)));
    assert(callbackFailure.breakpointCount == 0 && callbackFailure.watchCount == 0);
    fake.failReadPath = nullptr;

    fakeSetFile(&fake, "d:/projects/b/guidexos.debugger.json", serialized);
    FakeFile* primary = fakeFile(&fake, "d:/projects/a/guidexos.debugger.json", false);
    assert(primary);
    primary->used = false;
    FakeFile* backup = fakeFile(&fake, "d:/projects/a/guidexos.debugger.json.bak", false);
    assert(backup);
    backup->used = false;
    DebuggerWorkspace isolated = {};
    assert(!DebuggerWorkspaceStorageLoad(fileSystem, "D:/projects/a", "project-a", &isolated,
                                         status, sizeof(status)));
    assert(isolated.breakpointCount == 0 && isolated.watchCount == 0);
}

static void assertMaterializationPlan() {
    DebuggerWorkspace workspace = {};
    assert(DebuggerWorkspaceAddBreakpoint(&workspace, "src/z.cpp", 13, 2, true,
                                          1, 2, 3, "value > 0", "value={value}"));
    assert(DebuggerWorkspaceAddBreakpoint(&workspace, "src/disabled.cpp", 4, 1, false,
                                          0, 1, 7, "disabled", ""));
    assert(DebuggerWorkspaceAddBreakpoint(&workspace, "src/a.cpp", 99, 3, true,
                                          0, 1, 5, "input == 2", ""));
    assert(DebuggerWorkspaceAddBreakpoint(&workspace, "src/unconditional.cpp", 7, 1, true,
                                          0, 0, 0, "", ""));
    assert(DebuggerWorkspaceAddWatch(&workspace, "input + delta"));
    const DebuggerWorkspace before = workspace;
    DebuggerWorkspaceMaterializationEntry plan[kDebuggerWorkspaceMaxBreakpoints] = {};
    assert(BuildDebuggerWorkspaceMaterializationPlan(workspace, plan,
                                                     kDebuggerWorkspaceMaxBreakpoints) == 3);
    assert(std::strcmp(plan[0].sourcePath, "src/z.cpp") == 0);
    assert(plan[0].line == 13 && plan[0].column == 2);
    assert(plan[0].action == 1 && plan[0].hitPolicy == 2 && plan[0].hitThreshold == 3);
    assert(std::strcmp(plan[0].condition, "value > 0") == 0);
    assert(std::strcmp(plan[0].logTemplate, "value={value}") == 0);
    assert(std::strcmp(plan[1].sourcePath, "src/a.cpp") == 0);
    assert(plan[1].line == 99 && plan[1].column == 3);
    assert(plan[1].action == 0 && plan[1].hitPolicy == 1 && plan[1].hitThreshold == 5);
    assert(std::strcmp(plan[1].condition, "input == 2") == 0);
    assert(plan[1].logTemplate[0] == '\0');
    assert(std::strcmp(plan[2].sourcePath, "src/unconditional.cpp") == 0);
    assert(plan[2].line == 7 && plan[2].condition[0] == '\0');
    assert(std::memcmp(&before, &workspace, sizeof(workspace)) == 0);
    // This exhaustive aggregate shape permits only the source-level fields.
    // Runtime data must never enlarge the plan's public value type.
    struct SourcePolicyOnly {
        char sourcePath[kMaxProjectPathBytes];
        uint32_t line, column, action, hitPolicy, hitThreshold;
        char condition[kDebugWatchMaxExpressionBytes + 1];
        char logTemplate[GX_DEVELOPMENT_DEBUG_MAX_LOG_TEMPLATE_BYTES + 1u];
    };
    static_assert(sizeof(DebuggerWorkspaceMaterializationEntry) == sizeof(SourcePolicyOnly),
                  "materialization plan must not carry IDs, addresses, sessions or hit counts");
    auto& [sourcePath, line, column, action, hitPolicy, hitThreshold, condition, logTemplate] =
        plan[0];
    (void)sourcePath;
    (void)line;
    (void)column;
    (void)action;
    (void)hitPolicy;
    (void)hitThreshold;
    (void)condition;
    (void)logTemplate;
    assert(BuildDebuggerWorkspaceMaterializationPlan(workspace, plan, 1) == 1);
    assert(std::strcmp(plan[0].sourcePath, "src/z.cpp") == 0);
    assert(BuildDebuggerWorkspaceMaterializationPlan(workspace, nullptr, 0) == 0);
    assert(DebuggerWorkspaceSetBreakpointEnabled(&workspace, "src/z.cpp", 13, false));
    assert(DebuggerWorkspaceSetBreakpointEnabled(&workspace, "src/a.cpp", 99, false));
    assert(DebuggerWorkspaceSetBreakpointEnabled(&workspace, "src/unconditional.cpp", 7, false));
    assert(BuildDebuggerWorkspaceMaterializationPlan(workspace, plan,
                                                     kDebuggerWorkspaceMaxBreakpoints) == 0);
    DebuggerWorkspaceInit(&workspace);
    assert(BuildDebuggerWorkspaceMaterializationPlan(workspace, plan,
                                                     kDebuggerWorkspaceMaxBreakpoints) == 0);
}

int main() {
    assertMaterializationPlan();
    DebuggerWorkspace workspace = {};
    DebuggerWorkspaceInit(&workspace);

    assert(DebuggerWorkspaceAddBreakpoint(&workspace, "src/main.cpp", 13, 1, true, 0, 0, 0,
                                          "", ""));
    assert(DebuggerWorkspaceAddBreakpoint(&workspace, "tests/main.cpp", 4, 2, true, 1, 0, 0,
                                          "", "{input}"));
    assert(DebuggerWorkspaceAddWatch(&workspace, "input + delta"));
    assert(DebuggerWorkspaceAddWatch(&workspace, "adjusted * 2"));

    char serialized[4096] = {};
    uint32_t serializedBytes = 0;
    DebuggerWorkspaceErrorCode error = DebuggerWorkspaceErrorCode::None;
    assert(SerializeDebuggerWorkspace(workspace, serialized, sizeof(serialized), &serializedBytes, &error));
    assertStorageBehavior(workspace, serialized, serializedBytes);

    DebuggerWorkspace parsed = {};
    assert(ParseDebuggerWorkspace(serialized, serializedBytes, &parsed, &error));
    assert(parsed.breakpointCount == 2);
    assert(parsed.watchCount == 2);
    assert(std::strcmp(parsed.breakpoints[0].sourcePath, "src/main.cpp") == 0);
    assert(parsed.breakpoints[0].line == 13 && parsed.breakpoints[0].column == 1);
    assert(parsed.breakpoints[0].enabled && parsed.breakpoints[0].action == 0);
    assert(parsed.breakpoints[0].hitPolicy == 0 && parsed.breakpoints[0].hitThreshold == 0);
    assert(std::strcmp(parsed.breakpoints[0].condition, "") == 0);
    assert(std::strcmp(parsed.breakpoints[0].logTemplate, "") == 0);
    assert(std::strcmp(parsed.breakpoints[1].sourcePath, "tests/main.cpp") == 0);
    assert(parsed.breakpoints[1].line == 4 && parsed.breakpoints[1].column == 2);
    assert(parsed.breakpoints[1].enabled && parsed.breakpoints[1].action == 1);
    assert(parsed.breakpoints[1].hitPolicy == 0 && parsed.breakpoints[1].hitThreshold == 0);
    assert(std::strcmp(parsed.breakpoints[1].condition, "") == 0);
    assert(std::strcmp(parsed.breakpoints[1].logTemplate, "{input}") == 0);
    assert(std::strcmp(parsed.watches[0], "input + delta") == 0);
    assert(std::strcmp(parsed.watches[1], "adjusted * 2") == 0);

    char serializedAgain[4096] = {};
    uint32_t serializedAgainBytes = 0;
    assert(SerializeDebuggerWorkspace(workspace, serializedAgain, sizeof(serializedAgain),
                                     &serializedAgainBytes, &error));
    assert(serializedBytes == serializedAgainBytes);
    assert(std::memcmp(serialized, serializedAgain, serializedBytes) == 0);
    const char* runtimeOnlyNames[] = {
        "breakpointId", "targetAddress", "sessionGeneration", "rawHitCount",
        "originalByte", "installed", "result", "frame", "marker"
    };
    for (const char* name : runtimeOnlyNames) assert(std::strstr(serialized, name) == nullptr);

    assertRejected("{\"version\":1,\"breakpoints\":[", DebuggerWorkspaceErrorCode::MalformedJson);
    assertRejected("{\"version\":1,\"breakpoints\":[],\"watches\":[] garbage}", DebuggerWorkspaceErrorCode::MalformedJson);
    assertRejected("{\"version\":2,\"breakpoints\":[],\"watches\":[]}", DebuggerWorkspaceErrorCode::UnsupportedVersion);
    assertRejected("{\"version\":1,\"watches\":[]}", DebuggerWorkspaceErrorCode::MissingField);
    assertRejected("{\"version\":1,\"breakpoints\":[],\"watches\":[],\"extra\":1}", DebuggerWorkspaceErrorCode::UnknownField);
    assertRejected("{\"version\":1,\"version\":1,\"breakpoints\":[],\"watches\":[]}", DebuggerWorkspaceErrorCode::DuplicateField);
    assertRejected("{\"version\":1,\"breakpoints\":[{\"sourcePath\":\"src/main.cpp\",\"sourcePath\":\"src/main.cpp\",\"line\":13,\"column\":1,\"enabled\":true,\"action\":\"BREAK\",\"condition\":\"\",\"hitPolicy\":\"NONE\",\"hitThreshold\":0,\"logTemplate\":\"\"}],\"watches\":[]}", DebuggerWorkspaceErrorCode::DuplicateField);
    assertRejected("{\"version\":1,\"breakpoints\":[{\"sourcePath\":\"src/main.cpp\",\"line\":13,\"column\":1,\"enabled\":true,\"action\":\"TRACE\",\"condition\":\"\",\"hitPolicy\":\"NONE\",\"hitThreshold\":0,\"logTemplate\":\"\"}],\"watches\":[]}", DebuggerWorkspaceErrorCode::InvalidEnum);
    assertRejected("{\"version\":1,\"breakpoints\":[{\"sourcePath\":\"src/main.cpp\",\"line\":13,\"column\":1,\"enabled\":true,\"action\":\"BREAK\",\"condition\":\"\",\"hitPolicy\":\"INVALID\",\"hitThreshold\":0,\"logTemplate\":\"\"}],\"watches\":[]}", DebuggerWorkspaceErrorCode::InvalidEnum);
    assertRejected("{\"version\":1,\"breakpoints\":[{\"sourcePath\":\"src/main.cpp\",\"line\":13,\"column\":1,\"enabled\":true,\"action\":\"BREAK\",\"condition\":\"\",\"hitPolicy\":\"EQUAL\",\"hitThreshold\":0,\"logTemplate\":\"\"}],\"watches\":[]}", DebuggerWorkspaceErrorCode::InvalidThreshold);

    const std::string longText(257, 'x');
    assertRejected(std::string("{\"version\":1,\"breakpoints\":[{\"sourcePath\":\"src/main.cpp\",\"line\":13,\"column\":1,\"enabled\":true,\"action\":\"BREAK\",\"condition\":\"") + longText + "\",\"hitPolicy\":\"NONE\",\"hitThreshold\":0,\"logTemplate\":\"\"}],\"watches\":[]}", DebuggerWorkspaceErrorCode::StringTooLong);
    assertRejected(std::string("{\"version\":1,\"breakpoints\":[{\"sourcePath\":\"src/main.cpp\",\"line\":13,\"column\":1,\"enabled\":true,\"action\":\"BREAK\",\"condition\":\"\",\"hitPolicy\":\"NONE\",\"hitThreshold\":0,\"logTemplate\":\"") + longText + "\"}],\"watches\":[]}", DebuggerWorkspaceErrorCode::StringTooLong);
    assertRejected(std::string("{\"version\":1,\"breakpoints\":[],\"watches\":[\"") + longText + "\"]}", DebuggerWorkspaceErrorCode::StringTooLong);
    const std::string longPath(160, 'p');
    assertRejected(std::string("{\"version\":1,\"breakpoints\":[{\"sourcePath\":\"") + longPath + "\",\"line\":1,\"column\":1,\"enabled\":true,\"action\":\"BREAK\",\"condition\":\"\",\"hitPolicy\":\"NONE\",\"hitThreshold\":0,\"logTemplate\":\"\"}],\"watches\":[]}", DebuggerWorkspaceErrorCode::StringTooLong);

    std::string nineBreakpoints = "{\"version\":1,\"breakpoints\":[";
    for (int index = 0; index < 9; ++index) {
        if (index) nineBreakpoints += ',';
        nineBreakpoints += "{\"sourcePath\":\"src/" + std::to_string(index) + ".cpp\",\"line\":1,\"column\":1,\"enabled\":true,\"action\":\"BREAK\",\"condition\":\"\",\"hitPolicy\":\"NONE\",\"hitThreshold\":0,\"logTemplate\":\"\"}";
    }
    nineBreakpoints += "],\"watches\":[]}";
    assertRejected(nineBreakpoints, DebuggerWorkspaceErrorCode::OverCapacity);
    std::string nineWatches = "{\"version\":1,\"breakpoints\":[],\"watches\":[\"x\",\"x\",\"x\",\"x\",\"x\",\"x\",\"x\",\"x\",\"x\"]}";
    assertRejected(nineWatches, DebuggerWorkspaceErrorCode::OverCapacity);
    assertRejected("{\"version\":1,\"breakpoints\":[{\"sourcePath\":\"src/main.cpp\",\"line\":13,\"column\":1,\"enabled\":true,\"action\":\"BREAK\",\"condition\":\"\",\"hitPolicy\":\"NONE\",\"hitThreshold\":0,\"logTemplate\":\"\"},{\"sourcePath\":\"src\\\\main.cpp\",\"line\":13,\"column\":2,\"enabled\":true,\"action\":\"BREAK\",\"condition\":\"\",\"hitPolicy\":\"NONE\",\"hitThreshold\":0,\"logTemplate\":\"\"}],\"watches\":[]}", DebuggerWorkspaceErrorCode::DuplicateBreakpoint);
    assertRejected("{\"version\":1,\"breakpoints\":[{\"sourcePath\":\"/src/main.cpp\",\"line\":13,\"column\":1,\"enabled\":true,\"action\":\"BREAK\",\"condition\":\"\",\"hitPolicy\":\"NONE\",\"hitThreshold\":0,\"logTemplate\":\"\"}],\"watches\":[]}", DebuggerWorkspaceErrorCode::InvalidPath);
    assertRejected("{\"version\":1,\"breakpoints\":[{\"sourcePath\":\"../main.cpp\",\"line\":13,\"column\":1,\"enabled\":true,\"action\":\"BREAK\",\"condition\":\"\",\"hitPolicy\":\"NONE\",\"hitThreshold\":0,\"logTemplate\":\"\"}],\"watches\":[]}", DebuggerWorkspaceErrorCode::InvalidPath);
    assertRejected("{\"version\":1,\"breakpoints\":[],\"watches\":[]} trailing", DebuggerWorkspaceErrorCode::MalformedJson);

    DebuggerWorkspace direct = {};
    DebuggerWorkspaceInit(&direct);
    std::strcpy(direct.breakpoints[0].sourcePath, "src\\main.cpp");
    direct.breakpoints[0].line = 13;
    direct.breakpoints[0].column = 2;
    direct.breakpoints[0].enabled = true;
    direct.breakpoints[0].action = 0;
    direct.breakpoints[0].hitPolicy = 0;
    direct.breakpoints[0].hitThreshold = 0;
    direct.breakpointCount = 1;
    char directBytes[4096] = {};
    uint32_t directLength = 0;
    assert(SerializeDebuggerWorkspace(direct, directBytes, sizeof(directBytes), &directLength, &error));
    assert(std::strstr(directBytes, "\"sourcePath\":\"src/main.cpp\"") != nullptr);

    direct.breakpoints[0].action = 99;
    assert(!SerializeDebuggerWorkspace(direct, directBytes, sizeof(directBytes), &directLength, &error));
    assert(error == DebuggerWorkspaceErrorCode::InvalidEnum);
    direct.breakpoints[0].action = 0;
    std::strcpy(direct.breakpoints[1].sourcePath, "src/main.cpp");
    direct.breakpoints[1].line = 13;
    direct.breakpoints[1].column = 1;
    direct.breakpoints[1].enabled = true;
    direct.breakpoints[1].hitPolicy = 0;
    direct.breakpointCount = 2;
    assert(!SerializeDebuggerWorkspace(direct, directBytes, sizeof(directBytes), &directLength, &error));
    assert(error == DebuggerWorkspaceErrorCode::DuplicateBreakpoint);

    DebuggerWorkspace mutations = {};
    DebuggerWorkspaceInit(&mutations);
    assert(DebuggerWorkspaceToggleBreakpoint(&mutations, "src/first.cpp", 10, 1));
    assert(DebuggerWorkspaceToggleBreakpoint(&mutations, "src/middle.cpp", 20, 2));
    assert(DebuggerWorkspaceToggleBreakpoint(&mutations, "src/last.cpp", 30, 3));
    assert(mutations.breakpointCount == 3);
    assert(DebuggerWorkspaceToggleBreakpoint(&mutations, "src/middle.cpp", 20, 2));
    assert(!mutations.breakpoints[1].enabled);
    assert(DebuggerWorkspaceSetBreakpointEnabled(&mutations, "src/middle.cpp", 20, true));
    assert(mutations.breakpoints[1].enabled);
    assert(DebuggerWorkspaceRemoveBreakpoint(&mutations, "src/middle.cpp", 20));
    assert(mutations.breakpointCount == 2);
    assert(DebuggerWorkspaceUpdateBreakpoint(&mutations, "src/last.cpp", 30,
                                             1, 2, 3, "value > 0", "value={value}"));
    assert(std::strcmp(mutations.breakpoints[0].sourcePath, "src/first.cpp") == 0);
    assert(std::strcmp(mutations.breakpoints[1].sourcePath, "src/last.cpp") == 0);
    assert(DebuggerWorkspaceAddWatch(&mutations, "first"));
    assert(DebuggerWorkspaceAddWatch(&mutations, "middle"));
    assert(DebuggerWorkspaceAddWatch(&mutations, "last"));
    assert(DebuggerWorkspaceEditWatch(&mutations, 2, "last_edited"));
    assert(DebuggerWorkspaceRemoveWatch(&mutations, 1));
    assert(mutations.watchCount == 2);
    assert(std::strcmp(mutations.watches[0], "first") == 0);
    assert(std::strcmp(mutations.watches[1], "last_edited") == 0);

    char mutationBytes[4096] = {};
    uint32_t mutationLength = 0;
    assert(SerializeDebuggerWorkspace(mutations, mutationBytes, sizeof(mutationBytes),
                                     &mutationLength, &error));
    assert(std::strstr(mutationBytes, "breakpointId") == nullptr);
    assert(std::strstr(mutationBytes, "targetAddress") == nullptr);
    assert(std::strstr(mutationBytes, "rawHitCount") == nullptr);
    DebuggerWorkspace reparsedMutations = {};
    assert(ParseDebuggerWorkspace(mutationBytes, mutationLength, &reparsedMutations, &error));
    assert(reparsedMutations.breakpointCount == 2);
    assert(reparsedMutations.breakpoints[0].enabled);
    assert(reparsedMutations.breakpoints[1].enabled);
    assert(std::strcmp(reparsedMutations.breakpoints[0].sourcePath, "src/first.cpp") == 0);
    assert(std::strcmp(reparsedMutations.breakpoints[1].sourcePath, "src/last.cpp") == 0);
    assert(reparsedMutations.breakpoints[1].action == 1);
    assert(reparsedMutations.breakpoints[1].hitPolicy == 2);
    assert(reparsedMutations.breakpoints[1].hitThreshold == 3);
    assert(std::strcmp(reparsedMutations.breakpoints[1].condition, "value > 0") == 0);
    assert(std::strcmp(reparsedMutations.breakpoints[1].logTemplate, "value={value}") == 0);
    assert(reparsedMutations.watchCount == 2);
    assert(std::strcmp(reparsedMutations.watches[0], "first") == 0);
    assert(std::strcmp(reparsedMutations.watches[1], "last_edited") == 0);

    char maxWatch[kDebugWatchMaxExpressionBytes + 1] = {};
    std::memset(maxWatch, 'w', kDebugWatchMaxExpressionBytes);
    assert(DebuggerWorkspaceAddWatch(&reparsedMutations, maxWatch));
    for (uint32_t i = reparsedMutations.watchCount; i < kDebuggerWorkspaceMaxWatches; ++i)
        assert(DebuggerWorkspaceAddWatch(&reparsedMutations, "capacity"));
    assert(!DebuggerWorkspaceAddWatch(&reparsedMutations, "overflow"));
    assert(reparsedMutations.lastError == DebuggerWorkspaceErrorCode::OverCapacity);
    assert(std::strcmp(reparsedMutations.lastErrorMessage, "OverCapacity") == 0);

    DebuggerWorkspace invalidWatch = {};
    DebuggerWorkspaceInit(&invalidWatch);
    char oversizedWatch[kDebugWatchMaxExpressionBytes + 2] = {};
    std::memset(oversizedWatch, 'x', kDebugWatchMaxExpressionBytes + 1);
    assert(!DebuggerWorkspaceAddWatch(&invalidWatch, oversizedWatch));
    assert(invalidWatch.lastError == DebuggerWorkspaceErrorCode::StringTooLong);
    assert(std::strcmp(invalidWatch.lastErrorMessage, "StringTooLong") == 0);
    assert(DebuggerWorkspaceAddWatch(&invalidWatch, "original"));
    assert(!DebuggerWorkspaceEditWatch(&invalidWatch, 0, oversizedWatch));
    assert(invalidWatch.lastError == DebuggerWorkspaceErrorCode::StringTooLong);
    assert(std::strcmp(invalidWatch.lastErrorMessage, "StringTooLong") == 0);
    assert(std::strcmp(invalidWatch.watches[0], "original") == 0);

    std::cout << "Developer Studio debugger workspace persistence PASS\n";
    return 0;
}
