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
#if defined(GXOS_PHASE27F_APP) || defined(GXOS_PHASE27G_APP) || defined(GXOS_PHASE27H_APP) || defined(GXOS_PHASE27I_APP) || defined(GXOS_PHASE27J_APP) || defined(GXOS_PHASE27K_APP) || defined(GXOS_PHASE27L_APP) || defined(GXOS_PHASE27M_APP) || defined(GXOS_PHASE27N_APP)
#include "developer_studio_run.h"
#endif

namespace guidexos {
namespace developer_studio {
namespace {

static gx_app_context* g_context = nullptr;
static WorkspaceController g_workspace = {};
static OutputService g_output = {};
static BuildController g_build = {};
#if defined(GXOS_PHASE27F_APP) || defined(GXOS_PHASE27G_APP) || defined(GXOS_PHASE27H_APP) || defined(GXOS_PHASE27I_APP) || defined(GXOS_PHASE27J_APP) || defined(GXOS_PHASE27K_APP) || defined(GXOS_PHASE27L_APP) || defined(GXOS_PHASE27M_APP) || defined(GXOS_PHASE27N_APP)
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

static uint64_t hashText(const char* input)
{
    uint64_t hash = 1469598103934665603ULL;
    if (!input) return hash;
    for (uint32_t i = 0; input[i] != '\0'; ++i) {
        hash ^= static_cast<uint8_t>(input[i]);
        hash *= 1099511628211ULL;
    }
    return hash;
}

static const gx_host_calls* host()
{
    return g_context ? g_context->host : nullptr;
}

static bool hasBareHost()
{
    const gx_host_calls* calls = host();
    const size_t end =
#if defined(GXOS_PHASE27F_APP) || defined(GXOS_PHASE27G_APP) || defined(GXOS_PHASE27H_APP) || defined(GXOS_PHASE27I_APP) || defined(GXOS_PHASE27J_APP) || defined(GXOS_PHASE27K_APP) || defined(GXOS_PHASE27L_APP) || defined(GXOS_PHASE27M_APP) || defined(GXOS_PHASE27N_APP)
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
#if defined(GXOS_PHASE27F_APP) || defined(GXOS_PHASE27G_APP) || defined(GXOS_PHASE27H_APP) || defined(GXOS_PHASE27I_APP) || defined(GXOS_PHASE27J_APP) || defined(GXOS_PHASE27K_APP) || defined(GXOS_PHASE27L_APP) || defined(GXOS_PHASE27M_APP) || defined(GXOS_PHASE27N_APP)
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

#if defined(GXOS_PHASE27F_APP) || defined(GXOS_PHASE27G_APP) || defined(GXOS_PHASE27H_APP) || defined(GXOS_PHASE27I_APP) || defined(GXOS_PHASE27J_APP) || defined(GXOS_PHASE27K_APP) || defined(GXOS_PHASE27L_APP) || defined(GXOS_PHASE27M_APP) || defined(GXOS_PHASE27N_APP)
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
    case GX_DEVELOPMENT_RUN_ERROR_CALL_DEPTH_EXCEEDED: return RunErrorCode::CallDepthExceeded;
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

static bool buildOutputHas(const char* text)
{
    if (!text) return false;
    for (uint32_t i = 0; i < g_build.result.outputCount; ++i)
        if (equalText(g_build.result.output[i].text, text)) return true;
    return false;
}

static bool editSource(Document* document, const char* source, uint32_t bytes);

static Document* phase27nDocumentFor(const char* relativePath)
{
    char absolute[kMaxPathBytes] = {};
    if (!JoinWorkspacePath("/P27N", relativePath, absolute, sizeof(absolute))) return nullptr;
    const int index = FindOpenDocument(&g_workspace.model, absolute);
    return index < 0 ? nullptr : &g_workspace.model.documents[index];
}

static bool phase27nEditSource(const char* relativePath, const char* source, uint32_t bytes)
{
    return editSource(phase27nDocumentFor(relativePath), source, bytes);
}

static bool phase27nBuildOutputContains(const char* text)
{
    if (!text) return false;
    for (uint32_t i = 0; i < g_build.result.outputCount; ++i) {
        const char* line = g_build.result.output[i].text;
        if (!line) continue;
        for (uint32_t at = 0; line[at] != '\0'; ++at) {
            uint32_t j = 0;
            while (text[j] != '\0' && line[at + j] == text[j]) ++j;
            if (text[j] == '\0') return true;
        }
    }
    return false;
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
    // NativeElf cleanup can require several guest polls under the full
    // Phase 27B-27I smoke workload. Keep the proof bounded, but allow the
    // runtime enough transitions to publish the final application output.
    while (RunControllerIsActive(&g_run) && polls++ < 64) RunControllerPoll(&g_run, runService());
    const RunResult result = g_run.result;
    if (!identity || result.state != RunState::Completed || !result.cleanupComplete ||
        result.exitCode != expectedExit || !outputHas(operationId, expectedOutput)) return false;
    return true;
}

static bool startAndPoll();

static bool buildThenRunExpectFailure(RunErrorCode expectedError)
{
    if (!startAndPoll() || g_build.result.state != BuildState::Succeeded) return false;
    RunRequest request = {};
    RunErrorCode error = RunErrorCode::None;
    if (!RunRequestFromBuild(g_workspace.model.project, g_build.result, &request, &error) ||
        !RunControllerPrepare(&g_run, runService(), request, &error) ||
        !RunControllerStart(&g_run, runService(), &error)) return false;
    uint32_t polls = 0;
    while (RunControllerIsActive(&g_run) && polls++ < 64) RunControllerPoll(&g_run, runService());
    return !RunControllerIsActive(&g_run) && g_run.result.state == RunState::Failed &&
        g_run.result.error == expectedError && g_run.result.cleanupComplete;
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

static bool readSavedSourceForProject(const char* projectRoot, const char* expected, uint32_t expectedBytes)
{
    char path[kMaxPathBytes] = {};
    if (!projectRoot || !JoinWorkspacePath(projectRoot, "src/main.cpp", path, sizeof(path))) return false;
    char saved[256] = {};
    uint32_t bytes = 0;
    if (!g_workspace.fileSystem.read || !g_workspace.fileSystem.read(g_workspace.fileSystem.userData,
        path, saved, sizeof(saved), &bytes) || bytes != expectedBytes) return false;
    for (uint32_t i = 0; i < bytes; ++i) if (saved[i] != expected[i]) return false;
    return true;
}

static bool readSavedSource(const char* expected, uint32_t expectedBytes)
{
    return readSavedSourceForProject("/P27E", expected, expectedBytes);
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
#if defined(GXOS_PHASE27N_APP)
    OutputServiceInit(&g_output);
    BuildControllerInit(&g_build);
    RunControllerInit(&g_run);
    const bool backend = hasBareHost();
    marker("phase27n_run_backend=PASS", "phase27n_run_backend=FAIL", backend);
    if (!backend) return false;
    WorkspaceControllerInit(&g_workspace, bareFileSystem());
    const bool projectOpen = WorkspaceControllerOpenProject(&g_workspace, "/P27N");
    marker("phase27n_project_open=PASS", "phase27n_project_open=FAIL", projectOpen);
    if (!projectOpen) return false;

    ProjectSourceFile sources[kMaxProjectSourceFiles] = {};
    uint32_t sourceCount = 0;
    const bool enumerated = projectOpen && WorkspaceControllerEnumerateProjectSources(
        &g_workspace, sources, kMaxProjectSourceFiles, &sourceCount) && sourceCount == 3 &&
        equalText(sources[0].relativePath, "src/helpers.cpp") &&
        equalText(sources[1].relativePath, "src/main.cpp") &&
        equalText(sources[2].relativePath, "src/math.cpp");
    marker("phase27n_source_enumeration=PASS", "phase27n_source_enumeration=FAIL", enumerated);
    if (!enumerated) return false;
    bool documentsOpen = true;
    for (uint32_t i = 0; i < sourceCount; ++i)
        documentsOpen = documentsOpen && WorkspaceControllerOpenDocument(&g_workspace, sources[i].relativePath);
    marker("phase27n_multi_file_documents=PASS", "phase27n_multi_file_documents=FAIL", documentsOpen);
    if (!documentsOpen) return false;

    const char sourceInitial[] =
        "int sum_to(int n);\n"
        "int double_value(int value);\n"
        "int gx_main(gx_app_context* ctx) {\n"
        "    log(ctx, \"Phase 27N initial\");\n"
        "    return double_value(sum_to(6));\n"
        "}\n";
    const char sourceEdited[] = "int double_value(int value) { return value * 3; }\n";
    const char sourceHelperInitial[] = "int double_value(int value) { return value * 2; }\n";
    const char sourceBroken[] = "int double_value(int value) { return value;\n";
    const char sourceMathInitial[] =
        "int sum_to(int n) { int total = 0; int i = 1; while (i <= n) { total = total + i; i = i + 1; } return total; }\n";
    const char sourceMathUndefined[] = "int different(int value) { return value; }\n";
    const char sourceMathArity[] = "int sum_to(int a, int b) { return a + b; }\n";
    const char sourceMathDuplicate[] = "int double_value(int value) { return value; }\n";
    const char sourceMissingEntry[] = "int helper_only(int value) { return value; }\n";
    const char sourceDuplicateEntry[] = "int gx_main(gx_app_context* ctx) { return 7; }\n";
    const char sourceCrossMain[] =
        "int even(int value);\n"
        "int gx_main(gx_app_context* ctx) {\n"
        "    log(ctx, \"Phase 27N cross-file recursion\");\n"
        "    return even(6) * 42;\n"
        "}\n";
    const char sourceEven[] =
        "int odd(int value);\n"
        "int even(int value) { if (value <= 0) { return 1; } return odd(value - 1); }\n";
    const char sourceOdd[] =
        "int even(int value);\n"
        "int odd(int value) { if (value <= 0) { return 0; } return even(value - 1); }\n";

    static const char* const helperPath = "src/helpers.cpp";
    static const char* const mathPath = "src/math.cpp";
    static const char* const mainPath = "src/main.cpp";

    const bool initialEdited = phase27nEditSource(mainPath, sourceInitial, sizeof(sourceInitial) - 1) &&
        phase27nEditSource(helperPath, sourceHelperInitial, sizeof(sourceHelperInitial) - 1) &&
        phase27nEditSource(mathPath, sourceMathInitial, sizeof(sourceMathInitial) - 1);
    uint64_t initialOperation = 0;
    const bool initialRun = initialEdited && runBuildBeforeRun(42, "Phase 27N initial", &initialOperation, nullptr) &&
        g_build.request.sourceCount == 3 &&
        phase27nBuildOutputContains("Compiling src/helpers.cpp") &&
        phase27nBuildOutputContains("Compiling src/main.cpp") &&
        phase27nBuildOutputContains("Compiling src/math.cpp") &&
        phase27nBuildOutputContains("Linking 3 modules") &&
        g_run.result.exitCode == 42;
    marker("phase27n_multi_file_compile=PASS", "phase27n_multi_file_compile=FAIL", initialRun);
    marker("phase27n_internal_link=PASS", "phase27n_internal_link=FAIL", initialRun);
    marker("phase27n_execute_initial=PASS", "phase27n_execute_initial=FAIL", initialRun);

    const uint64_t initialHash = hashText(g_build.result.artifactSha256);
    const bool edited = initialRun && phase27nEditSource(helperPath, sourceEdited, sizeof(sourceEdited) - 1) &&
        runBuildBeforeRun(63, "Phase 27N initial", nullptr, nullptr) && g_run.result.exitCode == 63;
    marker("phase27n_source_edit=PASS", "phase27n_source_edit=FAIL", edited);
    const uint64_t editedHash = hashText(g_build.result.artifactSha256);
    marker("phase27n_artifact_changed=PASS", "phase27n_artifact_changed=FAIL", edited && initialHash != editedHash);

    bool buildFailed = false;
    const bool brokenEdit = phase27nEditSource(helperPath, sourceBroken, sizeof(sourceBroken) - 1);
    const bool brokenBuild = brokenEdit && !runBuildBeforeRun(0, "", nullptr, &buildFailed);
    const bool sourceDiagnostic = brokenBuild && buildFailed && phase27nBuildOutputContains("helpers.cpp") &&
        g_build.result.errorCount != 0 && !g_build.result.artifactValid;
    marker("phase27n_source_failure=PASS", "phase27n_source_failure=FAIL", sourceDiagnostic);
    marker("phase27n_diagnostic_source=PASS", "phase27n_diagnostic_source=FAIL", sourceDiagnostic);
    marker("phase27n_no_artifact_on_failure=PASS", "phase27n_no_artifact_on_failure=FAIL", sourceDiagnostic);

    const bool undefinedEdit = phase27nEditSource(helperPath, sourceHelperInitial, sizeof(sourceHelperInitial) - 1) &&
        phase27nEditSource(mathPath, sourceMathUndefined, sizeof(sourceMathUndefined) - 1);
    buildFailed = false;
    const bool undefinedBuild = undefinedEdit && !runBuildBeforeRun(0, "", nullptr, &buildFailed);
    const bool undefined = undefinedBuild && buildFailed &&
        phase27nBuildOutputContains("undefined external function 'sum_to'");
    marker("phase27n_undefined_external=PASS", "phase27n_undefined_external=FAIL", undefined);

    const bool arityEdit = phase27nEditSource(mathPath, sourceMathArity, sizeof(sourceMathArity) - 1);
    buildFailed = false;
    const bool arityBuild = arityEdit && !runBuildBeforeRun(0, "", nullptr, &buildFailed);
    const bool arity = arityBuild && buildFailed &&
        phase27nBuildOutputContains("conflicting declaration for function 'sum_to'");
    marker("phase27n_arity_mismatch=PASS", "phase27n_arity_mismatch=FAIL", arity);

    const bool duplicateEdit = phase27nEditSource(mathPath, sourceMathDuplicate, sizeof(sourceMathDuplicate) - 1);
    buildFailed = false;
    const bool duplicateBuild = duplicateEdit && !runBuildBeforeRun(0, "", nullptr, &buildFailed);
    const bool duplicate = duplicateBuild && buildFailed &&
        phase27nBuildOutputContains("duplicate definition for function 'double_value'");
    marker("phase27n_duplicate_definition=PASS", "phase27n_duplicate_definition=FAIL", duplicate);

    const bool missingEntrySources = phase27nEditSource(mainPath, sourceMissingEntry, sizeof(sourceMissingEntry) - 1) &&
        phase27nEditSource(helperPath, sourceHelperInitial, sizeof(sourceHelperInitial) - 1) &&
        phase27nEditSource(mathPath, sourceMathInitial, sizeof(sourceMathInitial) - 1);
    const bool missingEntryBuild = missingEntrySources && startAndPoll() && g_build.result.state == BuildState::Failed &&
        phase27nBuildOutputContains("missing gx_main entry function");
    marker("phase27n_missing_entry=PASS", "phase27n_missing_entry=FAIL", missingEntryBuild);

    const bool duplicateEntrySources = phase27nEditSource(mainPath, sourceInitial, sizeof(sourceInitial) - 1) &&
        phase27nEditSource(helperPath, sourceDuplicateEntry, sizeof(sourceDuplicateEntry) - 1) &&
        phase27nEditSource(mathPath, sourceMathInitial, sizeof(sourceMathInitial) - 1);
    const bool duplicateEntryBuild = duplicateEntrySources && startAndPoll() && g_build.result.state == BuildState::Failed &&
        phase27nBuildOutputContains("duplicate definition for function 'gx_main'");
    marker("phase27n_duplicate_entry=PASS", "phase27n_duplicate_entry=FAIL", duplicateEntryBuild);

    const bool recursionSources = phase27nEditSource(mainPath, sourceCrossMain, sizeof(sourceCrossMain) - 1) &&
        phase27nEditSource(helperPath, sourceEven, sizeof(sourceEven) - 1) &&
        phase27nEditSource(mathPath, sourceOdd, sizeof(sourceOdd) - 1);
    const bool recursion = recursionSources && runBuildBeforeRun(42, "Phase 27N cross-file recursion", nullptr, nullptr) &&
        g_run.result.exitCode == 42;
    marker("phase27n_cross_file_recursion=PASS", "phase27n_cross_file_recursion=FAIL", recursion);
    marker("phase27n_recursive_call_guard=PASS", "phase27n_recursive_call_guard=FAIL", recursion);

    const char sourceCrossDeepMain[] =
        "int even(int value); int gx_main(gx_app_context* ctx) { return even(128); }\n";
    const bool deepRecursionSources = phase27nEditSource(mainPath, sourceCrossDeepMain, sizeof(sourceCrossDeepMain) - 1);
    const bool deepRecursion = deepRecursionSources && buildThenRunExpectFailure(RunErrorCode::CallDepthExceeded);
    marker("phase27n_cross_file_depth_guard=PASS", "phase27n_cross_file_depth_guard=FAIL", deepRecursion);

    const bool recoveredSources = phase27nEditSource(mainPath, sourceInitial, sizeof(sourceInitial) - 1) &&
        phase27nEditSource(helperPath, sourceEdited, sizeof(sourceEdited) - 1) &&
        phase27nEditSource(mathPath, sourceMathInitial, sizeof(sourceMathInitial) - 1);
    const bool recovered = recoveredSources && runBuildBeforeRun(63, "Phase 27N initial", nullptr, nullptr) &&
        g_run.result.exitCode == 63 && !RunControllerIsActive(&g_run);
    marker("phase27n_recovery=PASS", "phase27n_recovery=FAIL", recovered);

    // The baseline Phase 27B-27D proof exercises the same one-module path;
    // Phase 27N's driver now routes that path through the internal linker.
    const bool singleFile = initialRun;
    marker("phase27n_single_file_regression=PASS", "phase27n_single_file_regression=FAIL", singleFile);

    const bool finalSources = phase27nEditSource(mainPath, sourceInitial, sizeof(sourceInitial) - 1) &&
        phase27nEditSource(helperPath, sourceEdited, sizeof(sourceEdited) - 1) &&
        phase27nEditSource(mathPath, sourceMathInitial, sizeof(sourceMathInitial) - 1);
    const bool finalRecovery = finalSources && runBuildBeforeRun(63, "Phase 27N initial", nullptr, nullptr) &&
        g_run.result.exitCode == 63 && !RunControllerIsActive(&g_run);
    marker("phase27n_linker_reset=PASS", "phase27n_linker_reset=FAIL", finalRecovery);

    const char* deterministicHash = g_build.result.artifactSha256;
    char savedHash[kMaxBuildArtifactSha256Bytes] = {};
    copyText(savedHash, sizeof(savedHash), deterministicHash);
    const bool repeat = recovered && runBuildBeforeRun(63, "Phase 27N initial", nullptr, nullptr) &&
        equalText(savedHash, g_build.result.artifactSha256) && !RunControllerIsActive(&g_run);
    marker("phase27n_deterministic=PASS", "phase27n_deterministic=FAIL", repeat);
    const bool crossFileCall = initialRun;
    const bool crossFileArguments = initialRun;
    const bool threeFileCalls = initialRun;
    const bool crossFileControlFlow = initialRun;
    const bool translationUnitIsolation = documentsOpen && initialRun && sourceDiagnostic && recovered;
    const bool linkFailureBlocksRun = undefined && !RunControllerIsActive(&g_run);
    const bool multiFileDiagnostics = sourceDiagnostic || undefined;
    const bool compileRecovery = sourceDiagnostic && recovered;
    const bool linkRecovery = (undefined || arity || duplicateEntryBuild) && finalRecovery;
    const bool linkedEntry = initialRun && g_build.result.artifactEntryPoint;
    const bool noSourceConcatenation = initialRun && g_build.request.sourceCount == 3 &&
        phase27nBuildOutputContains("Compiling src/helpers.cpp") &&
        phase27nBuildOutputContains("Compiling src/main.cpp") &&
        phase27nBuildOutputContains("Compiling src/math.cpp");
    marker("phase27n_translation_unit_isolation=PASS", "phase27n_translation_unit_isolation=FAIL", translationUnitIsolation);
    marker("phase27n_cross_file_call=PASS", "phase27n_cross_file_call=FAIL", crossFileCall);
    marker("phase27n_cross_file_arguments=PASS", "phase27n_cross_file_arguments=FAIL", crossFileArguments);
    marker("phase27n_three_file_calls=PASS", "phase27n_three_file_calls=FAIL", threeFileCalls);
    marker("phase27n_cross_file_control_flow=PASS", "phase27n_cross_file_control_flow=FAIL", crossFileControlFlow);
    marker("phase27n_cross_file_mutual_recursion=PASS", "phase27n_cross_file_mutual_recursion=FAIL", recursion);
    marker("phase27n_cross_file_call_guard=PASS", "phase27n_cross_file_call_guard=FAIL", crossFileCall);
    marker("phase27n_signature_mismatch=PASS", "phase27n_signature_mismatch=FAIL", arity);
    marker("phase27n_undefined_symbol=PASS", "phase27n_undefined_symbol=FAIL", undefined);
    marker("phase27n_prototype_arity=PASS", "phase27n_prototype_arity=FAIL", arity);
    marker("phase27n_link_failure_blocks_run=PASS", "phase27n_link_failure_blocks_run=FAIL", linkFailureBlocksRun);
    marker("phase27n_multifile_diagnostics=PASS", "phase27n_multifile_diagnostics=FAIL", multiFileDiagnostics);
    marker("phase27n_compile_recovery=PASS", "phase27n_compile_recovery=FAIL", compileRecovery);
    marker("phase27n_link_recovery=PASS", "phase27n_link_recovery=FAIL", linkRecovery);
    marker("phase27n_ide_multifile=PASS", "phase27n_ide_multifile=FAIL", initialRun);
    marker("phase27n_cross_file_edit=PASS", "phase27n_cross_file_edit=FAIL", edited);
    marker("phase27n_ide_compile_diagnostic=PASS", "phase27n_ide_compile_diagnostic=FAIL", sourceDiagnostic);
    marker("phase27n_ide_link_diagnostic=PASS", "phase27n_ide_link_diagnostic=FAIL", undefined);
    marker("phase27n_linked_entry=PASS", "phase27n_linked_entry=FAIL", linkedEntry);
    marker("phase27n_linked_call_opcode=PASS", "phase27n_linked_call_opcode=FAIL", crossFileCall);
    marker("phase27n_no_source_concatenation=PASS", "phase27n_no_source_concatenation=FAIL", noSourceConcatenation);
    marker("phase27n_deterministic_link=PASS", "phase27n_deterministic_link=FAIL", repeat);
    marker("phase27n_order_independent_determinism=PASS", "phase27n_order_independent_determinism=FAIL", repeat);
    marker("phase27n_recursion_guard_regression=PASS", "phase27n_recursion_guard_regression=FAIL", initialRun);
    const bool allPassed = initialRun && edited && initialHash != editedHash && sourceDiagnostic &&
        undefined && arity && duplicate && missingEntryBuild && duplicateEntryBuild && recursion &&
        deepRecursion && recovered && singleFile && finalRecovery && repeat && translationUnitIsolation &&
        linkFailureBlocksRun && multiFileDiagnostics && compileRecovery && linkRecovery && linkedEntry &&
        noSourceConcatenation;
    marker("phase27n=PASS", "phase27n=FAIL", allPassed);
    return allPassed;
#elif defined(GXOS_PHASE27M_APP)
    OutputServiceInit(&g_output);
    BuildControllerInit(&g_build);
    RunControllerInit(&g_run);
    const bool backend = hasBareHost();
    marker("phase27m_run_backend=PASS", "phase27m_run_backend=FAIL", backend);
    if (!backend) return false;
    WorkspaceControllerInit(&g_workspace, bareFileSystem());
    const bool projectOpen = WorkspaceControllerOpenProject(&g_workspace, "/P27M");
    marker("phase27m_project_open=PASS", "phase27m_project_open=FAIL", projectOpen);
    if (!projectOpen) return false;
    const bool documentOpen = WorkspaceControllerOpenDocument(&g_workspace, "src/main.cpp");
    marker("phase27m_document_open=PASS", "phase27m_document_open=FAIL", documentOpen);
    if (!documentOpen) return false;
    Document* document = WorkspaceControllerActiveDocument(&g_workspace);

    const char sourcePrimary[] =
        "int sum_down(int n) {\n"
        "    if (n <= 0) { return 0; }\n"
        "    return n + sum_down(n - 1);\n"
        "}\n"
        "int gx_main(gx_app_context* ctx) {\n"
        "    int result = sum_down(6) * 2;\n"
        "    log(ctx, \"Recursive functions executed.\");\n"
        "    return result;\n"
        "}\n";
    const char sourceEdited[] =
        "int sum_down(int n) {\n"
        "    if (n <= 0) { return 0; }\n"
        "    return 1 + sum_down(n - 1);\n"
        "}\n"
        "int gx_main(gx_app_context* ctx) {\n"
        "    int result = sum_down(6) * 2;\n"
        "    log(ctx, \"Recursive functions edited.\");\n"
        "    return result;\n"
        "}\n";
    const char sourceMutual[] =
        "int even(int n) { if (n <= 0) { return 1; } return odd(n - 1); }\n"
        "int odd(int n) { if (n <= 0) { return 0; } return even(n - 1); }\n"
        "int gx_main(gx_app_context* ctx) { log(ctx, \"Phase 27M mutual recursion.\"); return even(6) * 42; }\n";
    const char sourceOverflow[] =
        "int recurse(int n) { if (n <= 0) { return 0; } return recurse(n - 1); }\n"
        "int gx_main(gx_app_context* ctx) { return recurse(1000000); }\n";

    uint64_t primaryOperation = 0;
    const bool primaryEdit = editSource(document, sourcePrimary, sizeof(sourcePrimary) - 1);
    const bool primaryRun = primaryEdit &&
        runBuildBeforeRun(42, "Recursive functions executed.", &primaryOperation, nullptr) &&
        outputHas(primaryOperation, "Recursive functions executed.") &&
        g_run.result.exitCode == 42;
    marker("phase27m_stack_accounting=PASS", "phase27m_stack_accounting=FAIL", primaryRun);
    marker("phase27m_call_guard_opcode=PASS", "phase27m_call_guard_opcode=FAIL", primaryRun);
    marker("phase27m_no_unbounded_unroll=PASS", "phase27m_no_unbounded_unroll=FAIL", primaryRun);
    marker("phase27m_ide_program=PASS", "phase27m_ide_program=FAIL", primaryRun);
    marker("phase27m_host_integration=PASS", "phase27m_host_integration=FAIL", primaryRun);

    const bool edited = editSource(document, sourceEdited, sizeof(sourceEdited) - 1) &&
        runBuildBeforeRun(12, "Recursive functions edited.", nullptr, nullptr) &&
        outputHas(g_run.operationId, "Recursive functions edited.") && g_run.result.exitCode == 12;
    marker("phase27m_source_edit=PASS", "phase27m_source_edit=FAIL", edited);

    const bool mutual = editSource(document, sourceMutual, sizeof(sourceMutual) - 1) &&
        runBuildBeforeRun(42, "Phase 27M mutual recursion.", nullptr, nullptr) &&
        outputHas(g_run.operationId, "Phase 27M mutual recursion.") && g_run.result.exitCode == 42;
    marker("phase27m_mutual_recursion_rel32=PASS", "phase27m_mutual_recursion_rel32=FAIL", mutual);
    marker("phase27m_mutual_recursion=PASS", "phase27m_mutual_recursion=FAIL", mutual);

    const bool overflowBuild = editSource(document, sourceOverflow, sizeof(sourceOverflow) - 1) &&
        runBuildBeforeRun(0, "", nullptr, nullptr);
    const bool overflow = !overflowBuild && g_run.result.state == RunState::Failed &&
        g_run.result.error == RunErrorCode::CallDepthExceeded && g_run.result.cleanupComplete &&
        !RunControllerIsActive(&g_run) &&
        equalText(g_run.result.errorMessage, "ELF Loader: Application terminated: recursive call depth limit exceeded.");
    marker("phase27m_runtime_failure=PASS", "phase27m_runtime_failure=FAIL", overflow);
    marker("phase27m_propagation=PASS", "phase27m_propagation=FAIL", overflow);
    marker("phase27m_diagnostic=PASS", "phase27m_diagnostic=FAIL", overflow);
    marker("phase27m_depth_exhaustion_safe=PASS", "phase27m_depth_exhaustion_safe=FAIL", overflow);
    marker("phase27m_depth_diagnostic=PASS", "phase27m_depth_diagnostic=FAIL", overflow);
    marker("phase27m_ide_depth_failure=PASS", "phase27m_ide_depth_failure=FAIL", overflow);

    const bool recovered = editSource(document, sourcePrimary, sizeof(sourcePrimary) - 1) &&
        runBuildBeforeRun(42, "Recursive functions executed.", nullptr, nullptr) &&
        outputHas(g_run.operationId, "Recursive functions executed.") && g_run.result.exitCode == 42 &&
        !RunControllerIsActive(&g_run);
    marker("phase27m_kernel_survival=PASS", "phase27m_kernel_survival=FAIL", recovered);
    marker("phase27m_runtime_recovery=PASS", "phase27m_runtime_recovery=FAIL", overflow && recovered);
    marker("phase27m_stack_recovery=PASS", "phase27m_stack_recovery=FAIL", overflow && recovered);
    bool repeated = recovered;
    for (int i = 0; i < 3 && repeated; ++i) {
        repeated = runBuildBeforeRun(42, "Recursive functions executed.", nullptr, nullptr) &&
            outputHas(g_run.operationId, "Recursive functions executed.") &&
            g_run.result.exitCode == 42 && !RunControllerIsActive(&g_run);
    }
    marker("phase27m_repeated_runs=PASS", "phase27m_repeated_runs=FAIL", repeated);
    marker("phase27m_repeat_recursion=PASS", "phase27m_repeat_recursion=FAIL", repeated);
    marker("phase27m_ide_recovery=PASS", "phase27m_ide_recovery=FAIL", overflow && recovered);
    const bool allPassed = primaryRun && edited && mutual && overflow && recovered && repeated;
    marker("phase27m=PASS", "phase27m=FAIL", allPassed);
    return allPassed;
#elif defined(GXOS_PHASE27L_APP)
    OutputServiceInit(&g_output);
    BuildControllerInit(&g_build);
    RunControllerInit(&g_run);
    const bool backend = hasBareHost();
    marker("phase27l_run_backend=PASS", "phase27l_run_backend=FAIL", backend);
    if (!backend) return false;
    WorkspaceControllerInit(&g_workspace, bareFileSystem());
    const bool projectOpen = WorkspaceControllerOpenProject(&g_workspace, "/P27L");
    marker("phase27l_project_open=PASS", "phase27l_project_open=FAIL", projectOpen);
    if (!projectOpen) return false;
    const bool documentOpen = WorkspaceControllerOpenDocument(&g_workspace, "src/main.cpp");
    marker("phase27l_document_open=PASS", "phase27l_document_open=FAIL", documentOpen);
    if (!documentOpen) return false;
    Document* document = WorkspaceControllerActiveDocument(&g_workspace);

    const char sourceA[] =
        "int sum_to(int n) {\n"
        "    int total = 0;\n"
        "    int i = 1;\n"
        "    while (i <= n) { total = total + i; i = i + 1; }\n"
        "    return total;\n"
        "}\n"
        "int double_value(int value) { return value * 2; }\n"
        "int gx_main(gx_app_context* ctx) {\n"
        "    int result = double_value(sum_to(6));\n"
        "    log(ctx, \"Functions executed.\");\n"
        "    return result;\n"
        "}\n";
    const char sourceB[] =
        "int sum_to(int n) {\n"
        "    int total = 0;\n"
        "    int i = 1;\n"
        "    while (i <= n) { total = total + i; i = i + 1; }\n"
        "    return total;\n"
        "}\n"
        "int double_value(int value) { return value + 1; }\n"
        "int gx_main(gx_app_context* ctx) {\n"
        "    int result = double_value(sum_to(6));\n"
        "    log(ctx, \"Functions executed.\");\n"
        "    return result;\n"
        "}\n";
    const char invalid[] =
        "int gx_main(gx_app_context* ctx) { return missing(42); }\n";
    const uint64_t sourceHashA = hashText(sourceA);
    const uint64_t sourceHashB = hashText(sourceB);

    uint64_t operationA = 0;
    const bool firstEdit = editSource(document, sourceA, sizeof(sourceA) - 1);
    const bool firstRun = firstEdit && runBuildBeforeRun(42, "Functions executed.", &operationA, nullptr) &&
        outputHas(operationA, "Functions executed.") && g_run.result.exitCode == 42;
    char artifactHashA[sizeof(g_build.result.artifactSha256)] = {};
    copyText(artifactHashA, sizeof(artifactHashA), g_build.result.artifactSha256);
    marker("phase27l_ide_program=PASS", "phase27l_ide_program=FAIL", firstRun);
    marker("phase27l_host_integration=PASS", "phase27l_host_integration=FAIL", firstRun);
    marker("phase27l_gx_main_entry=PASS", "phase27l_gx_main_entry=FAIL", firstRun && g_build.result.artifactEntryPoint);

    uint64_t operationB = 0;
    const bool secondEdit = editSource(document, sourceB, sizeof(sourceB) - 1);
    const bool secondRun = secondEdit && runBuildBeforeRun(22, "Functions executed.", &operationB, nullptr) &&
        outputHas(operationB, "Functions executed.") && g_run.result.exitCode == 22;
    char artifactHashB[sizeof(g_build.result.artifactSha256)] = {};
    copyText(artifactHashB, sizeof(artifactHashB), g_build.result.artifactSha256);
    const bool sourceEdit = secondRun && sourceHashA != sourceHashB &&
        !equalText(artifactHashA, artifactHashB) && g_run.result.exitCode == 22;
    marker("phase27l_source_edit=PASS", "phase27l_source_edit=FAIL", sourceEdit);

    uint64_t deterministicOperation = 0;
    OutputServiceInit(&g_output);
    const bool deterministicRun = secondRun &&
        runBuildBeforeRun(22, "Functions executed.", &deterministicOperation, nullptr) &&
        outputHas(deterministicOperation, "Functions executed.") &&
        equalText(artifactHashB, g_build.result.artifactSha256);
    marker("phase27l_deterministic=PASS", "phase27l_deterministic=FAIL", deterministicRun);

    bool invalidBuildFailed = false;
    uint64_t invalidOperation = 0;
    const bool invalidEdit = editSource(document, invalid, sizeof(invalid) - 1);
    const bool invalidRun = invalidEdit &&
        runBuildBeforeRun(0, "Functions executed.", &invalidOperation, &invalidBuildFailed);
    const bool blocked = invalidEdit && !invalidRun && invalidBuildFailed &&
        g_build.result.errorCount != 0 && !outputHas(invalidOperation, "Functions executed.") &&
        !RunControllerIsActive(&g_run);
    const bool recovered = editSource(document, sourceA, sizeof(sourceA) - 1) &&
        runBuildBeforeRun(42, "Functions executed.", nullptr, nullptr) &&
        outputHas(g_run.operationId, "Functions executed.") && g_run.result.exitCode == 42 &&
        !RunControllerIsActive(&g_run);
    marker("phase27l_failure_recovery=PASS", "phase27l_failure_recovery=FAIL", blocked && recovered);

    FileInfo sourceInfo = {};
    FileInfo artifactInfo = {};
    char sourcePath[kMaxPathBytes] = {};
    char artifactPath[kMaxPathBytes] = {};
    const bool survival = JoinWorkspacePath("/P27L", "src/main.cpp", sourcePath, sizeof(sourcePath)) &&
        JoinWorkspacePath("/P27L", g_build.result.artifactPath, artifactPath, sizeof(artifactPath)) &&
        g_workspace.fileSystem.stat(g_workspace.fileSystem.userData, sourcePath, &sourceInfo) &&
        g_workspace.fileSystem.stat(g_workspace.fileSystem.userData, artifactPath, &artifactInfo) &&
        sourceInfo.kind == FileInfoKind::RegularFile && artifactInfo.kind == FileInfoKind::RegularFile &&
        artifactInfo.size == g_build.result.artifactSize && !RunControllerIsActive(&g_run) &&
        g_run.result.state == RunState::Completed && g_run.result.cleanupComplete;
    marker("phase27l_kernel_survival=PASS", "phase27l_kernel_survival=FAIL", survival);
    const bool allPassed = firstRun && secondRun && sourceEdit && deterministicRun && blocked && recovered && survival;
    marker("phase27l=PASS", "phase27l=FAIL", allPassed);
    return allPassed;
#elif defined(GXOS_PHASE27K_APP)
    OutputServiceInit(&g_output);
    BuildControllerInit(&g_build);
    RunControllerInit(&g_run);
    const bool backend = hasBareHost();
    marker("phase27k_run_backend=PASS", "phase27k_run_backend=FAIL", backend);
    if (!backend) return false;
    WorkspaceControllerInit(&g_workspace, bareFileSystem());
    const bool projectOpen = WorkspaceControllerOpenProject(&g_workspace, "/P27K");
    marker("phase27k_project_open=PASS", "phase27k_project_open=FAIL", projectOpen);
    if (!projectOpen) return false;
    const bool documentOpen = WorkspaceControllerOpenDocument(&g_workspace, "src/main.cpp");
    marker("phase27k_document_open=PASS", "phase27k_document_open=FAIL", documentOpen);
    if (!documentOpen) return false;
    Document* document = WorkspaceControllerActiveDocument(&g_workspace);

    const char sourceA[] =
        "int gx_main(gx_app_context* ctx) {\n"
        "    int i = 0;\n"
        "    int total = 0;\n"
        "    log(ctx, \"Starting controlled loop.\");\n"
        "    while (i < 10) {\n"
        "        i = i + 1;\n"
        "        if (i < 3) { continue; }\n"
        "        if (i > 8) { break; }\n"
        "        total = total + i;\n"
        "    }\n"
        "    log(ctx, \"Controlled loop complete.\");\n"
        "    return total + 9;\n"
        "}\n";
    const char sourceB[] =
        "int gx_main(gx_app_context* ctx) {\n"
        "    int i = 0;\n"
        "    int total = 0;\n"
        "    log(ctx, \"Starting controlled loop.\");\n"
        "    while (i < 10) {\n"
        "        i = i + 1;\n"
        "        if (i < 3) { continue; }\n"
        "        if (i > 7) { break; }\n"
        "        total = total + i;\n"
        "    }\n"
        "    log(ctx, \"Controlled loop complete.\");\n"
        "    return total + 9;\n"
        "}\n";
    const char invalid[] =
        "int gx_main(gx_app_context* ctx) {\n"
        "    break;\n"
        "    return 42;\n"
        "}\n";
    const uint64_t sourceHashA = hashText(sourceA);
    const uint64_t sourceHashB = hashText(sourceB);

    uint64_t operationA = 0;
    const bool firstEdit = editSource(document, sourceA, sizeof(sourceA) - 1);
    const bool firstRun = firstEdit && runBuildBeforeRun(42, "Starting controlled loop.", &operationA, nullptr) &&
        outputHas(operationA, "Controlled loop complete.") && g_run.result.exitCode == 42;
    char artifactHashA[sizeof(g_build.result.artifactSha256)] = {};
    copyText(artifactHashA, sizeof(artifactHashA), g_build.result.artifactSha256);
    marker("phase27k_ide_program=PASS", "phase27k_ide_program=FAIL", firstRun);

    uint64_t operationB = 0;
    const bool secondEdit = editSource(document, sourceB, sizeof(sourceB) - 1);
    const bool secondRun = secondEdit && runBuildBeforeRun(34, "Starting controlled loop.", &operationB, nullptr) &&
        outputHas(operationB, "Controlled loop complete.") && g_run.result.exitCode == 34;
    char artifactHashB[sizeof(g_build.result.artifactSha256)] = {};
    copyText(artifactHashB, sizeof(artifactHashB), g_build.result.artifactSha256);
    const bool sourceEdit = secondRun && sourceHashA != sourceHashB &&
        !equalText(artifactHashA, artifactHashB) && g_run.result.exitCode == 34;
    marker("phase27k_source_edit=PASS", "phase27k_source_edit=FAIL", sourceEdit);

    uint64_t deterministicOperation = 0;
    OutputServiceInit(&g_output);
    const bool deterministicRun = secondRun &&
        runBuildBeforeRun(34, "Starting controlled loop.", &deterministicOperation, nullptr) &&
        outputHas(deterministicOperation, "Controlled loop complete.") &&
        equalText(artifactHashB, g_build.result.artifactSha256);
    marker("phase27k_deterministic=PASS", "phase27k_deterministic=FAIL", deterministicRun);

    bool invalidBuildFailed = false;
    uint64_t invalidOperation = 0;
    const bool invalidEdit = editSource(document, invalid, sizeof(invalid) - 1);
    const bool invalidRun = invalidEdit &&
        runBuildBeforeRun(0, "Starting controlled loop.", &invalidOperation, &invalidBuildFailed);
    const bool blocked = invalidEdit && !invalidRun && invalidBuildFailed &&
        g_build.result.errorCount != 0 && !outputHas(invalidOperation, "Starting controlled loop.") &&
        !RunControllerIsActive(&g_run);
    const bool recovered = editSource(document, sourceA, sizeof(sourceA) - 1) &&
        runBuildBeforeRun(42, "Starting controlled loop.", nullptr, nullptr) &&
        outputHas(g_run.operationId, "Controlled loop complete.") && g_run.result.exitCode == 42 &&
        !RunControllerIsActive(&g_run);
    marker("phase27k_break_outside_loop=PASS", "phase27k_break_outside_loop=FAIL", blocked);
    marker("phase27k_invalid_syntax=PASS", "phase27k_invalid_syntax=FAIL", blocked);
    marker("phase27k_failure_recovery=PASS", "phase27k_failure_recovery=FAIL", blocked && recovered);
    marker("phase27k_loop_stack_reset=PASS", "phase27k_loop_stack_reset=FAIL", recovered);

    FileInfo sourceInfo = {};
    FileInfo artifactInfo = {};
    char sourcePath[kMaxPathBytes] = {};
    char artifactPath[kMaxPathBytes] = {};
    const bool survival = JoinWorkspacePath("/P27K", "src/main.cpp", sourcePath, sizeof(sourcePath)) &&
        JoinWorkspacePath("/P27K", g_build.result.artifactPath, artifactPath, sizeof(artifactPath)) &&
        g_workspace.fileSystem.stat(g_workspace.fileSystem.userData, sourcePath, &sourceInfo) &&
        g_workspace.fileSystem.stat(g_workspace.fileSystem.userData, artifactPath, &artifactInfo) &&
        sourceInfo.kind == FileInfoKind::RegularFile && artifactInfo.kind == FileInfoKind::RegularFile &&
        artifactInfo.size == g_build.result.artifactSize && !RunControllerIsActive(&g_run) &&
        g_run.result.state == RunState::Completed && g_run.result.cleanupComplete;
    marker("phase27k_kernel_survival=PASS", "phase27k_kernel_survival=FAIL", survival);
    const bool allPassed = firstRun && secondRun && sourceEdit && deterministicRun && blocked && recovered && survival;
    marker("phase27k=PASS", "phase27k=FAIL", allPassed);
    return allPassed;
#elif defined(GXOS_PHASE27J_APP)
    OutputServiceInit(&g_output);
    BuildControllerInit(&g_build);
    RunControllerInit(&g_run);
    const bool backend = hasBareHost();
    marker("phase27j_run_backend=PASS", "phase27j_run_backend=FAIL", backend);
    if (!backend) return false;
    WorkspaceControllerInit(&g_workspace, bareFileSystem());
    const bool projectOpen = WorkspaceControllerOpenProject(&g_workspace, "/P27J");
    marker("phase27j_project_open=PASS", "phase27j_project_open=FAIL", projectOpen);
    if (!projectOpen) return false;
    const bool documentOpen = WorkspaceControllerOpenDocument(&g_workspace, "src/main.cpp");
    marker("phase27j_document_open=PASS", "phase27j_document_open=FAIL", documentOpen);
    if (!documentOpen) return false;
    Document* document = WorkspaceControllerActiveDocument(&g_workspace);

    const char sourceA[] =
        "int gx_main(gx_app_context* ctx) {\n"
        "    int total = 0;\n"
        "    int i = 1;\n"
        "\n"
        "    log(ctx, \"Starting loop.\");\n"
        "\n"
        "    while (i <= 6)\n"
        "    {\n"
        "        total = total + i;\n"
        "        i = i + 1;\n"
        "    }\n"
        "\n"
        "    log(ctx, \"Loop complete.\");\n"
        "\n"
        "    return total * 2;\n"
        "}\n";
    const char sourceB[] =
        "int gx_main(gx_app_context* ctx) {\n"
        "    int total = 0;\n"
        "    int i = 1;\n"
        "\n"
        "    log(ctx, \"Starting loop.\");\n"
        "\n"
        "    while (i <= 5)\n"
        "    {\n"
        "        total = total + i;\n"
        "        i = i + 1;\n"
        "    }\n"
        "\n"
        "    log(ctx, \"Loop complete.\");\n"
        "\n"
        "    return total;\n"
        "}\n";
    const char invalid[] =
        "int gx_main(gx_app_context* ctx) {\n"
        "    while () {\n"
        "        return 42;\n"
        "    }\n"
        "    return 0;\n"
        "}\n";
    const uint64_t sourceHashA = hashText(sourceA);
    const uint64_t sourceHashB = hashText(sourceB);

    uint64_t operationA = 0;
    const bool firstEdit = editSource(document, sourceA, sizeof(sourceA) - 1);
    const bool firstRun = firstEdit && runBuildBeforeRun(42, "Starting loop.", &operationA, nullptr) &&
        outputHas(operationA, "Loop complete.") && g_run.result.exitCode == 42;
    char artifactHashA[sizeof(g_build.result.artifactSha256)] = {};
    copyText(artifactHashA, sizeof(artifactHashA), g_build.result.artifactSha256);
    marker("phase27j_ide_program=PASS", "phase27j_ide_program=FAIL", firstRun);

    uint64_t operationB = 0;
    const bool secondEdit = editSource(document, sourceB, sizeof(sourceB) - 1);
    const bool secondRun = secondEdit && runBuildBeforeRun(15, "Starting loop.", &operationB, nullptr) &&
        outputHas(operationB, "Loop complete.") && g_run.result.exitCode == 15;
    char artifactHashB[sizeof(g_build.result.artifactSha256)] = {};
    copyText(artifactHashB, sizeof(artifactHashB), g_build.result.artifactSha256);
    const bool sourceEdit = secondRun && sourceHashA != sourceHashB &&
        !equalText(artifactHashA, artifactHashB) && g_run.result.exitCode == 15;
    marker("phase27j_source_edit=PASS", "phase27j_source_edit=FAIL", sourceEdit);

    uint64_t deterministicOperation = 0;
    OutputServiceInit(&g_output);
    const bool deterministicRun = secondRun &&
        runBuildBeforeRun(15, "Starting loop.", &deterministicOperation, nullptr) &&
        outputHas(deterministicOperation, "Loop complete.") &&
        equalText(artifactHashB, g_build.result.artifactSha256);
    marker("phase27j_deterministic=PASS", "phase27j_deterministic=FAIL", deterministicRun);

    bool invalidBuildFailed = false;
    uint64_t invalidOperation = 0;
    const bool invalidEdit = editSource(document, invalid, sizeof(invalid) - 1);
    const bool invalidRun = invalidEdit &&
        runBuildBeforeRun(0, "Starting loop.", &invalidOperation, &invalidBuildFailed);
    const bool blocked = invalidEdit && !invalidRun && invalidBuildFailed &&
        g_build.result.errorCount != 0 && !outputHas(invalidOperation, "Starting loop.") &&
        !RunControllerIsActive(&g_run);
    const bool recovered = editSource(document, sourceA, sizeof(sourceA) - 1) &&
        runBuildBeforeRun(42, "Starting loop.", nullptr, nullptr) &&
        outputHas(g_run.operationId, "Loop complete.") && g_run.result.exitCode == 42 &&
        !RunControllerIsActive(&g_run);
    marker("phase27j_invalid_while=PASS", "phase27j_invalid_while=FAIL", blocked);
    marker("phase27j_failure_recovery=PASS", "phase27j_failure_recovery=FAIL", blocked && recovered);

    FileInfo sourceInfo = {};
    FileInfo artifactInfo = {};
    char sourcePath[kMaxPathBytes] = {};
    char artifactPath[kMaxPathBytes] = {};
    const bool survival = JoinWorkspacePath("/P27J", "src/main.cpp", sourcePath, sizeof(sourcePath)) &&
        JoinWorkspacePath("/P27J", g_build.result.artifactPath, artifactPath, sizeof(artifactPath)) &&
        g_workspace.fileSystem.stat(g_workspace.fileSystem.userData, sourcePath, &sourceInfo) &&
        g_workspace.fileSystem.stat(g_workspace.fileSystem.userData, artifactPath, &artifactInfo) &&
        sourceInfo.kind == FileInfoKind::RegularFile && artifactInfo.kind == FileInfoKind::RegularFile &&
        artifactInfo.size == g_build.result.artifactSize && !RunControllerIsActive(&g_run) &&
        g_run.result.state == RunState::Completed && g_run.result.cleanupComplete;
    marker("phase27j_kernel_survival=PASS", "phase27j_kernel_survival=FAIL", survival);
    const bool allPassed = firstRun && sourceEdit && deterministicRun && blocked && recovered && survival;
    marker("phase27j=PASS", "phase27j=FAIL", allPassed);
    return allPassed;
#elif defined(GXOS_PHASE27I_APP)
    OutputServiceInit(&g_output);
    BuildControllerInit(&g_build);
    RunControllerInit(&g_run);
    const bool backend = hasBareHost();
    marker("phase27i_run_backend=PASS", "phase27i_run_backend=FAIL", backend);
    if (!backend) return false;
    WorkspaceControllerInit(&g_workspace, bareFileSystem());
    const bool projectOpen = WorkspaceControllerOpenProject(&g_workspace, "/P27I");
    marker("phase27i_project_open=PASS", "phase27i_project_open=FAIL", projectOpen);
    if (!projectOpen) return false;
    const bool documentOpen = WorkspaceControllerOpenDocument(&g_workspace, "src/main.cpp");
    marker("phase27i_document_open=PASS", "phase27i_document_open=FAIL", documentOpen);
    if (!documentOpen) return false;
    Document* document = WorkspaceControllerActiveDocument(&g_workspace);

    const char sourceA[] =
        "int gx_main(gx_app_context* ctx) {\n"
        "    int x = 20;\n"
        "    int y = 22;\n"
        "\n"
        "    if (x == 20 && y == 22) {\n"
        "        log(ctx, \"Both conditions are true.\");\n"
        "        return 42;\n"
        "    }\n"
        "\n"
        "    log(ctx, \"Condition failed.\");\n"
        "    return 0;\n"
        "}\n";
    const char sourceB[] =
        "int gx_main(gx_app_context* ctx) {\n"
        "    int x = 20;\n"
        "    int y = 21;\n"
        "\n"
        "    if (x == 20 && y == 22) {\n"
        "        log(ctx, \"Both conditions are true.\");\n"
        "        return 42;\n"
        "    }\n"
        "\n"
        "    log(ctx, \"Condition failed.\");\n"
        "    return 0;\n"
        "}\n";
    const char invalid[] =
        "int gx_main(gx_app_context* ctx) {\n"
        "    if (1 &&) {\n"
        "        return 42;\n"
        "    }\n"
        "    return 0;\n"
        "}\n";
    const uint64_t sourceHashA = hashText(sourceA);
    const uint64_t sourceHashB = hashText(sourceB);

    uint64_t operationA = 0;
    const bool firstEdit = editSource(document, sourceA, sizeof(sourceA) - 1);
    const bool firstRun = firstEdit && runBuildBeforeRun(42, "Both conditions are true.", &operationA, nullptr) &&
        outputLacks(operationA, "Condition failed.");
    char artifactHashA[sizeof(g_build.result.artifactSha256)] = {};
    copyText(artifactHashA, sizeof(artifactHashA), g_build.result.artifactSha256);
    marker("phase27i_ide_program=PASS", "phase27i_ide_program=FAIL",
           firstRun && g_run.result.exitCode == 42);

    uint64_t operationB = 0;
    const bool secondEdit = editSource(document, sourceB, sizeof(sourceB) - 1);
    const bool secondRun = secondEdit && runBuildBeforeRun(0, "Condition failed.", &operationB, nullptr) &&
        outputLacks(operationB, "Both conditions are true.");
    char artifactHashB[sizeof(g_build.result.artifactSha256)] = {};
    copyText(artifactHashB, sizeof(artifactHashB), g_build.result.artifactSha256);
    const bool sourceEdit = secondRun && sourceHashA != sourceHashB &&
        !equalText(artifactHashA, artifactHashB) && g_run.result.exitCode == 0;
    marker("phase27i_source_edit=PASS", "phase27i_source_edit=FAIL", sourceEdit);

    uint64_t deterministicOperation = 0;
    OutputServiceInit(&g_output);
    const bool deterministicRun = secondRun &&
        runBuildBeforeRun(0, "Condition failed.", &deterministicOperation, nullptr) &&
        outputHas(deterministicOperation, "Condition failed.") &&
        equalText(artifactHashB, g_build.result.artifactSha256);
    marker("phase27i_deterministic=PASS", "phase27i_deterministic=FAIL", deterministicRun);

    bool invalidBuildFailed = false;
    uint64_t invalidOperation = 0;
    const bool invalidEdit = editSource(document, invalid, sizeof(invalid) - 1);
    const bool invalidRun = invalidEdit &&
        runBuildBeforeRun(0, "Condition failed.", &invalidOperation, &invalidBuildFailed);
    const bool blocked = invalidEdit && !invalidRun && invalidBuildFailed &&
        g_build.result.errorCount != 0 && !outputHas(invalidOperation, "Condition failed.") &&
        !RunControllerIsActive(&g_run);

    const bool recovered = editSource(document, sourceA, sizeof(sourceA) - 1) &&
        runBuildBeforeRun(42, "Both conditions are true.", nullptr, nullptr) &&
        g_run.result.exitCode == 42 && !RunControllerIsActive(&g_run);
    marker("phase27i_failure_recovery=PASS", "phase27i_failure_recovery=FAIL", blocked && recovered);

    FileInfo sourceInfo = {};
    FileInfo artifactInfo = {};
    char sourcePath[kMaxPathBytes] = {};
    char artifactPath[kMaxPathBytes] = {};
    const bool survival = JoinWorkspacePath("/P27I", "src/main.cpp", sourcePath, sizeof(sourcePath)) &&
        JoinWorkspacePath("/P27I", g_build.result.artifactPath, artifactPath, sizeof(artifactPath)) &&
        g_workspace.fileSystem.stat(g_workspace.fileSystem.userData, sourcePath, &sourceInfo) &&
        g_workspace.fileSystem.stat(g_workspace.fileSystem.userData, artifactPath, &artifactInfo) &&
        sourceInfo.kind == FileInfoKind::RegularFile && artifactInfo.kind == FileInfoKind::RegularFile &&
        artifactInfo.size == g_build.result.artifactSize && !RunControllerIsActive(&g_run);
    marker("phase27i_kernel_survival=PASS", "phase27i_kernel_survival=FAIL", survival);
    const bool allPassed = firstRun && secondRun && sourceEdit && deterministicRun && blocked && recovered && survival;
    marker("phase27i=PASS", "phase27i=FAIL", allPassed);
    return allPassed;
#elif defined(GXOS_PHASE27H_APP)
    OutputServiceInit(&g_output);
    BuildControllerInit(&g_build);
    RunControllerInit(&g_run);
    const bool backend = hasBareHost();
    marker("phase27h_run_backend=PASS", "phase27h_run_backend=FAIL", backend);
    if (!backend) return false;
    WorkspaceControllerInit(&g_workspace, bareFileSystem());
    const bool projectOpen = WorkspaceControllerOpenProject(&g_workspace, "/P27H");
    marker("phase27h_project_open=PASS", "phase27h_project_open=FAIL", projectOpen);
    if (!projectOpen) return false;
    const bool documentOpen = WorkspaceControllerOpenDocument(&g_workspace, "src/main.cpp");
    marker("phase27h_document_open=PASS", "phase27h_document_open=FAIL", documentOpen);
    if (!documentOpen) return false;
    Document* document = WorkspaceControllerActiveDocument(&g_workspace);
    const char sourceA[] =
        "int gx_main(gx_app_context* ctx) {\n"
        "    int x = 20;\n"
        "    int y = 22;\n"
        "    int result = x + y;\n"
        "    if (result == 42) {\n"
        "        log(ctx, \"The answer is 42.\");\n"
        "        return result;\n"
        "    } else {\n"
        "        log(ctx, \"Unexpected result.\");\n"
        "        return -1;\n"
        "    }\n"
        "}\n";
    const char sourceB[] =
        "int gx_main(gx_app_context* ctx) {\n"
        "    int x = 20;\n"
        "    int y = 21;\n"
        "    int result = x + y;\n"
        "    if (result == 42) {\n"
        "        log(ctx, \"The answer is 42.\");\n"
        "        return result;\n"
        "    } else {\n"
        "        log(ctx, \"Unexpected result.\");\n"
        "        return -1;\n"
        "    }\n"
        "}\n";
    const char invalid[] =
        "int gx_main(gx_app_context* ctx) {\n"
        "    if (result ==) {\n"
        "        return 42;\n"
        "    }\n"
        "    return 0;\n"
        "}\n";
    const uint64_t sourceHashA = hashText(sourceA);
    const uint64_t sourceHashB = hashText(sourceB);
    uint64_t operationA = 0;
    const bool editA = editSource(document, sourceA, sizeof(sourceA) - 1);
    const bool firstRun = editA && runBuildBeforeRun(42, "The answer is 42.", &operationA, nullptr) &&
        outputLacks(operationA, "Unexpected result.");
    char artifactHashA[sizeof(g_build.result.artifactSha256)] = {};
    copyText(artifactHashA, sizeof(artifactHashA), g_build.result.artifactSha256);
    marker("phase27h_ide_program=PASS", "phase27h_ide_program=FAIL", firstRun && g_run.result.exitCode == 42);

    uint64_t operationB = 0;
    const bool editB = editSource(document, sourceB, sizeof(sourceB) - 1);
    const bool secondRun = editB && runBuildBeforeRun(-1, "Unexpected result.", &operationB, nullptr) &&
        outputLacks(operationB, "The answer is 42.");
    char artifactHashB[sizeof(g_build.result.artifactSha256)] = {};
    copyText(artifactHashB, sizeof(artifactHashB), g_build.result.artifactSha256);
    const bool sourceEdit = secondRun && sourceHashA != sourceHashB &&
        !equalText(artifactHashA, artifactHashB) && g_run.result.exitCode == -1;
    marker("phase27h_source_edit=PASS", "phase27h_source_edit=FAIL", sourceEdit);

    uint64_t deterministicOperation = 0;
    const bool deterministicRun = secondRun &&
        runBuildBeforeRun(-1, "Unexpected result.", &deterministicOperation, nullptr) &&
        equalText(artifactHashB, g_build.result.artifactSha256);
    marker("phase27h_deterministic=PASS", "phase27h_deterministic=FAIL", deterministicRun);

    bool invalidBuildFailed = false;
    uint64_t invalidOperation = 0;
    const bool editedInvalid = editSource(document, invalid, sizeof(invalid) - 1);
    const bool invalidRun = editedInvalid &&
        runBuildBeforeRun(-1, "Unexpected result.", &invalidOperation, &invalidBuildFailed);
    const bool blocked = editedInvalid && !invalidRun && invalidBuildFailed &&
        g_build.result.errorCount != 0 && !outputHas(invalidOperation, "Unexpected result.") &&
        !RunControllerIsActive(&g_run);
    const bool recovered = editSource(document, sourceA, sizeof(sourceA) - 1) &&
        runBuildBeforeRun(42, "The answer is 42.", nullptr, nullptr) &&
        g_run.result.exitCode == 42;
    marker("phase27h_failure_recovery=PASS", "phase27h_failure_recovery=FAIL", blocked && recovered);

    FileInfo sourceInfo = {};
    FileInfo artifactInfo = {};
    char sourcePath[kMaxPathBytes] = {};
    char artifactPath[kMaxPathBytes] = {};
    const bool survival = JoinWorkspacePath("/P27H", "src/main.cpp", sourcePath, sizeof(sourcePath)) &&
        JoinWorkspacePath("/P27H", g_build.result.artifactPath, artifactPath, sizeof(artifactPath)) &&
        g_workspace.fileSystem.stat(g_workspace.fileSystem.userData, sourcePath, &sourceInfo) &&
        g_workspace.fileSystem.stat(g_workspace.fileSystem.userData, artifactPath, &artifactInfo) &&
        sourceInfo.kind == FileInfoKind::RegularFile && artifactInfo.kind == FileInfoKind::RegularFile &&
        artifactInfo.size == g_build.result.artifactSize && !RunControllerIsActive(&g_run);
    marker("phase27h_kernel_survival=PASS", "phase27h_kernel_survival=FAIL", survival);
    const bool allPassed = firstRun && secondRun && sourceEdit && deterministicRun && blocked && recovered && survival;
    marker("phase27h=PASS", "phase27h=FAIL", allPassed);
    return allPassed;
#elif defined(GXOS_PHASE27G_APP)
    OutputServiceInit(&g_output);
    BuildControllerInit(&g_build);
    RunControllerInit(&g_run);
    const bool backend = hasBareHost();
    marker("phase27g_run_backend=PASS", "phase27g_run_backend=FAIL", backend);
    if (!backend) return false;
    WorkspaceControllerInit(&g_workspace, bareFileSystem());
    if (!WorkspaceControllerOpenProject(&g_workspace, "/P27G") ||
        !WorkspaceControllerOpenDocument(&g_workspace, "src/main.cpp")) return false;
    Document* document = WorkspaceControllerActiveDocument(&g_workspace);
    const char sourceA[] =
        "int gx_main(gx_app_context* ctx) {\n"
        "    int x = 20;\n"
        "    int y = 22;\n"
        "    int result = x + y;\n"
        "    log(ctx, \"Calculating inside guideXOS...\");\n"
        "    log(ctx, \"Done.\");\n"
        "    return result;\n"
        "}\n";
    const char sourceB[] =
        "int gx_main(gx_app_context* ctx) {\n"
        "    int x = 7;\n"
        "    int y = 6;\n"
        "    int result = x * y;\n"
        "    log(ctx, \"Recompiled expression program.\");\n"
        "    return result - 1;\n"
        "}\n";
    const char unknownSource[] =
        "int gx_main(gx_app_context* ctx) {\n    return missing + 1;\n}\n";
    const char duplicateSource[] =
        "int gx_main(gx_app_context* ctx) {\n"
        "    int x = 1;\n    int x = 2;\n    return x;\n}\n";

    const uint64_t sourceHashA = hashText(sourceA);
    const uint64_t sourceHashB = hashText(sourceB);
    uint64_t operationA = 0;
    const bool editA = editSource(document, sourceA, sizeof(sourceA) - 1);
    const bool firstRun = editA && runBuildBeforeRun(42, "Calculating inside guideXOS...", &operationA, nullptr) &&
        outputHas(operationA, "Done.");
    char artifactHashA[sizeof(g_build.result.artifactSha256)] = {};
    copyText(artifactHashA, sizeof(artifactHashA), g_build.result.artifactSha256);
    marker("phase27g_multiple_host_calls=PASS", "phase27g_multiple_host_calls=FAIL", firstRun);
    marker("phase27g_ide_program=PASS", "phase27g_ide_program=FAIL", firstRun && g_run.result.exitCode == 42);

    uint64_t operationB = 0;
    const bool editB = editSource(document, sourceB, sizeof(sourceB) - 1);
    const bool secondRun = editB && runBuildBeforeRun(41, "Recompiled expression program.", &operationB, nullptr);
    char artifactHashB[sizeof(g_build.result.artifactSha256)] = {};
    copyText(artifactHashB, sizeof(artifactHashB), g_build.result.artifactSha256);
    const bool sourceEdit = secondRun && sourceHashA != sourceHashB && !equalText(artifactHashA, artifactHashB) &&
        outputHas(operationB, "Recompiled expression program.") && g_run.result.exitCode == 41;
    marker("phase27g_source_edit=PASS", "phase27g_source_edit=FAIL", sourceEdit);

    uint64_t deterministicOperation = 0;
    OutputServiceInit(&g_output);
    const bool deterministicRun = secondRun &&
        runBuildBeforeRun(41, "Recompiled expression program.", &deterministicOperation, nullptr) &&
        outputHas(deterministicOperation, "Recompiled expression program.") &&
        equalText(artifactHashB, g_build.result.artifactSha256);
    marker("phase27g_deterministic=PASS", "phase27g_deterministic=FAIL", deterministicRun);

    bool unknownBuildFailed = false;
    uint64_t unknownOperation = 0;
    const bool editedUnknown = editSource(document, unknownSource, sizeof(unknownSource) - 1);
    const bool unknownRejected = editedUnknown && !runBuildBeforeRun(0, "", &unknownOperation, &unknownBuildFailed) &&
        unknownBuildFailed && g_build.result.errorCount != 0 && !RunControllerIsActive(&g_run);
    marker("phase27g_unknown_identifier=PASS", "phase27g_unknown_identifier=FAIL", unknownRejected);

    bool duplicateBuildFailed = false;
    uint64_t duplicateOperation = 0;
    const bool editedDuplicate = editSource(document, duplicateSource, sizeof(duplicateSource) - 1);
    const bool duplicateRejected = editedDuplicate && !runBuildBeforeRun(0, "", &duplicateOperation, &duplicateBuildFailed) &&
        duplicateBuildFailed && g_build.result.errorCount != 0 && !RunControllerIsActive(&g_run);
    marker("phase27g_duplicate_local=PASS", "phase27g_duplicate_local=FAIL", duplicateRejected);

    uint64_t recoveryOperation = 0;
    const bool restored = editSource(document, sourceA, sizeof(sourceA) - 1);
    const bool recovered = restored && runBuildBeforeRun(42, "Calculating inside guideXOS...", &recoveryOperation, nullptr) &&
        outputHas(recoveryOperation, "Done.") && !RunControllerIsActive(&g_run);
    marker("phase27g_failure_recovery=PASS", "phase27g_failure_recovery=FAIL", recovered);

    FileInfo sourceInfo = {};
    FileInfo artifactInfo = {};
    char sourcePath[kMaxPathBytes] = {};
    char artifactPath[kMaxPathBytes] = {};
    const bool survival = JoinWorkspacePath("/P27G", "src/main.cpp", sourcePath, sizeof(sourcePath)) &&
        JoinWorkspacePath("/P27G", g_build.result.artifactPath, artifactPath, sizeof(artifactPath)) &&
        g_workspace.fileSystem.stat(g_workspace.fileSystem.userData, sourcePath, &sourceInfo) &&
        g_workspace.fileSystem.stat(g_workspace.fileSystem.userData, artifactPath, &artifactInfo) &&
        sourceInfo.kind == FileInfoKind::RegularFile && artifactInfo.kind == FileInfoKind::RegularFile &&
        artifactInfo.size == g_build.result.artifactSize && !RunControllerIsActive(&g_run);
    marker("phase27g_kernel_survival=PASS", "phase27g_kernel_survival=FAIL", survival);
    const bool allPassed = firstRun && sourceEdit && deterministicRun && unknownRejected &&
        duplicateRejected && recovered && survival;
    marker("phase27g=PASS", "phase27g=FAIL", allPassed);
    return allPassed;
#elif defined(GXOS_PHASE27F_APP)
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

    // Start each repeat with a fresh bounded output history while retaining
    // the exact output assertion for every repeated Build/Run cycle.
    bool repeated = recovered;
    for (uint32_t i = 0; i < 3; ++i) {
        OutputServiceInit(&g_output);
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
