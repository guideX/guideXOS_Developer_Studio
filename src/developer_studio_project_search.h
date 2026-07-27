#pragma once

#include "developer_studio_projects.h"

namespace guidexos {
namespace developer_studio {

// The hosted filesystem has a full-file 256 KiB read limit.  These bounds are
// intentionally expressed here, beside the UI-independent service, so tests
// and the production shell share the same trust boundary.
static const uint32_t kProjectSearchMaxPatternBytes = 2048u;
static const uint32_t kProjectSearchMaxPatterns = 64u;
static const uint32_t kProjectSearchMaxPatternBytesEach = 256u;
static const uint32_t kProjectSearchMaxRelativePathBytes = kMaxPathBytes - 1u;
static const uint32_t kProjectSearchMaxDirectoryDepth = 64u;
static const uint32_t kProjectSearchMaxDirectories = 4096u;
static const uint32_t kProjectSearchMaxPendingDirectories = 512u;
static const uint32_t kProjectSearchMaxFiles = 100000u;
static const uint32_t kProjectSearchMaxFileBytes = kMaxEditorBytes;
static const uint64_t kProjectSearchMaxBytesScanned = 256ull * 1024ull * 1024ull;
static const uint32_t kProjectSearchMaxResultFiles = 256u;
static const uint32_t kProjectSearchMaxTotalMatches = 4096u;
static const uint32_t kProjectSearchMaxMatchesPerFile = 512u;
static const uint32_t kProjectSearchMaxPreviewBytes = 512u;
static const uint32_t kProjectSearchMaxEntriesPerPoll = 16u;
static const uint64_t kProjectSearchMaxDurationMs = 5ull * 60ull * 1000ull;

enum class ProjectSearchState {
    Idle = 0,
    Enumerating,
    Searching,
    Cancelling,
    Completed,
    Failed,
    Cancelled
};

enum class ProjectSearchResultKind {
    Match = 0,
    FileSkipped,
    SearchNotice
};

enum class ProjectSearchSourceKind {
    Disk = 0,
    DirtyDocument
};

enum class ProjectSearchErrorCode {
    None = 0,
    NoProject,
    EmptyQuery,
    QueryTooLong,
    InvalidRoot,
    InvalidIncludePattern,
    InvalidExcludePattern,
    PatternLimit,
    PatternTooLong,
    ProjectStale,
    ProjectMismatch,
    PathInvalid,
    PathOutsideProject,
    EnumerationFailed,
    DirectoryLimit,
    FileLimit,
    FileTooLarge,
    BinaryFile,
    UnsupportedFile,
    FileReadFailed,
    BytesLimit,
    MatchLimit,
    ResultLimit,
    Cancelled,
    Timeout,
    OperationStale,
    Released,
    Internal
};

struct ProjectSearchOptions {
    bool caseSensitive;
    bool wholeWord;
    char query[kFindMaxQueryBytes + 1];
    char includePattern[kProjectSearchMaxPatternBytes + 1];
    char excludePattern[kProjectSearchMaxPatternBytes + 1];
};

// A dirty document is copied into the operation at StartSearch.  The service
// never retains a pointer into WorkspaceModel or into a mutable TextBuffer.
struct ProjectSearchDocumentSnapshot {
    char relativePath[kMaxPathBytes];
    char data[kProjectSearchMaxFileBytes + 1];
    uint32_t length;
    uint64_t documentId;
    uint32_t documentGeneration;
};

struct ProjectSearchRequest {
    char projectId[kMaxProjectIdBytes];
    uint64_t projectGeneration;
    char rootPath[kMaxPathBytes];
    ProjectSearchOptions options;
    WorkspaceFileSystem fileSystem;
    const ProjectSearchDocumentSnapshot* dirtyDocuments;
    uint32_t dirtyDocumentCount;
};

struct ProjectSearchMatch {
    uint64_t byteOffset;
    uint32_t line;
    uint32_t column;
    uint32_t matchLength;
    char preview[kProjectSearchMaxPreviewBytes + 1];
    uint32_t previewMatchStart;
    uint32_t previewMatchLength;
    bool previewLeftTruncated;
    bool previewRightTruncated;
};

struct ProjectSearchFileGroup {
    char relativePath[kMaxPathBytes];
    uint32_t firstMatchIndex;
    uint32_t matchCount;
    ProjectSearchSourceKind sourceKind;
    uint64_t fileSize;
    uint64_t documentId;
    uint32_t documentGeneration;
};

struct ProjectSearchOperation {
    uint64_t operationId;
    uint64_t projectId;
    uint64_t projectGeneration;
    ProjectSearchState state;
    ProjectSearchOptions options;
    uint64_t filesEnumerated;
    uint64_t filesSearched;
    uint64_t bytesScanned;
    uint64_t matchesFound;
    uint32_t resultFileCount;
    uint32_t resultMatchCount;
    bool truncated;
    bool cancellationRequested;
    ProjectSearchErrorCode error;
    char notice[96];
};

struct ProjectSearchService {
    ProjectSearchOperation operation;
    WorkspaceFileSystem fileSystem;
    char rootPath[kMaxPathBytes];
    char projectIdText[kMaxProjectIdBytes];
    char pendingDirectories[kProjectSearchMaxPendingDirectories][kMaxPathBytes];
    uint32_t pendingHead;
    uint32_t pendingTail;
    uint32_t pendingCount;
    uint32_t directoriesVisited;
    FileListEntry currentEntries[kMaxWorkspaceEntries];
    uint32_t currentEntryCount;
    uint32_t currentEntryIndex;
    char currentDirectory[kMaxPathBytes];
    uint32_t currentDirectoryDepth;
    bool currentDirectoryActive;
    ProjectSearchOptions parsedOptions;
    char includePatterns[kProjectSearchMaxPatterns][kProjectSearchMaxPatternBytesEach + 1];
    char excludePatterns[kProjectSearchMaxPatterns][kProjectSearchMaxPatternBytesEach + 1];
    uint32_t includePatternCount;
    uint32_t excludePatternCount;
    ProjectSearchDocumentSnapshot dirtyDocuments[kMaxOpenDocuments];
    uint32_t dirtyDocumentCount;
    char scanBuffer[kProjectSearchMaxFileBytes + 1];
    ProjectSearchFileGroup groups[kProjectSearchMaxResultFiles];
    ProjectSearchMatch matches[kProjectSearchMaxTotalMatches];
    uint64_t startedAtMs;
    bool terminalReported;
};

const char* ProjectSearchStateName(ProjectSearchState state);
const char* ProjectSearchErrorName(ProjectSearchErrorCode error);
void ProjectSearchOptionsInit(ProjectSearchOptions* options);
void ProjectSearchServiceInit(ProjectSearchService* service);

bool ProjectSearchStart(ProjectSearchService* service, const ProjectSearchRequest* request,
                        uint64_t nowMs, uint64_t* outOperationId,
                        ProjectSearchErrorCode* outError);
bool ProjectSearchPoll(ProjectSearchService* service, uint64_t operationId,
                       uint32_t workBudget, uint64_t nowMs);
bool ProjectSearchCancel(ProjectSearchService* service, uint64_t operationId);
bool ProjectSearchRelease(ProjectSearchService* service, uint64_t operationId);
void ProjectSearchClearResults(ProjectSearchService* service);
bool ProjectSearchIsActive(const ProjectSearchService* service);
const ProjectSearchOperation* ProjectSearchOperationInfo(const ProjectSearchService* service);
uint32_t ProjectSearchQueryResultGroups(const ProjectSearchService* service);
const ProjectSearchFileGroup* ProjectSearchResultGroupAt(const ProjectSearchService* service, uint32_t index);
const ProjectSearchMatch* ProjectSearchResultMatchAt(const ProjectSearchService* service,
                                                     const ProjectSearchFileGroup* group,
                                                     uint32_t matchIndex);
const char* ProjectSearchQuery(const ProjectSearchService* service);

} // namespace developer_studio
} // namespace guidexos
