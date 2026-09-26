#include "developer_studio_startup.h"

#include <stddef.h>

namespace guidexos {
namespace developer_studio {
namespace {

static void clearBytes(void* value, size_t size) {
    unsigned char* bytes = static_cast<unsigned char*>(value);
    for (size_t i = 0; i < size; ++i) bytes[i] = 0;
}

static bool copyText(char* destination, uint32_t capacity, const char* source) {
    if (!destination || capacity == 0 || !source || !source[0]) return false;
    uint32_t length = 0;
    while (source[length]) {
        if (length + 1 >= capacity) return false;
        ++length;
    }
    for (uint32_t i = 0; i <= length; ++i) destination[i] = source[i];
    return true;
}

static bool ownerMatches(const DiagnosticStartupOwner* owner, uint64_t startupGeneration) {
    return owner && owner->state != DiagnosticStartupState::Uninitialized &&
        startupGeneration != 0 && owner->startupGeneration == startupGeneration;
}

static bool isTerminal(DiagnosticStartupState state) {
    return state == DiagnosticStartupState::NoDiagnosticFixture ||
        state == DiagnosticStartupState::RequestAccepted ||
        state == DiagnosticStartupState::RequestRejected;
}

} // namespace

void DiagnosticStartupOwnerInit(DiagnosticStartupOwner* owner) {
    if (!owner) return;
    clearBytes(owner, sizeof(*owner));
    owner->state = DiagnosticStartupState::Uninitialized;
}

bool DiagnosticStartupBegin(DiagnosticStartupOwner* owner, uint64_t applicationInstanceId,
                            uint64_t startupGeneration) {
    if (!owner || applicationInstanceId == 0 || startupGeneration == 0) return false;
    if (owner->state != DiagnosticStartupState::Uninitialized && !isTerminal(owner->state)) return false;
    clearBytes(owner, sizeof(*owner));
    owner->applicationInstanceId = applicationInstanceId;
    owner->startupGeneration = startupGeneration;
    owner->state = DiagnosticStartupState::Initializing;
    return true;
}

bool DiagnosticStartupResolveFixture(DiagnosticStartupOwner* owner, uint64_t startupGeneration,
                                     bool recognized) {
    if (!ownerMatches(owner, startupGeneration) ||
        owner->state != DiagnosticStartupState::Initializing) return false;
    owner->diagnosticFixtureRecognized = recognized;
    owner->state = recognized ? DiagnosticStartupState::DiagnosticFixtureRecognized :
        DiagnosticStartupState::NoDiagnosticFixture;
    return true;
}

bool DiagnosticStartupMarkUiReady(DiagnosticStartupOwner* owner, uint64_t startupGeneration) {
    if (!ownerMatches(owner, startupGeneration) ||
        owner->state != DiagnosticStartupState::DiagnosticFixtureRecognized) return false;
    owner->state = DiagnosticStartupState::UiReady;
    return true;
}

bool DiagnosticStartupMarkPumpEntered(DiagnosticStartupOwner* owner, uint64_t startupGeneration) {
    if (!ownerMatches(owner, startupGeneration) ||
        owner->state != DiagnosticStartupState::UiReady) return false;
    owner->state = DiagnosticStartupState::PumpEntered;
    return true;
}

bool DiagnosticStartupConstructProjectRequest(DiagnosticStartupOwner* owner,
                                              uint64_t startupGeneration,
                                              const char* projectPath,
                                              uint64_t* requestId) {
    if (requestId) *requestId = 0;
    if (!ownerMatches(owner, startupGeneration) || !requestId ||
        owner->state != DiagnosticStartupState::PumpEntered) return false;
    owner->request.requestId = startupGeneration;
    owner->request.startupGeneration = startupGeneration;
    owner->request.projectLifecycleRequestId = 0;
    if (!copyText(owner->request.projectPath, sizeof(owner->request.projectPath), projectPath)) {
        clearBytes(&owner->request, sizeof(owner->request));
        return false;
    }
    *requestId = owner->request.requestId;
    owner->state = DiagnosticStartupState::RequestPending;
    return true;
}

bool DiagnosticStartupMarkRequestSubmitted(DiagnosticStartupOwner* owner,
                                           uint64_t startupGeneration,
                                           uint64_t requestId) {
    if (!ownerMatches(owner, startupGeneration) || requestId == 0 ||
        owner->state != DiagnosticStartupState::RequestPending ||
        owner->request.requestId != requestId ||
        owner->request.startupGeneration != startupGeneration) return false;
    owner->state = DiagnosticStartupState::RequestSubmitted;
    return true;
}

bool DiagnosticStartupObserveProjectAccepted(DiagnosticStartupOwner* owner,
                                             uint64_t startupGeneration,
                                             uint64_t requestId,
                                             uint64_t projectLifecycleRequestId) {
    if (!ownerMatches(owner, startupGeneration) || requestId == 0 ||
        projectLifecycleRequestId == 0 ||
        owner->state != DiagnosticStartupState::RequestSubmitted ||
        owner->request.requestId != requestId ||
        owner->request.startupGeneration != startupGeneration) return false;
    owner->request.projectLifecycleRequestId = projectLifecycleRequestId;
    owner->state = DiagnosticStartupState::RequestAccepted;
    return true;
}

bool DiagnosticStartupRejectProjectRequest(DiagnosticStartupOwner* owner,
                                           uint64_t startupGeneration,
                                           uint64_t requestId) {
    if (!ownerMatches(owner, startupGeneration) || requestId == 0 ||
        (owner->state != DiagnosticStartupState::RequestPending &&
         owner->state != DiagnosticStartupState::RequestSubmitted) ||
        owner->request.requestId != requestId ||
        owner->request.startupGeneration != startupGeneration) return false;
    owner->state = DiagnosticStartupState::RequestRejected;
    return true;
}

const char* DiagnosticStartupStateName(DiagnosticStartupState state) {
    switch (state) {
    case DiagnosticStartupState::Uninitialized: return "uninitialized";
    case DiagnosticStartupState::Initializing: return "initializing";
    case DiagnosticStartupState::NoDiagnosticFixture: return "no_fixture";
    case DiagnosticStartupState::DiagnosticFixtureRecognized: return "fixture_recognized";
    case DiagnosticStartupState::UiReady: return "ui_ready";
    case DiagnosticStartupState::PumpEntered: return "pump_entered";
    case DiagnosticStartupState::RequestPending: return "request_pending";
    case DiagnosticStartupState::RequestSubmitted: return "request_submitted";
    case DiagnosticStartupState::RequestAccepted: return "request_accepted";
    case DiagnosticStartupState::RequestRejected: return "request_rejected";
    }
    return "unknown";
}

} // namespace developer_studio
} // namespace guidexos
