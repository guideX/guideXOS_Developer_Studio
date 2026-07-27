#include "developer_studio_workspace.h"

namespace guidexos {
namespace developer_studio {
namespace {

static void setControllerError(WorkspaceController* controller, ModelErrorCode code) {
    if (!controller) return;
    controller->lastError = code;
    for (uint32_t i = 0; i + 1 < sizeof(controller->model.lastError); ++i) {
        controller->model.lastError[i] = ModelErrorName(code)[i];
        if (ModelErrorName(code)[i] == '\0') return;
    }
    controller->model.lastError[sizeof(controller->model.lastError) - 1] = '\0';
}

static void setProjectError(WorkspaceController* controller, ProjectErrorCode code) {
    if (!controller) return;
    controller->lastProjectError = code;
}

static bool pathForBrowse(const WorkspaceModel& model, char* output, uint32_t outputSize) {
    if (model.browsePath[0] == '\0') return NormalizePath(model.rootPath, output, outputSize);
    return JoinWorkspacePath(model.rootPath, model.browsePath, output, outputSize);
}

static bool absolutePathForDocument(const WorkspaceModel& model, const char* path, char* output, uint32_t outputSize) {
    if (!path || path[0] == '\0') return false;
    if (path[0] == '/' || path[0] == static_cast<char>(92) || (path[1] == ':')) {
        return NormalizePath(path, output, outputSize);
    }
    return JoinWorkspacePath(model.rootPath, path, output, outputSize);
}

static bool copyEntryName(char* output, uint32_t outputSize, const char* input) {
    if (!output || !input || outputSize == 0) return false;
    uint32_t i = 0;
    while (i + 1 < outputSize && input[i] != '\0') { output[i] = input[i]; ++i; }
    if (input[i] != '\0') { output[0] = '\0'; return false; }
    output[i] = '\0';
    return true;
}

static bool isWithinRoot(const WorkspaceModel& model, const char* path) {
    char probe[kMaxPathBytes];
    if (!NormalizePath(path, probe, sizeof(probe))) return false;
    uint32_t rootLength = 0;
    while (rootLength < kMaxPathBytes && model.rootPath[rootLength] != '\0') ++rootLength;
    uint32_t pathLength = 0;
    while (pathLength < kMaxPathBytes && probe[pathLength] != '\0') ++pathLength;
    if (rootLength > pathLength) return false;
    for (uint32_t i = 0; i < rootLength; ++i) {
        char a = model.rootPath[i];
        char b = probe[i];
        if (a >= 'A' && a <= 'Z') a = static_cast<char>(a + ('a' - 'A'));
        if (b >= 'A' && b <= 'Z') b = static_cast<char>(b + ('a' - 'A'));
        if (a != b) return false;
    }
    return pathLength == rootLength || probe[rootLength] == '/';
}

static bool equalIdentifier(const char* left, const char* right) {
    if (!left || !right) return left == right;
    uint32_t index = 0;
    while (left[index] != '\0' && right[index] != '\0') {
        if (left[index] != right[index]) return false;
        ++index;
    }
    return left[index] == right[index];
}

} // namespace

void WorkspaceControllerInit(WorkspaceController* controller, const WorkspaceFileSystem& fileSystem) {
    if (!controller) return;
    WorkspaceModelInit(&controller->model);
    controller->fileSystem = fileSystem;
    controller->listingTruncated = false;
    controller->lastError = ModelErrorCode::None;
    controller->lastProjectError = ProjectErrorCode::None;
}

bool WorkspaceControllerOpenWorkspace(WorkspaceController* controller, const char* path) {
    if (!controller || !controller->fileSystem.stat || !controller->fileSystem.list) return false;
    if (controller->model.open && WorkspaceModelHasDirtyDocuments(&controller->model)) {
        setControllerError(controller, ModelErrorCode::UnsavedChanges);
        return false;
    }
    char normalized[kMaxPathBytes];
    if (!NormalizePath(path, normalized, sizeof(normalized))) { setControllerError(controller, ModelErrorCode::InvalidPath); return false; }
    FileInfo info;
    if (!controller->fileSystem.stat(controller->fileSystem.userData, normalized, &info)) { setControllerError(controller, ModelErrorCode::ReadFailed); return false; }
    if (info.kind != FileInfoKind::Directory) { setControllerError(controller, ModelErrorCode::NotDirectory); return false; }
    const char* name = BaseName(normalized);
    if (!WorkspaceModelSetRoot(&controller->model, normalized, name[0] ? name : normalized)) { setControllerError(controller, ModelErrorCode::InvalidPath); return false; }
    if (!WorkspaceControllerRefresh(controller)) return false;
    return true;
}

bool WorkspaceControllerOpenProject(WorkspaceController* controller, const char* path) {
    if (!controller || !controller->fileSystem.stat || !controller->fileSystem.read || !controller->fileSystem.list) {
        if (controller) setProjectError(controller, ProjectErrorCode::NullInput);
        return false;
    }
    if (controller->model.open && WorkspaceModelHasDirtyDocuments(&controller->model)) {
        setControllerError(controller, ModelErrorCode::UnsavedChanges);
        setProjectError(controller, ProjectErrorCode::UnsavedChanges);
        return false;
    }
    ProjectOperationResult result;
    if (!LoadProject(controller->fileSystem, path, &result)) {
        setProjectError(controller, result.error);
        return false;
    }
    if (!WorkspaceModelSetRoot(&controller->model, result.project.rootPath, result.project.displayName)) {
        setProjectError(controller, ProjectErrorCode::InvalidParentPath);
        return false;
    }
    controller->model.hasProject = true;
    controller->model.project = result.project;
    controller->lastProjectError = ProjectErrorCode::None;
    if (!WorkspaceControllerRefresh(controller)) {
        setProjectError(controller, ProjectErrorCode::RequiredFileMissing);
        return false;
    }
    return true;
}

bool WorkspaceControllerCreateProject(WorkspaceController* controller, const ProjectCreateRequest& request, ProjectOperationResult* result) {
    if (!controller || !result) return false;
    if (controller->model.open && WorkspaceModelHasDirtyDocuments(&controller->model)) {
        result->success = false;
        result->error = ProjectErrorCode::UnsavedChanges;
        result->rollbackAttempted = false;
        result->rollbackSucceeded = true;
        setProjectError(controller, result->error);
        return false;
    }
    if (controller->model.hasProject) {
        uint32_t i = 0;
        while (request.projectId[i] != '\0' && controller->model.project.projectId[i] != '\0' && request.projectId[i] == controller->model.project.projectId[i]) ++i;
        if (request.projectId[i] == '\0' && controller->model.project.projectId[i] == '\0') {
            result->success = false;
            result->error = ProjectErrorCode::ProjectIdCollision;
            result->rollbackAttempted = false;
            result->rollbackSucceeded = true;
            setProjectError(controller, result->error);
            return false;
        }
    }
    if (!CreateNativeGuiProject(controller->fileSystem, request, result)) {
        setProjectError(controller, result->error);
        return false;
    }
    setProjectError(controller, ProjectErrorCode::None);
    return true;
}

bool WorkspaceControllerReloadProject(WorkspaceController* controller) {
    if (!controller || !controller->model.open || !controller->model.hasProject) {
        if (controller) setProjectError(controller, ProjectErrorCode::RequiredFileMissing);
        return false;
    }
    ProjectOperationResult result;
    if (!LoadProject(controller->fileSystem, controller->model.rootPath, &result)) {
        setProjectError(controller, result.error);
        return false;
    }
    controller->model.project = result.project;
    controller->model.hasProject = true;
    WorkspaceModelAdvanceProjectGeneration(&controller->model);
    controller->lastProjectError = ProjectErrorCode::None;
    return true;
}

bool WorkspaceControllerRefresh(WorkspaceController* controller) {
    if (!controller || !controller->model.open || !controller->fileSystem.list) { if (controller) setControllerError(controller, ModelErrorCode::WorkspaceNotOpen); return false; }
    char directory[kMaxPathBytes];
    if (!pathForBrowse(controller->model, directory, sizeof(directory))) { setControllerError(controller, ModelErrorCode::InvalidPath); return false; }
    FileListEntry entries[kMaxWorkspaceEntries];
    bool truncated = false;
    uint32_t count = 0;
    if (!controller->fileSystem.list(controller->fileSystem.userData, directory, entries, kMaxWorkspaceEntries, &count, &truncated)) {
        setControllerError(controller, ModelErrorCode::ReadFailed);
        return false;
    }
    WorkspaceModelClearEntries(&controller->model);
    controller->listingTruncated = truncated;
    for (uint32_t i = 0; i < count && i < kMaxWorkspaceEntries; ++i) {
        if (entries[i].name[0] == '\0' || PathContainsTraversal(entries[i].name)) continue;
        WorkspaceEntry entry;
        for (uint32_t j = 0; j < sizeof(entry.name); ++j) entry.name[j] = '\0';
        for (uint32_t j = 0; j < sizeof(entry.relativePath); ++j) entry.relativePath[j] = '\0';
        if (!copyEntryName(entry.name, sizeof(entry.name), entries[i].name)) continue;
        uint32_t browseLength = 0;
        while (browseLength + 1 < sizeof(controller->model.browsePath) && controller->model.browsePath[browseLength] != '\0') ++browseLength;
        uint32_t nameLength = 0;
        while (nameLength + 1 < sizeof(entry.name) && entry.name[nameLength] != '\0') ++nameLength;
        char relative[kMaxPathBytes];
        uint32_t out = 0;
        for (uint32_t j = 0; j < browseLength && out + 1 < sizeof(relative); ++j) relative[out++] = controller->model.browsePath[j];
        if (browseLength > 0 && out + 1 < sizeof(relative)) relative[out++] = '/';
        for (uint32_t j = 0; j < nameLength && out + 1 < sizeof(relative); ++j) relative[out++] = entry.name[j];
        relative[out] = '\0';
        if (!copyEntryName(entry.relativePath, sizeof(entry.relativePath), relative)) continue;
        entry.size = entries[i].size;
        entry.depth = 0;
        for (uint32_t j = 0; relative[j] != '\0'; ++j) if (relative[j] == '/') ++entry.depth;
        entry.kind = entries[i].kind == FileInfoKind::Directory ? WorkspaceEntryKind::Directory :
            (IsSupportedTextPath(entry.name) ? WorkspaceEntryKind::SupportedTextFile : WorkspaceEntryKind::UnsupportedFile);
        WorkspaceModelAddEntry(&controller->model, entry);
    }
    WorkspaceModelSortEntries(&controller->model);
    controller->model.selectedEntry = controller->model.entryCount == 0 ? 0 :
        (controller->model.selectedEntry < controller->model.entryCount ? controller->model.selectedEntry : 0);
    setControllerError(controller, ModelErrorCode::None);
    return true;
}

bool WorkspaceControllerEnterSelected(WorkspaceController* controller) {
    if (!controller || !controller->model.open || controller->model.selectedEntry >= controller->model.entryCount) return false;
    WorkspaceEntry& entry = controller->model.entries[controller->model.selectedEntry];
    if (entry.kind == WorkspaceEntryKind::Directory) {
        if (!WorkspaceModelSetBrowsePath(&controller->model, entry.relativePath)) {
            setControllerError(controller, ModelErrorCode::InvalidPath);
            return false;
        }
        return WorkspaceControllerRefresh(controller);
    }
    return WorkspaceControllerOpenDocument(controller, entry.relativePath);
}

bool WorkspaceControllerGoUp(WorkspaceController* controller) {
    if (!controller || !controller->model.open) return false;
    uint32_t length = 0;
    while (controller->model.browsePath[length] != '\0' && length + 1 < sizeof(controller->model.browsePath)) ++length;
    if (length == 0) return false;
    while (length > 0 && controller->model.browsePath[length - 1] != '/') --length;
    if (length > 0) --length;
    controller->model.browsePath[length] = '\0';
    return WorkspaceControllerRefresh(controller);
}

bool WorkspaceControllerOpenDocument(WorkspaceController* controller, const char* path) {
    if (!controller || !controller->model.open || !controller->fileSystem.stat || !controller->fileSystem.read) { if (controller) setControllerError(controller, ModelErrorCode::WorkspaceNotOpen); return false; }
    char normalized[kMaxPathBytes];
    if (!absolutePathForDocument(controller->model, path, normalized, sizeof(normalized)) || !isWithinRoot(controller->model, normalized)) { setControllerError(controller, ModelErrorCode::OutsideWorkspace); return false; }
    FileInfo info;
    if (!controller->fileSystem.stat(controller->fileSystem.userData, normalized, &info)) { setControllerError(controller, ModelErrorCode::ReadFailed); return false; }
    if (info.kind != FileInfoKind::RegularFile) { setControllerError(controller, ModelErrorCode::NotFile); return false; }
    if (!IsSupportedTextPath(normalized)) { setControllerError(controller, ModelErrorCode::UnsupportedFile); return false; }
    if (info.size > kMaxEditorBytes) { setControllerError(controller, ModelErrorCode::FileTooLarge); return false; }
    char bytes[kMaxEditorBytes + 1];
    uint32_t count = 0;
    if (!controller->fileSystem.read(controller->fileSystem.userData, normalized, bytes, kMaxEditorBytes, &count)) { setControllerError(controller, ModelErrorCode::ReadFailed); return false; }
    if (count > kMaxEditorBytes || LooksBinary(bytes, count)) { setControllerError(controller, count > kMaxEditorBytes ? ModelErrorCode::FileTooLarge : ModelErrorCode::BinaryFile); return false; }
    ModelErrorCode error = ModelErrorCode::None;
    bool duplicate = false;
    if (!WorkspaceModelAddDocument(&controller->model, normalized, bytes, count, &error, &duplicate)) { setControllerError(controller, error); return false; }
    controller->lastError = duplicate ? ModelErrorCode::DuplicateDocument : ModelErrorCode::None;
    return true;
}

bool WorkspaceControllerSetCaretPosition(WorkspaceController* controller, uint32_t documentIndex, uint32_t line, uint32_t column,
                                          bool* outLocationClamped, OutputErrorCode* error) {
    if (outLocationClamped) *outLocationClamped = false;
    if (error) *error = OutputErrorCode::None;
    if (!controller || documentIndex >= kMaxOpenDocuments || !controller->model.documents[documentIndex].used) {
        if (error) *error = OutputErrorCode::NavigationOpenFailed;
        return false;
    }
    TextBuffer& buffer = controller->model.documents[documentIndex].buffer;
    const uint32_t count = TextBufferLineCount(&buffer);
    uint32_t zeroLine = line > 0 ? line - 1 : 0;
    if (zeroLine >= count) {
        zeroLine = count == 0 ? 0 : count - 1;
        if (outLocationClamped) *outLocationClamped = true;
    }
    const uint32_t start = TextBufferLineStart(&buffer, zeroLine);
    const uint32_t end = TextBufferLineEnd(&buffer, zeroLine);
    const uint32_t lineBytes = end >= start ? end - start : 0;
    uint32_t zeroColumn = column > 0 ? column - 1 : 0;
    if (zeroColumn > lineBytes) {
        zeroColumn = lineBytes;
        if (outLocationClamped) *outLocationClamped = true;
    }
    buffer.caret = start + zeroColumn;
    if (outLocationClamped && *outLocationClamped && error) *error = OutputErrorCode::NavigationLocationClamped;
    return true;
}

bool WorkspaceControllerOpenDocumentAtLocation(WorkspaceController* controller, const char* projectId, const char* relativePath,
                                               uint32_t line, uint32_t column, uint32_t* outDocumentIndex, OutputErrorCode* error) {
    if (outDocumentIndex) *outDocumentIndex = kMaxOpenDocuments;
    if (error) *error = OutputErrorCode::None;
    if (!controller || !controller->model.open || !controller->model.hasProject) {
        if (error) *error = OutputErrorCode::NavigationNoProject;
        return false;
    }
    if (!projectId || !equalIdentifier(projectId, controller->model.project.projectId)) {
        if (error) *error = OutputErrorCode::DiagnosticProjectMismatch;
        return false;
    }
    if (!relativePath || relativePath[0] == '\0' || PathContainsTraversal(relativePath) || relativePath[0] == '/' || relativePath[0] == '\\' || relativePath[1] == ':') {
        if (error) *error = OutputErrorCode::DiagnosticPathOutsideProject;
        return false;
    }
    char absolute[kMaxPathBytes] = {};
    if (!JoinWorkspacePath(controller->model.rootPath, relativePath, absolute, sizeof(absolute))) {
        if (error) *error = OutputErrorCode::DiagnosticPathOutsideProject;
        return false;
    }
    if (!WorkspaceControllerOpenDocument(controller, relativePath)) {
        if (error) *error = OutputErrorCode::DiagnosticFileNotFound;
        return false;
    }
    const int index = FindOpenDocument(&controller->model, absolute);
    if (index < 0) { if (error) *error = OutputErrorCode::NavigationOpenFailed; return false; }
    controller->model.activeDocument = static_cast<uint32_t>(index);
    bool clamped = false;
    if (!WorkspaceControllerSetCaretPosition(controller, static_cast<uint32_t>(index), line, column, &clamped, error)) return false;
    if (outDocumentIndex) *outDocumentIndex = static_cast<uint32_t>(index);
    return true;
}

bool WorkspaceControllerSaveDocument(WorkspaceController* controller, uint32_t documentIndex) {
    if (!controller || documentIndex >= kMaxOpenDocuments || !controller->model.documents[documentIndex].used || !controller->fileSystem.write) { if (controller) setControllerError(controller, ModelErrorCode::DocumentNotFound); return false; }
    Document& document = controller->model.documents[documentIndex];
    uint32_t written = 0;
    bool ok = controller->fileSystem.write(controller->fileSystem.userData, document.path, document.buffer.data, document.buffer.length, &written) && written == document.buffer.length;
    ModelErrorCode error = ModelErrorCode::None;
    if (!WorkspaceModelMarkSaved(&controller->model, documentIndex, ok, &error)) { setControllerError(controller, error); return false; }
    setControllerError(controller, ModelErrorCode::None);
    return true;
}

bool WorkspaceControllerSaveActive(WorkspaceController* controller) {
    if (!controller || controller->model.activeDocument >= kMaxOpenDocuments) { if (controller) setControllerError(controller, ModelErrorCode::DocumentNotFound); return false; }
    return WorkspaceControllerSaveDocument(controller, controller->model.activeDocument);
}

bool WorkspaceControllerSaveAll(WorkspaceController* controller) {
    if (!controller || !controller->model.open) { if (controller) setControllerError(controller, ModelErrorCode::WorkspaceNotOpen); return false; }
    for (uint32_t i = 0; i < kMaxOpenDocuments; ++i) {
        if (controller->model.documents[i].used && controller->model.documents[i].buffer.dirty && !WorkspaceControllerSaveDocument(controller, i)) return false;
    }
    return true;
}

bool WorkspaceControllerHasDirtyProjectDocuments(const WorkspaceController* controller) {
    if (!controller || !controller->model.open || !controller->model.hasProject) return false;
    for (uint32_t i = 0; i < kMaxOpenDocuments; ++i) {
        if (!controller->model.documents[i].used || !controller->model.documents[i].buffer.dirty) continue;
        if (isWithinRoot(controller->model, controller->model.documents[i].path)) return true;
    }
    return false;
}

bool WorkspaceControllerSaveAllProjectDocuments(WorkspaceController* controller) {
    if (!controller || !controller->model.open || !controller->model.hasProject) {
        if (controller) setControllerError(controller, ModelErrorCode::WorkspaceNotOpen);
        return false;
    }
    for (uint32_t i = 0; i < kMaxOpenDocuments; ++i) {
        if (!controller->model.documents[i].used || !controller->model.documents[i].buffer.dirty) continue;
        if (!isWithinRoot(controller->model, controller->model.documents[i].path)) continue;
        if (!WorkspaceControllerSaveDocument(controller, i)) return false;
    }
    return true;
}

bool WorkspaceControllerCloseDocument(WorkspaceController* controller, uint32_t documentIndex, CloseDecision decision) {
    if (!controller || documentIndex >= kMaxOpenDocuments || !controller->model.documents[documentIndex].used) { if (controller) setControllerError(controller, ModelErrorCode::DocumentNotFound); return false; }
    if (decision == CloseDecision::Save && controller->model.documents[documentIndex].buffer.dirty && !WorkspaceControllerSaveDocument(controller, documentIndex)) return false;
    ModelErrorCode error = ModelErrorCode::None;
    bool ok = WorkspaceModelCloseDocument(&controller->model, documentIndex, decision, decision != CloseDecision::Save || !controller->model.documents[documentIndex].buffer.dirty, &error);
    if (!ok) setControllerError(controller, error);
    return ok;
}

bool WorkspaceControllerCloseWorkspace(WorkspaceController* controller, CloseDecision decision) {
    if (!controller || !controller->model.open) return true;
    if (WorkspaceModelHasDirtyDocuments(&controller->model)) {
        if (decision == CloseDecision::Cancel) return false;
        if (decision == CloseDecision::Save && !WorkspaceControllerSaveAll(controller)) return false;
    }
    WorkspaceModelInit(&controller->model);
    controller->lastError = ModelErrorCode::None;
    controller->listingTruncated = false;
    return true;
}

Document* WorkspaceControllerActiveDocument(WorkspaceController* controller) {
    if (!controller || controller->model.activeDocument >= kMaxOpenDocuments) return nullptr;
    return controller->model.documents[controller->model.activeDocument].used ? &controller->model.documents[controller->model.activeDocument] : nullptr;
}

const Document* WorkspaceControllerActiveDocumentConst(const WorkspaceController* controller) {
    if (!controller || controller->model.activeDocument >= kMaxOpenDocuments) return nullptr;
    return controller->model.documents[controller->model.activeDocument].used ? &controller->model.documents[controller->model.activeDocument] : nullptr;
}

const char* WorkspaceControllerError(const WorkspaceController* controller) {
    return controller ? ModelErrorName(controller->lastError) : ModelErrorName(ModelErrorCode::InvalidPath);
}

const char* WorkspaceControllerProjectError(const WorkspaceController* controller) {
    return controller ? ProjectErrorName(controller->lastProjectError) : ProjectErrorName(ProjectErrorCode::NullInput);
}

} // namespace developer_studio
} // namespace guidexos
