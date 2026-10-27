#include "developer_studio_replace_preview.h"
#include "developer_studio_tools.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

using namespace guidexos::developer_studio;
namespace fs = std::filesystem;

struct FileSystemState {
    uint32_t writeCalls;
    uint32_t outsideReadCalls;
};

static bool statPath(void*, const char* path, FileInfo* info) {
    std::error_code ec;
    const fs::file_status status = fs::symlink_status(fs::path(path), ec);
    if (ec || !fs::exists(status)) return false;
    info->kind = fs::is_directory(status) ? FileInfoKind::Directory :
        (fs::is_regular_file(status) ? FileInfoKind::RegularFile : FileInfoKind::Unknown);
    info->size = info->kind == FileInfoKind::RegularFile ? fs::file_size(path, ec) : 0;
    return !ec;
}

static bool listPath(void*, const char* path, FileListEntry* entries, uint32_t capacity,
                     uint32_t* outCount, bool* outTruncated) {
    *outCount = 0;
    *outTruncated = false;
    std::error_code ec;
    for (const fs::directory_entry& item : fs::directory_iterator(fs::path(path), ec)) {
        if (ec) return false;
        if (*outCount >= capacity) { *outTruncated = true; break; }
        const fs::file_status status = item.symlink_status(ec);
        if (ec) return false;
        const std::string name = item.path().filename().string();
        std::strncpy(entries[*outCount].name, name.c_str(), sizeof(entries[*outCount].name) - 1);
        entries[*outCount].name[sizeof(entries[*outCount].name) - 1] = '\0';
        entries[*outCount].kind = fs::is_directory(status) ? FileInfoKind::Directory :
            (fs::is_regular_file(status) ? FileInfoKind::RegularFile : FileInfoKind::Unknown);
        entries[*outCount].size = entries[*outCount].kind == FileInfoKind::RegularFile ? item.file_size(ec) : 0;
        ++(*outCount);
    }
    return true;
}

static bool readPath(void*, const char* path, char* buffer, uint32_t capacity,
                     uint32_t* outBytes) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return false;
    input.seekg(0, std::ios::end);
    const std::streamoff size = input.tellg();
    if (size < 0 || static_cast<uint64_t>(size) > capacity) return false;
    input.seekg(0, std::ios::beg);
    input.read(buffer, size);
    if (!input && size > 0) return false;
    *outBytes = static_cast<uint32_t>(size);
    return true;
}

static bool writePath(void* userData, const char*, const char*, uint32_t, uint32_t*) {
    FileSystemState* state = static_cast<FileSystemState*>(userData);
    if (state) ++state->writeCalls;
    return false;
}

static void writeBytes(const fs::path& path, const std::string& bytes) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

