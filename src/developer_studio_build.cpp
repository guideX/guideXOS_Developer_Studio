#include "developer_studio_build.h"

namespace guidexos {
namespace developer_studio {
namespace {

static const char kBuildSystem[] = "guidexos-native-build-script-v1";
static const char kBuildScript[] = "build.ps1";
static const char kBuildConfiguration[] = "Debug";

static uint32_t textLength(const char* value, uint32_t capacity) {
    if (!value) return 0;
    uint32_t length = 0;
    while (length < capacity && value[length] != '\0') ++length;
    return length;
}

static bool copyText(char* output, uint32_t outputSize, const char* input) {
    if (!output || outputSize == 0 || !input) return false;
    uint32_t length = textLength(input, outputSize);
    if (length + 1 >= outputSize) { output[0] = '\0'; return false; }
    for (uint32_t i = 0; i < length; ++i) output[i] = input[i];
    output[length] = '\0';
    return true;
}

static void clearResult(BuildResult* result) {
    if (!result) return;
    *result = BuildResult();
    result->state = BuildState::Idle;
    result->error = BuildErrorCode::None;
}

static void setResultFailure(BuildController* controller, BuildState state, BuildErrorCode error) {
    controller->state = state;
    controller->result.state = state;
    controller->result.error = error;
    controller->active = false;
    controller->terminalPublished = true;
}

static bool appendArtifactPath(const Project& project, char* output, uint32_t outputSize) {
    uint32_t length = 0;
    const char prefix[] = "build/bin/amd64/";
    const char suffix[] = ".elf";
    output[0] = '\0';
    for (uint32_t i = 0; prefix[i] != '\0'; ++i) {
        if (length + 1 >= outputSize) return false;
        output[length++] = prefix[i];
    }
    for (uint32_t i = 0; project.outputName[i] != '\0'; ++i) {
        if (length + 1 >= outputSize) return false;
        output[length++] = project.outputName[i];
    }
    for (uint32_t i = 0; suffix[i] != '\0'; ++i) {
        if (length + 1 >= outputSize) return false;
        output[length++] = suffix[i];
    }
    output[length] = '\0';
    return true;
}

} // namespace

const char* BuildStateName(BuildState state) {
    switch (state) {
    case BuildState::Idle: return "Idle";
    case BuildState::Validating: return "Validating";
    case BuildState::Preparing: return "Preparing";
    case BuildState::Running: return "Running";
    case BuildState::ValidatingArtifact: return "ValidatingArtifact";
    case BuildState::Succeeded: return "Succeeded";
    case BuildState::Failed: return "Failed";
    case BuildState::Cancelled: return "Cancelled";
    }
    return "Unknown";
}

const char* BuildErrorName(BuildErrorCode error) {
    switch (error) {
    case BuildErrorCode::None: return "none";
    case BuildErrorCode::NoProject: return "no_project";
    case BuildErrorCode::WorkspaceOnly: return "workspace_only";
    case BuildErrorCode::UnsupportedProjectKind: return "unsupported_project_kind";
    case BuildErrorCode::UnsupportedTarget: return "unsupported_target";
    case BuildErrorCode::InvalidRequest: return "invalid_request";
    case BuildErrorCode::DirtyDocuments: return "dirty_documents";
    case BuildErrorCode::SaveFailed: return "save_failed";
    case BuildErrorCode::UserCancelled: return "user_cancelled";
    case BuildErrorCode::AlreadyRunning: return "already_running";
    case BuildErrorCode::HostUnavailable: return "host_unavailable";
    case BuildErrorCode::SdkNotFound: return "sdk_not_found";
    case BuildErrorCode::ToolchainNotFound: return "toolchain_not_found";
    case BuildErrorCode::PowerShellNotFound: return "powershell_not_found";
    case BuildErrorCode::BuildScriptMissing: return "build_script_missing";
    case BuildErrorCode::InvalidProjectRoot: return "invalid_project_root";
    case BuildErrorCode::ProcessStartFailed: return "process_start_failed";
    case BuildErrorCode::ProcessFailed: return "process_failed";
    case BuildErrorCode::BuildTimeout: return "build_timeout";
    case BuildErrorCode::ArtifactMissing: return "artifact_missing";
    case BuildErrorCode::ArtifactInvalid: return "artifact_invalid";
    case BuildErrorCode::ArtifactWrongArchitecture: return "artifact_wrong_architecture";
    case BuildErrorCode::EntryPointMissing: return "entry_point_missing";
    case BuildErrorCode::ManifestArtifactMismatch: return "manifest_artifact_mismatch";
    case BuildErrorCode::OutputTruncated: return "output_truncated";
    case BuildErrorCode::ServiceError: return "service_error";
    }
    return "unknown";
}

bool BuildRequestFromProject(const Project& project, BuildRequest* request, BuildErrorCode* error) {
    if (error) *error = BuildErrorCode::None;
    if (!request) { if (error) *error = BuildErrorCode::InvalidRequest; return false; }
    *request = BuildRequest();
    ProjectErrorCode projectError = ProjectErrorCode::None;
    if (!project.valid || !ValidateProjectMetadata(project, &projectError)) {
        if (error) *error = BuildErrorCode::InvalidRequest;
        return false;
    }
    if (!IsSupportedProjectKind(project.kind)) { if (error) *error = BuildErrorCode::UnsupportedProjectKind; return false; }
    if (!IsValidTargetProfile(InitialTargetProfile()) ||
        project.targetProfileId[0] == '\0' ||
        !PathsEqual(project.targetProfileId, InitialTargetProfile().id)) {
        if (error) *error = BuildErrorCode::UnsupportedTarget;
        return false;
    }
    if (!copyText(request->projectRoot, sizeof(request->projectRoot), project.rootPath) ||
        !copyText(request->projectId, sizeof(request->projectId), project.projectId) ||
        !copyText(request->projectKind, sizeof(request->projectKind), ToString(project.kind)) ||
        !copyText(request->targetProfile, sizeof(request->targetProfile), project.targetProfileId) ||
        !copyText(request->buildSystem, sizeof(request->buildSystem), kBuildSystem) ||
        !copyText(request->buildScript, sizeof(request->buildScript), kBuildScript) ||
        !copyText(request->configuration, sizeof(request->configuration), kBuildConfiguration) ||
        !appendArtifactPath(project, request->expectedArtifact, sizeof(request->expectedArtifact))) {
        if (error) *error = BuildErrorCode::InvalidRequest;
        return false;
    }
    return true;
}

bool BuildControllerInit(BuildController* controller) {
    if (!controller) return false;
    *controller = BuildController();
    controller->state = BuildState::Idle;
    controller->result.state = BuildState::Idle;
    controller->result.error = BuildErrorCode::None;
    return true;
}

bool BuildControllerStart(BuildController* controller, WorkspaceController* workspace, const HostedBuildService& service, BuildDirtyDecision dirtyDecision, BuildErrorCode* error) {
    if (error) *error = BuildErrorCode::None;
    if (!controller || !workspace) { if (error) *error = BuildErrorCode::InvalidRequest; return false; }
    if (controller->active) { if (error) *error = BuildErrorCode::AlreadyRunning; return false; }
    *controller = BuildController();
    controller->state = BuildState::Validating;
    controller->result.state = BuildState::Validating;
    BuildErrorCode local = BuildErrorCode::None;
    if (!workspace->model.open || !workspace->model.hasProject) local = workspace->model.open ? BuildErrorCode::WorkspaceOnly : BuildErrorCode::NoProject;
    else if (WorkspaceControllerHasDirtyProjectDocuments(workspace)) {
        if (dirtyDecision == BuildDirtyDecision::Cancel) local = BuildErrorCode::UserCancelled;
        else if (!WorkspaceControllerSaveAllProjectDocuments(workspace)) local = BuildErrorCode::SaveFailed;
    }
    if (local == BuildErrorCode::None && !BuildRequestFromProject(workspace->model.project, &controller->request, &local)) {
        if (local == BuildErrorCode::None) local = BuildErrorCode::InvalidRequest;
    }
    if (local != BuildErrorCode::None) {
        setResultFailure(controller, local == BuildErrorCode::UserCancelled ? BuildState::Cancelled : BuildState::Failed, local);
        if (error) *error = local;
        return false;
    }
    if (!service.start || !service.poll || !service.release) {
        local = BuildErrorCode::HostUnavailable;
        setResultFailure(controller, BuildState::Failed, local);
        if (error) *error = local;
        return false;
    }
    controller->state = BuildState::Preparing;
    controller->result.state = BuildState::Preparing;
    if (!service.start(service.userData, controller->request, &controller->handle, &local) || controller->handle == 0) {
        if (local == BuildErrorCode::None) local = BuildErrorCode::ServiceError;
        setResultFailure(controller, BuildState::Failed, local);
        if (error) *error = local;
        return false;
    }
    controller->active = true;
    controller->state = BuildState::Running;
    controller->result.state = BuildState::Running;
    return true;
}

bool BuildControllerPoll(BuildController* controller, const HostedBuildService& service) {
    if (!controller || !controller->active || !service.poll) return false;
    bool completed = false;
    BuildErrorCode error = BuildErrorCode::None;
    BuildResult snapshot = {};
    if (!service.poll(service.userData, controller->handle, &snapshot, &completed, &error)) {
        controller->result.error = error == BuildErrorCode::None ? BuildErrorCode::ServiceError : error;
        controller->result.state = BuildState::Failed;
        controller->state = BuildState::Failed;
        completed = true;
    } else {
        controller->result = snapshot;
        controller->state = snapshot.state;
    }
    if (!completed) return true;
    controller->active = false;
    controller->terminalPublished = true;
    if (service.release) service.release(service.userData, controller->handle);
    controller->handle = 0;
    return true;
}

bool BuildControllerIsActive(const BuildController* controller) {
    return controller && controller->active;
}

} // namespace developer_studio
} // namespace guidexos
