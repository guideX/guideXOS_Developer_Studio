#include "developer_studio_workspace.h"

namespace guidexos {
namespace developer_studio {
namespace {

static FileListEntry g_workspaceRefreshEntries[kMaxWorkspaceEntries];
static char g_workspaceDocumentReadBuffer[kMaxEditorBytes + 1];

struct WorkspaceProjectLoadTransaction {
    WorkspaceController* owner;
    uint64_t transactionId;
    uint64_t transactionGeneration;
    uint64_t requestId;
    uint64_t requestGeneration;
    uint64_t candidateId;
    uint64_t candidateGeneration;
    uint64_t candidateProjectGeneration;
    uint64_t refreshGeneration;
    uint64_t previousActiveProjectGeneration;
    uint32_t releaseCount;
    char previousActiveProjectId[kMaxProjectIdBytes];
    char candidateProjectId[kMaxProjectIdBytes];
    bool active;
    bool committed;
    WorkspaceModel candidate;
};

static WorkspaceProjectLoadTransaction g_workspaceProjectLoad = {};
static uint64_t g_nextWorkspaceProjectRequestGeneration = 1;
static uint64_t g_nextWorkspaceProjectCandidateId = 1;
static uint64_t g_nextWorkspaceProjectCandidateGeneration = 1;
static uint64_t g_nextWorkspaceProjectRefreshGeneration = 1;
static uint64_t g_nextWorkspaceProjectTransactionId = 1;
static uint64_t g_nextWorkspaceProjectTransactionGeneration = 1;
static uint32_t g_workspaceProjectOpenEntryDepth = 0;

static void setControllerError(WorkspaceController* controller, ModelErrorCode code);
static void setModelError(WorkspaceModel* model, ModelErrorCode code);
static void setProjectError(WorkspaceController* controller, ProjectErrorCode code);
static bool pathForBrowse(const WorkspaceModel& model, char* output, uint32_t outputSize);
static bool copyEntryName(char* output, uint32_t outputSize, const char* input);
static bool equalIdentifier(const char* left, const char* right);

static WorkspaceProjectLoadOwnershipResult projectLoadOwnershipResult(
    const WorkspaceController* controller, uint64_t requestId) {
    const WorkspaceProjectLoadTransaction& transaction = g_workspaceProjectLoad;
    if (!controller) return WorkspaceProjectLoadOwnershipResult::NoController;
    if (!transaction.active) return WorkspaceProjectLoadOwnershipResult::TransactionNotActive;
    if (transaction.owner != controller) return WorkspaceProjectLoadOwnershipResult::TransactionOwnerMismatch;
    if (!controller->projectOpenInProgress) return WorkspaceProjectLoadOwnershipResult::ControllerNotInProgress;
    if (transaction.transactionId != controller->projectOpenTransactionId)
        return WorkspaceProjectLoadOwnershipResult::TransactionIdMismatch;
    if (transaction.transactionGeneration != controller->projectOpenTransactionGeneration)
        return WorkspaceProjectLoadOwnershipResult::TransactionGenerationMismatch;
    if (transaction.requestId != requestId || transaction.requestId != controller->projectOpenRequestId)
        return WorkspaceProjectLoadOwnershipResult::RequestIdMismatch;
    if (transaction.requestGeneration != controller->projectOpenRequestGeneration)
        return WorkspaceProjectLoadOwnershipResult::RequestGenerationMismatch;
    if (transaction.candidateId != controller->projectOpenCandidateId)
        return WorkspaceProjectLoadOwnershipResult::CandidateIdMismatch;
    if (transaction.candidateGeneration != controller->projectOpenCandidateGeneration)
        return WorkspaceProjectLoadOwnershipResult::CandidateGenerationMismatch;
    if (transaction.candidateProjectGeneration != transaction.candidate.projectGeneration ||
        transaction.candidateProjectGeneration != controller->projectOpenCandidateProjectGeneration)
        return WorkspaceProjectLoadOwnershipResult::CandidateProjectGenerationMismatch;
    if (transaction.candidateProjectGeneration != 0 &&
        !equalIdentifier(transaction.candidateProjectId, transaction.candidate.project.projectId))
        return WorkspaceProjectLoadOwnershipResult::CandidateProjectIdMismatch;
    if (transaction.refreshGeneration != controller->projectOpenRefreshGeneration)
        return WorkspaceProjectLoadOwnershipResult::RefreshGenerationMismatch;
    const char* expectedActiveId = transaction.committed ? transaction.candidateProjectId :
        transaction.previousActiveProjectId;
    const uint64_t expectedActiveGeneration = transaction.committed ? transaction.candidateProjectGeneration :
        transaction.previousActiveProjectGeneration;
    const char* actualActiveId = controller->model.hasProject ? controller->model.project.projectId : "";
    if (!equalIdentifier(expectedActiveId, actualActiveId))
        return WorkspaceProjectLoadOwnershipResult::ActiveProjectIdMismatch;
    if (controller->model.projectGeneration != expectedActiveGeneration)
        return WorkspaceProjectLoadOwnershipResult::ActiveProjectGenerationMismatch;
    return WorkspaceProjectLoadOwnershipResult::Current;
}

static bool projectLoadIsCurrent(WorkspaceController* controller, uint64_t requestId) {
    const WorkspaceProjectLoadOwnershipResult result = projectLoadOwnershipResult(controller, requestId);
    if (controller) controller->lastProjectLoadOwnershipResult = result;
    return result == WorkspaceProjectLoadOwnershipResult::Current;
}

static uint64_t nextWorkspaceProjectId(uint64_t* value) {
    if (!value) return 0;
    const uint64_t result = *value == 0 ? 1 : *value;
    *value = result == UINT64_MAX ? 1 : result + 1;
    return result;
}

struct WorkspaceProjectOpenEntryScope {
    WorkspaceProjectOpenEntryScope() {
        if (g_workspaceProjectOpenEntryDepth != UINT32_MAX) ++g_workspaceProjectOpenEntryDepth;
    }
    ~WorkspaceProjectOpenEntryScope() {
        if (g_workspaceProjectOpenEntryDepth != 0) --g_workspaceProjectOpenEntryDepth;
    }
};

static WorkspaceProjectOpenEvent makeProjectOpenEvent(WorkspaceController* controller,
    WorkspaceProjectLoadCheckpoint checkpoint, const char* path, ProjectErrorCode error,
    uint64_t requestId);

static void emitProjectLoadTrace(WorkspaceController* controller,
    WorkspaceProjectLoadCheckpoint checkpoint, const char* path, ProjectErrorCode error,
    uint64_t requestId) {
    if (!controller || !controller->projectLoadTrace) return;
    const WorkspaceProjectOpenEvent event = makeProjectOpenEvent(controller, checkpoint, path, error, requestId);
    controller->lastProjectLoadOwnershipResult = event.ownershipResult;
    controller->projectLoadTrace(controller->projectLoadTraceUserData, event);
}

static void releaseProjectLoadTransaction(WorkspaceController* controller) {
    if (!controller) return;
    if (g_workspaceProjectLoad.active && g_workspaceProjectLoad.owner == controller &&
        g_workspaceProjectLoad.releaseCount == 0) {
        emitProjectLoadTrace(controller, WorkspaceProjectLoadCheckpoint::TransactionRelease,
                             controller->model.rootPath, controller->projectOpenFailure,
                             controller->projectOpenRequestId);
        ++g_workspaceProjectLoad.releaseCount;
        controller->projectOpenReleaseCount = 1;
        controller->projectOpenInProgress = false;
        g_workspaceProjectLoad.active = false;
        emitProjectLoadTrace(controller, WorkspaceProjectLoadCheckpoint::TransactionReleased,
                             controller->model.rootPath, controller->projectOpenFailure,
                             controller->projectOpenRequestId);
        g_workspaceProjectLoad.owner = nullptr;
        WorkspaceModelInit(&g_workspaceProjectLoad.candidate);
    } else if (g_workspaceProjectLoad.owner != controller) {
        // A stale caller may settle its own controller flag, but cannot release
        // a transaction now owned by another controller.
        controller->projectOpenInProgress = false;
    }
}

static void notifyProjectOpen(WorkspaceController* controller, WorkspaceProjectOpenState state,
                              const char* path, ProjectErrorCode error) {
    if (!controller) return;
    controller->projectOpenState = state;
    if (g_workspaceProjectLoad.active && g_workspaceProjectLoad.owner == controller &&
        controller->projectOpenLoadStageCount != UINT32_MAX)
        ++controller->projectOpenLoadStageCount;
    WorkspaceProjectLoadCheckpoint checkpoint = WorkspaceProjectLoadCheckpoint::Loaded;
    switch (state) {
    case WorkspaceProjectOpenState::LoadStarted: checkpoint = WorkspaceProjectLoadCheckpoint::LoadStarted; break;
    case WorkspaceProjectOpenState::Loaded: checkpoint = WorkspaceProjectLoadCheckpoint::Loaded; break;
    case WorkspaceProjectOpenState::CandidateAllocated: checkpoint = WorkspaceProjectLoadCheckpoint::CandidateCreated; break;
    case WorkspaceProjectOpenState::RefreshStarted: checkpoint = WorkspaceProjectLoadCheckpoint::RefreshStarted; break;
    case WorkspaceProjectOpenState::Validated: checkpoint = WorkspaceProjectLoadCheckpoint::AfterRefresh; break;
    case WorkspaceProjectOpenState::Committing: checkpoint = WorkspaceProjectLoadCheckpoint::CommitStarting; break;
    case WorkspaceProjectOpenState::Active: checkpoint = WorkspaceProjectLoadCheckpoint::ActivePublished; break;
    case WorkspaceProjectOpenState::Ready: checkpoint = WorkspaceProjectLoadCheckpoint::Ready; break;
    case WorkspaceProjectOpenState::Failed: checkpoint = WorkspaceProjectLoadCheckpoint::FailureRollback; break;
    default: break;
    }
    emitProjectLoadTrace(controller, checkpoint, path, error, controller->projectOpenRequestId);
    if (controller->projectOpenObserver) {
        emitProjectLoadTrace(controller, WorkspaceProjectLoadCheckpoint::ObserverCallback,
                             path, error, controller->projectOpenRequestId);
        const WorkspaceProjectOpenEvent event = makeProjectOpenEvent(controller, checkpoint, path, error,
                                                                       controller->projectOpenRequestId);
        controller->projectOpenObserver(controller->projectOpenObserverUserData, event);
        emitProjectLoadTrace(controller, WorkspaceProjectLoadCheckpoint::ObserverReturn,
                             path, error, controller->projectOpenRequestId);
    }
}

static WorkspaceProjectOpenEvent makeProjectOpenEvent(WorkspaceController* controller,
    WorkspaceProjectLoadCheckpoint checkpoint, const char* path, ProjectErrorCode error,
    uint64_t requestId) {
    WorkspaceProjectOpenEvent event = {};
    if (!controller) return event;
    const WorkspaceProjectLoadTransaction& transaction = g_workspaceProjectLoad;
    event.state = controller->projectOpenState;
    event.checkpoint = checkpoint;
    event.ownershipResult = projectLoadOwnershipResult(controller, requestId);
    controller->lastProjectLoadOwnershipResult = event.ownershipResult;
    event.checkedRequestId = requestId;
    event.transactionId = controller->projectOpenTransactionId;
    event.transactionGeneration = controller->projectOpenTransactionGeneration;
    event.requestId = controller->projectOpenRequestId;
    event.requestGeneration = controller->projectOpenRequestGeneration;
    event.activeProjectGeneration = controller->model.projectGeneration;
    event.actualActiveProjectGeneration = controller->model.projectGeneration;
    event.candidateId = controller->projectOpenCandidateId;
    event.candidateGeneration = controller->projectOpenCandidateGeneration;
    // Keep the released candidate's generation in the settlement tuple. The
    // inactive result explains that it no longer owns a live transaction.
    event.candidateProjectGeneration = transaction.candidate.projectGeneration;
    event.refreshGeneration = controller->projectOpenRefreshGeneration;
    event.expectedTransactionId = transaction.transactionId;
    event.expectedTransactionGeneration = transaction.transactionGeneration;
    event.expectedRequestId = transaction.requestId;
    event.expectedRequestGeneration = transaction.requestGeneration;
    event.expectedCandidateId = transaction.candidateId;
    event.expectedCandidateGeneration = transaction.candidateGeneration;
    event.expectedCandidateProjectGeneration = transaction.candidateProjectGeneration;
    event.expectedRefreshGeneration = transaction.refreshGeneration;
    event.expectedActiveProjectGeneration = transaction.committed ? transaction.candidateProjectGeneration :
        transaction.previousActiveProjectGeneration;
    event.expectedActiveProjectId = transaction.committed ? transaction.candidateProjectId :
        transaction.previousActiveProjectId;
    event.actualActiveProjectId = controller->model.hasProject ? controller->model.project.projectId : "";
    event.expectedCandidateProjectId = transaction.candidateProjectId;
    event.actualCandidateProjectId = transaction.candidate.project.projectId;
    event.entryDepth = g_workspaceProjectOpenEntryDepth;
    event.maximumEntryDepth = controller->projectOpenMaximumEntryDepth;
    event.reentryCount = controller->projectOpenReentryCount;
    event.manifestValidationCount = controller->projectOpenManifestValidationCount;
    event.loadStageCount = controller->projectOpenLoadStageCount;
    event.refreshCount = controller->projectOpenRefreshCount;
    event.commitCount = controller->projectOpenCommitCount;
    event.releaseCount = controller->projectOpenReleaseCount;
    event.transactionActive = transaction.active;
    event.controllerLoadInProgress = controller->projectOpenInProgress;
    event.transactionOwnerPointerMatches = transaction.owner == controller;
    event.transactionIdMatches = transaction.transactionId == controller->projectOpenTransactionId;
    event.transactionGenerationMatches = transaction.transactionGeneration == controller->projectOpenTransactionGeneration;
    event.requestIdMatches = transaction.requestId == requestId && transaction.requestId == controller->projectOpenRequestId;
    event.requestGenerationMatches = transaction.requestGeneration == controller->projectOpenRequestGeneration;
    event.candidateIdMatches = transaction.candidateId == controller->projectOpenCandidateId;
    event.candidateGenerationMatches = transaction.candidateGeneration == controller->projectOpenCandidateGeneration;
    event.candidateProjectGenerationMatches =
        transaction.candidateProjectGeneration == transaction.candidate.projectGeneration &&
        transaction.candidateProjectGeneration == controller->projectOpenCandidateProjectGeneration;
    event.candidateProjectIdMatches = transaction.candidateProjectGeneration == 0 ||
        equalIdentifier(transaction.candidateProjectId, transaction.candidate.project.projectId);
    event.refreshGenerationMatches = transaction.refreshGeneration == controller->projectOpenRefreshGeneration;
    event.activeProjectIdMatches = equalIdentifier(event.expectedActiveProjectId, event.actualActiveProjectId);
    event.activeProjectGenerationMatches =
        event.expectedActiveProjectGeneration == event.actualActiveProjectGeneration;
    event.transactionCommitted = transaction.committed;
    event.transactionOwnerMatches = event.ownershipResult == WorkspaceProjectLoadOwnershipResult::Current;
    event.caller = controller->projectOpenCaller;
    event.lastReentryCaller = controller->projectOpenLastReentryCaller;
    event.manifestRole = controller->lastManifestDiagnostic.validationRole;
    event.manifestPath = controller->lastManifestDiagnostic.manifestPath;
    event.error = error;
    event.path = path;
    event.manifestDiagnostic = &controller->lastManifestDiagnostic;
    return event;
}

static bool refreshWorkspaceModel(WorkspaceController* controller, WorkspaceModel* model,
                                  bool* outTruncated, bool publishControllerError) {
    if (!controller || !model || !model->open || !controller->fileSystem.list) {
        if (model) setModelError(model, ModelErrorCode::WorkspaceNotOpen);
        if (publishControllerError && controller) setControllerError(controller, ModelErrorCode::WorkspaceNotOpen);
        return false;
    }
    char directory[kMaxPathBytes];
    if (!pathForBrowse(*model, directory, sizeof(directory))) {
        setModelError(model, ModelErrorCode::InvalidPath);
        if (publishControllerError) setControllerError(controller, ModelErrorCode::InvalidPath);
        return false;
    }
    FileListEntry* entries = g_workspaceRefreshEntries;
    bool truncated = false;
    uint32_t count = 0;
    if (!controller->fileSystem.list(controller->fileSystem.userData, directory, entries,
                                     kMaxWorkspaceEntries, &count, &truncated)) {
        setModelError(model, ModelErrorCode::ReadFailed);
        if (publishControllerError) setControllerError(controller, ModelErrorCode::ReadFailed);
        return false;
    }
    WorkspaceModelClearEntries(model);
    if (outTruncated) *outTruncated = truncated;
    for (uint32_t i = 0; i < count && i < kMaxWorkspaceEntries; ++i) {
        if (entries[i].name[0] == '\0' || PathContainsTraversal(entries[i].name)) continue;
        WorkspaceEntry entry;
        for (uint32_t j = 0; j < sizeof(entry.name); ++j) entry.name[j] = '\0';
        for (uint32_t j = 0; j < sizeof(entry.relativePath); ++j) entry.relativePath[j] = '\0';
        if (!copyEntryName(entry.name, sizeof(entry.name), entries[i].name)) continue;
        uint32_t browseLength = 0;
        while (browseLength + 1 < sizeof(model->browsePath) && model->browsePath[browseLength] != '\0') ++browseLength;
        uint32_t nameLength = 0;
        while (nameLength + 1 < sizeof(entry.name) && entry.name[nameLength] != '\0') ++nameLength;
        char relative[kMaxPathBytes];
        uint32_t out = 0;
        for (uint32_t j = 0; j < browseLength && out + 1 < sizeof(relative); ++j) relative[out++] = model->browsePath[j];
        if (browseLength > 0 && out + 1 < sizeof(relative)) relative[out++] = '/';
        for (uint32_t j = 0; j < nameLength && out + 1 < sizeof(relative); ++j) relative[out++] = entry.name[j];
        relative[out] = '\0';
        if (!copyEntryName(entry.relativePath, sizeof(entry.relativePath), relative)) continue;
        entry.size = entries[i].size;
        entry.depth = 0;
        for (uint32_t j = 0; relative[j] != '\0'; ++j) if (relative[j] == '/') ++entry.depth;
        entry.kind = entries[i].kind == FileInfoKind::Directory ? WorkspaceEntryKind::Directory :
            (IsSupportedTextPath(entry.name) ? WorkspaceEntryKind::SupportedTextFile : WorkspaceEntryKind::UnsupportedFile);
        WorkspaceModelAddEntry(model, entry);
    }
    WorkspaceModelSortEntries(model);
    model->selectedEntry = model->entryCount == 0 ? 0 :
        (model->selectedEntry < model->entryCount ? model->selectedEntry : 0);
    setModelError(model, ModelErrorCode::None);
    if (publishControllerError) setControllerError(controller, ModelErrorCode::None);
    return true;
}

static void setControllerError(WorkspaceController* controller, ModelErrorCode code) {
    if (!controller) return;
    controller->lastError = code;
    setModelError(&controller->model, code);
}

static void setModelError(WorkspaceModel* model, ModelErrorCode code) {
    if (!model) return;
    for (uint32_t i = 0; i + 1 < sizeof(model->lastError); ++i) {
        model->lastError[i] = ModelErrorName(code)[i];
        if (ModelErrorName(code)[i] == '\0') return;
    }
    model->lastError[sizeof(model->lastError) - 1] = '\0';
}

static void setProjectError(WorkspaceController* controller, ProjectErrorCode code) {
    if (!controller) return;
    controller->lastProjectError = code;
}

static bool rejectProjectLoadReentry(WorkspaceController* controller) {
    if (!controller || !controller->projectOpenInProgress) return false;
    setProjectError(controller, ProjectErrorCode::LoadInProgress);
    return true;
}

static bool pathForBrowse(const WorkspaceModel& model, char* output, uint32_t outputSize) {
    if (model.browsePath[0] == '\0') return NormalizePath(model.rootPath, output, outputSize);
    return JoinWorkspacePath(model.rootPath, model.browsePath, output, outputSize);
}

static bool absolutePathForDocument(const WorkspaceModel& model, const char* path, char* output, uint32_t outputSize) {
    if (!path || path[0] == '\0') return false;
    if (path[0] == '/' || path[0] == static_cast<char>(92) || (path[1] == ':')) {
        return NormalizePath(path, output, outputSize);
    }
    return JoinWorkspacePath(model.rootPath, path, output, outputSize);
}

static bool copyEntryName(char* output, uint32_t outputSize, const char* input) {
    if (!output || !input || outputSize == 0) return false;
    uint32_t i = 0;
    while (i + 1 < outputSize && input[i] != '\0') { output[i] = input[i]; ++i; }
    if (input[i] != '\0') { output[0] = '\0'; return false; }
    output[i] = '\0';
    return true;
}

static bool isWithinRoot(const WorkspaceModel& model, const char* path) {
    char probe[kMaxPathBytes];
    if (!NormalizePath(path, probe, sizeof(probe))) return false;
    uint32_t rootLength = 0;
    while (rootLength < kMaxPathBytes && model.rootPath[rootLength] != '\0') ++rootLength;
    uint32_t pathLength = 0;
    while (pathLength < kMaxPathBytes && probe[pathLength] != '\0') ++pathLength;
    if (rootLength > pathLength) return false;
    for (uint32_t i = 0; i < rootLength; ++i) {
        char a = model.rootPath[i];
        char b = probe[i];
        if (a >= 'A' && a <= 'Z') a = static_cast<char>(a + ('a' - 'A'));
        if (b >= 'A' && b <= 'Z') b = static_cast<char>(b + ('a' - 'A'));
        if (a != b) return false;
    }
    return pathLength == rootLength || probe[rootLength] == '/';
}

static bool equalIdentifier(const char* left, const char* right) {
    if (!left || !right) return left == right;
    uint32_t index = 0;
    while (left[index] != '\0' && right[index] != '\0') {
        if (left[index] != right[index]) return false;
        ++index;
    }
    return left[index] == right[index];
}

static bool sourceExtension(const char* name) {
    if (!name) return false;
    uint32_t length = 0;
    while (length < kMaxNameBytes && name[length] != '\0') ++length;
    if (length >= 2 && name[length - 2] == '.' &&
        (name[length - 1] == 'c' || name[length - 1] == 'C')) return true;
    return length >= 4 && name[length - 4] == '.' &&
        (name[length - 3] == 'c' || name[length - 3] == 'C') &&
        (name[length - 2] == 'p' || name[length - 2] == 'P') &&
        (name[length - 1] == 'p' || name[length - 1] == 'P');
}

static bool pathBefore(const char* left, const char* right) {
    uint32_t i = 0;
    while (left && right && left[i] && right[i] && left[i] == right[i]) ++i;
    return static_cast<unsigned char>(left && left[i] ? left[i] : 0) <
           static_cast<unsigned char>(right && right[i] ? right[i] : 0);
}

} // namespace

const char* WorkspaceProjectLoadOwnershipResultName(WorkspaceProjectLoadOwnershipResult result) {
    switch (result) {
    case WorkspaceProjectLoadOwnershipResult::Current: return "CURRENT";
    case WorkspaceProjectLoadOwnershipResult::NoController: return "NO_CONTROLLER";
    case WorkspaceProjectLoadOwnershipResult::TransactionNotActive: return "TRANSACTION_NOT_ACTIVE";
    case WorkspaceProjectLoadOwnershipResult::ControllerNotInProgress: return "CONTROLLER_NOT_IN_PROGRESS";
    case WorkspaceProjectLoadOwnershipResult::TransactionOwnerMismatch: return "TRANSACTION_OWNER_MISMATCH";
    case WorkspaceProjectLoadOwnershipResult::RequestIdMismatch: return "REQUEST_ID_MISMATCH";
    case WorkspaceProjectLoadOwnershipResult::RequestGenerationMismatch: return "REQUEST_GENERATION_MISMATCH";
    case WorkspaceProjectLoadOwnershipResult::TransactionIdMismatch: return "TRANSACTION_ID_MISMATCH";
    case WorkspaceProjectLoadOwnershipResult::TransactionGenerationMismatch: return "TRANSACTION_GENERATION_MISMATCH";
    case WorkspaceProjectLoadOwnershipResult::CandidateIdMismatch: return "CANDIDATE_ID_MISMATCH";
    case WorkspaceProjectLoadOwnershipResult::CandidateGenerationMismatch: return "CANDIDATE_GENERATION_MISMATCH";
    case WorkspaceProjectLoadOwnershipResult::CandidateProjectGenerationMismatch: return "CANDIDATE_PROJECT_GENERATION_MISMATCH";
    case WorkspaceProjectLoadOwnershipResult::CandidateProjectIdMismatch: return "CANDIDATE_PROJECT_ID_MISMATCH";
    case WorkspaceProjectLoadOwnershipResult::RefreshGenerationMismatch: return "REFRESH_GENERATION_MISMATCH";
    case WorkspaceProjectLoadOwnershipResult::ActiveProjectIdMismatch: return "ACTIVE_PROJECT_ID_MISMATCH";
    case WorkspaceProjectLoadOwnershipResult::ActiveProjectGenerationMismatch: return "ACTIVE_PROJECT_GENERATION_MISMATCH";
    }
    return "UNKNOWN";
}

const char* WorkspaceProjectLoadCheckpointName(WorkspaceProjectLoadCheckpoint checkpoint) {
    switch (checkpoint) {
    case WorkspaceProjectLoadCheckpoint::RequestAccepted: return "request_accepted";
    case WorkspaceProjectLoadCheckpoint::TransactionCreated: return "transaction_created";
    case WorkspaceProjectLoadCheckpoint::CandidateCreated: return "candidate_created";
    case WorkspaceProjectLoadCheckpoint::LoadStarted: return "load_started";
    case WorkspaceProjectLoadCheckpoint::LoadStartedReturned: return "load_started_return";
    case WorkspaceProjectLoadCheckpoint::LoadStartedOwnerCheck: return "owner_check_current";
    case WorkspaceProjectLoadCheckpoint::MetadataPathBegin: return "metadata_path_begin";
    case WorkspaceProjectLoadCheckpoint::MetadataPathReady: return "metadata_path_ready";
    case WorkspaceProjectLoadCheckpoint::MetadataStatCall: return "metadata_stat_call";
    case WorkspaceProjectLoadCheckpoint::ProjectMetadataValidated: return "metadata_validated";
    case WorkspaceProjectLoadCheckpoint::ApplicationManifestValidated: return "manifest_validated";
    case WorkspaceProjectLoadCheckpoint::Loaded: return "loaded";
    case WorkspaceProjectLoadCheckpoint::RefreshStarted: return "refresh_started";
    case WorkspaceProjectLoadCheckpoint::BeforeRefresh: return "before_refresh";
    case WorkspaceProjectLoadCheckpoint::AfterRefresh: return "after_refresh";
    case WorkspaceProjectLoadCheckpoint::CommitStarting: return "commit_starting";
    case WorkspaceProjectLoadCheckpoint::BeforeCommit: return "before_commit";
    case WorkspaceProjectLoadCheckpoint::AfterCommit: return "after_commit";
    case WorkspaceProjectLoadCheckpoint::ActivePublished: return "active_published";
    case WorkspaceProjectLoadCheckpoint::ObserverCallback: return "observer_callback";
    case WorkspaceProjectLoadCheckpoint::ObserverReturn: return "observer_return";
    case WorkspaceProjectLoadCheckpoint::Ready: return "ready";
    case WorkspaceProjectLoadCheckpoint::FailureRollback: return "failure_rollback";
    case WorkspaceProjectLoadCheckpoint::TransactionRelease: return "transaction_release";
    case WorkspaceProjectLoadCheckpoint::TransactionReleased: return "transaction_released";
    }
    return "unknown";
}

void WorkspaceControllerInit(WorkspaceController* controller, const WorkspaceFileSystem& fileSystem) {
    if (!controller) return;
    WorkspaceModelInit(&controller->model);
    controller->fileSystem = fileSystem;
    controller->listingTruncated = false;
    controller->lastError = ModelErrorCode::None;
    controller->lastProjectError = ProjectErrorCode::None;
    controller->symbolDatabase = nullptr;
    controller->projectOpenRequestId = 0;
    controller->projectOpenRequestGeneration = 0;
    controller->projectOpenTransactionId = 0;
    controller->projectOpenTransactionGeneration = 0;
    controller->projectOpenGeneration = 0;
    controller->projectOpenState = WorkspaceProjectOpenState::Idle;
    controller->projectOpenInProgress = false;
    controller->projectOpenCandidateId = 0;
    controller->projectOpenCandidateGeneration = 0;
    controller->projectOpenCandidateProjectGeneration = 0;
    controller->projectOpenRefreshGeneration = 0;
    controller->projectOpenEntryDepth = 0;
    controller->projectOpenMaximumEntryDepth = 0;
    controller->projectOpenReentryCount = 0;
    controller->projectOpenManifestValidationCount = 0;
    controller->projectOpenLoadStageCount = 0;
    controller->projectOpenRefreshCount = 0;
    controller->projectOpenCommitCount = 0;
    controller->projectOpenReleaseCount = 0;
    controller->lastProjectLoadOwnershipResult = WorkspaceProjectLoadOwnershipResult::TransactionNotActive;
    __builtin_memset(controller->projectOpenCaller, 0, sizeof(controller->projectOpenCaller));
    __builtin_memset(controller->projectOpenLastReentryCaller, 0, sizeof(controller->projectOpenLastReentryCaller));
    controller->projectOpenFailure = ProjectErrorCode::None;
    __builtin_memset(&controller->projectLoadScratch, 0, sizeof(controller->projectLoadScratch));
    __builtin_memset(&controller->lastManifestDiagnostic, 0, sizeof(controller->lastManifestDiagnostic));
    controller->projectOpenObserver = nullptr;
    controller->projectOpenObserverUserData = nullptr;
    controller->projectLoadTrace = nullptr;
    controller->projectLoadTraceUserData = nullptr;
}

void WorkspaceControllerAttachSymbolDatabase(WorkspaceController* controller, SymbolDatabase* database) {
    if (!controller) return;
#if defined(GXOS_DEVELOPER_STUDIO_BARE_METAL)
    (void)database;
    controller->symbolDatabase = nullptr;
#else
    controller->symbolDatabase = database;
    if (database) {
        SymbolDatabaseClear(database);
        if (controller->model.open && controller->model.hasProject)
            SymbolDatabaseIndexProject(database, controller->fileSystem, controller->model.rootPath,
                                        controller->model.documents, kMaxOpenDocuments,
                                        controller->model.projectGeneration);
    }
#endif
}

void WorkspaceControllerSetProjectOpenObserver(WorkspaceController* controller,
                                               WorkspaceProjectOpenObserver observer,
                                               void* userData) {
    if (!controller) return;
    controller->projectOpenObserver = observer;
    controller->projectOpenObserverUserData = userData;
}

void WorkspaceControllerSetProjectLoadTrace(WorkspaceController* controller,
                                            WorkspaceProjectLoadTrace trace,
                                            void* userData) {
    if (!controller) return;
    controller->projectLoadTrace = trace;
    controller->projectLoadTraceUserData = userData;
}

WorkspaceProjectLoadOwnershipResult WorkspaceControllerCheckProjectLoadOwnership(
    const WorkspaceController* controller, uint64_t requestId) {
    return projectLoadOwnershipResult(controller, requestId);
}

WorkspaceProjectOpenState WorkspaceControllerProjectOpenState(const WorkspaceController* controller) {
    return controller ? controller->projectOpenState : WorkspaceProjectOpenState::Failed;
}

const char* WorkspaceProjectOpenStateName(WorkspaceProjectOpenState state) {
    switch (state) {
    case WorkspaceProjectOpenState::Idle: return "idle";
    case WorkspaceProjectOpenState::LoadStarted: return "load_started";
    case WorkspaceProjectOpenState::Loaded: return "loaded";
    case WorkspaceProjectOpenState::CandidateAllocated: return "candidate_allocated";
    case WorkspaceProjectOpenState::RefreshStarted: return "refresh_started";
    case WorkspaceProjectOpenState::Validated: return "validated";
    case WorkspaceProjectOpenState::Committing: return "committing";
    case WorkspaceProjectOpenState::Active: return "active";
    case WorkspaceProjectOpenState::Ready: return "ready";
    case WorkspaceProjectOpenState::Failed: return "failed";
    }
    return "unknown";
}

static void workspaceProjectLoadCheckpoint(void* userData, ProjectLoadCheckpoint checkpoint,
    const ManifestValidationDiagnostic* diagnostic, const char* path) {
    WorkspaceController* controller = static_cast<WorkspaceController*>(userData);
    if (!controller) return;
    WorkspaceProjectLoadCheckpoint workspaceCheckpoint;
    switch (checkpoint) {
    case ProjectLoadCheckpoint::MetadataPathBegin:
        workspaceCheckpoint = WorkspaceProjectLoadCheckpoint::MetadataPathBegin;
        break;
    case ProjectLoadCheckpoint::MetadataPathReady:
        workspaceCheckpoint = WorkspaceProjectLoadCheckpoint::MetadataPathReady;
        break;
    case ProjectLoadCheckpoint::MetadataStatCall:
        workspaceCheckpoint = WorkspaceProjectLoadCheckpoint::MetadataStatCall;
        break;
    case ProjectLoadCheckpoint::ProjectMetadataValidated:
        if (!diagnostic) return;
        workspaceCheckpoint = WorkspaceProjectLoadCheckpoint::ProjectMetadataValidated;
        controller->lastManifestDiagnostic = *diagnostic;
        controller->projectOpenManifestValidationCount = diagnostic->validationCount;
        break;
    case ProjectLoadCheckpoint::ApplicationManifestValidated:
        if (!diagnostic) return;
        workspaceCheckpoint = WorkspaceProjectLoadCheckpoint::ApplicationManifestValidated;
        controller->lastManifestDiagnostic = *diagnostic;
        controller->projectOpenManifestValidationCount = diagnostic->validationCount;
        break;
    default:
        return;
    }
    emitProjectLoadTrace(controller, workspaceCheckpoint, path ? path : controller->model.rootPath,
                         ProjectErrorCode::None, controller->projectOpenRequestId);
}

bool WorkspaceControllerOpenWorkspace(WorkspaceController* controller, const char* path) {
    if (!controller || !controller->fileSystem.stat || !controller->fileSystem.list) return false;
    if (controller->projectOpenInProgress) {
        setProjectError(controller, ProjectErrorCode::LoadInProgress);
        return false;
    }
    if (controller->model.open && WorkspaceModelHasDirtyDocuments(&controller->model)) {
        setControllerError(controller, ModelErrorCode::UnsavedChanges);
        return false;
    }
    char normalized[kMaxPathBytes];
    if (!NormalizePath(path, normalized, sizeof(normalized))) { setControllerError(controller, ModelErrorCode::InvalidPath); return false; }
    FileInfo info;
    if (!controller->fileSystem.stat(controller->fileSystem.userData, normalized, &info)) { setControllerError(controller, ModelErrorCode::ReadFailed); return false; }
    if (info.kind != FileInfoKind::Directory) { setControllerError(controller, ModelErrorCode::NotDirectory); return false; }
    const char* name = BaseName(normalized);
    if (!WorkspaceModelSetRoot(&controller->model, normalized, name[0] ? name : normalized)) { setControllerError(controller, ModelErrorCode::InvalidPath); return false; }
#if !defined(GXOS_DEVELOPER_STUDIO_BARE_METAL)
    if (controller->symbolDatabase) SymbolDatabaseClear(controller->symbolDatabase);
#endif
    if (!WorkspaceControllerRefresh(controller)) return false;
    return true;
}

bool WorkspaceControllerOpenProject(WorkspaceController* controller, const char* path) {
    return WorkspaceControllerOpenProjectFrom(controller, path, "workspace_api");
}

bool WorkspaceControllerOpenProjectFrom(WorkspaceController* controller, const char* path,
                                        const char* caller) {
    WorkspaceProjectOpenEntryScope entryScope;
    if (!controller || !controller->fileSystem.stat || !controller->fileSystem.read || !controller->fileSystem.list) {
        if (controller) setProjectError(controller, ProjectErrorCode::NullInput);
        return false;
    }
    if (controller->projectOpenInProgress || g_workspaceProjectLoad.active) {
        WorkspaceController* owner = g_workspaceProjectLoad.active ? g_workspaceProjectLoad.owner : controller;
        if (owner) {
            if (owner->projectOpenReentryCount != UINT32_MAX) ++owner->projectOpenReentryCount;
            if (g_workspaceProjectOpenEntryDepth > owner->projectOpenMaximumEntryDepth)
                owner->projectOpenMaximumEntryDepth = g_workspaceProjectOpenEntryDepth;
            const char* reentryCaller = caller ? caller : "workspace_api";
            (void)copyEntryName(owner->projectOpenLastReentryCaller,
                                sizeof(owner->projectOpenLastReentryCaller), reentryCaller);
        }
        setProjectError(controller, ProjectErrorCode::LoadInProgress);
        return false;
    }
    controller->projectOpenRequestId = controller->projectOpenRequestId == UINT64_MAX ? 1 :
        controller->projectOpenRequestId + 1;
    controller->projectOpenRequestGeneration = nextWorkspaceProjectId(&g_nextWorkspaceProjectRequestGeneration);
    controller->projectOpenTransactionId = nextWorkspaceProjectId(&g_nextWorkspaceProjectTransactionId);
    controller->projectOpenTransactionGeneration =
        nextWorkspaceProjectId(&g_nextWorkspaceProjectTransactionGeneration);
    controller->projectOpenGeneration = 0;
    controller->projectOpenInProgress = true;
    controller->projectOpenCandidateId = nextWorkspaceProjectId(&g_nextWorkspaceProjectCandidateId);
    controller->projectOpenCandidateGeneration = nextWorkspaceProjectId(&g_nextWorkspaceProjectCandidateGeneration);
    controller->projectOpenCandidateProjectGeneration = 0;
    controller->projectOpenRefreshGeneration = 0;
    controller->projectOpenEntryDepth = g_workspaceProjectOpenEntryDepth;
    controller->projectOpenMaximumEntryDepth = g_workspaceProjectOpenEntryDepth;
    controller->projectOpenReentryCount = 0;
    controller->projectOpenManifestValidationCount = 0;
    controller->projectOpenLoadStageCount = 0;
    controller->projectOpenRefreshCount = 0;
    controller->projectOpenCommitCount = 0;
    controller->projectOpenReleaseCount = 0;
    controller->projectOpenState = WorkspaceProjectOpenState::Idle;
    __builtin_memset(controller->projectOpenLastReentryCaller, 0,
                      sizeof(controller->projectOpenLastReentryCaller));
    (void)copyEntryName(controller->projectOpenCaller, sizeof(controller->projectOpenCaller),
                        caller ? caller : "workspace_api");
    controller->projectOpenFailure = ProjectErrorCode::None;
    __builtin_memset(&controller->lastManifestDiagnostic, 0, sizeof(controller->lastManifestDiagnostic));
    g_workspaceProjectLoad.owner = controller;
    g_workspaceProjectLoad.transactionId = controller->projectOpenTransactionId;
    g_workspaceProjectLoad.transactionGeneration = controller->projectOpenTransactionGeneration;
    g_workspaceProjectLoad.requestId = controller->projectOpenRequestId;
    g_workspaceProjectLoad.requestGeneration = controller->projectOpenRequestGeneration;
    g_workspaceProjectLoad.candidateId = controller->projectOpenCandidateId;
    g_workspaceProjectLoad.candidateGeneration = controller->projectOpenCandidateGeneration;
    g_workspaceProjectLoad.candidateProjectGeneration = 0;
    g_workspaceProjectLoad.refreshGeneration = 0;
    g_workspaceProjectLoad.previousActiveProjectGeneration = controller->model.projectGeneration;
    g_workspaceProjectLoad.releaseCount = 0;
    __builtin_memset(g_workspaceProjectLoad.previousActiveProjectId, 0,
                     sizeof(g_workspaceProjectLoad.previousActiveProjectId));
    __builtin_memset(g_workspaceProjectLoad.candidateProjectId, 0,
                     sizeof(g_workspaceProjectLoad.candidateProjectId));
    if (controller->model.hasProject)
        (void)copyEntryName(g_workspaceProjectLoad.previousActiveProjectId,
                            sizeof(g_workspaceProjectLoad.previousActiveProjectId),
                            controller->model.project.projectId);
    g_workspaceProjectLoad.active = true;
    g_workspaceProjectLoad.committed = false;
    WorkspaceModelInit(&g_workspaceProjectLoad.candidate);
    const uint64_t requestId = controller->projectOpenRequestId;
    ManifestValidationGeneration validationGeneration = {};
    validationGeneration.requestId = requestId;
    validationGeneration.requestGeneration = controller->projectOpenRequestGeneration;
    validationGeneration.candidateId = controller->projectOpenCandidateId;
    validationGeneration.candidateGeneration = controller->projectOpenCandidateGeneration;
    validationGeneration.expectedIdentityGeneration = controller->projectOpenCandidateGeneration;
    validationGeneration.parsedIdentityGeneration = controller->projectOpenCandidateGeneration;
    const char* requestPath = path;
    emitProjectLoadTrace(controller, WorkspaceProjectLoadCheckpoint::RequestAccepted,
                         requestPath, ProjectErrorCode::None, requestId);
    emitProjectLoadTrace(controller, WorkspaceProjectLoadCheckpoint::TransactionCreated,
                         requestPath, ProjectErrorCode::None, requestId);
    notifyProjectOpen(controller, WorkspaceProjectOpenState::LoadStarted, requestPath, ProjectErrorCode::None);
    emitProjectLoadTrace(controller, WorkspaceProjectLoadCheckpoint::LoadStartedReturned,
                         requestPath, ProjectErrorCode::None, requestId);
    controller->lastProjectLoadOwnershipResult = projectLoadOwnershipResult(controller, requestId);
    emitProjectLoadTrace(controller, WorkspaceProjectLoadCheckpoint::LoadStartedOwnerCheck,
                         requestPath, ProjectErrorCode::None, requestId);
    if (controller->model.open && WorkspaceModelHasDirtyDocuments(&controller->model)) {
        setControllerError(controller, ModelErrorCode::UnsavedChanges);
        setProjectError(controller, ProjectErrorCode::UnsavedChanges);
        controller->projectOpenFailure = ProjectErrorCode::UnsavedChanges;
        notifyProjectOpen(controller, WorkspaceProjectOpenState::Failed, requestPath, ProjectErrorCode::UnsavedChanges);
        setProjectError(controller, ProjectErrorCode::UnsavedChanges);
        releaseProjectLoadTransaction(controller);
        return false;
    }
    ProjectOperationResult result;
    if (!LoadProject(controller->fileSystem, path, &result, &controller->projectLoadScratch,
                     &validationGeneration, workspaceProjectLoadCheckpoint, controller)) {
        controller->lastManifestDiagnostic = result.manifestDiagnostic;
        controller->projectOpenManifestValidationCount = result.manifestDiagnostic.validationCount;
        setProjectError(controller, result.error);
        controller->projectOpenFailure = result.error;
        notifyProjectOpen(controller, WorkspaceProjectOpenState::Failed, requestPath, result.error);
        setProjectError(controller, result.error);
        releaseProjectLoadTransaction(controller);
        return false;
    }
    controller->lastManifestDiagnostic = result.manifestDiagnostic;
    controller->projectOpenManifestValidationCount = result.manifestDiagnostic.validationCount;
    notifyProjectOpen(controller, WorkspaceProjectOpenState::Loaded, result.project.rootPath, ProjectErrorCode::None);
    if (!projectLoadIsCurrent(controller, requestId)) {
        // The observer is synchronous. Keep the owner snapshot in the failure
        // event so diagnostics can show which identity component changed.
        setProjectError(controller, ProjectErrorCode::LoadInProgress);
        controller->projectOpenFailure = ProjectErrorCode::LoadInProgress;
        notifyProjectOpen(controller, WorkspaceProjectOpenState::Failed, result.project.rootPath, ProjectErrorCode::LoadInProgress);
        releaseProjectLoadTransaction(controller);
        return false;
    }
    if (!WorkspaceModelSetRoot(&g_workspaceProjectLoad.candidate, result.project.rootPath, result.project.displayName)) {
        setProjectError(controller, ProjectErrorCode::InvalidParentPath);
        controller->projectOpenFailure = ProjectErrorCode::InvalidParentPath;
        notifyProjectOpen(controller, WorkspaceProjectOpenState::Failed, result.project.rootPath, ProjectErrorCode::InvalidParentPath);
        setProjectError(controller, ProjectErrorCode::InvalidParentPath);
        releaseProjectLoadTransaction(controller);
        return false;
    }
    g_workspaceProjectLoad.candidate.hasProject = true;
    g_workspaceProjectLoad.candidate.project = result.project;
    g_workspaceProjectLoad.candidateProjectGeneration = g_workspaceProjectLoad.candidate.projectGeneration;
    controller->projectOpenCandidateProjectGeneration = g_workspaceProjectLoad.candidateProjectGeneration;
    (void)copyEntryName(g_workspaceProjectLoad.candidateProjectId,
                        sizeof(g_workspaceProjectLoad.candidateProjectId), result.project.projectId);
    notifyProjectOpen(controller, WorkspaceProjectOpenState::CandidateAllocated,
                      result.project.rootPath, ProjectErrorCode::None);
    g_workspaceProjectLoad.refreshGeneration = nextWorkspaceProjectId(&g_nextWorkspaceProjectRefreshGeneration);
    controller->projectOpenRefreshGeneration = g_workspaceProjectLoad.refreshGeneration;
    controller->projectOpenRefreshCount = 1;
    notifyProjectOpen(controller, WorkspaceProjectOpenState::RefreshStarted, result.project.rootPath, ProjectErrorCode::None);
    emitProjectLoadTrace(controller, WorkspaceProjectLoadCheckpoint::BeforeRefresh,
                         result.project.rootPath, ProjectErrorCode::None, requestId);
    // Symbol publication is deliberately deferred until the candidate model
    // has passed refresh and is committed below.
    bool candidateListingTruncated = false;
    const WorkspaceProjectLoadOwnershipResult beforeRefreshResult =
        projectLoadOwnershipResult(controller, requestId);
    controller->lastProjectLoadOwnershipResult = beforeRefreshResult;
    const bool refreshCurrent = beforeRefreshResult == WorkspaceProjectLoadOwnershipResult::Current;
    const bool refreshSucceeded = refreshCurrent &&
        refreshWorkspaceModel(controller, &g_workspaceProjectLoad.candidate, &candidateListingTruncated, false);
    if (!refreshSucceeded) {
        const ProjectErrorCode error = refreshCurrent ? ProjectErrorCode::RequiredFileMissing : ProjectErrorCode::LoadInProgress;
        setProjectError(controller, error);
        controller->projectOpenFailure = error;
        notifyProjectOpen(controller, WorkspaceProjectOpenState::Failed, result.project.rootPath, error);
        setProjectError(controller, error);
        releaseProjectLoadTransaction(controller);
        return false;
    }
    notifyProjectOpen(controller, WorkspaceProjectOpenState::Validated,
                      result.project.rootPath, ProjectErrorCode::None);
    if (!projectLoadIsCurrent(controller, requestId)) {
        setProjectError(controller, ProjectErrorCode::LoadInProgress);
        controller->projectOpenFailure = ProjectErrorCode::LoadInProgress;
        notifyProjectOpen(controller, WorkspaceProjectOpenState::Failed,
                          result.project.rootPath, ProjectErrorCode::LoadInProgress);
        setProjectError(controller, ProjectErrorCode::LoadInProgress);
        releaseProjectLoadTransaction(controller);
        return false;
    }
    notifyProjectOpen(controller, WorkspaceProjectOpenState::Committing,
                      result.project.rootPath, ProjectErrorCode::None);
    const WorkspaceProjectLoadOwnershipResult beforeCommitResult =
        projectLoadOwnershipResult(controller, requestId);
    controller->lastProjectLoadOwnershipResult = beforeCommitResult;
    emitProjectLoadTrace(controller, WorkspaceProjectLoadCheckpoint::BeforeCommit,
                         result.project.rootPath, ProjectErrorCode::None, requestId);
    if (beforeCommitResult != WorkspaceProjectLoadOwnershipResult::Current) {
        setProjectError(controller, ProjectErrorCode::LoadInProgress);
        controller->projectOpenFailure = ProjectErrorCode::LoadInProgress;
        notifyProjectOpen(controller, WorkspaceProjectOpenState::Failed,
                          result.project.rootPath, ProjectErrorCode::LoadInProgress);
        releaseProjectLoadTransaction(controller);
        return false;
    }
    ++controller->projectOpenCommitCount;
    controller->model = g_workspaceProjectLoad.candidate;
    // Keep the active model's generation publication explicit. This value was
    // validated against the private candidate immediately before commit.
    controller->model.projectGeneration = g_workspaceProjectLoad.candidateProjectGeneration;
    controller->listingTruncated = candidateListingTruncated;
    controller->lastProjectError = ProjectErrorCode::None;
    controller->projectOpenGeneration = controller->model.projectGeneration;
    g_workspaceProjectLoad.committed = true;
    emitProjectLoadTrace(controller, WorkspaceProjectLoadCheckpoint::AfterCommit,
                         controller->model.rootPath, ProjectErrorCode::None, requestId);
    notifyProjectOpen(controller, WorkspaceProjectOpenState::Active,
                      controller->model.rootPath, ProjectErrorCode::None);
#if !defined(GXOS_DEVELOPER_STUDIO_BARE_METAL)
    if (controller->symbolDatabase) {
        SymbolDatabaseClear(controller->symbolDatabase);
        SymbolDatabaseIndexProject(controller->symbolDatabase, controller->fileSystem, controller->model.rootPath,
                                    controller->model.documents, kMaxOpenDocuments, controller->model.projectGeneration);
    }
#endif
    notifyProjectOpen(controller, WorkspaceProjectOpenState::Ready, controller->model.rootPath, ProjectErrorCode::None);
    setProjectError(controller, ProjectErrorCode::None);
    controller->projectOpenFailure = ProjectErrorCode::None;
    releaseProjectLoadTransaction(controller);
    return true;
}

bool WorkspaceControllerCreateProject(WorkspaceController* controller, const ProjectCreateRequest& request, ProjectOperationResult* result) {
    if (!controller || !result) return false;
    if (controller->model.open && WorkspaceModelHasDirtyDocuments(&controller->model)) {
        result->success = false;
        result->error = ProjectErrorCode::UnsavedChanges;
        result->rollbackAttempted = false;
        result->rollbackSucceeded = true;
        setProjectError(controller, result->error);
        return false;
    }
    if (controller->model.hasProject) {
        uint32_t i = 0;
        while (request.projectId[i] != '\0' && controller->model.project.projectId[i] != '\0' && request.projectId[i] == controller->model.project.projectId[i]) ++i;
        if (request.projectId[i] == '\0' && controller->model.project.projectId[i] == '\0') {
            result->success = false;
            result->error = ProjectErrorCode::ProjectIdCollision;
            result->rollbackAttempted = false;
            result->rollbackSucceeded = true;
            setProjectError(controller, result->error);
            return false;
        }
    }
    if (!CreateNativeGuiProject(controller->fileSystem, request, result, &controller->projectLoadScratch)) {
        setProjectError(controller, result->error);
        return false;
    }
    setProjectError(controller, ProjectErrorCode::None);
    return true;
}

bool WorkspaceControllerReloadProject(WorkspaceController* controller) {
    if (rejectProjectLoadReentry(controller)) return false;
    if (!controller || !controller->model.open || !controller->model.hasProject) {
        if (controller) setProjectError(controller, ProjectErrorCode::RequiredFileMissing);
        return false;
    }
    ProjectOperationResult result;
    ManifestValidationGeneration validationGeneration = {};
    validationGeneration.requestId = controller->projectOpenRequestId;
    validationGeneration.requestGeneration = nextWorkspaceProjectId(&g_nextWorkspaceProjectRequestGeneration);
    validationGeneration.candidateId = nextWorkspaceProjectId(&g_nextWorkspaceProjectCandidateId);
    validationGeneration.candidateGeneration = nextWorkspaceProjectId(&g_nextWorkspaceProjectCandidateGeneration);
    validationGeneration.expectedIdentityGeneration = validationGeneration.candidateGeneration;
    validationGeneration.parsedIdentityGeneration = validationGeneration.candidateGeneration;
    if (!LoadProject(controller->fileSystem, controller->model.rootPath, &result,
                     &controller->projectLoadScratch, &validationGeneration)) {
        setProjectError(controller, result.error);
        return false;
    }
    controller->model.project = result.project;
    controller->model.hasProject = true;
    WorkspaceModelAdvanceProjectGeneration(&controller->model);
    controller->lastProjectError = ProjectErrorCode::None;
#if !defined(GXOS_DEVELOPER_STUDIO_BARE_METAL)
    if (controller->symbolDatabase)
        SymbolDatabaseIndexProject(controller->symbolDatabase, controller->fileSystem, controller->model.rootPath,
                                    controller->model.documents, kMaxOpenDocuments, controller->model.projectGeneration);
#endif
    return true;
}

bool WorkspaceControllerRefresh(WorkspaceController* controller) {
    bool truncated = false;
    if (controller && controller->projectOpenInProgress) {
        setProjectError(controller, ProjectErrorCode::LoadInProgress);
        return false;
    }
    if (!refreshWorkspaceModel(controller, controller ? &controller->model : nullptr, &truncated, true)) return false;
    controller->listingTruncated = truncated;
    return true;
}

bool WorkspaceControllerEnumerateProjectSources(const WorkspaceController* controller,
                                                ProjectSourceFile* files,
                                                uint32_t capacity,
                                                uint32_t* outCount) {
    if (outCount) *outCount = 0;
    if (!controller || !files || capacity == 0 || capacity > kMaxProjectSourceFiles || !outCount ||
        !controller->model.open || !controller->model.hasProject ||
        !controller->fileSystem.stat || !controller->fileSystem.list) return false;
    const Project& project = controller->model.project;
    if (project.sourceEntry[0] != '\0') {
        if (PathContainsTraversal(project.sourceEntry)) return false;
        char relative[kMaxProjectPathBytes] = {};
        if (!copyEntryName(relative, sizeof(relative), project.sourceRoot) ||
            relative[0] == '\0') return false;
        uint32_t length = 0;
        while (relative[length] != '\0') ++length;
        if (length + 1 >= sizeof(relative)) return false;
        relative[length++] = '/';
        if (!copyEntryName(relative + length, sizeof(relative) - length, project.sourceEntry)) return false;
        char absolute[kMaxPathBytes] = {};
        if (!JoinWorkspacePath(project.rootPath, relative, absolute, sizeof(absolute))) return false;
        FileInfo info = {};
        if (!controller->fileSystem.stat(controller->fileSystem.userData, absolute, &info) ||
            info.kind != FileInfoKind::RegularFile) return false;
        copyEntryName(files[0].relativePath, sizeof(files[0].relativePath), relative);
        files[0].size = info.size;
        *outCount = 1;
        return true;
    }

    char directory[kMaxPathBytes] = {};
    if (!JoinWorkspacePath(project.rootPath, project.sourceRoot, directory, sizeof(directory))) return false;
    FileListEntry entries[kMaxWorkspaceEntries] = {};
    uint32_t entryCount = 0;
    bool truncated = false;
    if (!controller->fileSystem.list(controller->fileSystem.userData, directory, entries,
                                     kMaxWorkspaceEntries, &entryCount, &truncated) || truncated) return false;
    uint32_t count = 0;
    for (uint32_t i = 0; i < entryCount; ++i) {
        if (entries[i].kind != FileInfoKind::RegularFile || !sourceExtension(entries[i].name)) continue;
        if (count >= capacity) return false;
        ProjectSourceFile& file = files[count];
        file = {};
        uint32_t rootLength = 0;
        while (project.sourceRoot[rootLength] != '\0') ++rootLength;
        if (!copyEntryName(file.relativePath, sizeof(file.relativePath), project.sourceRoot) ||
            rootLength + 1 >= sizeof(file.relativePath)) return false;
        file.relativePath[rootLength++] = '/';
        if (!copyEntryName(file.relativePath + rootLength, sizeof(file.relativePath) - rootLength, entries[i].name)) return false;
        file.size = entries[i].size;
        ++count;
    }
    for (uint32_t i = 0; i < count; ++i) {
        for (uint32_t j = i + 1; j < count; ++j) {
            if (pathBefore(files[j].relativePath, files[i].relativePath)) {
                ProjectSourceFile swap = files[i];
                files[i] = files[j];
                files[j] = swap;
            }
        }
        if (i != 0 && !pathBefore(files[i - 1].relativePath, files[i].relativePath) &&
            !pathBefore(files[i].relativePath, files[i - 1].relativePath)) return false;
    }
    if (count == 0) return false;
    *outCount = count;
    return true;
}

bool WorkspaceControllerEnterSelected(WorkspaceController* controller) {
    if (!controller || !controller->model.open || controller->model.selectedEntry >= controller->model.entryCount) return false;
    WorkspaceEntry& entry = controller->model.entries[controller->model.selectedEntry];
    if (entry.kind == WorkspaceEntryKind::Directory) {
        if (!WorkspaceModelSetBrowsePath(&controller->model, entry.relativePath)) {
            setControllerError(controller, ModelErrorCode::InvalidPath);
            return false;
        }
        return WorkspaceControllerRefresh(controller);
    }
    return WorkspaceControllerOpenDocument(controller, entry.relativePath);
}

bool WorkspaceControllerGoUp(WorkspaceController* controller) {
    if (!controller || !controller->model.open) return false;
    uint32_t length = 0;
    while (controller->model.browsePath[length] != '\0' && length + 1 < sizeof(controller->model.browsePath)) ++length;
    if (length == 0) return false;
    while (length > 0 && controller->model.browsePath[length - 1] != '/') --length;
    if (length > 0) --length;
    controller->model.browsePath[length] = '\0';
    return WorkspaceControllerRefresh(controller);
}

bool WorkspaceControllerOpenDocument(WorkspaceController* controller, const char* path) {
    if (rejectProjectLoadReentry(controller)) return false;
    if (!controller || !controller->model.open || !controller->fileSystem.stat || !controller->fileSystem.read) { if (controller) setControllerError(controller, ModelErrorCode::WorkspaceNotOpen); return false; }
    char normalized[kMaxPathBytes];
    if (!absolutePathForDocument(controller->model, path, normalized, sizeof(normalized)) || !isWithinRoot(controller->model, normalized)) { setControllerError(controller, ModelErrorCode::OutsideWorkspace); return false; }
    FileInfo info;
    if (!controller->fileSystem.stat(controller->fileSystem.userData, normalized, &info)) { setControllerError(controller, ModelErrorCode::ReadFailed); return false; }
    if (info.kind != FileInfoKind::RegularFile) { setControllerError(controller, ModelErrorCode::NotFile); return false; }
    if (!IsSupportedTextPath(normalized)) { setControllerError(controller, ModelErrorCode::UnsupportedFile); return false; }
    if (info.size > kMaxEditorBytes) { setControllerError(controller, ModelErrorCode::FileTooLarge); return false; }
    char* bytes = g_workspaceDocumentReadBuffer;
    uint32_t count = 0;
    if (!controller->fileSystem.read(controller->fileSystem.userData, normalized, bytes, kMaxEditorBytes, &count)) { setControllerError(controller, ModelErrorCode::ReadFailed); return false; }
    if (count > kMaxEditorBytes || LooksBinary(bytes, count)) { setControllerError(controller, count > kMaxEditorBytes ? ModelErrorCode::FileTooLarge : ModelErrorCode::BinaryFile); return false; }
    ModelErrorCode error = ModelErrorCode::None;
    bool duplicate = false;
    if (!WorkspaceModelAddDocument(&controller->model, normalized, bytes, count, &error, &duplicate)) { setControllerError(controller, error); return false; }
    controller->lastError = duplicate ? ModelErrorCode::DuplicateDocument : ModelErrorCode::None;
#if !defined(GXOS_DEVELOPER_STUDIO_BARE_METAL)
    if (controller->symbolDatabase) WorkspaceControllerUpdateDocumentSymbols(
        controller, static_cast<uint32_t>(FindOpenDocument(&controller->model, normalized)));
#endif
    return true;
}

bool WorkspaceControllerUpdateDocumentSymbols(WorkspaceController* controller, uint32_t documentIndex) {
    if (rejectProjectLoadReentry(controller)) return false;
#if defined(GXOS_DEVELOPER_STUDIO_BARE_METAL)
    (void)controller;
    (void)documentIndex;
    return false;
#else
    if (!controller || !controller->symbolDatabase || documentIndex >= kMaxOpenDocuments ||
        !controller->model.documents[documentIndex].used) return false;
    const Document& document = controller->model.documents[documentIndex];
    return SymbolDatabaseIndexDocument(controller->symbolDatabase, document.path, document.documentId,
                                       document.buffer.generation, document.buffer.dirty,
                                       document.buffer.data, document.buffer.length);
#endif
}

bool WorkspaceControllerSetCaretPosition(WorkspaceController* controller, uint32_t documentIndex, uint32_t line, uint32_t column,
                                          bool* outLocationClamped, OutputErrorCode* error) {
    if (outLocationClamped) *outLocationClamped = false;
    if (error) *error = OutputErrorCode::None;
    if (!controller || documentIndex >= kMaxOpenDocuments || !controller->model.documents[documentIndex].used) {
        if (error) *error = OutputErrorCode::NavigationOpenFailed;
        return false;
    }
    TextBuffer& buffer = controller->model.documents[documentIndex].buffer;
    const uint32_t count = TextBufferLineCount(&buffer);
    uint32_t zeroLine = line > 0 ? line - 1 : 0;
    if (zeroLine >= count) {
        zeroLine = count == 0 ? 0 : count - 1;
        if (outLocationClamped) *outLocationClamped = true;
    }
    const uint32_t start = TextBufferLineStart(&buffer, zeroLine);
    const uint32_t end = TextBufferLineEnd(&buffer, zeroLine);
    const uint32_t lineBytes = end >= start ? end - start : 0;
    uint32_t zeroColumn = column > 0 ? column - 1 : 0;
    if (zeroColumn > lineBytes) {
        zeroColumn = lineBytes;
        if (outLocationClamped) *outLocationClamped = true;
    }
    buffer.caret = start + zeroColumn;
    if (outLocationClamped && *outLocationClamped && error) *error = OutputErrorCode::NavigationLocationClamped;
    return true;
}

bool WorkspaceControllerOpenDocumentAtLocation(WorkspaceController* controller, const char* projectId, const char* relativePath,
                                               uint32_t line, uint32_t column, uint32_t* outDocumentIndex, OutputErrorCode* error) {
    if (outDocumentIndex) *outDocumentIndex = kMaxOpenDocuments;
    if (error) *error = OutputErrorCode::None;
    if (!controller || !controller->model.open || !controller->model.hasProject) {
        if (error) *error = OutputErrorCode::NavigationNoProject;
        return false;
    }
    if (!projectId || !equalIdentifier(projectId, controller->model.project.projectId)) {
        if (error) *error = OutputErrorCode::DiagnosticProjectMismatch;
        return false;
    }
    if (!relativePath || relativePath[0] == '\0' || PathContainsTraversal(relativePath) || relativePath[0] == '/' || relativePath[0] == '\\' || relativePath[1] == ':') {
        if (error) *error = OutputErrorCode::DiagnosticPathOutsideProject;
        return false;
    }
    char absolute[kMaxPathBytes] = {};
    if (!JoinWorkspacePath(controller->model.rootPath, relativePath, absolute, sizeof(absolute))) {
        if (error) *error = OutputErrorCode::DiagnosticPathOutsideProject;
        return false;
    }
    if (!WorkspaceControllerOpenDocument(controller, relativePath)) {
        if (error) *error = OutputErrorCode::DiagnosticFileNotFound;
        return false;
    }
    const int index = FindOpenDocument(&controller->model, absolute);
    if (index < 0) { if (error) *error = OutputErrorCode::NavigationOpenFailed; return false; }
    controller->model.activeDocument = static_cast<uint32_t>(index);
    bool clamped = false;
    if (!WorkspaceControllerSetCaretPosition(controller, static_cast<uint32_t>(index), line, column, &clamped, error)) return false;
    if (outDocumentIndex) *outDocumentIndex = static_cast<uint32_t>(index);
    return true;
}

bool WorkspaceControllerSaveDocument(WorkspaceController* controller, uint32_t documentIndex) {
    if (rejectProjectLoadReentry(controller)) return false;
    if (!controller || documentIndex >= kMaxOpenDocuments || !controller->model.documents[documentIndex].used || !controller->fileSystem.write) { if (controller) setControllerError(controller, ModelErrorCode::DocumentNotFound); return false; }
    Document& document = controller->model.documents[documentIndex];
    uint32_t written = 0;
    bool ok = controller->fileSystem.write(controller->fileSystem.userData, document.path, document.buffer.data, document.buffer.length, &written) && written == document.buffer.length;
    ModelErrorCode error = ModelErrorCode::None;
    if (!WorkspaceModelMarkSaved(&controller->model, documentIndex, ok, &error)) { setControllerError(controller, error); return false; }
#if !defined(GXOS_DEVELOPER_STUDIO_BARE_METAL)
    if (ok) WorkspaceControllerUpdateDocumentSymbols(controller, documentIndex);
#endif
    setControllerError(controller, ModelErrorCode::None);
    return true;
}

bool WorkspaceControllerSaveActive(WorkspaceController* controller) {
    if (!controller || controller->model.activeDocument >= kMaxOpenDocuments) { if (controller) setControllerError(controller, ModelErrorCode::DocumentNotFound); return false; }
    return WorkspaceControllerSaveDocument(controller, controller->model.activeDocument);
}

bool WorkspaceControllerSaveAll(WorkspaceController* controller) {
    if (!controller || !controller->model.open) { if (controller) setControllerError(controller, ModelErrorCode::WorkspaceNotOpen); return false; }
    for (uint32_t i = 0; i < kMaxOpenDocuments; ++i) {
        if (controller->model.documents[i].used && controller->model.documents[i].buffer.dirty && !WorkspaceControllerSaveDocument(controller, i)) return false;
    }
    return true;
}

bool WorkspaceControllerHasDirtyProjectDocuments(const WorkspaceController* controller) {
    if (!controller || !controller->model.open || !controller->model.hasProject) return false;
    for (uint32_t i = 0; i < kMaxOpenDocuments; ++i) {
        if (!controller->model.documents[i].used || !controller->model.documents[i].buffer.dirty) continue;
        if (isWithinRoot(controller->model, controller->model.documents[i].path)) return true;
    }
    return false;
}

bool WorkspaceControllerSaveAllProjectDocuments(WorkspaceController* controller) {
    if (!controller || !controller->model.open || !controller->model.hasProject) {
        if (controller) setControllerError(controller, ModelErrorCode::WorkspaceNotOpen);
        return false;
    }
    for (uint32_t i = 0; i < kMaxOpenDocuments; ++i) {
        if (!controller->model.documents[i].used || !controller->model.documents[i].buffer.dirty) continue;
        if (!isWithinRoot(controller->model, controller->model.documents[i].path)) continue;
        if (!WorkspaceControllerSaveDocument(controller, i)) return false;
    }
    return true;
}

bool WorkspaceControllerCloseDocument(WorkspaceController* controller, uint32_t documentIndex, CloseDecision decision) {
    if (rejectProjectLoadReentry(controller)) return false;
    if (!controller || documentIndex >= kMaxOpenDocuments || !controller->model.documents[documentIndex].used) { if (controller) setControllerError(controller, ModelErrorCode::DocumentNotFound); return false; }
    char closedPath[kMaxPathBytes] = {};
    for (uint32_t i = 0; i + 1 < sizeof(closedPath); ++i) {
        closedPath[i] = controller->model.documents[documentIndex].path[i];
        if (closedPath[i] == '\0') break;
    }
    if (decision == CloseDecision::Save && controller->model.documents[documentIndex].buffer.dirty && !WorkspaceControllerSaveDocument(controller, documentIndex)) return false;
    ModelErrorCode error = ModelErrorCode::None;
    bool ok = WorkspaceModelCloseDocument(&controller->model, documentIndex, decision, decision != CloseDecision::Save || !controller->model.documents[documentIndex].buffer.dirty, &error);
    if (!ok) setControllerError(controller, error);
#if !defined(GXOS_DEVELOPER_STUDIO_BARE_METAL)
    else if (controller->symbolDatabase && controller->model.hasProject)
        SymbolDatabaseIndexDiskDocument(controller->symbolDatabase, controller->fileSystem, closedPath,
                                        controller->model.projectGeneration);
    else if (ok && controller->symbolDatabase)
        SymbolDatabaseRemoveDocument(controller->symbolDatabase, closedPath);
#endif
    return ok;
}

bool WorkspaceControllerCloseWorkspace(WorkspaceController* controller, CloseDecision decision) {
    if (rejectProjectLoadReentry(controller)) return false;
    if (!controller || !controller->model.open) return true;
    if (WorkspaceModelHasDirtyDocuments(&controller->model)) {
        if (decision == CloseDecision::Cancel) return false;
        if (decision == CloseDecision::Save && !WorkspaceControllerSaveAll(controller)) return false;
    }
    WorkspaceModelInit(&controller->model);
#if !defined(GXOS_DEVELOPER_STUDIO_BARE_METAL)
    if (controller->symbolDatabase) SymbolDatabaseClear(controller->symbolDatabase);
#endif
    controller->lastError = ModelErrorCode::None;
    controller->listingTruncated = false;
    return true;
}

Document* WorkspaceControllerActiveDocument(WorkspaceController* controller) {
    if (!controller || controller->model.activeDocument >= kMaxOpenDocuments) return nullptr;
    return controller->model.documents[controller->model.activeDocument].used ? &controller->model.documents[controller->model.activeDocument] : nullptr;
}

const Document* WorkspaceControllerActiveDocumentConst(const WorkspaceController* controller) {
    if (!controller || controller->model.activeDocument >= kMaxOpenDocuments) return nullptr;
    return controller->model.documents[controller->model.activeDocument].used ? &controller->model.documents[controller->model.activeDocument] : nullptr;
}

const char* WorkspaceControllerError(const WorkspaceController* controller) {
    return controller ? ModelErrorName(controller->lastError) : ModelErrorName(ModelErrorCode::InvalidPath);
}

const char* WorkspaceControllerProjectError(const WorkspaceController* controller) {
    return controller ? ProjectErrorName(controller->lastProjectError) : ProjectErrorName(ProjectErrorCode::NullInput);
}

} // namespace developer_studio
} // namespace guidexos
