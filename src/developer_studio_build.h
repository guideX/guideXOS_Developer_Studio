#pragma once

#include "developer_studio_workspace.h"
#include "developer_studio_output.h"

namespace guidexos {
namespace developer_studio {

static const uint32_t kMaxBuildLines = 32;
static const uint32_t kMaxBuildLineBytes = 256;
static const uint32_t kMaxBuildSystemBytes = 64;
static const uint32_t kMaxBuildConfigurationBytes = 32;
static const uint32_t kMaxBuildArtifactSha256Bytes = 65;

enum class BuildState {
    Idle = 0,
    Validating,
    Preparing,
    Running,
    ValidatingArtifact,
    Succeeded,
    Failed,
    Cancelled
};

enum class BuildErrorCode {
    None = 0,
    NoProject,
    WorkspaceOnly,
    UnsupportedProjectKind,
    UnsupportedTarget,
    InvalidRequest,
    DirtyDocuments,
    SaveFailed,
    UserCancelled,
    AlreadyRunning,
    HostUnavailable,
    SdkNotFound,
    ToolchainNotFound,
    PowerShellNotFound,
    BuildScriptMissing,
    InvalidProjectRoot,
    ProcessStartFailed,
    ProcessFailed,
    BuildTimeout,
    ArtifactMissing,
    ArtifactInvalid,
    ArtifactWrongArchitecture,
    EntryPointMissing,
    ManifestArtifactMismatch,
    OutputTruncated,
    ServiceError
};

enum class BuildDirtyDecision {
    SaveAll = 0,
    Cancel
};

struct BuildOutputLine {
    bool standardError;
    char text[kMaxBuildLineBytes];
};

struct BuildRequest {
    char projectRoot[kMaxPathBytes];
    char projectId[kMaxProjectIdBytes];
    char projectKind[kMaxBuildSystemBytes];
    char targetProfile[kMaxNameBytes];
    char buildSystem[kMaxBuildSystemBytes];
    char buildScript[kMaxProjectPathBytes];
    char expectedArtifact[kMaxProjectPathBytes];
    char configuration[kMaxBuildConfigurationBytes];
};

struct BuildResult {
    BuildState state;
    int32_t exitCode;
    BuildErrorCode error;
    uint64_t elapsedMilliseconds;
    uint32_t warningCount;
    uint32_t errorCount;
    uint32_t outputCount;
    bool outputTruncated;
    uint64_t artifactSize;
    bool artifactValid;
    bool artifactEntryPoint;
    char artifactPath[kMaxProjectPathBytes];
    char artifactSha256[kMaxBuildArtifactSha256Bytes];
    char artifactArchitecture[32];
    char errorMessage[128];
    BuildOutputLine output[kMaxBuildLines];
};

struct HostedBuildService {
    void* userData;
    bool (*start)(void* userData, const BuildRequest& request, uint64_t* outHandle, BuildErrorCode* error);
    bool (*poll)(void* userData, uint64_t handle, BuildResult* result, bool* completed, BuildErrorCode* error);
    bool (*release)(void* userData, uint64_t handle);
};

struct BuildController {
    BuildState state;
    bool active;
    bool terminalPublished;
    uint64_t handle;
    uint64_t operationId;
    uint32_t publishedOutputCount;
    bool outputTruncationPublished;
    OutputService* output;
    BuildRequest request;
    BuildResult result;
};

const char* BuildStateName(BuildState state);
const char* BuildErrorName(BuildErrorCode error);
bool BuildRequestFromProject(const Project& project, BuildRequest* request, BuildErrorCode* error);
bool BuildRequestEnableDebugInfo(BuildRequest* request);
bool BuildControllerInit(BuildController* controller);
bool BuildControllerStart(BuildController* controller, WorkspaceController* workspace, const HostedBuildService& service, BuildDirtyDecision dirtyDecision, BuildErrorCode* error, OutputService* output = nullptr, bool debugInfo = false);
bool BuildControllerPoll(BuildController* controller, const HostedBuildService& service);
bool BuildControllerIsActive(const BuildController* controller);
bool WorkspaceControllerHasDirtyProjectDocuments(const WorkspaceController* controller);
bool WorkspaceControllerSaveAllProjectDocuments(WorkspaceController* controller);

} // namespace developer_studio
} // namespace guidexos
