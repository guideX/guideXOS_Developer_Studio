#include "developer_studio_models.h"

namespace guidexos {
namespace developer_studio {
namespace {

static const Capability kInitialCapabilities[] = {
    { "native-gui", "Native GUI application", true, CapabilityMaturity::Supported },
    { "app-model-manifest", "App Model manifest registration", true, CapabilityMaturity::Supported },
    { "hosted-native-runner", "Hosted Native ELF runner", true, CapabilityMaturity::Experimental },
    { "workspace-model", "Workspace and document model", true, CapabilityMaturity::Partial },
    { "filesystem-workspace", "Bounded hosted workspace filesystem", true, CapabilityMaturity::Experimental },
    { "build-integration", "Project build integration", false, CapabilityMaturity::Unavailable }
};

static const TargetProfile kInitialTargetProfile = {
    "guidexos.amd64.hosted.native",
    "guideXOS AMD64 Hosted - Native",
    "amd64",
    "guidexos-c-abi-v1",
    "Windows hosted guideXOS Server",
    "guideXOS Native SDK v1",
    "LLVM/LLD Native ELF",
    "guideXOS Hosted Native ELF Runtime",
    kInitialCapabilities,
    sizeof(kInitialCapabilities) / sizeof(kInitialCapabilities[0]),
    CapabilityMaturity::Experimental
};

static uint32_t textLength(const char* text, uint32_t limit) {
    if (!text) return 0;
    uint32_t length = 0;
    while (length < limit && text[length] != '\0') ++length;
    return length;
}

static bool copyText(char* output, uint32_t outputSize, const char* input) {
    if (!output || outputSize == 0 || !input) return false;
    uint32_t i = 0;
    while (i + 1 < outputSize && input[i] != '\0') {
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

static bool equalText(const char* left, const char* right, bool foldCase) {
    if (!left || !right) return left == right;
    uint32_t i = 0;
    while (left[i] != '\0' && right[i] != '\0') {
        char a = left[i];
        char b = right[i];
        if (foldCase) {
            if (a >= 'A' && a <= 'Z') a = static_cast<char>(a + ('a' - 'A'));
            if (b >= 'A' && b <= 'Z') b = static_cast<char>(b + ('a' - 'A'));
        }
        if (a != b) return false;
        ++i;
    }
    return left[i] == right[i];
}

static char lowerAscii(char value) {
    return value >= 'A' && value <= 'Z' ? static_cast<char>(value + ('a' - 'A')) : value;
}

static bool isSlash(char value) {
    return value == '/' || value == static_cast<char>(92);
}

static bool isDotSegment(const char* segment, uint32_t length) {
    return length == 1 && segment[0] == '.';
}

static bool isDotDotSegment(const char* segment, uint32_t length) {
    return length == 2 && segment[0] == '.' && segment[1] == '.';
}

static bool appendChar(char* output, uint32_t outputSize, uint32_t& length, char value) {
    if (length + 1 >= outputSize) return false;
    output[length++] = value;
    output[length] = '\0';
    return true;
}

static bool appendSegment(char* output, uint32_t outputSize, uint32_t& length, const char* segment, uint32_t segmentLength) {
    for (uint32_t i = 0; i < segmentLength; ++i) {
        if (!appendChar(output, outputSize, length, segment[i])) return false;
    }
    return true;
}

static bool hasPrefixPath(const char* root, const char* path) {
    uint32_t rootLength = textLength(root, kMaxPathBytes);
    uint32_t pathLength = textLength(path, kMaxPathBytes);
    if (rootLength > pathLength) return false;
    for (uint32_t i = 0; i < rootLength; ++i) {
        if (lowerAscii(root[i]) != lowerAscii(path[i])) return false;
    }
    return pathLength == rootLength || path[rootLength] == '/';
}

static void setError(WorkspaceModel* model, ModelErrorCode code) {
    if (!model) return;
    copyText(model->lastError, sizeof(model->lastError), ModelErrorName(code));
}

static void clearDocument(Document& document) {
    document.used = false;
    document.path[0] = '\0';
    document.name[0] = '\0';
    TextBufferInit(&document.buffer);
}

static uint32_t lineForCaret(const TextBuffer* buffer) {
    if (!buffer) return 0;
    uint32_t line = 0;
    for (uint32_t i = 0; i < buffer->caret && i < buffer->length; ++i) {
        if (buffer->data[i] == '\n') ++line;
    }
    return line;
}

static uint32_t lineColumnForCaret(const TextBuffer* buffer, uint32_t lineStart) {
    if (!buffer || lineStart > buffer->caret) return 0;
    return buffer->caret - lineStart;
}

static bool entryBefore(const WorkspaceEntry& left, const WorkspaceEntry& right) {
    if (left.kind == WorkspaceEntryKind::Directory && right.kind != WorkspaceEntryKind::Directory) return true;
    if (left.kind != WorkspaceEntryKind::Directory && right.kind == WorkspaceEntryKind::Directory) return false;
    uint32_t i = 0;
    while (left.name[i] != '\0' && right.name[i] != '\0') {
        char a = lowerAscii(left.name[i]);
        char b = lowerAscii(right.name[i]);
        if (a != b) return a < b;
        ++i;
    }
    return left.name[i] < right.name[i];
}

} // namespace

const TargetProfile& InitialTargetProfile() {
    return kInitialTargetProfile;
}

bool IsValidTargetProfile(const TargetProfile& profile) {
    return profile.id && profile.displayName && profile.architecture && profile.abi && profile.runner;
}

const char* ToString(ProjectKind kind) {
    switch (kind) {
    case ProjectKind::NativeGuiApplication: return "Native GUI application";
    case ProjectKind::ConsoleApplication: return "Console application";
    case ProjectKind::ComponentLibrary: return "Component/library";
    case ProjectKind::WebsiteOrHttpService: return "Website or HTTP service";
    case ProjectKind::Game2D: return "2D game";
    case ProjectKind::SystemComponent: return "System component";
    case ProjectKind::ExperimentalOther: return "Experimental/other";
    default: return "Unknown project kind";
    }
}

const char* ToString(CapabilityMaturity maturity) {
    switch (maturity) {
    case CapabilityMaturity::Unavailable: return "unavailable";
    case CapabilityMaturity::Experimental: return "experimental";
    case CapabilityMaturity::Partial: return "partial";
    case CapabilityMaturity::Supported: return "supported";
    case CapabilityMaturity::Deprecated: return "deprecated";
    default: return "unknown";
    }
}

void WorkspaceModelInit(WorkspaceModel* model) {
    if (!model) return;
    model->open = false;
    model->hasProject = false;
    model->project = Project();
    model->displayName[0] = '\0';
    model->rootPath[0] = '\0';
    model->browsePath[0] = '\0';
    model->entryCount = 0;
    model->selectedEntry = 0;
    model->activeDocument = kMaxOpenDocuments;
    model->lastError[0] = '\0';
    for (uint32_t i = 0; i < kMaxOpenDocuments; ++i) clearDocument(model->documents[i]);
}

bool NormalizePath(const char* input, char* output, uint32_t outputSize) {
    if (!input || !output || outputSize < 2) return false;
    output[0] = '\0';
    uint32_t inputLength = textLength(input, kMaxPathBytes);
    if (inputLength == kMaxPathBytes) return false;

    char segments[32][kMaxNameBytes];
    uint32_t segmentCount = 0;
    uint32_t i = 0;
    uint32_t outputLength = 0;
    bool absolute = false;
    char driveLetter = '\0';

    if (inputLength >= 2 && input[1] == ':') {
        driveLetter = lowerAscii(input[0]);
        absolute = inputLength >= 3 && isSlash(input[2]);
        i = 2;
        if (!appendChar(output, outputSize, outputLength, driveLetter)) return false;
        if (!appendChar(output, outputSize, outputLength, ':')) return false;
        if (absolute && !appendChar(output, outputSize, outputLength, '/')) return false;
    } else if (inputLength > 0 && isSlash(input[0])) {
        absolute = true;
        i = 1;
        if (!appendChar(output, outputSize, outputLength, '/')) return false;
    }

    while (i <= inputLength) {
        while (i < inputLength && isSlash(input[i])) ++i;
        uint32_t start = i;
        while (i < inputLength && !isSlash(input[i])) ++i;
        uint32_t length = i - start;
        if (length == 0) break;
        if (length >= kMaxNameBytes) return false;
        char segment[kMaxNameBytes];
        for (uint32_t j = 0; j < length; ++j) segment[j] = input[start + j];
        segment[length] = '\0';
        if (isDotSegment(segment, length)) continue;
        if (isDotDotSegment(segment, length)) {
            if (segmentCount == 0) return false;
            --segmentCount;
            continue;
        }
        if (segmentCount >= 32) return false;
        copyText(segments[segmentCount], kMaxNameBytes, segment);
        ++segmentCount;
    }

    for (uint32_t segment = 0; segment < segmentCount; ++segment) {
        bool needsSlash = outputLength > 0 && output[outputLength - 1] != '/';
        if (needsSlash && !appendChar(output, outputSize, outputLength, '/')) return false;
        if (!appendSegment(output, outputSize, outputLength, segments[segment], textLength(segments[segment], kMaxNameBytes))) return false;
    }

    if (outputLength == 0) {
        if (absolute) return copyText(output, outputSize, driveLetter ? "d:/" : "/");
        return copyText(output, outputSize, ".");
    }
    if (absolute && outputLength > 1 && output[outputLength - 1] == '/' && !(driveLetter && outputLength == 3)) {
        output[outputLength - 1] = '\0';
    }
    return true;
}

bool PathsEqual(const char* left, const char* right) {
    char normalizedLeft[kMaxPathBytes];
    char normalizedRight[kMaxPathBytes];
    if (!NormalizePath(left, normalizedLeft, sizeof(normalizedLeft)) ||
        !NormalizePath(right, normalizedRight, sizeof(normalizedRight))) return false;
    return equalText(normalizedLeft, normalizedRight, true);
}

bool PathContainsTraversal(const char* path) {
    if (!path) return true;
    uint32_t length = textLength(path, kMaxPathBytes);
    uint32_t start = 0;
    for (uint32_t i = 0; i <= length; ++i) {
        if (i < length && !isSlash(path[i])) continue;
        uint32_t segmentLength = i - start;
        if (segmentLength == 2 && path[start] == '.' && path[start + 1] == '.') return true;
        start = i + 1;
    }
    return false;
}

bool JoinWorkspacePath(const char* root, const char* relative, char* output, uint32_t outputSize) {
    if (!root || !relative || !output || relative[0] == '\0' || PathContainsTraversal(relative)) return false;
    if (isSlash(relative[0]) || (relative[1] == ':')) return false;
    char combined[kMaxPathBytes * 2];
    uint32_t rootLength = textLength(root, kMaxPathBytes);
    uint32_t relativeLength = textLength(relative, kMaxPathBytes);
    if (rootLength + relativeLength + 2 >= sizeof(combined)) return false;
    for (uint32_t i = 0; i < rootLength; ++i) combined[i] = root[i];
    uint32_t length = rootLength;
    if (length > 0 && combined[length - 1] != '/') combined[length++] = '/';
    for (uint32_t i = 0; i < relativeLength; ++i) combined[length++] = relative[i];
    combined[length] = '\0';
    if (!NormalizePath(combined, output, outputSize)) return false;
    char normalizedRoot[kMaxPathBytes];
    if (!NormalizePath(root, normalizedRoot, sizeof(normalizedRoot))) return false;
    return hasPrefixPath(normalizedRoot, output);
}

const char* BaseName(const char* path) {
    if (!path) return "";
    uint32_t length = textLength(path, kMaxPathBytes);
    uint32_t end = length;
    while (end > 0 && isSlash(path[end - 1])) --end;
    uint32_t start = end;
    while (start > 0 && !isSlash(path[start - 1])) --start;
    return path + start;
}

bool IsSupportedTextPath(const char* path) {
    const char* name = BaseName(path);
    if (equalText(name, "guidexos.project", true)) return true;
    uint32_t length = textLength(name, kMaxNameBytes);
    uint32_t dot = length;
    for (uint32_t i = 0; i < length; ++i) if (name[i] == '.') dot = i;
    if (dot == length || dot + 1 >= length) return false;
    const char* extension = name + dot;
    static const char* const extensions[] = {
        ".c", ".cc", ".cpp", ".h", ".hh", ".hpp", ".txt", ".md", ".json", ".xml", ".css", ".html", ".htm",
        ".js", ".ts", ".ps1", ".cmd", ".bat", ".cmake", ".ini", ".cfg", ".log", ".mk", ".yml", ".yaml"
    };
    for (uint32_t i = 0; i < sizeof(extensions) / sizeof(extensions[0]); ++i) {
        if (equalText(extension, extensions[i], true)) return true;
    }
    return false;
}

bool LooksBinary(const char* bytes, uint32_t length) {
    if (!bytes) return length != 0;
    for (uint32_t i = 0; i < length; ++i) {
        if (bytes[i] == '\0') return true;
    }
    return false;
}

const char* ModelErrorName(ModelErrorCode code) {
    switch (code) {
    case ModelErrorCode::None: return "none";
    case ModelErrorCode::WorkspaceNotOpen: return "workspace_not_open";
    case ModelErrorCode::InvalidPath: return "invalid_path";
    case ModelErrorCode::OutsideWorkspace: return "outside_workspace";
    case ModelErrorCode::NotDirectory: return "not_directory";
    case ModelErrorCode::NotFile: return "not_file";
    case ModelErrorCode::UnsupportedFile: return "unsupported_file";
    case ModelErrorCode::BinaryFile: return "binary_file";
    case ModelErrorCode::FileTooLarge: return "file_too_large";
    case ModelErrorCode::TooManyDocuments: return "too_many_documents";
    case ModelErrorCode::ReadFailed: return "read_failed";
    case ModelErrorCode::WriteFailed: return "write_failed";
    case ModelErrorCode::UnsavedChanges: return "unsaved_changes";
    case ModelErrorCode::DuplicateDocument: return "duplicate_document";
    case ModelErrorCode::DocumentNotFound: return "document_not_found";
    default: return "unknown";
    }
}

bool WorkspaceModelSetRoot(WorkspaceModel* model, const char* normalizedRoot, const char* displayName) {
    if (!model || !normalizedRoot || !displayName || !NormalizePath(normalizedRoot, model->rootPath, sizeof(model->rootPath))) {
        if (model) setError(model, ModelErrorCode::InvalidPath);
        return false;
    }
    if (!copyText(model->displayName, sizeof(model->displayName), displayName)) {
        setError(model, ModelErrorCode::InvalidPath);
        return false;
    }
    model->open = true;
    model->hasProject = false;
    model->project = Project();
    model->browsePath[0] = '\0';
    model->entryCount = 0;
    model->selectedEntry = 0;
    model->activeDocument = kMaxOpenDocuments;
    for (uint32_t i = 0; i < kMaxOpenDocuments; ++i) clearDocument(model->documents[i]);
    model->lastError[0] = '\0';
    return true;
}

bool WorkspaceModelSetBrowsePath(WorkspaceModel* model, const char* relativePath) {
    if (!model || !model->open) return false;
    if (!relativePath || relativePath[0] == '\0' || equalText(relativePath, ".", true)) {
        model->browsePath[0] = '\0';
        return true;
    }
    if (PathContainsTraversal(relativePath)) return false;
    uint32_t depth = 1;
    for (uint32_t i = 0; relativePath[i] != '\0'; ++i) if (relativePath[i] == '/' || relativePath[i] == static_cast<char>(92)) ++depth;
    if (depth > kMaxWorkspaceDepth) return false;
    return NormalizePath(relativePath, model->browsePath, sizeof(model->browsePath));
}

void WorkspaceModelClearEntries(WorkspaceModel* model) {
    if (!model) return;
    model->entryCount = 0;
    model->selectedEntry = 0;
}

bool WorkspaceModelAddEntry(WorkspaceModel* model, const WorkspaceEntry& entry) {
    if (!model || model->entryCount >= kMaxWorkspaceEntries) return false;
    model->entries[model->entryCount++] = entry;
    return true;
}

void WorkspaceModelSortEntries(WorkspaceModel* model) {
    if (!model) return;
    for (uint32_t i = 0; i < model->entryCount; ++i) {
        for (uint32_t j = i + 1; j < model->entryCount; ++j) {
            if (entryBefore(model->entries[j], model->entries[i])) {
                WorkspaceEntry temp = model->entries[i];
                model->entries[i] = model->entries[j];
                model->entries[j] = temp;
            }
        }
    }
}

int FindOpenDocument(const WorkspaceModel* model, const char* normalizedPath) {
    if (!model || !normalizedPath) return -1;
    for (uint32_t i = 0; i < kMaxOpenDocuments; ++i) {
        if (model->documents[i].used && PathsEqual(model->documents[i].path, normalizedPath)) return static_cast<int>(i);
    }
    return -1;
}

bool WorkspaceModelAddDocument(WorkspaceModel* model, const char* normalizedPath, const char* bytes, uint32_t length, ModelErrorCode* error, bool* duplicate) {
    if (error) *error = ModelErrorCode::None;
    if (duplicate) *duplicate = false;
    if (!model || !model->open) { if (error) *error = ModelErrorCode::WorkspaceNotOpen; return false; }
    char path[kMaxPathBytes];
    if (!NormalizePath(normalizedPath, path, sizeof(path))) { if (error) *error = ModelErrorCode::InvalidPath; return false; }
    if (!hasPrefixPath(model->rootPath, path)) { if (error) *error = ModelErrorCode::OutsideWorkspace; return false; }
    if (!IsSupportedTextPath(path)) { if (error) *error = ModelErrorCode::UnsupportedFile; return false; }
    if (length > kMaxEditorBytes) { if (error) *error = ModelErrorCode::FileTooLarge; return false; }
    if (LooksBinary(bytes, length)) { if (error) *error = ModelErrorCode::BinaryFile; return false; }
    int existing = FindOpenDocument(model, path);
    if (existing >= 0) {
        model->activeDocument = static_cast<uint32_t>(existing);
        if (duplicate) *duplicate = true;
        if (error) *error = ModelErrorCode::DuplicateDocument;
        return true;
    }
    uint32_t slot = kMaxOpenDocuments;
    for (uint32_t i = 0; i < kMaxOpenDocuments; ++i) if (!model->documents[i].used) { slot = i; break; }
    if (slot == kMaxOpenDocuments) { if (error) *error = ModelErrorCode::TooManyDocuments; return false; }
    Document& document = model->documents[slot];
    document.used = true;
    copyText(document.path, sizeof(document.path), path);
    copyText(document.name, sizeof(document.name), BaseName(path));
    TextBufferInit(&document.buffer);
    if (!TextBufferSet(&document.buffer, bytes, length)) {
        clearDocument(document);
        if (error) *error = ModelErrorCode::FileTooLarge;
        return false;
    }
    model->activeDocument = slot;
    return true;
}

bool WorkspaceModelCloseDocument(WorkspaceModel* model, uint32_t documentIndex, CloseDecision decision, bool saveSucceeded, ModelErrorCode* error) {
    if (error) *error = ModelErrorCode::None;
    if (!model || documentIndex >= kMaxOpenDocuments || !model->documents[documentIndex].used) {
        if (error) *error = ModelErrorCode::DocumentNotFound;
        return false;
    }
    Document& document = model->documents[documentIndex];
    if (document.buffer.dirty) {
        if (decision == CloseDecision::Cancel) return false;
        if (decision == CloseDecision::Save) {
            if (!saveSucceeded) {
                if (error) *error = ModelErrorCode::WriteFailed;
                return false;
            }
            TextBufferClearDirty(&document.buffer);
        }
    }
    clearDocument(document);
    if (model->activeDocument == documentIndex) {
        model->activeDocument = kMaxOpenDocuments;
        for (uint32_t i = 0; i < kMaxOpenDocuments; ++i) if (model->documents[i].used) { model->activeDocument = i; break; }
    }
    return true;
}

bool WorkspaceModelMarkSaved(WorkspaceModel* model, uint32_t documentIndex, bool writeSucceeded, ModelErrorCode* error) {
    if (error) *error = ModelErrorCode::None;
    if (!model || documentIndex >= kMaxOpenDocuments || !model->documents[documentIndex].used) {
        if (error) *error = ModelErrorCode::DocumentNotFound;
        return false;
    }
    if (!writeSucceeded) {
        if (error) *error = ModelErrorCode::WriteFailed;
        return false;
    }
    TextBufferClearDirty(&model->documents[documentIndex].buffer);
    return true;
}

bool WorkspaceModelHasDirtyDocuments(const WorkspaceModel* model) {
    if (!model) return false;
    for (uint32_t i = 0; i < kMaxOpenDocuments; ++i) {
        if (model->documents[i].used && model->documents[i].buffer.dirty) return true;
    }
    return false;
}

void TextBufferInit(TextBuffer* buffer) {
    if (!buffer) return;
    buffer->length = 0;
    buffer->caret = 0;
    buffer->dirty = false;
    buffer->data[0] = '\0';
}

bool TextBufferSet(TextBuffer* buffer, const char* bytes, uint32_t length) {
    if (!buffer || (!bytes && length != 0) || length > kMaxEditorBytes || LooksBinary(bytes, length)) return false;
    for (uint32_t i = 0; i < length; ++i) buffer->data[i] = bytes[i];
    buffer->length = length;
    buffer->caret = 0;
    buffer->data[length] = '\0';
    buffer->dirty = false;
    return true;
}

bool TextBufferInsert(TextBuffer* buffer, const char* bytes, uint32_t length) {
    if (!buffer || !bytes || length == 0 || buffer->length + length > kMaxEditorBytes || LooksBinary(bytes, length)) return false;
    for (uint32_t i = buffer->length; i > buffer->caret; --i) buffer->data[i + length - 1] = buffer->data[i - 1];
    for (uint32_t i = 0; i < length; ++i) buffer->data[buffer->caret + i] = bytes[i];
    buffer->length += length;
    buffer->caret += length;
    buffer->data[buffer->length] = '\0';
    buffer->dirty = true;
    return true;
}

bool TextBufferBackspace(TextBuffer* buffer) {
    if (!buffer || buffer->caret == 0) return false;
    uint32_t removeAt = buffer->caret - 1;
    for (uint32_t i = removeAt; i + 1 < buffer->length; ++i) buffer->data[i] = buffer->data[i + 1];
    --buffer->length;
    --buffer->caret;
    buffer->data[buffer->length] = '\0';
    buffer->dirty = true;
    return true;
}

bool TextBufferDelete(TextBuffer* buffer) {
    if (!buffer || buffer->caret >= buffer->length) return false;
    for (uint32_t i = buffer->caret; i + 1 < buffer->length; ++i) buffer->data[i] = buffer->data[i + 1];
    --buffer->length;
    buffer->data[buffer->length] = '\0';
    buffer->dirty = true;
    return true;
}

void TextBufferMoveLeft(TextBuffer* buffer) {
    if (buffer && buffer->caret > 0) --buffer->caret;
}

void TextBufferMoveRight(TextBuffer* buffer) {
    if (buffer && buffer->caret < buffer->length) ++buffer->caret;
}

void TextBufferMoveUp(TextBuffer* buffer) {
    if (!buffer) return;
    uint32_t line = lineForCaret(buffer);
    if (line == 0) return;
    uint32_t column = lineColumnForCaret(buffer, TextBufferLineStart(buffer, line));
    uint32_t previousStart = TextBufferLineStart(buffer, line - 1);
    uint32_t previousEnd = TextBufferLineEnd(buffer, line - 1);
    buffer->caret = previousStart + (column < previousEnd - previousStart ? column : previousEnd - previousStart);
}

void TextBufferMoveDown(TextBuffer* buffer) {
    if (!buffer) return;
    uint32_t line = lineForCaret(buffer);
    uint32_t count = TextBufferLineCount(buffer);
    if (line + 1 >= count) return;
    uint32_t column = lineColumnForCaret(buffer, TextBufferLineStart(buffer, line));
    uint32_t nextStart = TextBufferLineStart(buffer, line + 1);
    uint32_t nextEnd = TextBufferLineEnd(buffer, line + 1);
    buffer->caret = nextStart + (column < nextEnd - nextStart ? column : nextEnd - nextStart);
}

void TextBufferHome(TextBuffer* buffer) {
    if (!buffer) return;
    uint32_t line = lineForCaret(buffer);
    buffer->caret = TextBufferLineStart(buffer, line);
}

void TextBufferEnd(TextBuffer* buffer) {
    if (!buffer) return;
    uint32_t line = lineForCaret(buffer);
    buffer->caret = TextBufferLineEnd(buffer, line);
}

uint32_t TextBufferLineCount(const TextBuffer* buffer) {
    if (!buffer) return 0;
    uint32_t count = 1;
    for (uint32_t i = 0; i < buffer->length; ++i) if (buffer->data[i] == '\n') ++count;
    return count;
}

uint32_t TextBufferLineStart(const TextBuffer* buffer, uint32_t line) {
    if (!buffer) return 0;
    uint32_t current = 0;
    uint32_t index = 0;
    while (index < buffer->length && current < line) {
        if (buffer->data[index++] == '\n') ++current;
    }
    return index;
}

uint32_t TextBufferLineEnd(const TextBuffer* buffer, uint32_t line) {
    uint32_t start = TextBufferLineStart(buffer, line);
    if (!buffer) return start;
    uint32_t index = start;
    while (index < buffer->length && buffer->data[index] != '\n') ++index;
    if (index > start && buffer->data[index - 1] == '\r') --index;
    return index;
}

void TextBufferClearDirty(TextBuffer* buffer) {
    if (buffer) buffer->dirty = false;
}

} // namespace developer_studio
} // namespace guidexos
