#include "developer_studio_startup.h"

#include <cassert>
#include <cstring>
#include <iostream>

using namespace guidexos::developer_studio;

static void testExplicitInitializationAndAcceptedHandoff() {
    DiagnosticStartupOwner owner;
    std::memset(&owner, 0xA5, sizeof(owner));
    DiagnosticStartupOwnerInit(&owner);
    assert(owner.state == DiagnosticStartupState::Uninitialized);
    assert(owner.applicationInstanceId == 0);
    assert(owner.startupGeneration == 0);
    assert(owner.request.requestId == 0);
    assert(owner.request.projectPath[0] == '\0');

    assert(DiagnosticStartupBegin(&owner, 11, 29));
    assert(owner.state == DiagnosticStartupState::Initializing);
    assert(DiagnosticStartupResolveFixture(&owner, 29, true));
    assert(owner.state == DiagnosticStartupState::DiagnosticFixtureRecognized);
    assert(DiagnosticStartupMarkUiReady(&owner, 29));
    assert(DiagnosticStartupMarkPumpEntered(&owner, 29));

    uint64_t requestId = 0;
    assert(!DiagnosticStartupConstructProjectRequest(&owner, 29, "", &requestId));
    assert(owner.state == DiagnosticStartupState::PumpEntered);
    assert(DiagnosticStartupConstructProjectRequest(&owner, 29, "/P28Q", &requestId));
    assert(requestId == 29);
    assert(owner.state == DiagnosticStartupState::RequestPending);
    assert(std::strcmp(owner.request.projectPath, "/P28Q") == 0);
    assert(owner.request.projectLifecycleRequestId == 0);

    // A nested/repeated startup callback cannot replace or duplicate the live request.
    assert(!DiagnosticStartupBegin(&owner, 12, 30));
    assert(!DiagnosticStartupMarkPumpEntered(&owner, 29));
    uint64_t duplicateRequestId = 0;
    assert(!DiagnosticStartupConstructProjectRequest(&owner, 29, "/P28Q", &duplicateRequestId));
    assert(duplicateRequestId == 0);
    assert(owner.request.requestId == requestId);

    assert(DiagnosticStartupMarkRequestSubmitted(&owner, 29, requestId));
    assert(owner.state == DiagnosticStartupState::RequestSubmitted);
    assert(!DiagnosticStartupRejectProjectRequest(&owner, 28, requestId));
    assert(owner.state == DiagnosticStartupState::RequestSubmitted);
    assert(DiagnosticStartupObserveProjectAccepted(&owner, 29, requestId, 7));
    assert(owner.state == DiagnosticStartupState::RequestAccepted);
    assert(owner.request.projectLifecycleRequestId == 7);
    assert(!DiagnosticStartupObserveProjectAccepted(&owner, 29, requestId, 8));
}

static void testAbsentFixtureAndStaleGeneration() {
    DiagnosticStartupOwner owner;
    DiagnosticStartupOwnerInit(&owner);
    assert(DiagnosticStartupBegin(&owner, 21, 40));
    assert(DiagnosticStartupResolveFixture(&owner, 40, false));
    assert(owner.state == DiagnosticStartupState::NoDiagnosticFixture);
    assert(!DiagnosticStartupMarkUiReady(&owner, 40));
    uint64_t requestId = 0;
    assert(!DiagnosticStartupConstructProjectRequest(&owner, 40, "/P28Q", &requestId));
    assert(!DiagnosticStartupResolveFixture(&owner, 41, true));

    // Closing the prior lifecycle and launching a new app gets a fresh owner generation.
    assert(DiagnosticStartupBegin(&owner, 22, 41));
    assert(DiagnosticStartupResolveFixture(&owner, 41, true));
    assert(DiagnosticStartupMarkUiReady(&owner, 41));
    assert(DiagnosticStartupMarkPumpEntered(&owner, 41));
    assert(!DiagnosticStartupConstructProjectRequest(&owner, 40, "/P28Q", &requestId));
    assert(owner.state == DiagnosticStartupState::PumpEntered);
    assert(DiagnosticStartupConstructProjectRequest(&owner, 41, "/P28Q", &requestId));
    assert(requestId == 41);
}

static void testPendingRequestMustBeSettled() {
    DiagnosticStartupOwner owner;
    DiagnosticStartupOwnerInit(&owner);
    assert(DiagnosticStartupBegin(&owner, 31, 50));
    assert(DiagnosticStartupResolveFixture(&owner, 50, true));
    assert(DiagnosticStartupMarkUiReady(&owner, 50));
    assert(DiagnosticStartupMarkPumpEntered(&owner, 50));

    uint64_t requestId = 0;
    assert(DiagnosticStartupConstructProjectRequest(&owner, 50, "/P28Q", &requestId));
    assert(owner.state == DiagnosticStartupState::RequestPending);
    assert(owner.request.projectPath[0] == '/');
    assert(!DiagnosticStartupBegin(&owner, 32, 51));
    assert(DiagnosticStartupMarkRequestSubmitted(&owner, 50, requestId));
    assert(owner.state == DiagnosticStartupState::RequestSubmitted);
    assert(DiagnosticStartupRejectProjectRequest(&owner, 50, requestId));
    assert(owner.state == DiagnosticStartupState::RequestRejected);
    assert(owner.request.requestId == requestId);
    assert(std::strcmp(DiagnosticStartupStateName(owner.state), "request_rejected") == 0);

    assert(DiagnosticStartupBegin(&owner, 32, 51));
    assert(owner.applicationInstanceId == 32);
    assert(owner.startupGeneration == 51);
    assert(owner.state == DiagnosticStartupState::Initializing);
}

int main() {
    testExplicitInitializationAndAcceptedHandoff();
    testAbsentFixtureAndStaleGeneration();
    testPendingRequestMustBeSettled();
    std::cout << "Developer Studio startup lifecycle test passed\n";
    return 0;
}
