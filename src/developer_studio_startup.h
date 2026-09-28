#ifndef GUIDEXOS_DEVELOPER_STUDIO_STARTUP_H
#define GUIDEXOS_DEVELOPER_STUDIO_STARTUP_H

#include <stdint.h>

#include "developer_studio_diagnostic_sentinel.h"

namespace guidexos {
namespace developer_studio {

enum class DiagnosticStartupState : uint8_t {
    Uninitialized,
    Initializing,
    NoDiagnosticFixture,
    DiagnosticFixtureRecognized,
    UiReady,
    PumpEntered,
    RequestPending,
    RequestSubmitted,
    RequestAccepted,
    RequestRejected
};

enum class DiagnosticSentinelState : uint8_t {
    Uninitialized,
    WaitingForFilesystem,
    Present,
    Absent,
    Error
};

enum class DiagnosticSentinelReason : uint8_t {
    None,
    FilesystemNotReady,
    MountNotReady,
    WrongMount,
    PathInvalid,
    NotFound,
    IoError,
    NotRegularFile,
    InvalidContent
};

enum class DiagnosticSentinelMountState : uint8_t {
    NotReady,
    WrongMount,
    Ready
};

enum class DiagnosticSentinelIoResult : uint8_t {
    Found,
    NotFound,
    MountUnavailable,
    InvalidPath,
    IoError
};

enum class DiagnosticSentinelPathFailure : uint8_t {
    None,
    InvalidArgument,
    NotAbsolute,
    OutputTooSmall,
    EmptyPath,
    TraversalComponent
};

struct DiagnosticSentinelIo {
    void* userData;
    DiagnosticSentinelIoResult (*stat)(void* userData, const char* path,
                                       bool* outRegularFile, uint64_t* outSize);
    DiagnosticSentinelIoResult (*read)(void* userData, const char* path,
                                       char* buffer, uint32_t capacity, uint32_t* outBytes);
};

struct DiagnosticSentinelDetection {
    uint64_t startupGeneration;
    DiagnosticSentinelState state;
    DiagnosticSentinelReason reason;
    char normalizedPath[96];
    bool pathNormalized;
    uint32_t pathInputLength;
    uint32_t pathFailureOffset;
    uint8_t pathFirstByte;
    DiagnosticSentinelPathFailure pathFailure;
};

static const uint32_t kDiagnosticStartupProjectPathCapacity = 96;

struct DiagnosticProjectOpenRequest {
    uint64_t requestId;
    uint64_t startupGeneration;
    uint64_t projectLifecycleRequestId;
    char projectPath[kDiagnosticStartupProjectPathCapacity];
};

struct DiagnosticStartupOwner {
    uint64_t applicationInstanceId;
    uint64_t startupGeneration;
    DiagnosticStartupState state;
    bool diagnosticFixtureRecognized;
    DiagnosticProjectOpenRequest request;
};

void DiagnosticStartupOwnerInit(DiagnosticStartupOwner* owner);
bool DiagnosticStartupBegin(DiagnosticStartupOwner* owner, uint64_t applicationInstanceId,
                            uint64_t startupGeneration);
bool DiagnosticStartupResolveFixture(DiagnosticStartupOwner* owner, uint64_t startupGeneration,
                                     bool recognized);
bool DiagnosticStartupMarkUiReady(DiagnosticStartupOwner* owner, uint64_t startupGeneration);
bool DiagnosticStartupMarkPumpEntered(DiagnosticStartupOwner* owner, uint64_t startupGeneration);
bool DiagnosticStartupConstructProjectRequest(DiagnosticStartupOwner* owner,
                                              uint64_t startupGeneration,
                                              const char* projectPath,
                                              uint64_t* requestId);
bool DiagnosticStartupMarkRequestSubmitted(DiagnosticStartupOwner* owner,
                                           uint64_t startupGeneration,
                                           uint64_t requestId);
bool DiagnosticStartupObserveProjectAccepted(DiagnosticStartupOwner* owner,
                                             uint64_t startupGeneration,
                                             uint64_t requestId,
                                             uint64_t projectLifecycleRequestId);
bool DiagnosticStartupRejectProjectRequest(DiagnosticStartupOwner* owner,
                                           uint64_t startupGeneration,
                                           uint64_t requestId);
const char* DiagnosticStartupStateName(DiagnosticStartupState state);

void DiagnosticSentinelDetectionBegin(DiagnosticSentinelDetection* detection,
                                      uint64_t startupGeneration);
bool DiagnosticSentinelNormalizePath(const char* path, char* output, uint32_t capacity);
bool DiagnosticSentinelNormalizePathDetailed(const char* path, char* output, uint32_t capacity,
                                             DiagnosticSentinelPathFailure* failure,
                                             uint32_t* failureOffset);
const char* DiagnosticSentinelPathFailureName(DiagnosticSentinelPathFailure failure);
bool DiagnosticSentinelDetectionEvaluate(DiagnosticSentinelDetection* detection,
                                         uint64_t startupGeneration,
                                         bool filesystemReady,
                                         DiagnosticSentinelMountState mountState,
                                         const DiagnosticSentinelIo* io);
const char* DiagnosticSentinelStateName(DiagnosticSentinelState state);
const char* DiagnosticSentinelReasonName(DiagnosticSentinelReason reason);
const char* DiagnosticSentinelIoResultName(DiagnosticSentinelIoResult result);

} // namespace developer_studio
} // namespace guidexos

#endif
