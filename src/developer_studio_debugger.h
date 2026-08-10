#pragma once

#include "developer_studio_run.h"

namespace guidexos {
namespace developer_studio {

static const uint32_t kDebugMaxBreakpoints = 128;
static const uint32_t kDebugMaxEvents = 64;
static const uint32_t kDebugMaxMessageBytes = 160;
static const uint32_t kDebugMaxBackendNameBytes = 64;
static const uint32_t kDebugMaxAbiBytes = 64;
static const uint32_t kDebugMaxArchitectureBytes = 32;
static const uint32_t kDebugMaxArgumentsBytes = 256;
static const uint32_t kDebugMaxMappedAddresses = 8;

enum class DebugSessionState {
    Idle = 0,
    Launching,
    Running,
    Paused,
    Stopping,
    Exited,
    Failed
};

enum class DebugErrorCode {
    None = 0,
    InvalidRequest,
    InvalidTransition,
    NoProject,
    NoRunnableTarget,
    BackendUnavailable,
    CapabilityUnavailable,
    LaunchFailed,
    StopFailed,
    PollFailed,
    StaleSession,
    OutsideProject,
    InvalidSourcePath,
    InvalidLine,
    DuplicateBreakpoint,
    BreakpointNotFound,
    BreakpointRejected,
    SourceMappingUnavailable,
    ProjectGenerationMismatch,
    BackendError,
    NoDebugInfo,
    MissingLineSection,
    MalformedElf,
    MalformedDwarf,
    UnsupportedDwarfVersion,
    UnsupportedForm,
    UnsupportedArchitecture,
    ArtifactChanged,
    SourceNotFound,
    LineNotMapped,
    Truncated,
    MappingLimitExceeded,
    UnsupportedOpcode
};

enum class DebugBreakpointState {
    Pending = 0,
    Mapped,
    Verified,
    Rejected,
    Disabled,
    Stale
};

enum class DebugMappingState {
    Unavailable = 0,
    Pending,
    Mapped,
    Rejected
};

enum class DebugStopReason {
    None = 0,
    Breakpoint,
    UserRequested,
    Exited,
    Exception,
    Signal,
    EntryPoint,
    Unknown
};

enum class DebugEventKind {
    None = 0,
    Launched,
    Running,
    Paused,
    BreakpointBound,
    BreakpointRejected,
    Stopped,
    Exited,
    Failed,
    TargetCreated,
    BreakpointUnbound,
    BreakpointTrap,
    BreakpointHit,
    BreakpointRestoreFailed,
    UnexpectedTrap
};

struct DebugCapabilities {
    bool canLaunch;
    bool canStop;
    bool canPause;
    bool canContinue;
    bool canSetInstructionBreakpoint;
    bool canSetSourceBreakpoint;
    bool canStepInto;
    bool canStepOver;
    bool canStepOut;
    bool canReadRegisters;
    bool canReadMemory;
    bool canWriteMemory;
    bool canEnumerateThreads;
    bool canReadCallStack;
    bool canResolveSourceLocations;
    bool canEvaluateExpressions;
    bool canBindSoftwareBreakpoint;
    bool canObserveBreakpointTrap;
    bool canRestoreBreakpoint;
    bool canReadInstructionPointer;
};

struct DebugAddress {
    bool valid;
    uint64_t value;
};

struct DebugSourceLocation {
    char relativePath[kMaxProjectPathBytes];
    uint32_t line;
    uint32_t column;
    DebugAddress instructionAddress;
    DebugMappingState mapping;
    uint64_t projectGeneration;
    uint32_t sourceGeneration;
};

struct DebugTarget {
    char projectId[kMaxProjectIdBytes];
    char projectRoot[kMaxPathBytes];
    uint64_t projectGeneration;
    char targetProfile[kMaxNameBytes];
    char architecture[kDebugMaxArchitectureBytes];
    char abi[kDebugMaxAbiBytes];
    char applicationId[kMaxRunApplicationIdBytes];
    char manifestPath[kMaxProjectPathBytes];
    char executablePath[kMaxProjectPathBytes];
    uint64_t artifactSize;
    char artifactSha256[kMaxRunArtifactSha256Bytes];
    char workingDirectory[kMaxPathBytes];
    char arguments[kDebugMaxArgumentsBytes];
};

struct DebugBreakpoint {
    uint64_t id;
    char projectId[kMaxProjectIdBytes];
    DebugSourceLocation location;
    bool enabled;
    DebugBreakpointState state;
    DebugErrorCode mappingError;
    uint32_t mappedAddressCount;
    DebugAddress mappedAddresses[kDebugMaxMappedAddresses];
    uint64_t backendBindingId;
    uint64_t sessionGeneration;
    bool lastHit;
    uint64_t lastHitSessionGeneration;
    char message[kDebugMaxMessageBytes];
};

struct DebugDwarfMapper;

struct DebugEvent {
    uint64_t sequence;
    uint64_t sessionGeneration;
    DebugEventKind kind;
    DebugSessionState state;
    DebugStopReason stopReason;
    uint64_t processId;
    uint64_t nativeRuntimeId;
    uint64_t breakpointId;
    uint64_t targetAddress;
    uint64_t threadId;
    uint64_t projectGeneration;
    int32_t exitCode;
    DebugSourceLocation location;
    char message[kDebugMaxMessageBytes];
};

struct DebugBackendSnapshot {
    uint64_t sessionGeneration;
    uint64_t debugHandle;
    DebugSessionState state;
    uint64_t processId;
    uint64_t nativeRuntimeId;
    int32_t exitCode;
    bool cleanupComplete;
    DebugStopReason stopReason;
    char backendName[kDebugMaxBackendNameBytes];
    char errorMessage[kDebugMaxMessageBytes];
    bool breakpointTrap;
    uint64_t threadId;
    uint64_t instructionPointer;
    DebugAddress targetAddress;
    uint64_t breakpointBindingId;
    uint8_t originalByte;
    uint8_t installedByte;
    bool originalByteValid;
};

struct DebugBackendBinding {
    bool accepted;
    uint64_t bindingId;
    uint8_t originalByte;
    uint8_t installedByte;
    bool originalByteValid;
    char message[kDebugMaxMessageBytes];
};

typedef bool (*DebugBackendBindFn)(void* userData, const DebugTarget& target,
                                   uint64_t sessionGeneration, uint64_t processId,
                                   uint64_t nativeRuntimeId, const DebugBreakpoint& breakpoint,
                                   DebugBackendBinding* outBinding);
typedef bool (*DebugBackendCommandFn)(void* userData, HostedDebugCommand command,
                                      uint64_t handle, uint64_t sessionGeneration,
                                      uint64_t processId, uint64_t nativeRuntimeId,
                                      uint64_t breakpointId, uint64_t targetAddress,
                                      const char* artifactSha256, HostedDebugResult* outResult);

typedef bool (*DebugBackendLaunchFn)(void* userData, const DebugTarget& target,
                                     uint64_t sessionGeneration, DebugBackendSnapshot* outSnapshot);
typedef bool (*DebugBackendPollFn)(void* userData, uint64_t sessionGeneration,
                                   DebugBackendSnapshot* outSnapshot);
typedef bool (*DebugBackendStopFn)(void* userData, uint64_t sessionGeneration);
typedef bool (*DebugBackendPauseFn)(void* userData, uint64_t sessionGeneration);
typedef bool (*DebugBackendContinueFn)(void* userData, uint64_t sessionGeneration);

struct DebugBackend {
    void* userData;
    DebugCapabilities capabilities;
    char name[kDebugMaxBackendNameBytes];
    DebugBackendLaunchFn launch;
    DebugBackendPollFn poll;
    DebugBackendStopFn stop;
    DebugBackendPauseFn pause;
    DebugBackendContinueFn continueExecution;
    DebugBackendBindFn bindSoftwareBreakpoint;
    DebugBackendCommandFn debugCommand;
};

struct DebugSourceMapper {
    void* userData;
    bool (*sourceToAddress)(void* userData, const DebugTarget& target, DebugSourceLocation* location);
    bool (*addressToSource)(void* userData, const DebugTarget& target, DebugSourceLocation* location);
    bool (*symbolToAddress)(void* userData, const DebugTarget& target, const char* symbol, DebugAddress* address);
};

struct DebugController {
    DebugSessionState state;
    DebugErrorCode error;
    bool active;
    uint64_t sessionGeneration;
    uint64_t nextBreakpointId;
    uint64_t projectGeneration;
    char projectId[kMaxProjectIdBytes];
    char projectRoot[kMaxPathBytes];
    DebugTarget target;
    DebugCapabilities capabilities;
    char backendName[kDebugMaxBackendNameBytes];
    uint64_t processId;
    uint64_t nativeRuntimeId;
    uint64_t debugHandle;
    int32_t exitCode;
    DebugStopReason stopReason;
    char lastMessage[kDebugMaxMessageBytes];
    bool targetExecutionReleased;
    uint64_t nextEventSequence;
    DebugAddress currentInstructionAddress;
    DebugSourceLocation currentLocation;
    uint64_t currentThreadId;
    uint64_t reportedInstructionPointer;
    uint64_t lastBreakpointId;
    DebugBreakpoint breakpoints[kDebugMaxBreakpoints];
    uint32_t breakpointCount;
    DebugEvent events[kDebugMaxEvents];
    uint32_t eventCount;
};

const char* DebugSessionStateName(DebugSessionState state);
const char* DebugErrorName(DebugErrorCode error);
const char* DebugBreakpointStateName(DebugBreakpointState state);
const char* DebugStopReasonName(DebugStopReason reason);
const char* DebugEventKindName(DebugEventKind kind);
bool DebugCapabilitiesEqual(const DebugCapabilities& left, const DebugCapabilities& right);
bool DebugCapabilitiesHasPause(const DebugCapabilities& capabilities);
bool DebugCapabilitiesHasContinue(const DebugCapabilities& capabilities);

bool DebugTargetFromBuild(const Project& project, const BuildResult& build,
                          uint64_t projectGeneration, DebugTarget* target, DebugErrorCode* error);
bool DebugRelativeSourcePath(const char* projectRoot, const char* absolutePath,
                             char* relativePath, uint32_t relativePathSize);

bool DebugControllerInit(DebugController* controller);
bool DebugControllerSetProjectContext(DebugController* controller, const char* projectId,
                                      const char* projectRoot, uint64_t projectGeneration);
void DebugControllerClearBreakpoints(DebugController* controller);
bool DebugControllerStart(DebugController* controller, const DebugBackend& backend,
                          const DebugTarget& target, DebugErrorCode* error);
bool DebugControllerPoll(DebugController* controller, const DebugBackend& backend);
bool DebugControllerRequestStop(DebugController* controller, const DebugBackend& backend,
                                DebugErrorCode* error);
bool DebugControllerPause(DebugController* controller, const DebugBackend& backend,
                          DebugErrorCode* error);
bool DebugControllerContinue(DebugController* controller, const DebugBackend& backend,
                             DebugErrorCode* error);
bool DebugControllerIsActive(const DebugController* controller);
bool DebugControllerCanStart(const DebugController* controller);
bool DebugControllerCanStop(const DebugController* controller);
bool DebugControllerCanPause(const DebugController* controller);
bool DebugControllerCanContinue(const DebugController* controller);
bool DebugControllerApplySnapshot(DebugController* controller, uint64_t sessionGeneration,
                                  const DebugBackendSnapshot& snapshot);
bool DebugControllerResolveCurrentStop(DebugController* controller, const DebugDwarfMapper* mapper,
                                       DebugErrorCode* error);

bool DebugControllerToggleBreakpoint(DebugController* controller, const char* projectId,
                                     const char* projectRoot, uint64_t projectGeneration,
                                     const char* sourcePath, uint32_t line, uint32_t column,
                                     uint32_t sourceGeneration, uint64_t* outBreakpointId,
                                     DebugErrorCode* error);
bool DebugControllerAddBreakpoint(DebugController* controller, const char* projectId,
                                   const char* projectRoot, uint64_t projectGeneration,
                                   const char* sourcePath, uint32_t line, uint32_t column,
                                   uint32_t sourceGeneration, uint64_t* outBreakpointId,
                                   DebugErrorCode* error);
bool DebugControllerDeleteBreakpoint(DebugController* controller, uint64_t breakpointId,
                                     DebugErrorCode* error);
bool DebugControllerSetBreakpointEnabled(DebugController* controller, uint64_t breakpointId,
                                         bool enabled, DebugErrorCode* error);
bool DebugControllerApplyBreakpointBinding(DebugController* controller, uint64_t sessionGeneration,
                                            uint64_t breakpointId, bool accepted,
                                            uint64_t backendBindingId, DebugAddress address,
                                            const char* message, DebugErrorCode* error);
void DebugControllerMarkProjectGeneration(DebugController* controller, uint64_t projectGeneration);
void DebugControllerMarkArtifactStale(DebugController* controller, const char* message);
bool DebugControllerMapBreakpoints(DebugController* controller, const DebugDwarfMapper* mapper,
                                   DebugErrorCode* error);
void DebugControllerMarkSourceGeneration(DebugController* controller, const char* projectId,
                                         const char* sourcePath, uint32_t sourceGeneration);
const DebugBreakpoint* DebugControllerBreakpointAt(const DebugController* controller, uint32_t index);
const DebugEvent* DebugControllerEventAt(const DebugController* controller, uint32_t index);

} // namespace developer_studio
} // namespace guidexos
