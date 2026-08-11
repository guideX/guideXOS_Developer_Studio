#pragma once

#include "developer_studio_debugger.h"

namespace guidexos {
namespace developer_studio {

struct HostedDebugBackend {
    HostedDevelopmentRunService runService;
    RunController runController;
    bool userStepStopPending;
    bool internalTrapStopPending;
    DebugBackendSnapshot userStepStopSnapshot;
};

void HostedDebugBackendInit(HostedDebugBackend* backend,
                            const HostedDevelopmentRunService& runService);
DebugBackend HostedDebugBackendCreate(HostedDebugBackend* backend);

} // namespace developer_studio
} // namespace guidexos
