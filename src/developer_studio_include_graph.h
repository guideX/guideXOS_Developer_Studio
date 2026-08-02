#pragma once

#include <stdint.h>

#include "developer_studio_projects.h"

namespace guidexos {
namespace developer_studio {

// The graph deliberately uses the same fixed-storage style as the hosted
// Developer Studio models.  These are hard safety bounds, not promises that
// every project is indexed completely.
static const uint32_t kIncludeGraphMaxRequestedPathBytes = 2048u;
static const uint32_t kIncludeGraphMaxLogicalDirectiveBytes = 16u * 1024u;
static const uint32_t kIncludeGraphMaxContinuedLines = 64u;
static const uint32_t kIncludeGraphMaxDirectivesPerFile = 4096u;
// Keep two graph generations resident so a cancelled rebuild can leave the
// last completed result visible.  These bounds are intentionally conservative
// for the freestanding ELF's static storage budget.
static const uint32_t kIncludeGraphMaxFiles = 1024u;
static const uint32_t kIncludeGraphMaxNodes = 1024u;
static const uint32_t kIncludeGraphMaxEdges = 4096u;
static const uint32_t kIncludeGraphMaxAmbiguousCandidates = 256u;
static const uint32_t kIncludeGraphMaxStoredCandidates = 4096u;
static const uint32_t kIncludeGraphMaxDirectories = 4096u;
static const uint32_t kIncludeGraphMaxPendingDirectories = 512u;
static const uint32_t kIncludeGraphMaxDepth = 64u;
static const uint32_t kIncludeGraphMaxScanFileBytes = kMaxEditorBytes;
static const uint64_t kIncludeGraphMaxBytesScanned = 256ull * 1024ull * 1024ull;
static const uint32_t kIncludeGraphMaxTraversalNodes = 20000u;
static const uint32_t kIncludeGraphMaxTraversalEdges = 100000u;
static const uint32_t kIncludeGraphMaxTraversalDepth = 256u;
static const uint32_t kIncludeGraphMaxCycleGroups = 256u;
static const uint32_t kIncludeGraphMaxCycleMembers = 4096u;
static const uint32_t kIncludeGraphMaxCyclePath = 256u;
static const uint32_t kIncludeGraphMaxIncludeRoots = 8u;
static const uint64_t kIncludeGraphMaxDurationMs = 5ull * 60ull * 1000ull;

enum class IncludeDelimiterKind {
    Quoted = 0,
    Angled
};

enum class IncludeDirectiveState {
    Active = 0,
    InactiveIfZero,
    ConditionalUnknown,
    Malformed
};

enum class IncludeResolutionState {
    Resolved = 0,
    Missing,
    ExternalUnresolved,
    Ambiguous,
    OutsideProject,
    UnsupportedMacro,
    InvalidPath,
    Unreadable
};

struct IncludeDirective {
    uint64_t directiveId;
    uint64_t sourceDocumentId;
    uint64_t sourceDocumentGeneration;
    char sourceRelativePath[kMaxPathBytes];
    uint64_t directiveByteOffset;
    uint64_t includeTextByteOffset;
    uint32_t line;
    uint32_t column;
    uint32_t includeTextLength;
    IncludeDelimiterKind delimiterKind;
    IncludeDirectiveState directiveState;
    bool literalPath;
    char requestedPath[kIncludeGraphMaxRequestedPathBytes + 1];
};

struct IncludeResolution {
    IncludeResolutionState state;
    char resolvedRelativePath[kMaxPathBytes];
    uint32_t ambiguousCandidateOffset;
    uint32_t ambiguousCandidateCount;
    uint32_t resolutionPriority;
    bool caseMismatched;
    char resolutionRoot[kMaxPathBytes];
    char statusCode[64];
};

struct IncludeEdge {
    uint64_t edgeId;
    char sourceRelativePath[kMaxPathBytes];
    char targetRelativePath[kMaxPathBytes];
    IncludeDirective directive;
    IncludeResolution resolution;
    bool fromDirtySnapshot;
    bool stale;
};

struct IncludeNode {
    uint64_t nodeId;
    char relativePath[kMaxPathBytes];
    uint32_t outgoingEdgeOffset;
    uint32_t outgoingEdgeCount;
    uint32_t incomingEdgeOffset;
    uint32_t incomingEdgeCount;
    bool sourceFile;
    bool headerFile;
    bool dirty;
    bool missingPlaceholder;
};

struct IncludeCycleGroup {
    uint64_t cycleId;
    uint32_t memberOffset;
    uint32_t memberCount;
    uint32_t edgeOffset;
    uint32_t edgeCount;
    char representativePath[kMaxPathBytes];
    bool containsConditionalEdge;
    bool selfCycle;
};

struct IncludeGraph {
    uint64_t graphId;
    uint64_t projectId;
    uint64_t projectGeneration;
    uint64_t graphGeneration;
    IncludeNode* nodes;
    IncludeEdge* edges;
    uint32_t* outgoingEdgeIndices;
    uint32_t* incomingEdgeIndices;
    IncludeCycleGroup* cycles;
    uint32_t* cycleMembers;
    uint32_t* cycleEdges;
    char (*ambiguousCandidates)[kMaxPathBytes];
    uint32_t nodeCount;
    uint32_t edgeCount;
    uint32_t cycleCount;
    uint32_t ambiguousCandidateCount;
    uint32_t cycleMemberCount;
    uint32_t cycleEdgeCount;
    uint32_t nodeCapacity;
    uint32_t edgeCapacity;
    uint32_t cycleCapacity;
    uint32_t ambiguousCandidateCapacity;
    uint32_t cycleMemberCapacity;
    uint32_t cycleEdgeCapacity;
    bool complete;
    bool truncated;
};

struct IncludeGraphStorage {
    IncludeNode nodes[kIncludeGraphMaxNodes];
    IncludeEdge edges[kIncludeGraphMaxEdges];
    uint32_t outgoingEdgeIndices[kIncludeGraphMaxEdges];
    uint32_t incomingEdgeIndices[kIncludeGraphMaxEdges];
    IncludeCycleGroup cycles[kIncludeGraphMaxCycleGroups];
    uint32_t cycleMembers[kIncludeGraphMaxCycleMembers];
    uint32_t cycleEdges[kIncludeGraphMaxCycleMembers];
    char ambiguousCandidates[kIncludeGraphMaxStoredCandidates][kMaxPathBytes];
};

struct IncludeGraphDocumentSnapshot {
    char relativePath[kMaxPathBytes];
    char data[kIncludeGraphMaxScanFileBytes + 1];
    uint32_t length;
    uint64_t documentId;
    uint32_t documentGeneration;
};

struct IncludeGraphRequest {
    char projectId[kMaxProjectIdBytes];
    uint64_t projectGeneration;
    char rootPath[kMaxPathBytes];
    char includeRoots[kIncludeGraphMaxIncludeRoots][kMaxPathBytes];
    uint32_t includeRootCount;
    bool allowProjectRoot;
    WorkspaceFileSystem fileSystem;
    const IncludeGraphDocumentSnapshot* dirtyDocuments;
    uint32_t dirtyDocumentCount;
};

enum class IncludeGraphBuildState {
    Idle = 0,
    Enumerating,
    Scanning,
    Resolving,
    BuildingReverseEdges,
    DetectingCycles,
    Completed,
    Cancelling,
    Cancelled,
    Failed
};

enum class IncludeGraphErrorCode {
    None = 0,
    NoProject,
    ProjectStale,
    BuildStale,
    Cancelled,
    Timeout,
    FileLimit,
    DirectoryLimit,
    ByteLimit,
    DirectiveLimit,
    EdgeLimit,
    NodeLimit,
    CycleLimit,
    EnumerationFailed,
    FileReadFailed,
    FileTooLarge,
    BinaryFile,
    InvalidDirective,
    DirectiveTooLong,
    ContinuationLimit,
    PathTooLong,
    PathInvalid,
    PathOutsideProject,
    TraversalLimit,
    Released,
    Internal
};

struct IncludeGraphBuildOperation {
    uint64_t operationId;
    uint64_t projectId;
    uint64_t projectGeneration;
    IncludeGraphBuildState state;
    IncludeGraphErrorCode error;
    IncludeGraph* graph;
    WorkspaceFileSystem fileSystem;
    char rootPath[kMaxPathBytes];
    char includeRoots[kIncludeGraphMaxIncludeRoots][kMaxPathBytes];
    uint32_t includeRootCount;
    bool allowProjectRoot;
    uint64_t filesEnumerated;
    uint64_t filesScanned;
    uint64_t bytesScanned;
    uint64_t directivesFound;
    uint64_t edgesResolved;
    uint64_t unresolvedEdges;
    bool cancellationRequested;
    bool truncated;
    bool terminalReported;
    uint64_t startedAtMs;
    uint64_t nextDirectiveId;
    uint64_t nextEdgeId;
    char pendingDirectories[kIncludeGraphMaxPendingDirectories][kMaxPathBytes];
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
    uint32_t scanIndex;
    uint32_t resolveIndex;
    uint32_t analysisIndex;
    uint32_t scratchNodeStack[kIncludeGraphMaxNodes];
    uint32_t scratchEdgeStack[kIncludeGraphMaxNodes];
    uint32_t scratchOrder[kIncludeGraphMaxNodes];
    uint32_t scratchComponent[kIncludeGraphMaxNodes];
    bool scratchVisited[kIncludeGraphMaxNodes];
    char scanBuffer[kIncludeGraphMaxScanFileBytes + 1];
    IncludeGraphDocumentSnapshot dirtyDocuments[kMaxOpenDocuments];
    uint32_t dirtyDocumentCount;
    char status[128];
};

enum class IncludeGraphTraversalDirection {
    Includes = 0,
    IncludedBy
};

struct IncludeGraphTraversalResult {
    uint32_t* nodeIndices;
    uint32_t* depths;
    uint32_t* parentEdgeIndices;
    uint32_t nodeCount;
    uint32_t nodeCapacity;
    uint32_t edgeCount;
    uint32_t maxDepth;
    bool truncated;
};

const char* IncludeDelimiterName(IncludeDelimiterKind kind);
const char* IncludeDirectiveStateName(IncludeDirectiveState state);
const char* IncludeResolutionStateName(IncludeResolutionState state);
const char* IncludeGraphBuildStateName(IncludeGraphBuildState state);
const char* IncludeGraphErrorName(IncludeGraphErrorCode error);
const char* IncludeGraphStatusText(IncludeGraphErrorCode error);

bool IsIncludeGraphSourcePath(const char* path);
bool IsIncludeGraphHeaderPath(const char* path);
void IncludeGraphStorageInit(IncludeGraphStorage* storage);
void IncludeGraphInit(IncludeGraph* graph, IncludeGraphStorage* storage,
                      const char* projectId, uint64_t projectGeneration);
bool IncludeGraphIsCurrent(const IncludeGraph* graph, const char* projectId, uint64_t projectGeneration);
const IncludeNode* IncludeGraphNodeAt(const IncludeGraph* graph, uint32_t index);
const IncludeEdge* IncludeGraphEdgeAt(const IncludeGraph* graph, uint32_t index);
const IncludeCycleGroup* IncludeGraphCycleAt(const IncludeGraph* graph, uint32_t index);
const char* IncludeGraphCandidateAt(const IncludeGraph* graph, const IncludeResolution& resolution, uint32_t index);

typedef bool (*IncludeDirectiveVisitor)(void* userData, const IncludeDirective* directive);
bool IncludeGraphScanDocument(const char* data, uint32_t length,
                              const char* sourceRelativePath, uint64_t documentId,
                              uint64_t documentGeneration, IncludeDirectiveVisitor visitor,
                              void* userData, uint32_t* outDirectives, bool* outTruncated);

bool IncludeGraphAddNode(IncludeGraph* graph, const char* relativePath, bool dirty, uint64_t* outNodeId);
bool IncludeGraphAddEdge(IncludeGraph* graph, const IncludeEdge& edge);
bool IncludeGraphResolveAll(IncludeGraph* graph, const IncludeGraphRequest& request,
                            uint64_t* outResolved, uint64_t* outUnresolved);
bool IncludeGraphBuildReverseEdges(IncludeGraph* graph);
bool IncludeGraphDetectCycles(IncludeGraph* graph, uint32_t* outCycleCount);

bool IncludeGraphStart(IncludeGraphBuildOperation* operation, IncludeGraph* graph,
                       const IncludeGraphRequest& request, uint64_t nowMs,
                       uint64_t* outOperationId, IncludeGraphErrorCode* outError);
bool IncludeGraphPoll(IncludeGraphBuildOperation* operation, uint64_t operationId,
                      uint32_t workBudget, uint64_t nowMs);
bool IncludeGraphCancel(IncludeGraphBuildOperation* operation, uint64_t operationId);
bool IncludeGraphRelease(IncludeGraphBuildOperation* operation, uint64_t operationId);
bool IncludeGraphIsActive(const IncludeGraphBuildOperation* operation);
const IncludeGraphBuildOperation* IncludeGraphOperationInfo(const IncludeGraphBuildOperation* operation);

bool IncludeGraphRescanDocument(IncludeGraph* graph, const IncludeGraphRequest& request,
                                const char* relativePath, uint64_t documentId,
                                uint64_t documentGeneration, const char* data,
                                uint32_t length, bool fromDirtySnapshot,
                                IncludeGraphErrorCode* outError);

bool IncludeGraphBuildTraversal(const IncludeGraph* graph, const char* startRelativePath,
                                IncludeGraphTraversalDirection direction, bool includeConditional,
                                IncludeGraphTraversalResult* result);

const IncludeEdge* IncludeGraphFindDirectiveAt(const IncludeGraph* graph,
                                                const char* sourceRelativePath,
                                                uint64_t caretOffset);

} // namespace developer_studio
} // namespace guidexos
