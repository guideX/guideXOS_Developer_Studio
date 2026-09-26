#ifndef GUIDEXOS_DEVELOPER_STUDIO_STARTUP_H
#define GUIDEXOS_DEVELOPER_STUDIO_STARTUP_H

#include <stdint.h>

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

} // namespace developer_studio
} // namespace guidexos

#endif
