#include "developer_studio_run.h"

namespace guidexos {
namespace developer_studio {
namespace {

static uint32_t textLength(const char* value, uint32_t capacity) {
    if (!value) return 0;
    uint32_t length = 0;
    while (length < capacity && value[length] != '\0') ++length;
    return length;
}

static bool copyText(char* output, uint32_t outputSize, const char* input) {
    if (!output || outputSize == 0 || !input) return false;
    const uint32_t length = textLength(input, outputSize);
    if (length >= outputSize) { output[0] = '\0'; return false; }
    for (uint32_t i = 0; i < length; ++i) output[i] = input[i];
    output[length] = '\0';
    return true;
}

static void setFailure(RunController* controller, RunState state, RunErrorCode error) {
    controller->state = state;
    controller->result.state = state;
    controller->result.error = error;
    controller->active = false;
    controller->terminalPublished = true;
}

static bool isTerminal(RunState state) {
    return state == RunState::Completed || state == RunState::Failed;
}

} // namespace

const char* RunStateName(RunState state) {
    switch (state) {
    case RunState::Idle: return "Idle";
    case RunState::Validating: return "Validating";
    case RunState::Prepared: return "Prepared";
    case RunState::Registered: return "Registered";
    case RunState::Launching: return "Launching";
    case RunState::Running: return "Running";
    case RunState::Exited: return "Exited";
    case RunState::CleaningUp: return "CleaningUp";
    case RunState::Completed: return "Completed";
    case RunState::Failed: return "Failed";
    }
    return "Unknown";
}

const char* RunErrorName(RunErrorCode error) {
    switch (error) {
    case RunErrorCode::None: return "none";
    case RunErrorCode::InvalidRequest: return "invalid_request";
    case RunErrorCode::BuildRequired: return "build_required";
    case RunErrorCode::ArtifactInvalid: return "artifact_invalid";
    case RunErrorCode::ServiceUnavailable: return "service_unavailable";
    case RunErrorCode::AlreadyActive: return "already_active";
    case RunErrorCode::OwnerMismatch: return "owner_mismatch";
    case RunErrorCode::StaleDeployment: return "stale_deployment";
    case RunErrorCode::LaunchFailed: return "launch_failed";
    case RunErrorCode::ApplicationExited: return "application_exited";
    case RunErrorCode::UserCancelled: return "user_cancelled";
    }
    return "unknown";
}

bool RunRequestFromBuild(const Project& project, const BuildResult& build, RunRequest* request, RunErrorCode* error) {
    if (error) *error = RunErrorCode::None;
    if (!request) { if (error) *error = RunErrorCode::InvalidRequest; return false; }
    *request = RunRequest();
    if (!project.valid || project.kind != ProjectKind::NativeGuiApplication || build.state != BuildState::Succeeded) {
        if (error) *error = RunErrorCode::BuildRequired;
        return false;
    }
    if (!build.artifactValid || !build.artifactEntryPoint || build.artifactSha256[0] == '\0') {
        if (error) *error = RunErrorCode::ArtifactInvalid;
        return false;
    }
    if (!copyText(request->projectRoot, sizeof(request->projectRoot), project.rootPath) ||
        !copyText(request->projectId, sizeof(request->projectId), project.projectId) ||
        !copyText(request->projectKind, sizeof(request->projectKind), "native-gui-application") ||
        !copyText(request->targetProfile, sizeof(request->targetProfile), project.targetProfileId) ||
        !copyText(request->manifestPath, sizeof(request->manifestPath), project.manifestPath) ||
        !copyText(request->artifactPath, sizeof(request->artifactPath), build.artifactPath) ||
        !copyText(request->artifactSha256, sizeof(request->artifactSha256), build.artifactSha256)) {
        if (error) *error = RunErrorCode::InvalidRequest;
        return false;
    }
    return true;
}

bool RunControllerInit(RunController* controller) {
    if (!controller) return false;
    *controller = RunController();
    controller->state = RunState::Idle;
    controller->result.state = RunState::Idle;
    controller->result.error = RunErrorCode::None;
    return true;
}

bool RunControllerPrepare(RunController* controller, const HostedDevelopmentRunService& service, const RunRequest& request, RunErrorCode* error) {
    if (error) *error = RunErrorCode::None;
    if (!controller || controller->active || !service.prepare) {
        if (error) *error = controller && controller->active ? RunErrorCode::AlreadyActive : RunErrorCode::ServiceUnavailable;
        return false;
    }
    *controller = RunController();
    controller->state = RunState::Validating;
    controller->result.state = RunState::Validating;
    controller->request = request;
    RunResult result = {};
    uint64_t handle = 0;
    if (!service.prepare(service.userData, request, &handle, &result) || handle == 0 || result.state == RunState::Failed) {
        RunErrorCode local = result.error == RunErrorCode::None ? RunErrorCode::ServiceUnavailable : result.error;
        controller->result = result;
        if (handle != 0 && service.release) service.release(service.userData, handle);
        setFailure(controller, RunState::Failed, local);
        if (error) *error = local;
        return false;
    }
    controller->handle = handle;
    controller->result = result;
    controller->state = result.state == RunState::Registered ? RunState::Prepared : result.state;
    controller->result.state = controller->state;
    controller->active = true;
    return true;
}

bool RunControllerStart(RunController* controller, const HostedDevelopmentRunService& service, RunErrorCode* error) {
    if (error) *error = RunErrorCode::None;
    if (!controller || !controller->active || controller->handle == 0 || !service.start) {
        if (error) *error = RunErrorCode::ServiceUnavailable;
        return false;
    }
    RunResult result = controller->result;
    if (!service.start(service.userData, controller->handle, &result)) {
        const RunErrorCode local = result.error == RunErrorCode::None ? RunErrorCode::LaunchFailed : result.error;
        controller->result = result;
        if (service.release) service.release(service.userData, controller->handle);
        controller->handle = 0;
        setFailure(controller, RunState::Failed, local);
        if (error) *error = local;
        return false;
    }
    controller->result = result;
    controller->state = result.state == RunState::Idle ? RunState::Launching : result.state;
    controller->result.state = controller->state;
    return true;
}

bool RunControllerPoll(RunController* controller, const HostedDevelopmentRunService& service) {
    if (!controller || !controller->active || !service.poll) return false;
    RunResult result = controller->result;
    if (!service.poll(service.userData, controller->handle, &result)) {
        result.error = result.error == RunErrorCode::None ? RunErrorCode::ServiceUnavailable : result.error;
        setFailure(controller, RunState::Failed, result.error);
        controller->result = result;
        if (service.release) service.release(service.userData, controller->handle);
        controller->handle = 0;
        return false;
    }
    controller->result = result;
    controller->state = result.state;
    if (isTerminal(result.state) && result.cleanupComplete) {
        controller->terminalPublished = true;
        controller->active = false;
        if (service.release) service.release(service.userData, controller->handle);
        controller->handle = 0;
    }
    return true;
}

bool RunControllerRequestClose(RunController* controller, const HostedDevelopmentRunService& service) {
    if (!controller || !controller->active || controller->handle == 0 || !service.requestClose) return false;
    if (!service.requestClose(service.userData, controller->handle)) return false;
    controller->closeRequested = true;
    return true;
}

bool RunControllerIsActive(const RunController* controller) {
    return controller && controller->active;
}

bool RunControllerIsTransitionActive(const RunController* controller) {
    if (!controller || !controller->active) return false;
    return controller->state != RunState::Completed && controller->state != RunState::Failed;
}

} // namespace developer_studio
} // namespace guidexos
