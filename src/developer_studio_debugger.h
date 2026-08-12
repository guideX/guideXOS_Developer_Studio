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
static const uint32_t kDebugMaxSourceStepInstructions = 1024;
static const uint32_t kDebugMaxStepOverCalls = 32;
static const uint32_t kDebugMaxInstructionBytes = 16;
static const uint32_t kDebugMaxStackFrames = 64;
static const uint32_t kDebugMaxStackFrameDelta = 1024u * 1024u;
static const uint32_t kDebugMaxFunctionNameBytes = 128;

enum class DebugArchitecture {
    Unknown = 0,
    Amd64,
    Arm64,
    RiscV64
};

enum class DebugSessionState {
    Idle = 0,
    Launching,
    Running,
    Paused,
    Stopping,
    Exited,
    Failed,
    Stepping
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
    UnsupportedOpcode,
    StaleStopContext,
    OriginalByteRestoreFailed,
    ContextReadFailed,
    ContextWriteFailed,
    RipCorrectionFailed,
    TrapFlagSetFailed,
    TargetResumeFailed,
    SingleStepNotObserved,
    BreakpointReinstallFailed,
    TrapFlagClearFailed,
    SourceStepUnavailable,
    SourceStepLimitExceeded,
    SourceStepFailed,
    StaleSourceStep,
    WrongStepThread,
    StepNotObserved,
    InstructionReadFailed,
    UnsupportedCallEncoding,
    StepOverLimitExceeded,
    StepOverFailed,
    StaleStepOver
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
    Unknown,
    Step
};

enum class DebugBackendExecutionState {
    None = 0,
    Running,
    PausedAtBreakpoint,
    PreparingBreakpointResume,
    SingleStepPending,
    UserSourceStepPending,
    PausedAtSourceStep,
    StepOverPending,
    PausedAtStepOver
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
    UnexpectedTrap,
    ContinueRequested,
    SingleStepComplete,
    BreakpointRebound,
    StepRequested,
    StepComplete,
    SourceStepLimitExceeded,
    StepOverStarted,
    StepOverCallDetected,
    StepOverTemporaryBreakpointBound,
    StepOverReturnReached,
    StepOverCompleted,
    StepOverInterrupted,
    StepOverLimitExceeded
};

