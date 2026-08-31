#pragma once

#include "developer_studio_build.h"

namespace guidexos {
namespace developer_studio {

static const uint32_t kMaxRunApplicationIdBytes = 96;
static const uint32_t kMaxRunDisplayNameBytes = 96;
static const uint32_t kMaxRunArtifactSha256Bytes = 65;
static const uint32_t kMaxRunErrorBytes = 128;
static const uint32_t kMaxRunOutputLines = 16;
static const uint32_t kMaxRunOutputLineBytes = 256;

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
    CallDepthExceeded,
    ApplicationExited,
    UserCancelled
};

enum class RunBackendKind {
    Hosted = 0,
    BareMetal
};

struct RunRequest {
    char projectRoot[kMaxPathBytes];
    char projectId[kMaxProjectIdBytes];
    char projectKind[kMaxBuildSystemBytes];
    char targetProfile[kMaxNameBytes];
    char manifestPath[kMaxPathBytes];
    char artifactPath[kMaxProjectPathBytes];
    char artifactSha256[kMaxRunArtifactSha256Bytes];
    uint64_t artifactSize;
    char artifactArchitecture[32];
    char artifactAbi[64];
    bool debugControlled;
};

struct RunOutputLine {
    char text[kMaxRunOutputLineBytes];
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
    uint32_t outputCount;
    bool outputTruncated;
    RunOutputLine output[kMaxRunOutputLines];
};

enum class HostedDebugCommand {
    BindSoftwareBreakpoint = 1,
    ReleaseExecution = 2,
    Poll = 3,
    RestoreAll = 4,
    CancelExecution = 5,
    ContinueBreakpoint = 6,
    StepInstruction = 7,
    ResumeStep = 8,
    ReadMemory = 9,
    RemoveSoftwareBreakpointOwner = 10,
    StepOverCall = 11,
    ResumeInternalTrap = 12,
    StepInternalTrap = 13,
    StepOutReturn = 14
};

enum class HostedDebugSingleStepKind {
    None = 0,
    InternalBreakpoint = 1,
    UserSource = 2
};

enum class HostedDebugInternalBreakpointPurpose {
    None = 0,
    StepOver = 1,
    StepOut = 2
};

struct HostedDebugRegisterSnapshot {
    bool valid;
    uint32_t architecture;
    uint64_t processId;
    uint64_t nativeRuntimeId;
    uint64_t threadId;
    uint64_t sessionGeneration;
    uint64_t stopGeneration;
    uint64_t rip;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t rbp;
    uint64_t rax;
    uint64_t rbx;
    uint64_t rcx;
    uint64_t rdx;
    uint64_t rsi;
    uint64_t rdi;
    uint64_t r8;
    uint64_t r9;
    uint64_t r10;
    uint64_t r11;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
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
    uint64_t stopGeneration = 0;
    uint32_t executionState = 0;
    uint32_t singleStepKind = 0;
    uint64_t rflagsBeforeStep = 0;
    uint64_t rflagsWithTrapFlag = 0;
    uint64_t rflagsAfterTrapFlagClear = 0;
    HostedDebugRegisterSnapshot registerContext = {};
    bool internalBreakpointTrap = false;
    uint64_t internalBreakpointId = 0;
    uint32_t internalBreakpointPurpose = 0;
    uint32_t byteCount = 0;
    uint8_t bytes[16] = {};
    uint64_t stackLow = 0;
    uint64_t stackHigh = 0;
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
                         uint64_t threadId, uint64_t stopGeneration, bool reinstallBreakpoint,
    uint64_t auxiliaryAddress, uint32_t readByteCount,
                         HostedDebugResult* outResult);
    RunBackendKind backend;
};

struct RunController {
    RunState state;
    bool active;
    bool closeRequested;
    bool terminalPublished;
    uint64_t handle;
    uint64_t operationId;
    uint32_t publishedOutputCount;
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
