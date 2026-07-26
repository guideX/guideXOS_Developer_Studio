#pragma once

#include "developer_studio_build.h"

namespace guidexos {
namespace developer_studio {

static const uint32_t kMaxRunApplicationIdBytes = 96;
static const uint32_t kMaxRunDisplayNameBytes = 96;
static const uint32_t kMaxRunArtifactSha256Bytes = 65;
static const uint32_t kMaxRunErrorBytes = 128;

enum class RunState {
    Idle = 0,
    Validating,
    Prepared,
    Registered,
    Launching,
    Running,
    Exited,
    CleaningUp,
    Completed,
    Failed
};

enum class RunErrorCode {
    None = 0,
    InvalidRequest,
    BuildRequired,
    ArtifactInvalid,
    ServiceUnavailable,
    AlreadyActive,
    OwnerMismatch,
    StaleDeployment,
    LaunchFailed,
    ApplicationExited,
    UserCancelled
};

struct RunRequest {
    char projectRoot[kMaxPathBytes];
    char projectId[kMaxProjectIdBytes];
    char projectKind[kMaxBuildSystemBytes];
    char targetProfile[kMaxNameBytes];
    char manifestPath[kMaxPathBytes];
    char artifactPath[kMaxProjectPathBytes];
    char artifactSha256[kMaxRunArtifactSha256Bytes];
};

struct RunResult {
    RunState state;
    RunErrorCode error;
    uint64_t handle;
    uint64_t processId;
    uint64_t nativeRuntimeId;
    uint32_t windowCount;
    uint32_t createdWindowCount;
    int32_t exitCode;
    bool cleanupComplete;
    char applicationId[kMaxRunApplicationIdBytes];
    char displayName[kMaxRunDisplayNameBytes];
    char artifactSha256[kMaxRunArtifactSha256Bytes];
    char errorMessage[kMaxRunErrorBytes];
};

struct HostedDevelopmentRunService {
    void* userData;
    bool (*prepare)(void* userData, const RunRequest& request, uint64_t* outHandle, RunResult* outResult);
    bool (*start)(void* userData, uint64_t handle, RunResult* outResult);
    bool (*poll)(void* userData, uint64_t handle, RunResult* outResult);
    bool (*requestClose)(void* userData, uint64_t handle);
    bool (*release)(void* userData, uint64_t handle);
};

struct RunController {
    RunState state;
    bool active;
    bool closeRequested;
    bool terminalPublished;
    uint64_t handle;
    RunRequest request;
    RunResult result;
};

const char* RunStateName(RunState state);
const char* RunErrorName(RunErrorCode error);
bool RunRequestFromBuild(const Project& project, const BuildResult& build, RunRequest* request, RunErrorCode* error);
bool RunControllerInit(RunController* controller);
bool RunControllerPrepare(RunController* controller, const HostedDevelopmentRunService& service, const RunRequest& request, RunErrorCode* error);
bool RunControllerStart(RunController* controller, const HostedDevelopmentRunService& service, RunErrorCode* error);
bool RunControllerPoll(RunController* controller, const HostedDevelopmentRunService& service);
bool RunControllerRequestClose(RunController* controller, const HostedDevelopmentRunService& service);
bool RunControllerIsActive(const RunController* controller);
bool RunControllerIsTransitionActive(const RunController* controller);

} // namespace developer_studio
} // namespace guidexos