enum class DebugSourceStepStatus {
    None = 0,
    Active,
    Completed,
    LimitExceeded,
    Cancelled,
    Failed
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

struct DebugSourceStepOperation {
    bool active;
    DebugSourceStepStatus status;
    uint16_t reserved;
    uint64_t sessionGeneration;
    uint64_t stopGeneration;
    uint64_t processId;
    uint64_t threadId;
    uint64_t startingAddress;
    DebugSourceLocation startingSourceLocation;
    uint32_t startingSequence;
    uint32_t stepCount;
    uint32_t maxStepCount;
    uint64_t lastAddress;
    DebugSourceLocation lastSourceLocation;
    uint64_t breakpointId;
    uint64_t bindingId;
    uint64_t breakpointAddress;
    bool reinstallBreakpoint;
    char reason[kDebugMaxMessageBytes];
};

enum class DebugStepOverMode {
    None = 0,
    SourceSingleStep,
    TemporaryReturnBreakpoint
};

enum class DebugStepOverStatus {
    None = 0,
    Active,
    Completed,
    LimitExceeded,
    Cancelled,
    Failed
};

struct DebugStepOverOperation {
    bool active;
    DebugStepOverStatus status;
    DebugStepOverMode mode;
    uint8_t reserved;
    uint64_t sessionGeneration;
    uint64_t stopGeneration;
    uint64_t processId;
    uint64_t nativeRuntimeId;
    uint64_t threadId;
    uint64_t startingAddress;
    DebugSourceLocation startingSourceLocation;
    uint64_t startingRsp;
    uint64_t startingRbp;
    uint64_t breakpointId;
    uint64_t bindingId;
    uint64_t breakpointAddress;
    bool reinstallBreakpoint;
    uint64_t callAddress;
    uint64_t returnAddress;
    uint64_t temporaryBreakpointId;
    uint64_t temporaryBindingId;
    uint64_t instructionCount;
    uint64_t maxInstructionCount;
    uint32_t callCount;
    uint32_t maxCallCount;
    uint8_t originalByte;
    uint8_t installedByte;
    bool originalByteValid;
    bool temporaryInstalled;
    DebugSourceLocation lastSourceLocation;
    uint64_t lastAddress;
    char reason[kDebugMaxMessageBytes];
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

// This is the debugger-core representation of a stopped target context. It
// intentionally contains only fixed-width values and bounded ownership
// identity; host CONTEXT pointers and other platform types must not cross this
// boundary.
struct DebugRegisterContext {
    bool valid;
    DebugArchitecture architecture;
    uint16_t reserved;
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
    uint64_t stackLow;
    uint64_t stackHigh;
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
    DebugBackendExecutionState executionState;
    uint64_t stopGeneration;
    bool singleStepTrap;
    uint32_t singleStepKind;
    DebugRegisterContext registerContext;
    bool internalBreakpointTrap;
    uint64_t internalBreakpointId;
    uint64_t stackLow;
    uint64_t stackHigh;
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
                                      const char* artifactSha256, uint64_t threadId,
                                      uint64_t stopGeneration, bool reinstallBreakpoint,
                                      uint64_t auxiliaryAddress, uint32_t readByteCount,
                                      HostedDebugResult* outResult);

typedef bool (*DebugBackendLaunchFn)(void* userData, const DebugTarget& target,
                                     uint64_t sessionGeneration, DebugBackendSnapshot* outSnapshot);
typedef bool (*DebugBackendPollFn)(void* userData, uint64_t sessionGeneration,
                                   DebugBackendSnapshot* outSnapshot);
typedef bool (*DebugBackendStopFn)(void* userData, uint64_t sessionGeneration);
typedef bool (*DebugBackendPauseFn)(void* userData, uint64_t sessionGeneration);
typedef bool (*DebugBackendContinueFn)(void* userData, uint64_t sessionGeneration,
                                       const DebugRegisterContext& context,
                                       uint64_t breakpointId, uint64_t bindingId,
                                       uint64_t targetAddress, bool reinstallBreakpoint);
typedef bool (*DebugBackendStepFn)(void* userData, uint64_t sessionGeneration,
                                   const DebugRegisterContext& context,
                                   uint64_t breakpointId, uint64_t bindingId,
                                   uint64_t targetAddress, bool reinstallBreakpoint);
typedef bool (*DebugBackendResumeFn)(void* userData, uint64_t sessionGeneration,
                                     const DebugRegisterContext& context);
typedef bool (*DebugBackendReadMemoryFn)(void* userData, uint64_t sessionGeneration,
                                         uint64_t processId, uint64_t nativeRuntimeId,
                                         uint64_t address, uint8_t* bytes, uint32_t requested,
                                         uint32_t* returned);
typedef bool (*DebugBackendReadTargetMemoryFn)(void* userData, uint64_t sessionGeneration,
                                               uint64_t processId, uint64_t nativeRuntimeId,
                                               uint64_t threadId, uint64_t stopGeneration,
                                               uint64_t address, uint8_t* bytes, uint32_t requested,
                                               uint32_t* returned);
typedef bool (*DebugBackendStepOverCallFn)(void* userData, uint64_t sessionGeneration,
                                           const DebugRegisterContext& context,
                                           uint64_t callAddress, uint64_t returnAddress,
                                           uint64_t temporaryBreakpointId);

struct DebugBackend {
    void* userData;
    DebugCapabilities capabilities;
    char name[kDebugMaxBackendNameBytes];
    DebugBackendLaunchFn launch;
    DebugBackendPollFn poll;
    DebugBackendStopFn stop;
    DebugBackendPauseFn pause;
    DebugBackendContinueFn continueExecution;
    DebugBackendStepFn stepInstruction;
    DebugBackendResumeFn resumeExecution;
    DebugBackendBindFn bindSoftwareBreakpoint;
    DebugBackendCommandFn debugCommand;
    DebugBackendReadMemoryFn readMemory;
    DebugBackendReadTargetMemoryFn readTargetMemory;
    DebugBackendStepOverCallFn stepOverCall;
};

struct DebugSourceMapper {
    void* userData;
    bool (*sourceToAddress)(void* userData, const DebugTarget& target, DebugSourceLocation* location);
    bool (*addressToSource)(void* userData, const DebugTarget& target, DebugSourceLocation* location);
    bool (*symbolToAddress)(void* userData, const DebugTarget& target, const char* symbol, DebugAddress* address);
};

enum class DebugStackFrameMappingState {
    Unmapped = 0,
    Mapped,
    External,
    Stale
};

enum class DebugStackFrameConfidence {
    ExactCurrent = 0,
    FramePointer,
    Unmapped,
    Invalid
};

enum class DebugUnwindTerminationReason {
    None = 0,
    EndOfStack,
    FrameLimit,
    InvalidFramePointer,
    ReadFailure,
    Cycle,
    OutsideStack,
    OutsideTarget,
    NoFramePointer,
    StaleContext,
    UnsupportedArchitecture
};

struct DebugStackFrame {
    uint32_t index;
    bool current;
    bool hasReturnAddress;
    DebugStackFrameMappingState mapping;
    DebugStackFrameConfidence confidence;
    uint64_t instructionAddress;
    uint64_t rawReturnAddress;
    uint64_t lookupAddress;
    uint64_t rsp;
    uint64_t rbp;
    uint64_t functionStartAddress;
    uint64_t functionSize;
    char functionName[kDebugMaxFunctionNameBytes];
    char sourcePath[kMaxProjectPathBytes];
    uint32_t sourceLine;
    uint32_t sourceColumn;
};

struct DebugUnwindResult {
    DebugStackFrame frames[kDebugMaxStackFrames];
    uint32_t frameCount;
    bool truncated;
    DebugUnwindTerminationReason terminationReason;
    char status[160];
};

struct DebugCallStack {
    bool valid;
    bool stale;
    uint16_t reserved;
    uint64_t sessionGeneration;
    uint64_t processId;
    uint64_t nativeRuntimeId;
    uint64_t threadId;
    uint64_t stopGeneration;
    uint32_t selectedFrameIndex;
    uint32_t mapperGeneration;
    char artifactSha256[kMaxRunArtifactSha256Bytes];
    char unwinderName[kDebugMaxBackendNameBytes];
    DebugUnwindResult result;
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
    DebugBackendExecutionState backendExecutionState;
    uint64_t stopGeneration;
    DebugRegisterContext stoppedContext;
    uint64_t nextEventSequence;
    DebugAddress currentInstructionAddress;
    DebugSourceLocation currentLocation;
    uint64_t currentThreadId;
    uint64_t reportedInstructionPointer;
    uint64_t lastBreakpointId;
    uint64_t nextTemporaryBreakpointId;
    DebugCallStack callStack;
    DebugSourceStepOperation sourceStep;
    DebugStepOverOperation stepOver;
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
bool DebugRegisterContextIsValid(const DebugRegisterContext& context);

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
bool DebugControllerPoll(DebugController* controller, const DebugBackend& backend,
                         const DebugDwarfMapper* mapper = nullptr);
bool DebugControllerRequestStop(DebugController* controller, const DebugBackend& backend,
                                DebugErrorCode* error);
bool DebugControllerPause(DebugController* controller, const DebugBackend& backend,
                          DebugErrorCode* error);
bool DebugControllerContinue(DebugController* controller, const DebugBackend& backend,
                             DebugErrorCode* error);
bool DebugControllerStepInto(DebugController* controller, const DebugBackend& backend,
                             const DebugDwarfMapper* mapper, DebugErrorCode* error);
bool DebugControllerStepOver(DebugController* controller, const DebugBackend& backend,
                             const DebugDwarfMapper* mapper, DebugErrorCode* error);
bool DebugControllerIsActive(const DebugController* controller);
bool DebugControllerCanStart(const DebugController* controller);
bool DebugControllerCanStop(const DebugController* controller);
bool DebugControllerCanPause(const DebugController* controller);
bool DebugControllerCanContinue(const DebugController* controller);
bool DebugControllerCanStepInto(const DebugController* controller);
bool DebugControllerCanStepOver(const DebugController* controller);
bool DebugControllerApplySnapshot(DebugController* controller, uint64_t sessionGeneration,
                                  const DebugBackendSnapshot& snapshot);
bool DebugControllerResolveCurrentStop(DebugController* controller, const DebugDwarfMapper* mapper,
                                       DebugErrorCode* error);
bool DebugUnwindAmd64FramePointer(const DebugRegisterContext& context,
                                  const DebugDwarfMapper* mapper,
                                  DebugBackendReadTargetMemoryFn readMemory,
                                  void* userData, uint64_t sessionGeneration,
                                  DebugUnwindResult* result);
const char* DebugStackFrameMappingStateName(DebugStackFrameMappingState state);
const char* DebugStackFrameConfidenceName(DebugStackFrameConfidence confidence);
const char* DebugUnwindTerminationReasonName(DebugUnwindTerminationReason reason);
bool DebugControllerBuildCallStack(DebugController* controller, const DebugBackend& backend,
                                   const DebugDwarfMapper* mapper, DebugErrorCode* error);
bool DebugControllerSelectCallStackFrame(DebugController* controller, uint32_t frameIndex,
                                         DebugErrorCode* error);
const DebugStackFrame* DebugControllerCallStackFrameAt(const DebugController* controller,
                                                       uint32_t index);

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

enum class DebugAmd64InstructionKind {
    Call = 0,
    NonCall,
    Unsupported,
    Invalid
};

struct DebugAmd64Instruction {
    DebugAmd64InstructionKind kind;
    uint32_t instructionLength;
    uint64_t returnAddress;
};

bool DebugDecodeAmd64Instruction(const uint8_t* bytes, uint32_t byteCount,
                                  uint64_t address, uint64_t executableEnd,
                                  DebugAmd64Instruction* instruction);

} // namespace developer_studio
} // namespace guidexos
