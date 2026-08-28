// Focused NativeElf integration app for the Phase 27E boot proof.
//
// This is intentionally a small Developer Studio shell: it uses the same
// WorkspaceController, TextBuffer, BuildController, and OutputService model
// as the full application, but keeps the embedded image bounded enough for
// the kernel's reusable NativeElf window.

#include <stddef.h>

#include "guidexos/abi.h"
#include "guidexos/app.h"
#include "developer_studio_build.h"
#include "developer_studio_output.h"
#include "developer_studio_workspace.h"
#if defined(GXOS_PHASE27F_APP)
#include "developer_studio_run.h"
#endif

namespace guidexos {
namespace developer_studio {
namespace {

static gx_app_context* g_context = nullptr;
static WorkspaceController g_workspace = {};
static OutputService g_output = {};
static BuildController g_build = {};
#if defined(GXOS_PHASE27F_APP)
static RunController g_run = {};
static bool g_identityProof = true;
#endif

static bool copyText(char* output, uint32_t capacity, const char* input)
{
    if (!output || capacity == 0 || !input) return false;
    uint32_t i = 0;
    while (i + 1 < capacity && input[i] != '\0') {
        output[i] = input[i];
        ++i;
    }
    if (input[i] != '\0') {
        output[0] = '\0';
        return false;
    }
    output[i] = '\0';
    return true;
}

static bool equalText(const char* left, const char* right)
{
    if (!left || !right) return left == right;
    uint32_t i = 0;
    while (left[i] != '\0' && right[i] != '\0') {
        if (left[i] != right[i]) return false;
        ++i;
    }
    return left[i] == right[i];
}

static const gx_host_calls* host()
{
    return g_context ? g_context->host : nullptr;
}

static bool hasBareHost()
{
    const gx_host_calls* calls = host();
    const size_t end =
#if defined(GXOS_PHASE27F_APP)
        offsetof(gx_host_calls, bare_metal_development_run_release) +
        sizeof(calls->bare_metal_development_run_release);
#else
        offsetof(gx_host_calls, bare_metal_file_remove) +
        sizeof(calls->bare_metal_file_remove);
#endif
    return calls && calls->size >= end && calls->version == GX_API_VERSION &&
        calls->bare_metal_build_project_start && calls->bare_metal_build_project_poll &&
        calls->bare_metal_build_project_release && calls->bare_metal_file_stat &&
        calls->bare_metal_file_read_workspace && calls->bare_metal_file_list &&
        calls->bare_metal_file_write_all && calls->bare_metal_file_create_directory &&
        calls->bare_metal_file_remove
#if defined(GXOS_PHASE27F_APP)
        && calls->bare_metal_development_run_prepare && calls->bare_metal_development_run_start &&
        calls->bare_metal_development_run_poll && calls->bare_metal_development_run_request_close &&
        calls->bare_metal_development_run_release
#endif
        ;
}

static bool fsStat(void*, const char* path, FileInfo* output)
{
    const gx_host_calls* calls = host();
    gx_file_info native = {};
    if (!calls || !output || calls->bare_metal_file_stat(g_context, path, &native) != GX_OK) return false;
    output->kind = native.type == GX_FILE_TYPE_DIRECTORY ? FileInfoKind::Directory :
        (native.type == GX_FILE_TYPE_REGULAR ? FileInfoKind::RegularFile : FileInfoKind::Unknown);
    output->size = native.size;
    return native.type != GX_FILE_TYPE_UNKNOWN;
}

static gx_file_entry g_entries[128] = {};

static bool fsList(void*, const char* path, FileListEntry* output, uint32_t capacity,
                   uint32_t* outCount, bool* outTruncated)
{
    const gx_host_calls* calls = host();
    if (!calls || !outCount || !outTruncated || capacity > 128) return false;
    uint32_t nativeCount = 0;
    uint32_t nativeTruncated = 0;
    if (calls->bare_metal_file_list(g_context, path, g_entries, capacity, &nativeCount,
                                    &nativeTruncated) != GX_OK) return false;
    *outCount = nativeCount;
    *outTruncated = nativeTruncated != 0;
    for (uint32_t i = 0; i < nativeCount; ++i) {
        output[i] = {};
        output[i].kind = g_entries[i].type == GX_FILE_TYPE_DIRECTORY ? FileInfoKind::Directory :
            (g_entries[i].type == GX_FILE_TYPE_REGULAR ? FileInfoKind::RegularFile : FileInfoKind::Unknown);
        output[i].size = g_entries[i].size;
        if (!copyText(output[i].name, sizeof(output[i].name), g_entries[i].name)) return false;
    }
    return true;
}

static bool fsRead(void*, const char* path, char* buffer, uint32_t capacity, uint32_t* outBytes)
{
    const gx_host_calls* calls = host();
    return calls && outBytes &&
        calls->bare_metal_file_read_workspace(g_context, path, buffer, capacity, outBytes) == GX_OK;
}

static bool fsWrite(void*, const char* path, const char* buffer, uint32_t bytes, uint32_t* outBytes)
{
    const gx_host_calls* calls = host();
    return calls && outBytes &&
        calls->bare_metal_file_write_all(g_context, path, buffer, bytes, outBytes) == GX_OK;
}

static bool fsCreateDirectory(void*, const char* path)
{
    const gx_host_calls* calls = host();
    return calls && calls->bare_metal_file_create_directory(g_context, path) == GX_OK;
}

static bool fsRemovePath(void*, const char* path)
{
    const gx_host_calls* calls = host();
    return calls && calls->bare_metal_file_remove(g_context, path) == GX_OK;
}

static WorkspaceFileSystem bareFileSystem()
{
    WorkspaceFileSystem fileSystem = {};
    fileSystem.stat = fsStat;
    fileSystem.list = fsList;
    fileSystem.read = fsRead;
    fileSystem.write = fsWrite;
    fileSystem.createDirectory = fsCreateDirectory;
    fileSystem.removePath = fsRemovePath;
    return fileSystem;
}

static BuildErrorCode mapBuildError(uint32_t error)
{
    switch (error) {
    case GX_BUILD_ERROR_NONE: return BuildErrorCode::None;
    case GX_BUILD_ERROR_BUSY: return BuildErrorCode::AlreadyRunning;
    case GX_BUILD_ERROR_PROCESS_FAILED:
    case GX_BUILD_ERROR_COMPILER_FAILED: return BuildErrorCode::ProcessFailed;
    case GX_BUILD_ERROR_SOURCE_SELECTION: return BuildErrorCode::InvalidRequest;
    case GX_BUILD_ERROR_UNSUPPORTED_PROJECT: return BuildErrorCode::UnsupportedTarget;
    case GX_BUILD_ERROR_ARTIFACT_MISSING: return BuildErrorCode::ArtifactMissing;
    case GX_BUILD_ERROR_ARTIFACT_INVALID: return BuildErrorCode::ArtifactInvalid;
    case GX_BUILD_ERROR_ARTIFACT_WRONG_ARCHITECTURE: return BuildErrorCode::ArtifactWrongArchitecture;
    case GX_BUILD_ERROR_ENTRY_POINT_MISSING: return BuildErrorCode::EntryPointMissing;
    case GX_BUILD_ERROR_MANIFEST_ARTIFACT_MISMATCH: return BuildErrorCode::ManifestArtifactMismatch;
    case GX_BUILD_ERROR_OUTPUT_TRUNCATED: return BuildErrorCode::OutputTruncated;
    default: return BuildErrorCode::ServiceError;
    }
}

static void mapSnapshot(const gx_build_snapshot& native, BuildResult* result)
{
    if (!result) return;
    *result = BuildResult();
    result->state = static_cast<BuildState>(native.state);
    result->exitCode = native.processExitCode;
    result->error = mapBuildError(native.errorCode);
    result->elapsedMilliseconds = native.elapsedMilliseconds;
    result->warningCount = native.warningCount;
    result->errorCount = native.errorCount;
    result->outputCount = native.outputCount <= kMaxBuildLines ? native.outputCount : kMaxBuildLines;
    result->outputTruncated = native.outputTruncated != 0 || native.outputCount > kMaxBuildLines;
    result->artifactSize = native.artifactSize;
    result->artifactValid = native.artifactValid != 0;
    result->artifactEntryPoint = native.artifactEntryPoint != 0;
    copyText(result->artifactPath, sizeof(result->artifactPath), native.artifactPath);
    copyText(result->artifactSha256, sizeof(result->artifactSha256), native.artifactSha256);
    copyText(result->artifactArchitecture, sizeof(result->artifactArchitecture), native.artifactArchitecture);
    copyText(result->errorMessage, sizeof(result->errorMessage), native.errorMessage);
    for (uint32_t i = 0; i < result->outputCount; ++i) {
        result->output[i].standardError = native.output[i].stream == 2;
        copyText(result->output[i].text, sizeof(result->output[i].text), native.output[i].text);
    }
}

static bool buildStart(void*, const BuildRequest& request, uint64_t* outHandle, BuildErrorCode* error)
{
    const gx_host_calls* calls = host();
    if (error) *error = BuildErrorCode::None;
    if (!calls || !outHandle) {
        if (error) *error = BuildErrorCode::HostUnavailable;
        return false;
    }
    gx_build_request native = {};
    native.size = sizeof(native);
    native.version = GX_BUILD_API_VERSION;
    native.projectRoot = request.projectRoot;
    native.projectId = request.projectId;
    native.projectKind = request.projectKind;
    native.targetProfile = request.targetProfile;
    native.buildSystem = request.buildSystem;
    native.buildScript = request.buildScript;
    native.expectedArtifact = request.expectedArtifact;
    native.configuration = request.configuration;
    gx_build_handle handle = 0;
    const gx_result status = calls->bare_metal_build_project_start(g_context, &native, &handle);
    if (status != GX_OK || handle == 0) {
        if (error) *error = status == GX_ERROR_BUSY ? BuildErrorCode::AlreadyRunning :
            (status == GX_ERROR_INVALID_ARGUMENT ? BuildErrorCode::InvalidRequest : BuildErrorCode::ServiceError);
        return false;
    }
    *outHandle = handle;
    return true;
}

static bool buildPoll(void*, uint64_t handle, BuildResult* result, bool* completed, BuildErrorCode* error)
{
    const gx_host_calls* calls = host();
    if (error) *error = BuildErrorCode::None;
    if (!calls || !result || !completed) {
        if (error) *error = BuildErrorCode::HostUnavailable;
        return false;
    }
    gx_build_snapshot native = {};
    const gx_result status = calls->bare_metal_build_project_poll(g_context, handle, &native);
    if (status != GX_OK) {
        if (error) *error = status == GX_ERROR_BUSY ? BuildErrorCode::AlreadyRunning : BuildErrorCode::ServiceError;
        return false;
    }
    mapSnapshot(native, result);
    *completed = native.state == GX_BUILD_SUCCEEDED || native.state == GX_BUILD_FAILED ||
                 native.state == GX_BUILD_CANCELLED;
    return true;
}

static bool buildRelease(void*, uint64_t handle)
{
    const gx_host_calls* calls = host();
    return calls && calls->bare_metal_build_project_release(g_context, handle) == GX_OK;
}

static HostedBuildService buildService()
{
    HostedBuildService service = {};
    service.start = buildStart;
    service.poll = buildPoll;
    service.release = buildRelease;
    service.backend = BuildBackendKind::BareMetal;
    return service;
}

#if defined(GXOS_PHASE27F_APP)
static RunErrorCode mapRunError(uint32_t error)
{
    switch (error) {
    case GX_DEVELOPMENT_RUN_ERROR_NONE: return RunErrorCode::None;
    case GX_DEVELOPMENT_RUN_ERROR_DEPLOYMENT_ALREADY_ACTIVE:
    case GX_DEVELOPMENT_RUN_ERROR_RUNTIME_BUSY: return RunErrorCode::AlreadyActive;
    case GX_DEVELOPMENT_RUN_ERROR_ARTIFACT_CHANGED:
    case GX_DEVELOPMENT_RUN_ERROR_ARTIFACT_SIZE_CHANGED:
    case GX_DEVELOPMENT_RUN_ERROR_ARTIFACT_INVALID: return RunErrorCode::ArtifactInvalid;
    case GX_DEVELOPMENT_RUN_ERROR_UNSUPPORTED_TARGET: return RunErrorCode::ServiceUnavailable;
    case GX_DEVELOPMENT_RUN_ERROR_LAUNCH_FAILED: return RunErrorCode::LaunchFailed;
    default: return RunErrorCode::InvalidRequest;
    }
}

static void mapRunSnapshot(const gx_development_run_snapshot& native, RunResult* result)
{
    if (!result) return;
    *result = RunResult();
    result->state = static_cast<RunState>(native.state);
    result->error = mapRunError(native.errorCode);
    result->handle = native.handle;
    result->processId = native.processId;
    result->nativeRuntimeId = native.nativeRuntimeId;
    result->windowCount = native.windowCount;
    result->createdWindowCount = native.createdWindowCount;
    result->exitCode = native.exitCode;
    result->cleanupComplete = native.cleanupComplete != 0;
    copyText(result->applicationId, sizeof(result->applicationId), native.applicationId);
    copyText(result->displayName, sizeof(result->displayName), native.displayName);
    copyText(result->artifactSha256, sizeof(result->artifactSha256), native.artifactSha256);
    copyText(result->errorMessage, sizeof(result->errorMessage), native.errorMessage);
    result->outputCount = native.outputCount > kMaxRunOutputLines ? kMaxRunOutputLines : native.outputCount;
    result->outputTruncated = native.outputTruncated != 0 || native.outputCount > kMaxRunOutputLines;
    for (uint32_t i = 0; i < result->outputCount; ++i)
        copyText(result->output[i].text, sizeof(result->output[i].text), native.output[i].text);
}

static bool runPrepare(void*, const RunRequest& request, uint64_t* outHandle, RunResult* outResult)
{
    const gx_host_calls* calls = host();
    if (outHandle) *outHandle = 0;
    if (outResult) *outResult = RunResult();
    if (!calls || !outHandle || !outResult || !calls->bare_metal_development_run_prepare) return false;
    gx_development_run_request nativeRequest = {};
    nativeRequest.size = sizeof(nativeRequest);
    nativeRequest.version = GX_DEVELOPMENT_RUN_API_VERSION;
    nativeRequest.projectRoot = request.projectRoot;
    nativeRequest.projectId = request.projectId;
    nativeRequest.projectKind = request.projectKind;
    nativeRequest.targetProfile = request.targetProfile;
    nativeRequest.manifestPath = request.manifestPath;
    nativeRequest.artifactPath = request.artifactPath;
    nativeRequest.artifactSha256 = request.artifactSha256;
    nativeRequest.artifactSize = request.artifactSize;
    nativeRequest.artifactArchitecture = request.artifactArchitecture;
    nativeRequest.artifactAbi = request.artifactAbi;
    nativeRequest.flags = request.debugControlled ? GX_DEVELOPMENT_RUN_FLAG_DEBUG_CONTROLLED : 0;
    gx_development_run_snapshot snapshot = {};
    snapshot.size = sizeof(snapshot);
    snapshot.version = GX_DEVELOPMENT_RUN_API_VERSION;
    const gx_result status = calls->bare_metal_development_run_prepare(g_context, &nativeRequest, outHandle, &snapshot);
    mapRunSnapshot(snapshot, outResult);
    return status == GX_OK;
}

static bool runStart(void*, uint64_t handle, RunResult* outResult)
{
    const gx_host_calls* calls = host();
    if (!calls || !outResult || handle == 0 || !calls->bare_metal_development_run_start ||
        calls->bare_metal_development_run_start(g_context, handle) != GX_OK) return false;
    outResult->handle = handle;
    outResult->state = RunState::Launching;
    gx_development_run_snapshot snapshot = {};
    snapshot.size = sizeof(snapshot);
    snapshot.version = GX_DEVELOPMENT_RUN_API_VERSION;
    if (!calls->bare_metal_development_run_poll ||
        calls->bare_metal_development_run_poll(g_context, handle, &snapshot) != GX_OK) return false;
    mapRunSnapshot(snapshot, outResult);
    return true;
}

static bool runPoll(void*, uint64_t handle, RunResult* outResult)
{
    const gx_host_calls* calls = host();
    if (!calls || !outResult || handle == 0 || !calls->bare_metal_development_run_poll) return false;
    gx_development_run_snapshot snapshot = {};
    snapshot.size = sizeof(snapshot);
    snapshot.version = GX_DEVELOPMENT_RUN_API_VERSION;
    if (calls->bare_metal_development_run_poll(g_context, handle, &snapshot) != GX_OK) return false;
    mapRunSnapshot(snapshot, outResult);
    return true;
}

static bool runClose(void*, uint64_t handle)
{
    const gx_host_calls* calls = host();
    return calls && handle != 0 && calls->bare_metal_development_run_request_close &&
        calls->bare_metal_development_run_request_close(g_context, handle) == GX_OK;
}

static bool runRelease(void*, uint64_t handle)
{
    const gx_host_calls* calls = host();
    return calls && handle != 0 && calls->bare_metal_development_run_release &&
        calls->bare_metal_development_run_release(g_context, handle) == GX_OK;
}

static HostedDevelopmentRunService runService()
{
    HostedDevelopmentRunService service = {};
    service.prepare = runPrepare;
    service.start = runStart;
    service.poll = runPoll;
    service.requestClose = runClose;
    service.release = runRelease;
    service.backend = RunBackendKind::BareMetal;
    return service;
}

static bool outputHas(uint64_t operationId, const char* text)
{
    for (uint32_t i = 0; i < OutputServiceRecordCount(&g_output); ++i) {
        const OutputRecord* record = OutputServiceRecordAt(&g_output, i);
        if (record && record->operationId == operationId && record->source == OutputSource::Application &&
            equalText(record->text, text)) return true;
    }
    return false;
}

static bool outputLacks(uint64_t operationId, const char* text)
{
    return !outputHas(operationId, text);
}

static bool runBuildBeforeRun(int32_t expectedExit, const char* expectedOutput,
                              uint64_t* outputOperationId, bool* buildFailed)
{
    if (outputOperationId) *outputOperationId = 0;
    if (buildFailed) *buildFailed = false;
    const uint64_t operationId = OutputServiceBeginOperation(&g_output, OutputOperationType::Run,
                                                               g_workspace.model.project.projectId);
    if (outputOperationId) *outputOperationId = operationId;
    RunControllerAttachOutput(&g_run, &g_output, operationId);
    BuildErrorCode buildError = BuildErrorCode::None;
    const bool started = BuildControllerStart(&g_build, &g_workspace, buildService(),
                                              BuildDirtyDecision::SaveAll, &buildError, &g_output);
    const bool built = started && BuildControllerPoll(&g_build, buildService()) &&
        !BuildControllerIsActive(&g_build) && g_build.result.state == BuildState::Succeeded;
    if (!built) {
        if (buildFailed) *buildFailed = true;
        OutputServiceCompleteOperation(&g_output, operationId, false,
                                       "Build failed | Run skipped", nullptr);
        return false;
    }

    RunRequest request = {};
    RunErrorCode runError = RunErrorCode::None;
    if (!RunRequestFromBuild(g_workspace.model.project, g_build.result, &request, &runError)) {
        OutputServiceCompleteOperation(&g_output, operationId, false,
                                       "Build succeeded | Run request rejected", nullptr);
        return false;
    }
    const bool identity = request.artifactSize == g_build.result.artifactSize &&
        equalText(request.artifactPath, g_build.result.artifactPath) &&
        equalText(request.artifactSha256, g_build.result.artifactSha256) &&
        equalText(request.artifactArchitecture, g_build.result.artifactArchitecture) &&
        equalText(request.artifactAbi, g_workspace.model.project.abi);
    g_identityProof = g_identityProof && identity;
    RunErrorCode prepareError = RunErrorCode::None;
    if (!RunControllerPrepare(&g_run, runService(), request, &prepareError) ||
        !RunControllerStart(&g_run, runService(), &prepareError)) return false;
    uint32_t polls = 0;
    while (RunControllerIsActive(&g_run) && polls++ < 4) RunControllerPoll(&g_run, runService());
    const RunResult result = g_run.result;
    if (!identity || result.state != RunState::Completed || !result.cleanupComplete ||
        result.exitCode != expectedExit || !outputHas(operationId, expectedOutput)) return false;
    return true;
}
#endif

static void marker(const char* passMarker, const char* failMarker, bool pass)
{
    const gx_host_calls* calls = host();
    if (calls && calls->log) calls->log(g_context, pass ? passMarker : failMarker);
}

static bool startAndPoll()
{
    BuildErrorCode error = BuildErrorCode::None;
    if (!BuildControllerStart(&g_build, &g_workspace, buildService(), BuildDirtyDecision::SaveAll,
                              &error, &g_output)) return false;
    return BuildControllerPoll(&g_build, buildService()) && !BuildControllerIsActive(&g_build);
}

static bool readSavedSource(const char* expected, uint32_t expectedBytes)
{
    char path[kMaxPathBytes] = {};
    if (!JoinWorkspacePath("/P27E", "src/main.cpp", path, sizeof(path))) return false;
    char saved[256] = {};
    uint32_t bytes = 0;
    if (!g_workspace.fileSystem.read || !g_workspace.fileSystem.read(g_workspace.fileSystem.userData,
        path, saved, sizeof(saved), &bytes) || bytes != expectedBytes) return false;
    for (uint32_t i = 0; i < bytes; ++i) if (saved[i] != expected[i]) return false;
    return true;
}

static bool editSource(Document* document, const char* source, uint32_t bytes)
{
    if (!document || !TextBufferSet(&document->buffer, source, bytes)) return false;
    document->buffer.caret = document->buffer.length;
    // TextBufferSet represents loading/replacing a document and clears dirty.
    // Insert and remove one byte so the proof uses the normal edit path while
    // preserving the exact source that Save All must persist.
    if (!TextBufferInsert(&document->buffer, " ", 1) || !TextBufferBackspace(&document->buffer)) return false;
    return document->buffer.dirty;
}

static bool runSmoke()
{
#if defined(GXOS_PHASE27F_APP)
    OutputServiceInit(&g_output);
    BuildControllerInit(&g_build);
    RunControllerInit(&g_run);
    const bool backend = hasBareHost();
    marker("phase27f_run_backend=PASS", "phase27f_run_backend=FAIL", backend);
    if (!backend) return false;
    WorkspaceControllerInit(&g_workspace, bareFileSystem());
    if (!WorkspaceControllerOpenProject(&g_workspace, "/P27F") ||
        !WorkspaceControllerOpenDocument(&g_workspace, "src/main.cpp")) return false;
    Document* document = WorkspaceControllerActiveDocument(&g_workspace);
    const char sourceA[] = "int gx_main(gx_app_context* ctx) {\n    log(ctx, \"Built and run from Developer Studio!\");\n    return 42;\n}\n";
    const char sourceB[] = "int gx_main(gx_app_context* ctx) {\n    log(ctx, \"Phase 27F edited run\");\n    return 41;\n}\n";
    const char invalid[] = "int gx_main(gx_app_context* ctx) {\n    return banana;\n}\n";
    uint64_t operationA = 0;
    const bool editA = editSource(document, sourceA, sizeof(sourceA) - 1);
    const bool firstRun = editA && runBuildBeforeRun(42, "Built and run from Developer Studio!", &operationA, nullptr);
    marker("phase27f_ide_run=PASS", "phase27f_ide_run=FAIL", firstRun);
    marker("phase27f_exit_code=PASS", "phase27f_exit_code=FAIL", firstRun && g_run.result.exitCode == 42);

    uint64_t operationB = 0;
    const bool editB = editSource(document, sourceB, sizeof(sourceB) - 1);
    const bool secondRun = editB && runBuildBeforeRun(41, "Phase 27F edited run", &operationB, nullptr);
    marker("phase27f_source_edit_run=PASS", "phase27f_source_edit_run=FAIL", secondRun);
    const bool isolated = secondRun && outputHas(operationB, "Phase 27F edited run") &&
        outputLacks(operationB, "Built and run from Developer Studio!");
    marker("phase27f_output_isolation=PASS", "phase27f_output_isolation=FAIL", isolated);
    marker("phase27f_artifact_identity=PASS", "phase27f_artifact_identity=FAIL", g_identityProof);

    bool blockedByBuildFailure = false;
    const bool editInvalid = editSource(document, invalid, sizeof(invalid) - 1);
    uint64_t invalidOperation = 0;
    const bool invalidRun = editInvalid &&
        runBuildBeforeRun(41, "Phase 27F edited run", &invalidOperation, &blockedByBuildFailure);
    const bool noLaunchOnFailure = editInvalid && blockedByBuildFailure && !invalidRun &&
        !outputHas(invalidOperation, "Phase 27F edited run") && !RunControllerIsActive(&g_run);
    marker("phase27f_build_failure_blocks_run=PASS", "phase27f_build_failure_blocks_run=FAIL", noLaunchOnFailure);

    uint64_t recoveryOperation = 0;
    const bool recoveredEdit = editSource(document, sourceB, sizeof(sourceB) - 1);
    const bool recovered = recoveredEdit &&
        runBuildBeforeRun(41, "Phase 27F edited run", &recoveryOperation, nullptr);
    marker("phase27f_recovery=PASS", "phase27f_recovery=FAIL", recovered);
    marker("phase27f_run_recovery=PASS", "phase27f_run_recovery=FAIL", recovered);

    bool repeated = recovered;
    for (uint32_t i = 0; i < 3; ++i) {
        uint64_t operation = 0;
        repeated = repeated && runBuildBeforeRun(41, "Phase 27F edited run", &operation, nullptr) &&
            outputHas(operation, "Phase 27F edited run") && !RunControllerIsActive(&g_run);
    }
    marker("phase27f_repeat=PASS", "phase27f_repeat=FAIL", repeated);
    marker("phase27f_repeat_run=PASS", "phase27f_repeat_run=FAIL", repeated);
    FileInfo sourceInfo = {};
    FileInfo artifactInfo = {};
    char sourcePath[kMaxPathBytes] = {};
    char artifactPath[kMaxPathBytes] = {};
    const bool survival = JoinWorkspacePath("/P27F", "src/main.cpp", sourcePath, sizeof(sourcePath)) &&
        JoinWorkspacePath("/P27F", g_build.result.artifactPath, artifactPath, sizeof(artifactPath)) &&
        g_workspace.fileSystem.stat(g_workspace.fileSystem.userData, sourcePath, &sourceInfo) &&
        g_workspace.fileSystem.stat(g_workspace.fileSystem.userData, artifactPath, &artifactInfo) &&
        sourceInfo.kind == FileInfoKind::RegularFile && artifactInfo.kind == FileInfoKind::RegularFile &&
        artifactInfo.size == g_build.result.artifactSize && !RunControllerIsActive(&g_run);
    marker("phase27f_kernel_survival=PASS", "phase27f_kernel_survival=FAIL", survival);
    const bool allPassed = firstRun && secondRun && isolated && g_identityProof && noLaunchOnFailure &&
        recovered && repeated && survival;
    marker("phase27f=PASS", "phase27f=FAIL", allPassed);
    return allPassed;
#else
    const bool backend = hasBareHost();
    marker("phase27e_build_backend=PASS", "phase27e_build_backend=FAIL", backend);
    if (!backend) return false;

    WorkspaceControllerInit(&g_workspace, bareFileSystem());
    OutputServiceInit(&g_output);
    BuildControllerInit(&g_build);
    const bool projectOpened = WorkspaceControllerOpenProject(&g_workspace, "/P27E");
    if (!projectOpened) {
        marker("phase27e_ide_build=PASS", "phase27e_ide_build=FAIL", false);
        return false;
    }
    const bool documentOpened = WorkspaceControllerOpenDocument(&g_workspace, "src/main.cpp");
    if (!documentOpened) {
        marker("phase27e_ide_build=PASS", "phase27e_ide_build=FAIL", false);
        return false;
    }

    const bool initialStarted = startAndPoll();
    const bool initialBuild = initialStarted && g_build.result.state == BuildState::Succeeded &&
        g_build.result.artifactValid && g_build.result.artifactEntryPoint;
    char initialHash[kMaxBuildArtifactSha256Bytes] = {};
    copyText(initialHash, sizeof(initialHash), g_build.result.artifactSha256);
    marker("phase27e_ide_build=PASS", "phase27e_ide_build=FAIL", initialBuild);

    Document* document = WorkspaceControllerActiveDocument(&g_workspace);
    const char invalidSource[] = "int gx_main(void* ctx) {\n    return banana;\n}\n";
    const bool dirtyInvalid = editSource(document, invalidSource, sizeof(invalidSource) - 1);
    const bool invalidFinished = dirtyInvalid && startAndPoll();
    const OutputRecord* problem = OutputServiceProblemAt(&g_output, "dev.guidexos.phase27e", 0);
    const bool diagnostic = invalidFinished && g_build.result.state == BuildState::Failed &&
        g_build.result.errorCount != 0 && problem && problem->hasLocation && problem->line == 2 &&
        problem->column != 0;
    const bool savedInvalid = dirtyInvalid && readSavedSource(invalidSource, sizeof(invalidSource) - 1);
    marker("phase27e_source_edit_build=PASS", "phase27e_source_edit_build=FAIL", savedInvalid && invalidFinished);
    marker("phase27e_ide_diagnostics=PASS", "phase27e_ide_diagnostics=FAIL", diagnostic);

    const char validSource[] = "int gx_main(void* ctx) {\n    return 41;\n}\n";
    const bool dirtyValid = editSource(document, validSource, sizeof(validSource) - 1);
    const bool rebuiltFinished = dirtyValid && startAndPoll();
    const bool rebuilt = rebuiltFinished && g_build.result.state == BuildState::Succeeded &&
        g_build.result.artifactValid && g_build.result.artifactSha256[0] != '\0' &&
        !equalText(initialHash, g_build.result.artifactSha256) &&
        readSavedSource(validSource, sizeof(validSource) - 1);
    marker("phase27e_rebuild_after_failure=PASS", "phase27e_rebuild_after_failure=FAIL", rebuilt);

    FileInfo artifact = {};
    char artifactPath[kMaxPathBytes] = {};
    const bool artifactPathOk = JoinWorkspacePath("/P27E", g_build.result.artifactPath,
                                                   artifactPath, sizeof(artifactPath));
    const bool kernelSurvival = artifactPathOk && g_workspace.fileSystem.stat &&
        g_workspace.fileSystem.stat(g_workspace.fileSystem.userData, artifactPath, &artifact) &&
        artifact.kind == FileInfoKind::RegularFile && artifact.size == g_build.result.artifactSize;
    marker("phase27e_kernel_survival=PASS", "phase27e_kernel_survival=FAIL", kernelSurvival);

    const bool allPassed = initialBuild && savedInvalid && diagnostic && rebuilt && kernelSurvival;
    marker("phase27e=PASS", "phase27e=FAIL", allPassed);
    return allPassed;
#endif
}

} // namespace
} // namespace developer_studio
} // namespace guidexos

extern "C" int GX_CALL gx_main(gx_app_context* context)
{
    using namespace guidexos::developer_studio;
    if (!context || context->size < sizeof(gx_app_context) || context->apiVersion != GX_API_VERSION ||
        !context->host || context->host->version != GX_API_VERSION || !context->host->log ||
        !context->host->get_api_version || context->host->get_api_version(context) != GX_API_VERSION) return 1;
    g_context = context;
    const bool passed = runSmoke();
    g_context = nullptr;
    return passed ? 0 : 1;
}
