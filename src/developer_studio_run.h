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
    bool debugControlled;
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

enum class HostedDebugCommand {
    BindSoftwareBreakpoint = 1,
    ReleaseExecution = 2,
    Poll = 3,
    RestoreAll = 4,
    CancelExecution = 5
};

struct HostedDebugResult {
    uint32_t status = 0;
    uint32_t trapKind = 0;
    uint64_t bindingId = 0;
    uint64_t processId = 0;
    uint64_t nativeRuntimeId = 0;
    uint64_t threadId = 0;
    uint64_t instructionPointer = 0;
    uint64_t targetAddress = 0;
    uint8_t originalByte = 0;
    uint8_t installedByte = 0;
    bool originalByteValid = false;
    bool bindingInstalled = false;
    uint32_t bindingCount = 0;
    char errorMessage[kMaxRunErrorBytes] = {};
};

struct HostedDevelopmentRunService {
    void* userData;
    bool (*prepare)(void* userData, const RunRequest& request, uint64_t* outHandle, RunResult* outResult);
    bool (*start)(void* userData, uint64_t handle, RunResult* outResult);
    bool (*poll)(void* userData, uint64_t handle, RunResult* outResult);
    bool (*requestClose)(void* userData, uint64_t handle);
    bool (*release)(void* userData, uint64_t handle);
    bool (*debugCommand)(void* userData, HostedDebugCommand command, uint64_t handle,
                         uint64_t sessionGeneration, uint64_t processId, uint64_t nativeRuntimeId,
                         uint64_t breakpointId, uint64_t targetAddress, const char* artifactSha256,
                         HostedDebugResult* outResult);
};

struct RunController {
    RunState state;
    bool active;
    bool closeRequested;
    bool terminalPublished;
    uint64_t handle;
    uint64_t operationId;
    OutputService* output;
    RunRequest request;
    RunResult result;
};

const char* RunStateName(RunState state);
const char* RunErrorName(RunErrorCode error);
bool RunRequestFromBuild(const Project& project, const BuildResult& build, RunRequest* request, RunErrorCode* error);
bool RunControllerInit(RunController* controller);
void RunControllerAttachOutput(RunController* controller, OutputService* output, uint64_t operationId);
bool RunControllerPrepare(RunController* controller, const HostedDevelopmentRunService& service, const RunRequest& request, RunErrorCode* error);
bool RunControllerStart(RunController* controller, const HostedDevelopmentRunService& service, RunErrorCode* error);
bool RunControllerPoll(RunController* controller, const HostedDevelopmentRunService& service);
bool RunControllerRequestClose(RunController* controller, const HostedDevelopmentRunService& service);
bool RunControllerIsActive(const RunController* controller);
bool RunControllerIsTransitionActive(const RunController* controller);

} // namespace developer_studio
} // namespace guidexos
