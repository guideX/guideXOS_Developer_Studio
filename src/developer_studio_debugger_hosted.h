#pragma once

#include "developer_studio_debugger.h"

namespace guidexos {
namespace developer_studio {

struct HostedDebugBackend {
    HostedDevelopmentRunService runService;
    RunController runController;
    bool userPauseStopPending;
    bool userStepStopPending;
    bool internalTrapStopPending;
    DebugBackendSnapshot userStepStopSnapshot;
    DebugBackendSnapshot lastSnapshot;
    bool lastSnapshotValid;
    uint32_t lastStopRoute;
    uint32_t lastStopStatus;
    bool lastStopFallbackFailed;
    uint64_t stopProcessId;
    uint64_t stopRuntimeId;
    bool resumeTerminalPending;
    uint64_t resumeTerminalGeneration;
    // Command responses contain bounded register/error storage. Keep the
    // reusable response out of the NativeElf callback stack.
    HostedDebugResult commandResult;
};

void HostedDebugBackendInit(HostedDebugBackend* backend,
                            const HostedDevelopmentRunService& runService);
void HostedDebugBackendSetStopIdentity(HostedDebugBackend* backend,
                                       uint64_t processId, uint64_t nativeRuntimeId);
DebugBackend HostedDebugBackendCreate(HostedDebugBackend* backend);

} // namespace developer_studio
} // namespace guidexos