static std::string readBytes(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

static void setText(char* output, uint32_t capacity, const char* value) {
    std::strncpy(output, value, capacity - 1);
    output[capacity - 1] = '\0';
}

struct PreviewRun {
    ProjectSearchService search;
    ReplacePreviewPlan plan;
    FileSystemState fileSystemState;
    uint64_t operationId;
    ProjectSearchErrorCode searchError;
    bool started;
};

static PreviewRun& runPreview(const fs::path& root, const char* findText,
                              const char* replacementText, bool caseSensitive = true) {
    static PreviewRun run = {};
    run.fileSystemState = {};
    run.operationId = 0;
    run.searchError = ProjectSearchErrorCode::None;
    run.started = false;
    ProjectSearchServiceInit(&run.search);
    ReplacePreviewPlanInit(&run.plan);
    const ReplacePreviewError begin = ReplacePreviewPlanBegin(&run.plan,
        "com.example.replace", 9, findText, replacementText, caseSensitive);
    if (begin != ReplacePreviewError::None) return run;

    ProjectSearchRequest request = {};
    setText(request.projectId, sizeof(request.projectId), "com.example.replace");
    request.projectGeneration = 9;
    setText(request.rootPath, sizeof(request.rootPath), root.string().c_str());
    ProjectSearchOptionsInit(&request.options);
    setText(request.options.query, sizeof(request.options.query), findText);
    request.options.caseSensitive = caseSensitive;
    setText(request.options.includePattern, sizeof(request.options.includePattern), "*");
    request.options.excludePattern[0] = '\0';
    request.fileSystem = { &run.fileSystemState, statPath, listPath, readPath,
                           writePath, nullptr, nullptr };
    request.scanVisitor = ReplacePreviewPlanVisitFile;
    request.scanVisitorUserData = &run.plan;
    run.started = ProjectSearchStart(&run.search, &request, 0,
                                    &run.operationId, &run.searchError);
    if (!run.started) {
        std::cerr << "replace scan start failed error=" << static_cast<int>(run.searchError)
                  << " id=" << run.operationId << '\n';
        return run;
    }
    for (uint32_t i = 0; i < 200000 && ProjectSearchIsActive(&run.search); ++i)
        assert(ProjectSearchPoll(&run.search, run.operationId, 16, i));
    const ProjectSearchOperation* operation = ProjectSearchOperationInfo(&run.search);
    assert(operation && !ProjectSearchIsActive(&run.search));
    if (!operation || operation->state != ProjectSearchState::Completed || operation->truncated)
        std::cerr << "replace scan terminal state=" << (operation ? static_cast<int>(operation->state) : -1)
                  << " error=" << (operation ? static_cast<int>(operation->error) : -1)
                  << " start_error=" << static_cast<int>(run.searchError)
                  << " skipped=" << (operation ? operation->diagnostics.filesSkipped : 0) << '\n';
    const bool changedOrRemovedDuringScan =
        operation->diagnostics.changedOrDisappearedFiles != 0;
    if (changedOrRemovedDuringScan) ReplacePreviewPlanMarkStale(&run.plan);
    ReplacePreviewPlanFinish(&run.plan,
        operation->state == ProjectSearchState::Completed && !operation->truncated &&
        operation->diagnostics.unreadableFiles == 0 &&
        operation->diagnostics.oversizedFiles == 0 && !changedOrRemovedDuringScan);
    if (!run.plan.complete)
        std::cerr << "replace plan incomplete observed=" << run.plan.matchesObserved
                  << " retained=" << run.plan.matchCount << " files=" << run.plan.filesAffected
                  << " capacity=" << run.plan.capacityExceeded << " stale=" << run.plan.stale << '\n';
    return run;
}

static bool activateTool(void*) { return true; }
static bool closeTool(void*) { return true; }

static bool traversalStat(void*, const char*, FileInfo* info) {
    info->kind = FileInfoKind::Directory;
    info->size = 0;
    return true;
}

static bool traversalList(void*, const char*, FileListEntry* entries, uint32_t capacity,
                          uint32_t* outCount, bool* outTruncated) {
    assert(capacity > 0);
    std::strcpy(entries[0].name, "../outside.cpp");
    entries[0].kind = FileInfoKind::RegularFile;
    entries[0].size = 7;
    *outCount = 1;
    *outTruncated = false;
    return true;
}

static bool traversalRead(void* userData, const char*, char*, uint32_t, uint32_t*) {
    FileSystemState* state = static_cast<FileSystemState*>(userData);
    ++state->outsideReadCalls;
    return false;
}

int main() {
    assert(ReplacePreviewFingerprint(nullptr, 0) == 14695981039346656037ull);
    assert(ReplacePreviewFingerprint("a", 1) == 0xaf63dc4c8601ec8cull);

    std::error_code ec;
    const fs::path root = fs::temp_directory_path() / "guidexos-replace-preview-test";
    fs::remove_all(root, ec);
    fs::create_directories(root / "src", ec);
    fs::create_directories(root / "build", ec);
    fs::create_directories(root / ".vs", ec);
    fs::create_directories(root / "src" / "bin", ec);
    fs::create_directories(root / ".hg", ec);
    fs::create_directories(root / ".svn", ec);
    fs::create_directories(root / ".cache", ec);
    fs::create_directories(root / "target", ec);
    fs::create_directories(root / "bazel-out", ec);
    fs::create_directories(root / "cmake-build-tools4", ec);
    const fs::path lfPath = root / "src" / "lf.cpp";
    const fs::path crlfPath = root / "src" / "crlf.txt";
    const std::string lfBytes = "alpha needle needle omega\nlast needle\n";
    const std::string crlfBytes = "first needle\r\nsecond needle\r\n";
    writeBytes(lfPath, lfBytes);
    writeBytes(crlfPath, crlfBytes);
    writeBytes(root / "build" / "excluded.cpp", "needle\n");
    writeBytes(root / ".vs" / "excluded.txt", "needle\n");
    writeBytes(root / "src" / "bin" / "excluded.cpp", "needle\n");
    writeBytes(root / ".hg" / "excluded.txt", "needle\n");
    writeBytes(root / ".svn" / "excluded.txt", "needle\n");
    writeBytes(root / ".cache" / "excluded.txt", "needle\n");
    writeBytes(root / "target" / "excluded.txt", "needle\n");
    writeBytes(root / "bazel-out" / "excluded.cpp", "needle\n");
    writeBytes(root / "cmake-build-tools4" / "excluded.cpp", "needle\n");
    writeBytes(root / "unsupported.bin", "needle\n");
    writeBytes(root / "binary.txt", std::string("needle\0hidden", 13));

    const fs::file_time_type lfTime = fs::last_write_time(lfPath, ec);
    const uintmax_t lfSize = fs::file_size(lfPath, ec);
    PreviewRun& run = runPreview(root, "needle", "replace");
    assert(run.started && run.plan.complete && !run.plan.stale);
    assert(run.plan.matchCount == 5 && run.plan.filesAffected == 2);
    assert(std::strcmp(ReplacePreviewPlanFileAt(&run.plan, 0)->relativePath, "src/crlf.txt") == 0);
    assert(std::strcmp(ReplacePreviewPlanFileAt(&run.plan, 1)->relativePath, "src/lf.cpp") == 0);
    const ReplacePreviewMatch* first = ReplacePreviewPlanMatchAt(&run.plan, 0);
    assert(first && first->fileIndex == 0 && first->line == 1 && first->column == 7);
    assert(std::strcmp(first->before, "first needle") == 0);
    assert(std::strcmp(first->after, "first replace") == 0);
    assert(readBytes(lfPath) == lfBytes && readBytes(crlfPath) == crlfBytes);
    assert(fs::file_size(lfPath, ec) == lfSize);
    assert(fs::last_write_time(lfPath, ec) == lfTime);

    assert(run.fileSystemState.writeCalls == 0);
    assert(DeveloperToolProjectIdentityMatches("com.example.replace", 9,
                                                "com.example.replace", 9));
    assert(!DeveloperToolProjectIdentityMatches("com.example.replace", 9,
                                                 "com.example.other", 9));

    // Distinct paths that differ only by case remain distinct on a
    // case-sensitive workspace, while ordering uses an exact-case tiebreak.
    ReplacePreviewPlan casePathPlan = {};
    assert(ReplacePreviewPlanBegin(&casePathPlan, "com.example.replace", 9,
                                   "x", "y", true) == ReplacePreviewError::None);
    ProjectSearchScanFile upperCasePath = {};
    upperCasePath.relativePath = "Case.txt";
    upperCasePath.data = "x";
    upperCasePath.length = 1;
    upperCasePath.sourceKind = ProjectSearchSourceKind::Disk;
    upperCasePath.fileSize = 1;
    ProjectSearchScanFile lowerCasePath = upperCasePath;
    lowerCasePath.relativePath = "case.txt";
    assert(ReplacePreviewPlanVisitFile(&casePathPlan, &upperCasePath));
    assert(ReplacePreviewPlanVisitFile(&casePathPlan, &lowerCasePath));
    ReplacePreviewPlanFinish(&casePathPlan, true);
    assert(casePathPlan.complete && casePathPlan.filesAffected == 2 &&
           casePathPlan.matchCount == 2);
    assert(std::strcmp(ReplacePreviewPlanFileAt(&casePathPlan, 0)->relativePath,
                       "Case.txt") == 0);
    assert(std::strcmp(ReplacePreviewPlanFileAt(&casePathPlan, 1)->relativePath,
                       "case.txt") == 0);

    // Multiple adjacent hits are retained left-to-right; overlapping hits are
    // resolved by the same leftmost non-overlapping rule used by scanning.
    writeBytes(root / "adjacent.cpp", "aaaaa ababa\n");
    runPreview(root, "aa", "b");
    assert(run.plan.complete && run.plan.matchCount == 2);
    const uint32_t adjacentFileIndex = 0;
    assert(std::strcmp(ReplacePreviewPlanFileAt(&run.plan, adjacentFileIndex)->relativePath,
                       "adjacent.cpp") == 0);
    assert(ReplacePreviewPlanMatchAt(&run.plan, 0)->byteOffset == 0);
    assert(ReplacePreviewPlanMatchAt(&run.plan, 1)->byteOffset == 2);
    runPreview(root, "aba", "x");
    assert(run.plan.complete && run.plan.matchCount == 1);
    assert(ReplacePreviewPlanMatchAt(&run.plan, 0)->byteOffset == 6);

    // A blank replacement is a deletion preview.
    runPreview(root, "needle", "");
    assert(run.plan.complete && run.plan.matchCount == 5);
    bool foundDeletion = false;
    for (uint32_t i = 0; i < run.plan.matchCount; ++i) {
        const ReplacePreviewMatch* match = ReplacePreviewPlanMatchAt(&run.plan, i);
        if (match && std::strstr(match->before, "needle") &&
            !std::strstr(match->after, "needle")) foundDeletion = true;
    }
    assert(foundDeletion);
    assert(readBytes(lfPath) == lfBytes && readBytes(crlfPath) == crlfBytes);

    // Empty find and overlong find/replacement inputs fail before scanning.
    ReplacePreviewPlan invalid = {};
    assert(ReplacePreviewPlanBegin(&invalid, "com.example.replace", 1, "", "x", false) ==
           ReplacePreviewError::EmptyFind);
    char longFind[kReplacePreviewMaxFindBytes + 2] = {};
    std::memset(longFind, 'x', sizeof(longFind) - 1);
    assert(ReplacePreviewPlanBegin(&invalid, "com.example.replace", 1, longFind, "x", false) ==
           ReplacePreviewError::FindTooLong);
    char longReplacement[kReplacePreviewMaxReplacementBytes + 2] = {};
    std::memset(longReplacement, 'x', sizeof(longReplacement) - 1);
    assert(ReplacePreviewPlanBegin(&invalid, "com.example.replace", 1, "x", longReplacement, false) ==
           ReplacePreviewError::ReplacementTooLong);

    // No match is a complete, empty plan. Special characters remain literal.
    runPreview(root, "missing", "x");
    assert(run.plan.complete && run.plan.matchCount == 0 && run.plan.filesAffected == 0);
    writeBytes(root / "literal.txt", "a.*[]$ aX[]$\n");
    runPreview(root, "a.*[]$", "literal");
    assert(run.plan.complete && run.plan.matchCount == 1);
    assert(std::strcmp(ReplacePreviewPlanMatchAt(&run.plan, 0)->after, "literal aX[]$") == 0);

    // Supported text extensions include TOOLS3 formats; unsupported and
    // binary files remain excluded, as do generated output directories.
    writeBytes(root / "cfg.toml", "needle\n");
    runPreview(root, "needle", "x");
    assert(run.plan.complete && run.plan.filesAffected == 3);
    for (uint32_t i = 0; i < run.plan.filesAffected; ++i) {
        const ReplacePreviewFile* file = ReplacePreviewPlanFileAt(&run.plan, i);
        assert(std::strcmp(file->relativePath, "binary.txt") != 0);
        assert(std::strcmp(file->relativePath, "unsupported.bin") != 0);
        assert(std::strstr(file->relativePath, "build/") == nullptr);
        assert(std::strstr(file->relativePath, ".hg/") == nullptr);
        assert(std::strstr(file->relativePath, ".svn/") == nullptr);
        assert(std::strstr(file->relativePath, ".cache/") == nullptr);
        assert(std::strstr(file->relativePath, "target/") == nullptr);
        assert(std::strstr(file->relativePath, "bazel-out/") == nullptr);
        assert(std::strstr(file->relativePath, "cmake-build-tools4/") == nullptr);
    }

    std::string oversizedContents(kProjectSearchMaxFileBytes + 1u, 'z');
    oversizedContents.replace(0, 6, "needle");
    writeBytes(root / "oversized.txt", oversizedContents);
    runPreview(root, "needle", "x");
    assert(!run.plan.complete && !run.plan.stale && run.plan.filesAffected == 3);
    assert(ProjectSearchOperationInfo(&run.search)->diagnostics.oversizedFiles >= 1);
    fs::remove(root / "oversized.txt", ec);

    // A very long line produces bounded, explicitly truncated excerpts.
    std::string longLine(400, 'q');
    longLine.replace(190, 6, "needle");
    writeBytes(root / "long.txt", longLine);
    runPreview(root, "needle", std::string(80, 'r').c_str());
    assert(run.plan.complete && run.plan.matchCount >= 1);
    const ReplacePreviewMatch* longMatch = nullptr;
    uint32_t longFileIndex = kReplacePreviewMaxAffectedFiles;
    for (uint32_t i = 0; i < run.plan.matchCount; ++i)
        if (ReplacePreviewPlanFileAt(&run.plan, ReplacePreviewPlanMatchAt(&run.plan, i)->fileIndex) &&
            std::strcmp(ReplacePreviewPlanFileAt(&run.plan, ReplacePreviewPlanMatchAt(&run.plan, i)->fileIndex)->relativePath,
                        "long.txt") == 0) {
            longMatch = ReplacePreviewPlanMatchAt(&run.plan, i);
            longFileIndex = longMatch->fileIndex;
        }
    assert(longMatch && longMatch->beforeRightTruncated && longMatch->afterRightTruncated);
    assert(std::strlen(longMatch->before) <= kReplacePreviewMaxPreviewBytes);
    assert(std::strlen(longMatch->after) <= kReplacePreviewMaxPreviewBytes);

    const ReplacePreviewFile* fingerprinted = ReplacePreviewPlanFileAt(&run.plan, longFileIndex);
    assert(fingerprinted);
    const std::string originalLongLine = readBytes(root / "long.txt");
    assert(ReplacePreviewFileFingerprintMatches(fingerprinted, originalLongLine.data(),
                                                static_cast<uint32_t>(originalLongLine.size())));
    writeBytes(root / "long.txt", originalLongLine + " ");
    const std::string changedLongLine = readBytes(root / "long.txt");
    assert(!ReplacePreviewFileFingerprintMatches(fingerprinted, changedLongLine.data(),
                                                 static_cast<uint32_t>(changedLongLine.size())));
    std::string changedSameSize = originalLongLine;
    changedSameSize[0] = 'z';
    writeBytes(root / "long.txt", changedSameSize);
    assert(!ReplacePreviewFileFingerprintMatches(fingerprinted, changedSameSize.data(),
                                                 static_cast<uint32_t>(changedSameSize.size())));
    ReplacePreviewPlanMarkStale(&run.plan);
    assert(run.plan.stale && !run.plan.complete);
    fs::remove(root / "long.txt", ec);
    FileInfo removedInfo = {};
    assert(!statPath(nullptr, (root / "long.txt").string().c_str(), &removedInfo));

    // Match capacity is explicit: retained data stays bounded and cannot be
    // called complete after the visitor reports overflow.
    std::string manyMatches;
    for (uint32_t i = 0; i <= kReplacePreviewMaxMatches; ++i) manyMatches += "x ";
    writeBytes(root / "many.cpp", manyMatches);
    runPreview(root, "x", "y");
    assert(run.plan.matchCount == kReplacePreviewMaxMatches);
    assert(run.plan.capacityExceeded && !run.plan.complete);
    assert(ProjectSearchOperationInfo(&run.search)->truncated);

    // The affected-file cap is separate from the match cap and also prevents
    // the plan from being reported as complete.
    for (uint32_t i = 0; i < kReplacePreviewMaxAffectedFiles + 1; ++i) {
        char name[32] = {};
        std::snprintf(name, sizeof(name), "cap-%03u.txt", i);
        writeBytes(root / name, "needle\n");
    }
    runPreview(root, "needle", "x");
    assert(run.plan.filesAffected == kReplacePreviewMaxAffectedFiles);
    assert(run.plan.capacityExceeded && !run.plan.complete);
    for (uint32_t i = 1; i < run.plan.filesAffected; ++i)
        assert(std::strcmp(ReplacePreviewPlanFileAt(&run.plan, i - 1)->relativePath,
                           ReplacePreviewPlanFileAt(&run.plan, i)->relativePath) < 0);

    // Cancellation leaves an explicitly incomplete plan. The scanner's
    // filesystem facade has no call to write source files.
    FileSystemState cancelState = {};
    ReplacePreviewPlan cancelPlan = {};
    assert(ReplacePreviewPlanBegin(&cancelPlan, "com.example.replace", 9,
                                   "needle", "x", true) == ReplacePreviewError::None);
    ProjectSearchRequest cancelRequest = {};
    setText(cancelRequest.projectId, sizeof(cancelRequest.projectId), "com.example.replace");
    cancelRequest.projectGeneration = 9;
    setText(cancelRequest.rootPath, sizeof(cancelRequest.rootPath), root.string().c_str());
    ProjectSearchOptionsInit(&cancelRequest.options);
    setText(cancelRequest.options.query, sizeof(cancelRequest.options.query), "needle");
    setText(cancelRequest.options.includePattern, sizeof(cancelRequest.options.includePattern), "*");
    cancelRequest.fileSystem = { &cancelState, statPath, listPath, readPath,
                                 writePath, nullptr, nullptr };
    cancelRequest.scanVisitor = ReplacePreviewPlanVisitFile;
    cancelRequest.scanVisitorUserData = &cancelPlan;
    static ProjectSearchService cancelSearch = {};
    ProjectSearchServiceInit(&cancelSearch);
    uint64_t cancelOperation = 0;
    ProjectSearchErrorCode cancelError = ProjectSearchErrorCode::None;
    assert(ProjectSearchStart(&cancelSearch, &cancelRequest, 0,
                              &cancelOperation, &cancelError));
    assert(ProjectSearchCancel(&cancelSearch, cancelOperation));
    assert(ProjectSearchPoll(&cancelSearch, cancelOperation, 1, 1));
    assert(ProjectSearchOperationInfo(&cancelSearch)->state == ProjectSearchState::Cancelled);
    ReplacePreviewPlanFinish(&cancelPlan, false);
    assert(!cancelPlan.complete && !cancelPlan.stale);
    assert(cancelState.writeCalls == 0);

    // Project traversal rejects hostile relative entries before any read.
    FileSystemState traversalState = {};
    WorkspaceFileSystem traversalFs = { &traversalState, traversalStat, traversalList,
                                        traversalRead, writePath, nullptr, nullptr };
    ReplacePreviewPlan traversalPlan = {};
    assert(ReplacePreviewPlanBegin(&traversalPlan, "com.example.replace", 1,
                                   "outside", "x", false) == ReplacePreviewError::None);
    ProjectSearchRequest traversalRequest = {};
    setText(traversalRequest.projectId, sizeof(traversalRequest.projectId), "com.example.replace");
    traversalRequest.projectGeneration = 1;
    setText(traversalRequest.rootPath, sizeof(traversalRequest.rootPath), "C:/project");
    ProjectSearchOptionsInit(&traversalRequest.options);
    setText(traversalRequest.options.query, sizeof(traversalRequest.options.query), "outside");
    setText(traversalRequest.options.includePattern, sizeof(traversalRequest.options.includePattern), "*");
    traversalRequest.fileSystem = traversalFs;
    traversalRequest.scanVisitor = ReplacePreviewPlanVisitFile;
    traversalRequest.scanVisitorUserData = &traversalPlan;
    static ProjectSearchService traversalSearch = {};
    ProjectSearchServiceInit(&traversalSearch);
    uint64_t traversalOperation = 0;
    ProjectSearchErrorCode traversalError = ProjectSearchErrorCode::None;
    assert(ProjectSearchStart(&traversalSearch, &traversalRequest, 0,
                              &traversalOperation, &traversalError));
    for (uint32_t i = 0; i < 4 && ProjectSearchIsActive(&traversalSearch); ++i)
        ProjectSearchPoll(&traversalSearch, traversalOperation, 16, i);
    ReplacePreviewPlanFinish(&traversalPlan, true);
    assert(traversalPlan.complete && traversalPlan.matchCount == 0);
    assert(traversalState.outsideReadCalls == 0);

    // The new command uses the existing fixed registry, preserving shortcuts
    // and making repeated activation/cancellation lifecycle testable.
    DeveloperToolRegistry registry = {};
    DeveloperToolRegistryInit(&registry);
    const DeveloperToolCommand replaceCommand = {
        DeveloperToolId::ReplaceInFiles, "Replace in Files", "Ctrl+Shift+H",
        activateTool, closeTool
    };
    assert(DeveloperToolRegistryRegister(&registry, &replaceCommand));
    assert(DeveloperToolIdFromShortcut(72, true, true, false) == DeveloperToolId::ReplaceInFiles);
    assert(DeveloperToolRegistryActivate(&registry, DeveloperToolId::ReplaceInFiles, nullptr));
    assert(DeveloperToolRegistryActivate(&registry, DeveloperToolId::ReplaceInFiles, nullptr));
    assert(DeveloperToolRegistryClose(&registry, DeveloperToolId::ReplaceInFiles, nullptr));

    assert(readBytes(lfPath) == lfBytes && readBytes(crlfPath) == crlfBytes);
    fs::remove_all(root, ec);
    std::cout << "Developer Studio replacement preview PASS\n";
    return 0;
}
