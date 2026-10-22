#include "developer_studio_build.h"

namespace guidexos {
namespace developer_studio {
namespace {

static const char kBuildSystem[] = "guidexos-native-build-script-v1";
static const char kBareMetalBuildSystem[] = "guidexos-native-baremetal-bootstrap-v1";
static const char kNativeGuiProjectKind[] = "native-gui-application";
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

static void appendText(char* output, uint32_t outputSize, const char* input) {
    if (!output || outputSize == 0 || !input) return;
    uint32_t offset = textLength(output, outputSize);
    for (uint32_t i = 0; offset + 1 < outputSize && input[i] != '\0'; ++i) output[offset++] = input[i];
    output[offset] = '\0';
}

static void appendUnsigned(char* output, uint32_t outputSize, uint64_t value) {
    char digits[24] = {};
    uint32_t count = 0;
    if (value == 0) digits[count++] = '0';
    while (value != 0 && count < sizeof(digits)) { digits[count++] = static_cast<char>('0' + (value % 10)); value /= 10; }
    while (count > 0 && textLength(output, outputSize) + 1 < outputSize) {
        const uint32_t offset = textLength(output, outputSize);
        output[offset] = digits[--count];
        output[offset + 1] = '\0';
    }
}

static void appendBuildOutput(BuildController* controller, OutputSeverity severity,
                              OutputCategory category, const char* text) {
    if (!controller || !controller->output || controller->operationId == 0 || !text) return;
    OutputServiceAppendText(controller->output, controller->operationId, OutputSource::Build,
                            severity, category, OutputStream::Unknown, text,
                            controller->request.projectId, nullptr);
}

static bool startsWithText(const char* value, const char* prefix) {
    if (!value || !prefix) return false;
    uint32_t i = 0;
    while (prefix[i] != '\0') {
        if (value[i] == '\0' || value[i] != prefix[i]) return false;
        ++i;
    }
    return true;
}

static char lowerAscii(char value) {
    return value >= 'A' && value <= 'Z' ? static_cast<char>(value + ('a' - 'A')) : value;
}

static bool startsWithInsensitiveText(const char* value, const char* prefix) {
    if (!value || !prefix) return false;
    uint32_t i = 0;
    while (prefix[i] != '\0') {
        if (value[i] == '\0' || lowerAscii(value[i]) != lowerAscii(prefix[i])) return false;
        ++i;
    }
    return true;
}

static bool containsInsensitiveText(const char* value, const char* needle) {
    if (!value || !needle || needle[0] == '\0') return false;
    for (uint32_t i = 0; value[i] != '\0'; ++i)
        if (startsWithInsensitiveText(value + i, needle)) return true;
    return false;
}

static void appendBuildHashByte(uint64_t* hash, uint8_t value) {
    if (!hash) return;
    *hash ^= value;
    *hash *= 1099511628211ULL;
}

static void copyBuildRequestIdentity(BuildResult* result, const BuildRequest& request) {
    if (!result) return;
    copyText(result->projectId, sizeof(result->projectId), request.projectId);
    copyText(result->projectRoot, sizeof(result->projectRoot), request.projectRoot);
    copyText(result->targetProfile, sizeof(result->targetProfile), request.targetProfile);
    copyText(result->configuration, sizeof(result->configuration), request.configuration);
    result->sourceMembershipHash = BuildRequestSourceMembershipHash(request);
}

static OutputCategory buildErrorCategory(BuildErrorCode error) {
    switch (error) {
    case BuildErrorCode::CompilerFailed:
    case BuildErrorCode::SourceReadFailed:
    case BuildErrorCode::ObjectGenerationFailed:
    case BuildErrorCode::LinkerFailed:
        return OutputCategory::BuildDiagnostic;
    case BuildErrorCode::ElfWriterFailed:
    case BuildErrorCode::ArtifactValidationFailed:
    case BuildErrorCode::ArtifactPublicationFailed:
    case BuildErrorCode::ArtifactMissing:
    case BuildErrorCode::ArtifactInvalid:
    case BuildErrorCode::ArtifactWrongArchitecture:
    case BuildErrorCode::EntryPointMissing:
    case BuildErrorCode::ManifestArtifactMismatch:
        return OutputCategory::Artifact;
    default:
        return OutputCategory::BuildLifecycle;
    }
}

static void publishTerminal(BuildController* controller) {
    if (!controller || controller->terminalPublished || !controller->output || controller->operationId == 0) return;
    const BuildResult& result = controller->result;
    char detail[512] = {};
    if (result.state == BuildState::Succeeded) {
        appendBuildOutput(controller, OutputSeverity::Success, OutputCategory::BuildLifecycle,
                          "Executable validation: PASS");
    }
    if (result.sourceFileCount != 0) {
        copyText(detail, sizeof(detail), "Compilation results: sources=");
        appendUnsigned(detail, sizeof(detail), result.sourceFileCount);
        appendText(detail, sizeof(detail), " compiled=");
        appendUnsigned(detail, sizeof(detail), result.compiledModuleCount);
        appendText(detail, sizeof(detail), " cached=");
        appendUnsigned(detail, sizeof(detail), result.cachedModuleCount);
        appendText(detail, sizeof(detail), " linked=");
        appendUnsigned(detail, sizeof(detail), result.linkedModuleCount);
        appendBuildOutput(controller, OutputSeverity::Information, OutputCategory::BuildLifecycle, detail);
    }
    if (result.artifactArchitecture[0]) {
        copyText(detail, sizeof(detail), "Artifact architecture: ");
        appendText(detail, sizeof(detail), result.artifactArchitecture);
        appendBuildOutput(controller, OutputSeverity::Information, OutputCategory::Artifact, detail);
    }
    if (result.artifactSize != 0) {
        copyText(detail, sizeof(detail), "Artifact size: ");
        appendUnsigned(detail, sizeof(detail), result.artifactSize);
        appendText(detail, sizeof(detail), " bytes");
        appendBuildOutput(controller, OutputSeverity::Information, OutputCategory::Artifact, detail);
    }
    copyText(detail, sizeof(detail), "Warnings reported: ");
    appendUnsigned(detail, sizeof(detail), result.warningCount);
    appendBuildOutput(controller, OutputSeverity::Information, OutputCategory::BuildLifecycle, detail);
    if (result.state == BuildState::Succeeded && result.artifactPath[0]) {
        copyText(detail, sizeof(detail), "Executable: ");
        appendText(detail, sizeof(detail), result.artifactPath);
        appendBuildOutput(controller, OutputSeverity::Information, OutputCategory::Artifact, detail);
    }
    if (result.errorMessage[0]) {
        copyText(detail, sizeof(detail), "Build detail: ");
        appendText(detail, sizeof(detail), result.errorMessage);
        appendBuildOutput(controller, OutputSeverity::Error, buildErrorCategory(result.error), detail);
    } else if (result.state == BuildState::Failed && result.error != BuildErrorCode::None) {
        copyText(detail, sizeof(detail), "Build failed: ");
        appendText(detail, sizeof(detail), BuildErrorName(result.error));
        appendBuildOutput(controller, OutputSeverity::Error, buildErrorCategory(result.error), detail);
    }
    if (result.serviceReleaseFailed) {
        appendBuildOutput(controller, OutputSeverity::Error, OutputCategory::BuildLifecycle,
                          "Build service result cleanup failed; the next request may be rejected as busy.");
    }
    char text[512] = {};
    appendText(text, sizeof(text), result.state == BuildState::Succeeded ? "Build Succeeded" :
        (result.state == BuildState::Cancelled ? "Build Cancelled" : "Build Failed"));
    appendText(text, sizeof(text), " | elapsed_ms=");
    appendUnsigned(text, sizeof(text), result.elapsedMilliseconds);
    appendText(text, sizeof(text), " | warnings=");
    appendUnsigned(text, sizeof(text), result.warningCount);
    appendText(text, sizeof(text), " | errors=");
    appendUnsigned(text, sizeof(text), result.errorCount);
    appendText(text, sizeof(text), " | artifact=");
    appendText(text, sizeof(text), result.artifactPath[0] ? result.artifactPath : "none");
    if (result.artifactSha256[0]) {
        appendText(text, sizeof(text), " | sha256=");
        appendText(text, sizeof(text), result.artifactSha256);
    }
    if (result.state != BuildState::Succeeded) {
        appendText(text, sizeof(text), " | failure=");
        appendText(text, sizeof(text), BuildErrorName(result.error));
    }
    OutputServiceCompleteOperation(controller->output, controller->operationId, result.state == BuildState::Succeeded, text, nullptr);
    controller->terminalPublished = true;
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
    if (controller->result.errorMessage[0] == '\0') {
        const char* detail = error == BuildErrorCode::SaveFailed
            ? "Save All failed; the build service was not started."
            : (error == BuildErrorCode::UserCancelled
                ? "Build cancelled before dispatch."
                : BuildErrorName(error));
        copyText(controller->result.errorMessage, sizeof(controller->result.errorMessage), detail);
    }
    controller->active = false;
    controller->terminalPublished = false;
    publishTerminal(controller);
}

static bool appendArtifactPath(const Project& project, BuildBackendKind backend, char* output, uint32_t outputSize) {
    uint32_t length = 0;
    const char prefix[] = "build/bin/";
    const char suffix[] = ".elf";
    const char* architecture = backend == BuildBackendKind::BareMetal &&
        PathsEqual(project.targetProfileId, BareMetalMultiTargetProfile().id) ? "arm64" : project.architecture;
    output[0] = '\0';
    for (uint32_t i = 0; prefix[i] != '\0'; ++i) {
        if (length + 1 >= outputSize) return false;
        output[length++] = prefix[i];
    }
    for (uint32_t i = 0; architecture[i] != '\0'; ++i) {
        if (length + 1 >= outputSize) return false;
        output[length++] = architecture[i];
    }
    if (length + 1 >= outputSize) return false;
    output[length++] = '/';
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
    case BuildState::Saving: return "Saving";
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
    case BuildErrorCode::CompilerFailed: return "compiler_failed";
    case BuildErrorCode::ProjectValidationFailed: return "project_validation_failed";
    case BuildErrorCode::SourceReadFailed: return "source_read_failed";
    case BuildErrorCode::ObjectGenerationFailed: return "object_generation_failed";
    case BuildErrorCode::LinkerFailed: return "linker_failed";
    case BuildErrorCode::ElfWriterFailed: return "elf_writer_failed";
    case BuildErrorCode::ArtifactValidationFailed: return "artifact_validation_failed";
    case BuildErrorCode::ArtifactPublicationFailed: return "artifact_publication_failed";
    }
    return "unknown";
}

BuildErrorCode ClassifyBuildFailureDetail(const char* detail) {
    if (startsWithText(detail, "[linker]")) return BuildErrorCode::LinkerFailed;
    if (startsWithText(detail, "[project]")) return BuildErrorCode::ProjectValidationFailed;
    if (startsWithText(detail, "[source]")) return BuildErrorCode::SourceReadFailed;
    if (startsWithText(detail, "[object]")) return BuildErrorCode::ObjectGenerationFailed;
    if (startsWithText(detail, "[ELF writer]")) return BuildErrorCode::ElfWriterFailed;
    if (startsWithText(detail, "[artifact validation]")) return BuildErrorCode::ArtifactValidationFailed;
    if (startsWithText(detail, "[artifact publication]")) return BuildErrorCode::ArtifactPublicationFailed;
    return BuildErrorCode::CompilerFailed;
}

BuildErrorCode ClassifyBuildDiagnosticLine(const char* line) {
    if (!containsInsensitiveText(line, "error:") && !containsInsensitiveText(line, "fatal error"))
        return BuildErrorCode::None;
    if (startsWithInsensitiveText(line, "linker:") || startsWithInsensitiveText(line, "ld.lld:") ||
        startsWithInsensitiveText(line, "lld-link:") || startsWithInsensitiveText(line, "link ") ||
        startsWithInsensitiveText(line, "link:")) return BuildErrorCode::LinkerFailed;
    return BuildErrorCode::CompilerFailed;
}

bool BuildRequestFromProject(const Project& project, BuildRequest* request, BuildErrorCode* error,
                             BuildBackendKind backend) {
    if (error) *error = BuildErrorCode::None;
    if (!request) { if (error) *error = BuildErrorCode::InvalidRequest; return false; }
    *request = BuildRequest();
    ProjectErrorCode projectError = ProjectErrorCode::None;
    if (!project.valid || !ValidateProjectMetadata(project, &projectError)) {
        if (error) *error = BuildErrorCode::InvalidRequest;
        return false;
    }
    if (!IsSupportedProjectKind(project.kind)) { if (error) *error = BuildErrorCode::UnsupportedProjectKind; return false; }
    const bool bareMetal = backend == BuildBackendKind::BareMetal;
    const TargetProfile& target = bareMetal ? BareMetalTargetProfile() : InitialTargetProfile();
    const bool targetMatches = bareMetal && PathsEqual(project.targetProfileId, BareMetalMultiTargetProfile().id)
        ? IsValidTargetProfile(BareMetalMultiTargetProfile())
        : IsValidTargetProfile(target) && PathsEqual(project.targetProfileId, target.id);
    if (!targetMatches || project.targetProfileId[0] == '\0') {
        if (error) *error = BuildErrorCode::UnsupportedTarget;
        return false;
    }
    if (!copyText(request->projectRoot, sizeof(request->projectRoot), project.rootPath) ||
        !copyText(request->projectId, sizeof(request->projectId), project.projectId) ||
        !copyText(request->projectKind, sizeof(request->projectKind), kNativeGuiProjectKind) ||
        !copyText(request->targetProfile, sizeof(request->targetProfile), project.targetProfileId) ||
        !copyText(request->buildSystem, sizeof(request->buildSystem), backend == BuildBackendKind::BareMetal ? kBareMetalBuildSystem : kBuildSystem) ||
        !copyText(request->buildScript, sizeof(request->buildScript), backend == BuildBackendKind::BareMetal ? "" : kBuildScript) ||
        !copyText(request->configuration, sizeof(request->configuration), kBuildConfiguration) ||
        !appendArtifactPath(project, backend, request->expectedArtifact, sizeof(request->expectedArtifact))) {
        if (error) *error = BuildErrorCode::InvalidRequest;
        return false;
    }
    if (project.hasSourceFiles) {
        if (project.sourceFileCount == 0 || project.sourceFileCount > kMaxBuildSources) {
            if (error) *error = BuildErrorCode::InvalidRequest;
            return false;
        }
        for (uint32_t i = 0; i < project.sourceFileCount; ++i) {
            char* destination = request->sourceFiles[i];
            if (!copyText(destination, sizeof(request->sourceFiles[i]), project.sourceRoot)) {
                if (error) *error = BuildErrorCode::InvalidRequest;
                return false;
            }
            uint32_t length = 0;
            while (destination[length]) ++length;
            if (length + 1 >= sizeof(request->sourceFiles[i])) {
                if (error) *error = BuildErrorCode::InvalidRequest;
                return false;
            }
            destination[length++] = '/';
            destination[length] = '\0';
            if (!copyText(destination + length, sizeof(request->sourceFiles[i]) - length,
                          project.sourceFiles[i])) {
                if (error) *error = BuildErrorCode::InvalidRequest;
                return false;
            }
        }
        request->sourceCount = project.sourceFileCount;
    } else if (project.sourceEntry[0] != '\0') {
        if (!copyText(request->sourceFiles[0], sizeof(request->sourceFiles[0]), project.sourceRoot)) {
            if (error) *error = BuildErrorCode::InvalidRequest;
            return false;
        }
        uint32_t length = 0;
        while (request->sourceFiles[0][length]) ++length;
        if (length + 1 >= sizeof(request->sourceFiles[0])) {
            if (error) *error = BuildErrorCode::InvalidRequest;
            return false;
        }
        request->sourceFiles[0][length++] = '/';
        if (!copyText(request->sourceFiles[0] + length, sizeof(request->sourceFiles[0]) - length,
                      project.sourceEntry)) {
            if (error) *error = BuildErrorCode::InvalidRequest;
            return false;
        }
        request->sourceCount = 1;
    }
    return true;
}

bool BuildRequestEnableDebugInfo(BuildRequest* request) {
    if (!request) return false;
    return copyText(request->configuration, sizeof(request->configuration), "DebugSymbols");
}

uint64_t BuildRequestSourceMembershipHash(const BuildRequest& request) {
    uint64_t hash = 1469598103934665603ULL;
    for (uint32_t shift = 0; shift < 32; shift += 8)
        appendBuildHashByte(&hash, static_cast<uint8_t>(request.sourceCount >> shift));
    const uint32_t count = request.sourceCount < kMaxBuildSources ? request.sourceCount : kMaxBuildSources;
    for (uint32_t i = 0; i < count; ++i) {
        for (uint32_t j = 0; j < sizeof(request.sourceFiles[i]) && request.sourceFiles[i][j] != '\0'; ++j)
            appendBuildHashByte(&hash, static_cast<uint8_t>(request.sourceFiles[i][j]));
        appendBuildHashByte(&hash, 0);
    }
    return hash;
}

bool BuildControllerInit(BuildController* controller) {
    if (!controller) return false;
    *controller = BuildController();
    controller->state = BuildState::Idle;
    controller->result.state = BuildState::Idle;
    controller->result.error = BuildErrorCode::None;
    return true;
}

static void beginBuildOutput(BuildController* controller, const Project& project,
                             BuildBackendKind backend, bool debugInfo) {
    if (!controller || !controller->output || !project.projectId[0]) return;
    copyText(controller->request.projectId, sizeof(controller->request.projectId), project.projectId);
    copyText(controller->result.projectId, sizeof(controller->result.projectId), project.projectId);
    copyText(controller->result.projectRoot, sizeof(controller->result.projectRoot), project.rootPath);
    copyText(controller->result.targetProfile, sizeof(controller->result.targetProfile), project.targetProfileId);
    OutputServiceClearProblemsForProject(controller->output, project.projectId);
    controller->operationId = OutputServiceBeginOperation(controller->output, OutputOperationType::Build,
                                                           project.projectId);
    if (controller->operationId == 0) return;
    OutputServiceSelectActiveChannel(controller->output, OutputChannel::Build);
    appendBuildOutput(controller, OutputSeverity::Information, OutputCategory::BuildLifecycle,
                      "Build requested");
    char line[256] = {};
    copyText(line, sizeof(line), "Project: ");
    appendText(line, sizeof(line), project.displayName[0] ? project.displayName : project.projectId);
    appendText(line, sizeof(line), " (");
    appendText(line, sizeof(line), project.projectId);
    appendText(line, sizeof(line), ")");
    appendBuildOutput(controller, OutputSeverity::Information, OutputCategory::BuildLifecycle, line);
    copyText(line, sizeof(line), "Target: ");
    appendText(line, sizeof(line), project.targetProfileId);
    appendText(line, sizeof(line), " | architecture: ");
    appendText(line, sizeof(line), project.architecture);
    appendBuildOutput(controller, OutputSeverity::Information, OutputCategory::BuildLifecycle, line);
    copyText(line, sizeof(line), "Backend: ");
    appendText(line, sizeof(line), backend == BuildBackendKind::BareMetal
        ? "Bare-metal kernel compiler (synchronous)" : "Hosted build service");
    appendBuildOutput(controller, OutputSeverity::Information, OutputCategory::BuildLifecycle, line);
    copyText(line, sizeof(line), debugInfo ? "Configuration: DebugSymbols" : "Configuration: Debug");
    appendBuildOutput(controller, OutputSeverity::Information, OutputCategory::BuildLifecycle, line);
}

bool BuildControllerStart(BuildController* controller, WorkspaceController* workspace, const HostedBuildService& service, BuildDirtyDecision dirtyDecision, BuildErrorCode* error, OutputService* output, bool debugInfo) {
    if (error) *error = BuildErrorCode::None;
    if (!controller || !workspace) { if (error) *error = BuildErrorCode::InvalidRequest; return false; }
    if (controller->active) {
        appendBuildOutput(controller, OutputSeverity::Warning, OutputCategory::BuildLifecycle,
                          "Build request ignored: another build is already active.");
        if (error) *error = BuildErrorCode::AlreadyRunning;
        return false;
    }
    *controller = BuildController();
    controller->output = output;
    controller->state = BuildState::Validating;
    controller->result.state = BuildState::Validating;
    BuildErrorCode local = BuildErrorCode::None;
    if (!workspace->model.open || !workspace->model.hasProject) {
        local = workspace->model.open ? BuildErrorCode::WorkspaceOnly : BuildErrorCode::NoProject;
    } else {
        beginBuildOutput(controller, workspace->model.project, service.backend, debugInfo);
        if (WorkspaceControllerHasDirtyProjectDocuments(workspace)) {
            uint32_t dirtyCount = 0;
            for (uint32_t i = 0; i < kMaxOpenDocuments; ++i)
                if (workspace->model.documents[i].used && workspace->model.documents[i].buffer.dirty) ++dirtyCount;
            char savingLine[128] = {};
            copyText(savingLine, sizeof(savingLine), "Dirty project documents: ");
            appendUnsigned(savingLine, sizeof(savingLine), dirtyCount);
            appendBuildOutput(controller, OutputSeverity::Information, OutputCategory::BuildLifecycle, savingLine);
            for (uint32_t i = 0; i < kMaxOpenDocuments; ++i) {
                const Document& document = workspace->model.documents[i];
                if (!document.used || !document.buffer.dirty) continue;
                char dirtyLine[256] = {};
                copyText(dirtyLine, sizeof(dirtyLine), "Dirty document: ");
                appendText(dirtyLine, sizeof(dirtyLine), document.path);
                appendBuildOutput(controller, OutputSeverity::Information, OutputCategory::BuildLifecycle, dirtyLine);
            }
            controller->state = BuildState::Saving;
            controller->result.state = BuildState::Saving;
            if (dirtyDecision == BuildDirtyDecision::Cancel) {
                local = BuildErrorCode::UserCancelled;
                appendBuildOutput(controller, OutputSeverity::Information, OutputCategory::BuildLifecycle,
                                  "Build cancelled before saving or dispatch.");
            } else {
                appendBuildOutput(controller, OutputSeverity::Information, OutputCategory::BuildLifecycle,
                                  "Saving modified project files");
                if (!WorkspaceControllerSaveAllProjectDocuments(workspace)) {
                    local = BuildErrorCode::SaveFailed;
                    appendBuildOutput(controller, OutputSeverity::Error, OutputCategory::BuildLifecycle,
                                      "Save All failed; the build service was not started.");
                } else {
                    appendBuildOutput(controller, OutputSeverity::Success, OutputCategory::BuildLifecycle,
                                      "Saved all modified project files");
                }
            }
        } else {
            appendBuildOutput(controller, OutputSeverity::Information, OutputCategory::BuildLifecycle,
                              "Source state is saved");
        }
    }
    if (local == BuildErrorCode::None && !BuildRequestFromProject(workspace->model.project, &controller->request, &local, service.backend)) {
        if (local == BuildErrorCode::None) local = BuildErrorCode::InvalidRequest;
    }
    if (local == BuildErrorCode::None && service.backend == BuildBackendKind::BareMetal &&
        controller->request.sourceCount == 0) {
        ProjectSourceFile files[kMaxBuildSources] = {};
        uint32_t sourceCount = 0;
        if (!WorkspaceControllerEnumerateProjectSources(workspace, files, kMaxBuildSources, &sourceCount)) {
            local = BuildErrorCode::InvalidRequest;
        } else {
            controller->request.sourceCount = sourceCount;
            for (uint32_t i = 0; i < sourceCount; ++i)
                copyText(controller->request.sourceFiles[i], sizeof(controller->request.sourceFiles[i]), files[i].relativePath);
        }
    }
    if (local == BuildErrorCode::None && debugInfo && !BuildRequestEnableDebugInfo(&controller->request))
        local = BuildErrorCode::InvalidRequest;
    if (local != BuildErrorCode::None) {
        setResultFailure(controller, local == BuildErrorCode::UserCancelled ? BuildState::Cancelled : BuildState::Failed, local);
        if (error) *error = local;
        return false;
    }
    copyBuildRequestIdentity(&controller->result, controller->request);
    if (!service.start || !service.poll || !service.release) {
        local = BuildErrorCode::HostUnavailable;
        setResultFailure(controller, BuildState::Failed, local);
        if (error) *error = local;
        return false;
    }
    appendBuildOutput(controller, OutputSeverity::Information, OutputCategory::BuildLifecycle,
                      "Preparing project for the build service");
    controller->state = BuildState::Preparing;
    controller->result.state = BuildState::Preparing;
    const bool serviceStarted = service.start(service.userData, controller->request, &controller->handle, &local);
    if (!serviceStarted || controller->handle == 0) {
        if (local == BuildErrorCode::None) local = BuildErrorCode::ServiceError;
        if (controller->handle != 0 && service.release && !service.release(service.userData, controller->handle))
            controller->result.serviceReleaseFailed = true;
        controller->handle = 0;
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
        copyText(controller->result.errorMessage, sizeof(controller->result.errorMessage),
                 "Build status could not be retrieved from the host service.");
        controller->state = BuildState::Failed;
        completed = true;
    } else {
        controller->result = snapshot;
        copyBuildRequestIdentity(&controller->result, controller->request);
        controller->state = snapshot.state;
        if (controller->output && controller->operationId != 0) {
            const uint32_t count = snapshot.outputCount;
            const uint32_t start = controller->publishedOutputCount <= count ? controller->publishedOutputCount : 0;
            for (uint32_t i = start; i < count; ++i) {
                OutputServiceAppendBuildLine(controller->output, controller->operationId, controller->request.projectRoot,
                                              controller->request.projectId,
                                              snapshot.output[i].standardError ? OutputStream::StandardError : OutputStream::StandardOutput,
                                              snapshot.output[i].text, nullptr);
            }
            controller->publishedOutputCount = count;
            if (snapshot.outputTruncated && !controller->outputTruncationPublished) {
                OutputRecord record = {};
                record.source = OutputSource::Build;
                record.severity = OutputSeverity::Warning;
                record.category = OutputCategory::BuildLifecycle;
                record.stream = OutputStream::Unknown;
                record.isTruncated = true;
                copyText(record.text, sizeof(record.text), "Build output was truncated by the host.");
                OutputServiceAppendRecord(controller->output, controller->operationId, record, nullptr);
                controller->outputTruncationPublished = true;
            }
        }
    }
    if (!completed) return true;
    controller->active = false;
    if (service.release && !service.release(service.userData, controller->handle)) {
        controller->result.serviceReleaseFailed = true;
        if (controller->result.state == BuildState::Succeeded) {
            controller->result.state = BuildState::Failed;
            controller->result.error = BuildErrorCode::ServiceError;
            controller->result.artifactValid = false;
            controller->result.artifactEntryPoint = false;
            controller->result.artifactPath[0] = '\0';
            controller->result.artifactSha256[0] = '\0';
            copyText(controller->result.errorMessage, sizeof(controller->result.errorMessage),
                     "Build service result cleanup failed; the executable is not available to Run.");
        }
    }
    controller->handle = 0;
    controller->state = controller->result.state;
    publishTerminal(controller);
    return true;
}

bool BuildControllerIsActive(const BuildController* controller) {
    return controller && controller->active;
}

} // namespace developer_studio
} // namespace guidexos
