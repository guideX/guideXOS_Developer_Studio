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
static const uint32_t kMaxBuildSources = kMaxProjectSourceFiles;

enum class BuildState {
    Idle = 0,
    Validating,
    Preparing,
    Running,
    ValidatingArtifact,
    Succeeded,
    Failed,
    Cancelled,
    // Internal stage only. The shared gx_build_state ABI remains unchanged.
    Saving = 8
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
    ServiceError,
    CompilerFailed,
    ProjectValidationFailed,
    SourceReadFailed,
    ObjectGenerationFailed,
    LinkerFailed,
    ElfWriterFailed,
    ArtifactValidationFailed,
    ArtifactPublicationFailed
};

enum class BuildDirtyDecision {
    SaveAll = 0,
    Cancel
};

enum class BuildBackendKind {
    Hosted = 0,
    BareMetal
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
    // Metadata for bounded multi-file backends. Native adapters that derive
    // sources from the project file may ignore this copied request view.
    uint32_t sourceCount;
    char sourceFiles[kMaxBuildSources][kMaxProjectPathBytes];
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
    uint32_t sourceFileCount;
    uint32_t compiledModuleCount;
    uint32_t cachedModuleCount;
    uint32_t linkedModuleCount;
    // Append-only multi-architecture/package publication evidence returned by
    // the resident bare-metal compiler.
    uint64_t siblingArtifactSize;
    bool siblingArtifactValid;
    bool packageWritten;
    uint64_t packageGeneration;
    char siblingArtifactPath[kMaxProjectPathBytes];
    char siblingArtifactSha256[kMaxBuildArtifactSha256Bytes];
    char siblingArtifactArchitecture[32];
    char packagePath[kMaxProjectPathBytes];
    // IDE-owned request identity; these fields are not part of gx_build_snapshot.
    char projectId[kMaxProjectIdBytes];
    char projectRoot[kMaxPathBytes];
    char targetProfile[kMaxNameBytes];
    bool serviceReleaseFailed;
    uint64_t sourceMembershipHash;
    char configuration[kMaxBuildConfigurationBytes];
};

struct HostedBuildService {
    void* userData;
    bool (*start)(void* userData, const BuildRequest& request, uint64_t* outHandle, BuildErrorCode* error);
    bool (*poll)(void* userData, uint64_t handle, BuildResult* result, bool* completed, BuildErrorCode* error);
    bool (*release)(void* userData, uint64_t handle);
    // Appended after the original callback shape so existing aggregate
    // initializers and hosted adapters remain source-compatible.
    BuildBackendKind backend;
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
bool BuildRequestFromProject(const Project& project, BuildRequest* request, BuildErrorCode* error,
                            BuildBackendKind backend = BuildBackendKind::Hosted);
bool BuildRequestEnableDebugInfo(BuildRequest* request);
bool BuildControllerInit(BuildController* controller);
bool BuildControllerStart(BuildController* controller, WorkspaceController* workspace, const HostedBuildService& service, BuildDirtyDecision dirtyDecision, BuildErrorCode* error, OutputService* output = nullptr, bool debugInfo = false);
bool BuildControllerPoll(BuildController* controller, const HostedBuildService& service);
bool BuildControllerIsActive(const BuildController* controller);
BuildErrorCode ClassifyBuildFailureDetail(const char* detail);
BuildErrorCode ClassifyBuildDiagnosticLine(const char* line);
uint64_t BuildRequestSourceMembershipHash(const BuildRequest& request);
bool WorkspaceControllerHasDirtyProjectDocuments(const WorkspaceController* controller);
bool WorkspaceControllerSaveAllProjectDocuments(WorkspaceController* controller);

} // namespace developer_studio
} // namespace guidexos
