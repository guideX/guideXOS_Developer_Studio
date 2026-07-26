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

static void appendUnsigned(char* output, uint32_t outputSize, int32_t value) {
    if (value < 0) { copyText(output, outputSize, "-"); value = -(value + 1) + 1; }
    char digits[16] = {};
    uint32_t count = 0;
    uint32_t magnitude = static_cast<uint32_t>(value);
    if (magnitude == 0) digits[count++] = '0';
    while (magnitude != 0 && count < sizeof(digits)) { digits[count++] = static_cast<char>('0' + (magnitude % 10)); magnitude /= 10; }
    while (count > 0 && textLength(output, outputSize) + 1 < outputSize) {
        uint32_t offset = textLength(output, outputSize);
        output[offset] = digits[--count];
        output[offset + 1] = '\0';
    }
}

static void appendRunText(RunController* controller, OutputSeverity severity, const char* text) {
    if (!controller || !controller->output || controller->operationId == 0) return;
    OutputServiceAppendText(controller->output, controller->operationId, OutputSource::Run, severity,
                            OutputCategory::RunLifecycle, OutputStream::Unknown, text, controller->request.projectId, nullptr);
}

static void publishTerminal(RunController* controller) {
    if (!controller || controller->terminalPublished || !controller->output || controller->operationId == 0) return;
    char text[256] = {};
    copyText(text, sizeof(text), controller->result.state == RunState::Completed && controller->result.exitCode == 0 ? "Run Succeeded" : "Run Failed");
    const uint32_t offset = textLength(text, sizeof(text));
    if (offset + 1 < sizeof(text)) { text[offset] = ' '; text[offset + 1] = '\0'; }
    const uint32_t next = textLength(text, sizeof(text));
    if (next + 10 < sizeof(text)) copyText(text + next, sizeof(text) - next, "exit_code=");
    appendUnsigned(text, sizeof(text), controller->result.exitCode);
    OutputServiceCompleteOperation(controller->output, controller->operationId,
                                   controller->result.state == RunState::Completed && controller->result.exitCode == 0,
                                   text, nullptr);
    controller->terminalPublished = true;
}

static void setFailure(RunController* controller, RunState state, RunErrorCode error) {
    controller->state = state;
    controller->result.state = state;
    controller->result.error = error;
    controller->active = false;
    controller->terminalPublished = false;
    publishTerminal(controller);
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

void RunControllerAttachOutput(RunController* controller, OutputService* output, uint64_t operationId) {
    if (!controller) return;
    controller->output = output;
    controller->operationId = operationId;
}

bool RunControllerPrepare(RunController* controller, const HostedDevelopmentRunService& service, const RunRequest& request, RunErrorCode* error) {
    if (error) *error = RunErrorCode::None;
    if (!controller || controller->active || !service.prepare) {
        if (error) *error = controller && controller->active ? RunErrorCode::AlreadyActive : RunErrorCode::ServiceUnavailable;
        return false;
    }
    OutputService* output = controller->output;
    const uint64_t operationId = controller->operationId;
    *controller = RunController();
    controller->output = output;
    controller->operationId = operationId;
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
    appendRunText(controller, OutputSeverity::Information, "Temporary deployment prepared");
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
    appendRunText(controller, OutputSeverity::Information, "Application launching");
    return true;
}

bool RunControllerPoll(RunController* controller, const HostedDevelopmentRunService& service) {
    if (!controller || !controller->active || !service.poll) return false;
    RunResult result = controller->result;
    if (!service.poll(service.userData, controller->handle, &result)) {
        result.error = result.error == RunErrorCode::None ? RunErrorCode::ServiceUnavailable : result.error;
        controller->result = result;
        setFailure(controller, RunState::Failed, result.error);
        if (service.release) service.release(service.userData, controller->handle);
        controller->handle = 0;
        return false;
    }
    const RunState previous = controller->state;
    controller->result = result;
    controller->state = result.state;
    if (previous != result.state) {
        if (result.state == RunState::Running) appendRunText(controller, OutputSeverity::Information, "Application running");
        else if (result.state == RunState::Exited) appendRunText(controller, OutputSeverity::Information, "Application exited");
        else if (result.state == RunState::CleaningUp) appendRunText(controller, OutputSeverity::Information, "Deployment cleanup started");
    }
    if (isTerminal(result.state) && result.cleanupComplete) {
        controller->active = false;
        if (service.release) service.release(service.userData, controller->handle);
        controller->handle = 0;
        publishTerminal(controller);
    }
    return true;
}

bool RunControllerRequestClose(RunController* controller, const HostedDevelopmentRunService& service) {
    if (!controller || !controller->active || controller->handle == 0 || !service.requestClose) return false;
    if (!service.requestClose(service.userData, controller->handle)) return false;
    controller->closeRequested = true;
    appendRunText(controller, OutputSeverity::Information, "Application close requested");
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
