#pragma once

#include "developer_studio_projects.h"
#include "developer_studio_output.h"
#include "developer_studio_symbols.h"

namespace guidexos {
namespace developer_studio {

#if defined(GXOS_DEVELOPER_STUDIO_BARE_METAL)
static const uint32_t kMaxProjectSourceFiles = 4;
#else
static const uint32_t kMaxProjectSourceFiles = 16;
#endif

struct ProjectSourceFile {
    char relativePath[kMaxProjectPathBytes];
    uint64_t size;
};

enum class WorkspaceProjectOpenState {
    Idle = 0,
    LoadStarted,
    Loaded,
    CandidateAllocated,
    RefreshStarted,
    Validated,
    Committing,
    Active,
    Ready,
    Failed
};

enum class WorkspaceProjectLoadOwnershipResult {
    Current = 0,
    NoController,
    TransactionNotActive,
    ControllerNotInProgress,
    TransactionOwnerMismatch,
    RequestIdMismatch,
    RequestGenerationMismatch,
    TransactionIdMismatch,
    TransactionGenerationMismatch,
    CandidateIdMismatch,
    CandidateGenerationMismatch,
    CandidateProjectGenerationMismatch,
    CandidateProjectIdMismatch,
    RefreshGenerationMismatch,
    ActiveProjectIdMismatch,
    ActiveProjectGenerationMismatch
};

enum class WorkspaceProjectLoadCheckpoint {
    RequestAccepted = 0,
    TransactionCreated,
    CandidateCreated,
    LoadStarted,
    LoadStartedReturned,
    LoadStartedOwnerCheck,
    MetadataPathBegin,
    MetadataPathReady,
    MetadataStatCall,
    ProjectMetadataValidated,
    ApplicationManifestValidated,
    Loaded,
    RefreshStarted,
    BeforeRefresh,
    AfterRefresh,
    CommitStarting,
    BeforeCommit,
    AfterCommit,
    ActivePublished,
    ObserverCallback,
    ObserverReturn,
    Ready,
    FailureRollback,
    TransactionRelease,
    TransactionReleased
};

const char* WorkspaceProjectLoadOwnershipResultName(WorkspaceProjectLoadOwnershipResult result);
const char* WorkspaceProjectLoadCheckpointName(WorkspaceProjectLoadCheckpoint checkpoint);

struct WorkspaceProjectOpenEvent {
    WorkspaceProjectOpenState state;
    WorkspaceProjectLoadCheckpoint checkpoint;
    WorkspaceProjectLoadOwnershipResult ownershipResult;
    uint64_t checkedRequestId;
    uint64_t transactionId;
    uint64_t transactionGeneration;
    uint64_t requestId;
    uint64_t requestGeneration;
    uint64_t activeProjectGeneration;
    uint64_t candidateId;
    uint64_t candidateGeneration;
    uint64_t candidateProjectGeneration;
    uint64_t refreshGeneration;
    uint64_t expectedTransactionId;
    uint64_t expectedTransactionGeneration;
    uint64_t expectedRequestId;
    uint64_t expectedRequestGeneration;
    uint64_t expectedCandidateId;
    uint64_t expectedCandidateGeneration;
    uint64_t expectedCandidateProjectGeneration;
    uint64_t expectedRefreshGeneration;
    uint64_t expectedActiveProjectGeneration;
    uint64_t actualActiveProjectGeneration;
    uint32_t entryDepth;
    uint32_t maximumEntryDepth;
    uint32_t reentryCount;
    uint32_t manifestValidationCount;
    uint32_t loadStageCount;
    uint32_t refreshCount;
    uint32_t commitCount;
    uint32_t releaseCount;
    bool transactionActive;
    bool controllerLoadInProgress;
    bool transactionOwnerPointerMatches;
    bool transactionIdMatches;
    bool transactionGenerationMatches;
    bool requestIdMatches;
    bool requestGenerationMatches;
    bool candidateIdMatches;
    bool candidateGenerationMatches;
    bool candidateProjectGenerationMatches;
    bool candidateProjectIdMatches;
    bool refreshGenerationMatches;
    bool activeProjectIdMatches;
    bool activeProjectGenerationMatches;
    bool transactionCommitted;
    bool transactionOwnerMatches;
    const char* expectedActiveProjectId;
    const char* actualActiveProjectId;
    const char* expectedCandidateProjectId;
    const char* actualCandidateProjectId;
    const char* caller;
    const char* lastReentryCaller;
    const char* manifestRole;
    const char* manifestPath;
    ProjectErrorCode error;
    const char* path;
    const ManifestValidationDiagnostic* manifestDiagnostic;
};

using WorkspaceProjectOpenObserver = void (*)(void* userData,
                                               const WorkspaceProjectOpenEvent& event);
using WorkspaceProjectLoadTrace = void (*)(void* userData,
                                           const WorkspaceProjectOpenEvent& event);

struct WorkspaceController {
    WorkspaceModel model;
    WorkspaceFileSystem fileSystem;
    bool listingTruncated;
    ModelErrorCode lastError;
    ProjectErrorCode lastProjectError;
    SymbolDatabase* symbolDatabase;
    uint64_t projectOpenRequestId;
    uint64_t projectOpenRequestGeneration;
    uint64_t projectOpenTransactionId;
    uint64_t projectOpenTransactionGeneration;
    uint64_t projectOpenGeneration;
    WorkspaceProjectOpenState projectOpenState;
    bool projectOpenInProgress;
    uint64_t projectOpenCandidateId;
    uint64_t projectOpenCandidateGeneration;
    uint64_t projectOpenCandidateProjectGeneration;
    uint64_t projectOpenRefreshGeneration;
    uint32_t projectOpenEntryDepth;
    uint32_t projectOpenMaximumEntryDepth;
    uint32_t projectOpenReentryCount;
    uint32_t projectOpenManifestValidationCount;
    uint32_t projectOpenLoadStageCount;
    uint32_t projectOpenRefreshCount;
    uint32_t projectOpenCommitCount;
    uint32_t projectOpenReleaseCount;
    WorkspaceProjectLoadOwnershipResult lastProjectLoadOwnershipResult;
    char projectOpenCaller[48];
    char projectOpenLastReentryCaller[48];
    ProjectErrorCode projectOpenFailure;
    ProjectLoadScratch projectLoadScratch;
    ManifestValidationDiagnostic lastManifestDiagnostic;
    WorkspaceProjectOpenObserver projectOpenObserver;
    void* projectOpenObserverUserData;
    WorkspaceProjectLoadTrace projectLoadTrace;
    void* projectLoadTraceUserData;
};

void WorkspaceControllerInit(WorkspaceController* controller, const WorkspaceFileSystem& fileSystem);
void WorkspaceControllerAttachSymbolDatabase(WorkspaceController* controller, SymbolDatabase* database);
void WorkspaceControllerSetProjectOpenObserver(WorkspaceController* controller,
                                               WorkspaceProjectOpenObserver observer,
                                               void* userData);
void WorkspaceControllerSetProjectLoadTrace(WorkspaceController* controller,
                                            WorkspaceProjectLoadTrace trace,
                                            void* userData);
WorkspaceProjectLoadOwnershipResult WorkspaceControllerCheckProjectLoadOwnership(
    const WorkspaceController* controller, uint64_t requestId);
WorkspaceProjectOpenState WorkspaceControllerProjectOpenState(const WorkspaceController* controller);
const char* WorkspaceProjectOpenStateName(WorkspaceProjectOpenState state);
bool WorkspaceControllerOpenWorkspace(WorkspaceController* controller, const char* path);
bool WorkspaceControllerOpenProject(WorkspaceController* controller, const char* path);
bool WorkspaceControllerOpenProjectFrom(WorkspaceController* controller, const char* path,
                                        const char* caller);
bool WorkspaceControllerCreateProject(WorkspaceController* controller, const ProjectCreateRequest& request, ProjectOperationResult* result);
bool WorkspaceControllerReloadProject(WorkspaceController* controller);
bool WorkspaceControllerRefresh(WorkspaceController* controller);
bool WorkspaceControllerEnumerateProjectSources(const WorkspaceController* controller,
                                                ProjectSourceFile* files,
                                                uint32_t capacity,
                                                uint32_t* outCount);
bool WorkspaceControllerEnterSelected(WorkspaceController* controller);
bool WorkspaceControllerGoUp(WorkspaceController* controller);
bool WorkspaceControllerOpenDocument(WorkspaceController* controller, const char* path);
bool IsAppModelDocumentPathSupported(const char* path);
bool WorkspaceControllerOpenStandaloneDocument(WorkspaceController* controller, const char* path);
bool WorkspaceControllerUpdateDocumentSymbols(WorkspaceController* controller, uint32_t documentIndex);
bool WorkspaceControllerOpenDocumentAtLocation(WorkspaceController* controller, const char* projectId, const char* relativePath,
                                               uint32_t line, uint32_t column, uint32_t* outDocumentIndex, OutputErrorCode* error);
bool WorkspaceControllerSetCaretPosition(WorkspaceController* controller, uint32_t documentIndex, uint32_t line, uint32_t column,
                                          bool* outLocationClamped, OutputErrorCode* error);
bool WorkspaceControllerSaveDocument(WorkspaceController* controller, uint32_t documentIndex);
bool WorkspaceControllerSaveActive(WorkspaceController* controller);
bool WorkspaceControllerSaveAll(WorkspaceController* controller);
bool WorkspaceControllerHasDirtyProjectDocuments(const WorkspaceController* controller);
bool WorkspaceControllerSaveAllProjectDocuments(WorkspaceController* controller);
bool WorkspaceControllerCloseDocument(WorkspaceController* controller, uint32_t documentIndex, CloseDecision decision);
bool WorkspaceControllerCloseWorkspace(WorkspaceController* controller, CloseDecision decision);
Document* WorkspaceControllerActiveDocument(WorkspaceController* controller);
const Document* WorkspaceControllerActiveDocumentConst(const WorkspaceController* controller);
const char* WorkspaceControllerError(const WorkspaceController* controller);
const char* WorkspaceControllerProjectError(const WorkspaceController* controller);

} // namespace developer_studio
} // namespace guidexos
