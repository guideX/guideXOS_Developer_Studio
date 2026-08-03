#include "developer_studio_rename.h"

#include "developer_studio_syntax.h"

namespace guidexos {
namespace developer_studio {
namespace {

static char g_renameReadScratch[kProjectSearchMaxFileBytes + 1u] = {};
static TextBuffer g_renameTemporaryBuffer = {};
static RenameUndoRecord g_renameWorkingUndo = {};

static void clearBytes(void* value, uint32_t size) {
    if (!value) return;
    unsigned char* bytes = static_cast<unsigned char*>(value);
    for (uint32_t i = 0; i < size; ++i) bytes[i] = 0;
}

static uint32_t lengthOf(const char* text, uint32_t limit) {
    if (!text) return 0;
    uint32_t length = 0;
    while (length < limit && text[length] != '\0') ++length;
    return length;
}

static void copyText(char* output, uint32_t outputSize, const char* input) {
    if (!output || outputSize == 0) return;
    uint32_t i = 0;
    if (input) while (i + 1 < outputSize && input[i] != '\0') { output[i] = input[i]; ++i; }
    output[i] = '\0';
}

static bool equalText(const char* left, const char* right, bool caseSensitive) {
    if (!left || !right) return left == right;
    uint32_t i = 0;
    while (left[i] != '\0' && right[i] != '\0') {
        char a = left[i];
        char b = right[i];
        if (!caseSensitive) {
            if (a >= 'A' && a <= 'Z') a = static_cast<char>(a + ('a' - 'A'));
            if (b >= 'A' && b <= 'Z') b = static_cast<char>(b + ('a' - 'A'));
        }
        if (a != b) return false;
        ++i;
    }
    return left[i] == right[i];
}

static uint64_t hashBytes(const char* bytes, uint32_t length) {
    uint64_t hash = 1469598103934665603ull;
    if (!bytes) return hash;
    for (uint32_t i = 0; i < length; ++i) {
        hash ^= static_cast<unsigned char>(bytes[i]);
        hash *= 1099511628211ull;
    }
    return hash == 0 ? 1 : hash;
}

static bool textAt(const char* data, uint32_t length, uint64_t offset,
                   const char* expected, uint32_t expectedLength) {
    if (!data || !expected || offset > length || expectedLength > length - offset) return false;
    for (uint32_t i = 0; i < expectedLength; ++i)
        if (data[static_cast<uint32_t>(offset) + i] != expected[i]) return false;
    return true;
}

static bool safeRelativePath(const char* path) {
    return path && path[0] != '\0' && !PathContainsTraversal(path) &&
        path[0] != '/' && path[0] != '\\' && !(path[1] == ':');
}

static bool appendTildeName(char* output, uint32_t outputSize, const char* name, bool tilde) {
    if (!output || outputSize == 0 || !name) return false;
    uint32_t out = 0;
    if (tilde) {
        if (out + 1 >= outputSize) return false;
        output[out++] = '~';
    }
    for (uint32_t i = 0; name[i] != '\0'; ++i) {
        if (out + 1 >= outputSize) return false;
        output[out++] = name[i];
    }
    output[out] = '\0';
    return true;
}

static bool isTildeText(const char* text) {
    return text && text[0] == '~';
}

static void setStatus(RenameModel* model, const char* status) {
    if (model) copyText(model->status, sizeof(model->status), status ? status : "");
}

static void setError(RenameModel* model, RenameErrorCode error, const char* status) {
    if (!model) return;
    model->error = error;
    setStatus(model, status ? status : RenameErrorName(error));
}

static void logicalName(const char* identifier, char* output, uint32_t outputSize) {
    if (!output || outputSize == 0) return;
    output[0] = '\0';
    if (!identifier) return;
    copyText(output, outputSize, identifier[0] == '~' ? identifier + 1 : identifier);
}

static bool isTypeKind(SymbolKind kind) {
    return kind == SymbolKind::Class || kind == SymbolKind::Struct ||
        kind == SymbolKind::Union || kind == SymbolKind::Enum;
}

static bool kindRelated(SymbolKind left, SymbolKind right) {
    if (left == right) return true;
    return isTypeKind(left) && (right == SymbolKind::Constructor || right == SymbolKind::Destructor);
}

static const char* signatureParameters(const char* signature) {
    if (!signature) return "";
    for (uint32_t i = 0; signature[i] != '\0'; ++i) if (signature[i] == '(') return signature + i;
    return signature;
}

static bool buildQualifiedName(const ReferenceTarget& target, const char* newName,
                               char* output, uint32_t outputSize) {
    if (!output || outputSize == 0 || !newName) return false;
    output[0] = '\0';
    const char* qualified = target.qualifiedName[0] ? target.qualifiedName : target.identifier;
    uint32_t length = lengthOf(qualified, kRenameMaxQualifiedNameBytes + 1u);
    if (length == 0 || length > kRenameMaxQualifiedNameBytes) return false;
    uint32_t separator = 0;
    for (uint32_t i = 0; i + 1 < length; ++i)
        if (qualified[i] == ':' && qualified[i + 1] == ':') separator = i + 2;
    uint32_t out = 0;
    for (uint32_t i = 0; i < separator; ++i) {
        if (out + 1 >= outputSize) return false;
        output[out++] = qualified[i];
    }
    for (uint32_t i = 0; newName[i] != '\0'; ++i) {
        if (out + 1 >= outputSize) return false;
        output[out++] = newName[i];
    }
    output[out] = '\0';
    return true;
}

static bool readClosedFile(const WorkspaceFileSystem& fileSystem, const char* path,
                           char* output, uint32_t* outLength, uint64_t* outSize) {
    if (outLength) *outLength = 0;
    if (outSize) *outSize = 0;
    if (!fileSystem.stat || !fileSystem.read || !path || !output) return false;
    FileInfo info = {};
    if (!fileSystem.stat(fileSystem.userData, path, &info) || info.kind != FileInfoKind::RegularFile ||
        info.size > kProjectSearchMaxFileBytes) return false;
    uint32_t bytes = 0;
    if (!fileSystem.read(fileSystem.userData, path, output, kProjectSearchMaxFileBytes, &bytes) ||
        bytes > kProjectSearchMaxFileBytes || LooksBinary(output, bytes)) return false;
    output[bytes] = '\0';
    if (outLength) *outLength = bytes;
    if (outSize) *outSize = info.size;
    return true;
}

static bool absoluteFor(const WorkspaceModel& workspace, const char* relative,
                        char* output, uint32_t outputSize) {
    return safeRelativePath(relative) && JoinWorkspacePath(workspace.rootPath, relative, output, outputSize);
}

static int32_t filePlanFor(const RenameTransactionPlan& plan, const char* path) {
    for (uint32_t i = 0; i < plan.fileCount; ++i)
        if (equalText(plan.files[i].relativePath, path, false)) return static_cast<int32_t>(i);
    return -1;
}

static void updateSelectedCount(RenameModel* model) {
    if (!model) return;
    model->selectedCount = 0;
    for (uint32_t i = 0; i < model->candidateCount; ++i)
        if (model->candidates[i].state == RenameCandidateState::Selected) ++model->selectedCount;
}

static bool buildPreviewAfter(const ReferenceMatch& match, const char* replacement,
                              char* output, uint32_t outputSize) {
    if (!output || outputSize == 0 || !replacement) return false;
    output[0] = '\0';
    const uint32_t sourceLength = lengthOf(match.previewText, kRenameMaxPreviewBytes + 1u);
    const uint32_t start = match.previewMatchStart;
    const uint32_t length = match.previewMatchLength;
    if (start > sourceLength || length > sourceLength - start) return copyText(output, outputSize, match.previewText), false;
    uint32_t out = 0;
    for (uint32_t i = 0; i < start && out + 1 < outputSize; ++i) output[out++] = match.previewText[i];
    for (uint32_t i = 0; replacement[i] != '\0' && out + 1 < outputSize; ++i) output[out++] = replacement[i];
    for (uint32_t i = start + length; i < sourceLength && out + 1 < outputSize; ++i) output[out++] = match.previewText[i];
    output[out] = '\0';
    return true;
}

static bool buildCandidatePreviewAfter(const RenameEditCandidate& candidate, const char* replacement,
                                       char* output, uint32_t outputSize) {
    if (!output || outputSize == 0 || !replacement) return false;
    output[0] = '\0';
    const uint32_t sourceLength = lengthOf(candidate.previewBefore, sizeof(candidate.previewBefore));
    if (candidate.previewMatchStart > sourceLength || candidate.previewMatchLength > sourceLength - candidate.previewMatchStart) {
        copyText(output, outputSize, candidate.previewBefore);
        return false;
    }
    uint32_t out = 0;
    for (uint32_t i = 0; i < candidate.previewMatchStart && out + 1 < outputSize; ++i) output[out++] = candidate.previewBefore[i];
    for (uint32_t i = 0; replacement[i] != '\0' && out + 1 < outputSize; ++i) output[out++] = replacement[i];
    for (uint32_t i = candidate.previewMatchStart + candidate.previewMatchLength; i < sourceLength && out + 1 < outputSize; ++i)
        output[out++] = candidate.previewBefore[i];
    output[out] = '\0';
    return true;
}

static void setCandidateReplacement(RenameEditCandidate* candidate, const char* newName) {
    if (!candidate || !newName) return;
    appendTildeName(candidate->replacementText, sizeof(candidate->replacementText), newName,
                    isTildeText(candidate->expectedText));
}

static bool appendPlanEdit(RenameModel* model, RenameFilePlan* file,
                           const RenameEditCandidate& candidate) {
    if (!model || !file || model->plan.totalEdits >= kRenameMaxTotalEdits ||
        file->editCount >= kRenameMaxEditsPerFile) return false;
    const uint32_t index = model->plan.totalEdits++;
    RenameTextEdit& edit = model->planEdits[index];
    edit.byteOffset = candidate.byteOffset;
    edit.oldLength = candidate.oldLength;
    copyText(edit.expectedText, sizeof(edit.expectedText), candidate.expectedText);
    copyText(edit.replacementText, sizeof(edit.replacementText), candidate.replacementText);
    if (file->editCount == 0) file->firstEditIndex = index;
    ++file->editCount;
    return true;
}

static void sortPlanEditsDescending(RenameModel* model, RenameFilePlan& file) {
    if (!model) return;
    for (uint32_t i = 0; i < file.editCount; ++i) {
        for (uint32_t j = i + 1; j < file.editCount; ++j) {
            RenameTextEdit& left = model->planEdits[file.firstEditIndex + i];
            RenameTextEdit& right = model->planEdits[file.firstEditIndex + j];
            if (right.byteOffset > left.byteOffset) {
                RenameTextEdit temp = left;
                left = right;
                right = temp;
            }
        }
    }
}

static bool validatePlanBuffer(const RenameModel& model, const RenameFilePlan& file,
                               const char* data, uint32_t length) {
    if (!data || static_cast<uint64_t>(length) != file.expectedFileSize ||
        hashBytes(data, length) != file.expectedContentHash) return false;
    for (uint32_t i = 0; i < file.editCount; ++i) {
        const RenameTextEdit& edit = model.planEdits[file.firstEditIndex + i];
        const uint32_t expectedLength = lengthOf(edit.expectedText, sizeof(edit.expectedText));
        if (expectedLength != edit.oldLength || !textAt(data, length, edit.byteOffset,
                                                          edit.expectedText, expectedLength)) return false;
    }
    return true;
}

static uint32_t transformOffset(uint32_t offset, const RenameModel& model, const RenameFilePlan& file) {
    int64_t transformed = offset;
    for (uint32_t i = file.editCount; i > 0; --i) {
        const RenameTextEdit& edit = model.planEdits[file.firstEditIndex + i - 1];
        const uint32_t replacementLength = lengthOf(edit.replacementText, sizeof(edit.replacementText));
        if (offset >= edit.byteOffset + edit.oldLength)
            transformed += static_cast<int64_t>(replacementLength) - edit.oldLength;
        else if (offset >= edit.byteOffset)
            transformed = static_cast<int64_t>(edit.byteOffset) + replacementLength;
    }
    if (transformed < 0) return 0;
    return transformed > 0xFFFFFFFFll ? 0xFFFFFFFFu : static_cast<uint32_t>(transformed);
}

static void restoreBufferState(TextBuffer* buffer, uint32_t caret, uint32_t anchor,
                               bool selectionActive, bool dirty, uint32_t generation) {
    if (!buffer) return;
    buffer->caret = caret > buffer->length ? buffer->length : caret;
    buffer->selectionAnchor = anchor > buffer->length ? buffer->length : anchor;
    buffer->selectionActive = selectionActive && buffer->selectionAnchor != buffer->caret;
    buffer->dirty = dirty;
    buffer->generation = generation == 0 ? 1 : generation;
}

static bool applyForwardToBuffer(TextBuffer* buffer, const RenameModel& model,
                                 const RenameFilePlan& file) {
    if (!buffer) return false;
    for (uint32_t i = 0; i < file.editCount; ++i) {
        const RenameTextEdit& edit = model.planEdits[file.firstEditIndex + i];
        const uint32_t replacementLength = lengthOf(edit.replacementText, sizeof(edit.replacementText));
        if (!textAt(buffer->data, buffer->length, edit.byteOffset, edit.expectedText, edit.oldLength) ||
            !ReplaceTextRange(buffer, edit.byteOffset, edit.oldLength,
                              edit.replacementText, replacementLength)) return false;
    }
    return true;
}

static bool applyUndoToBuffer(TextBuffer* buffer, const RenameUndoRecord& record,
                              const RenameUndoFile& file) {
    if (!buffer) return false;
    int64_t delta = 0;
    for (uint32_t i = 0; i < file.editCount; ++i) {
        const RenameUndoEdit& edit = record.edits[file.firstEditIndex + i];
        const int64_t signedOffset = static_cast<int64_t>(edit.byteOffset) + delta;
        if (signedOffset < 0 || signedOffset > 0xFFFFFFFFll) return false;
        char expected[kRenameMaxTextBytes] = {};
        char replacement[kRenameMaxTextBytes] = {};
        if (!appendTildeName(expected, sizeof(expected), record.newName, edit.tildePrefixed) ||
            !appendTildeName(replacement, sizeof(replacement), record.oldName, edit.tildePrefixed)) return false;
        const uint32_t offset = static_cast<uint32_t>(signedOffset);
        const uint32_t expectedLength = lengthOf(expected, sizeof(expected));
        const uint32_t replacementLength = lengthOf(replacement, sizeof(replacement));
        if (expectedLength != edit.newLength || !textAt(buffer->data, buffer->length, offset, expected, expectedLength) ||
            !ReplaceTextRange(buffer, offset, expectedLength, replacement, replacementLength)) return false;
        delta += static_cast<int64_t>(edit.oldLength) - edit.newLength;
    }
    return true;
}

static bool applyForwardUndoToBuffer(TextBuffer* buffer, const RenameUndoRecord& record,
                                     const RenameUndoFile& file) {
    if (!buffer) return false;
    for (uint32_t i = file.editCount; i > 0; --i) {
        const RenameUndoEdit& edit = record.edits[file.firstEditIndex + i - 1];
        char expected[kRenameMaxTextBytes] = {};
        char replacement[kRenameMaxTextBytes] = {};
        if (!appendTildeName(expected, sizeof(expected), record.oldName, edit.tildePrefixed) ||
            !appendTildeName(replacement, sizeof(replacement), record.newName, edit.tildePrefixed)) return false;
        const uint32_t expectedLength = lengthOf(expected, sizeof(expected));
        const uint32_t replacementLength = lengthOf(replacement, sizeof(replacement));
        if (expectedLength != edit.oldLength || !textAt(buffer->data, buffer->length, edit.byteOffset,
                                                        expected, expectedLength) ||
            !ReplaceTextRange(buffer, edit.byteOffset, expectedLength, replacement, replacementLength)) return false;
    }
    return true;
}

static bool restoreUndoneFile(const RenameUndoRecord& record, RenameUndoFile& file,
                              WorkspaceModel* workspace, const WorkspaceFileSystem& fileSystem) {
    if (!workspace) return false;
    char absolute[kMaxPathBytes] = {};
    if (!absoluteFor(*workspace, file.relativePath, absolute, sizeof(absolute))) return false;
    const int documentIndex = FindOpenDocument(workspace, absolute);
    if (file.openDocument) {
        if (documentIndex < 0) return false;
        Document& document = workspace->documents[documentIndex];
        if (document.documentId != file.sourceDocumentId || hashBytes(document.buffer.data, document.buffer.length) != file.beforeHash ||
            !applyForwardUndoToBuffer(&document.buffer, record, file)) return false;
        DocumentUpdateSyntax(&document);
        restoreBufferState(&document.buffer, file.afterCaret, file.afterSelectionAnchor,
                           file.afterSelectionActive, true, file.afterDocumentGeneration);
        return true;
    }
    if (documentIndex >= 0) return false;
    uint32_t length = 0;
    uint64_t size = 0;
    if (!readClosedFile(fileSystem, absolute, g_renameReadScratch, &length, &size) ||
        length != file.beforeLength || hashBytes(g_renameReadScratch, length) != file.beforeHash ||
        !TextBufferSet(&g_renameTemporaryBuffer, g_renameReadScratch, length) ||
        !applyForwardUndoToBuffer(&g_renameTemporaryBuffer, record, file)) return false;
    uint32_t written = 0;
    return fileSystem.write && fileSystem.write(fileSystem.userData, absolute,
        g_renameTemporaryBuffer.data, g_renameTemporaryBuffer.length, &written) &&
        written == g_renameTemporaryBuffer.length;
}

static bool undoBufferMatches(const TextBuffer& buffer, const RenameUndoRecord& record,
                              const RenameUndoFile& file) {
    if (buffer.length != file.afterLength || hashBytes(buffer.data, buffer.length) != file.afterHash) return false;
    int64_t delta = 0;
    for (uint32_t i = 0; i < file.editCount; ++i) {
        const RenameUndoEdit& edit = record.edits[file.firstEditIndex + i];
        const int64_t signedOffset = static_cast<int64_t>(edit.byteOffset) + delta;
        char expected[kRenameMaxTextBytes] = {};
        if (signedOffset < 0 || !appendTildeName(expected, sizeof(expected), record.newName, edit.tildePrefixed)) return false;
        const uint32_t expectedLength = lengthOf(expected, sizeof(expected));
        if (expectedLength != edit.newLength || !textAt(buffer.data, buffer.length,
                                                         static_cast<uint64_t>(signedOffset), expected, expectedLength)) return false;
        delta += static_cast<int64_t>(edit.newLength) - edit.oldLength;
    }
    return true;
}

static void buildUndoFromPlan(const RenameModel& model, RenameUndoRecord* record) {
    if (!record) return;
    *record = RenameUndoRecord();
    record->active = true;
    record->transactionId = model.plan.transactionId;
    record->projectGeneration = model.target.projectGeneration;
    record->targetId = model.target.targetId;
    copyText(record->projectId, sizeof(record->projectId), model.target.projectIdText);
    copyText(record->oldName, sizeof(record->oldName), model.currentName);
    copyText(record->newName, sizeof(record->newName), model.newName);
    record->fileCount = model.plan.fileCount;
    for (uint32_t i = 0; i < model.plan.fileCount; ++i) {
        const RenameFilePlan& source = model.plan.files[i];
        RenameUndoFile& target = record->files[i];
        copyText(target.relativePath, sizeof(target.relativePath), source.relativePath);
        target.sourceDocumentId = source.sourceDocumentId;
        target.openDocument = source.openDocument;
        target.beforeLength = static_cast<uint32_t>(source.expectedFileSize);
        target.beforeHash = source.expectedContentHash;
        target.firstEditIndex = record->editCount;
        target.editCount = source.editCount;
        for (uint32_t j = source.editCount; j > 0; --j) {
            const RenameTextEdit& edit = model.planEdits[source.firstEditIndex + j - 1];
            RenameUndoEdit& undoEdit = record->edits[record->editCount++];
            undoEdit.byteOffset = edit.byteOffset;
            undoEdit.oldLength = edit.oldLength;
            undoEdit.newLength = lengthOf(edit.replacementText, sizeof(edit.replacementText));
            undoEdit.tildePrefixed = isTildeText(edit.expectedText);
        }
    }
}

static void pushUndo(RenameUndoManager* manager, const RenameUndoRecord& record) {
    if (!manager) return;
    manager->records[manager->nextIndex] = record;
    manager->records[manager->nextIndex].active = true;
    manager->nextIndex = (manager->nextIndex + 1u) % kRenameMaxUndoTransactions;
    if (manager->count < kRenameMaxUndoTransactions) ++manager->count;
}

static bool restoreAppliedRecord(RenameUndoRecord* record, WorkspaceModel* workspace,
                                 const WorkspaceFileSystem& fileSystem, uint32_t projectGeneration,
                                 bool* rollbackFailed) {
    if (rollbackFailed) *rollbackFailed = false;
    if (!record || !workspace) return false;
    bool success = true;
    for (uint32_t reverse = record->fileCount; reverse > 0; --reverse) {
        RenameUndoFile& file = record->files[reverse - 1];
        if (!file.applied) continue;
        char absolute[kMaxPathBytes] = {};
        if (!absoluteFor(*workspace, file.relativePath, absolute, sizeof(absolute))) { success = false; continue; }
        const int documentIndex = FindOpenDocument(workspace, absolute);
        if (file.openDocument) {
            if (documentIndex < 0) { success = false; continue; }
            Document& document = workspace->documents[documentIndex];
            if (document.documentId != file.sourceDocumentId || document.buffer.generation != file.afterDocumentGeneration ||
                !undoBufferMatches(document.buffer, *record, file) || !applyUndoToBuffer(&document.buffer, *record, file)) {
                success = false;
                continue;
            }
            DocumentUpdateSyntax(&document);
            restoreBufferState(&document.buffer, file.beforeCaret, file.beforeSelectionAnchor,
                               file.beforeSelectionActive, file.beforeDirty, file.beforeDocumentGeneration);
        } else {
            uint32_t length = 0;
            uint64_t size = 0;
            if (!readClosedFile(fileSystem, absolute, g_renameReadScratch, &length, &size) ||
                length != file.afterLength || hashBytes(g_renameReadScratch, length) != file.afterHash ||
                !TextBufferSet(&g_renameTemporaryBuffer, g_renameReadScratch, length) ||
                !applyUndoToBuffer(&g_renameTemporaryBuffer, *record, file)) {
                success = false;
                continue;
            }
            uint32_t written = 0;
            if (!fileSystem.write || !fileSystem.write(fileSystem.userData, absolute,
                                                        g_renameTemporaryBuffer.data,
                                                        g_renameTemporaryBuffer.length, &written) ||
                written != g_renameTemporaryBuffer.length) {
                success = false;
                continue;
            }
        }
    }
    if (!success && rollbackFailed) *rollbackFailed = true;
    (void)projectGeneration;
    return success;
}

static bool validateUndoRecord(const RenameUndoRecord& record, WorkspaceModel* workspace,
                               const WorkspaceFileSystem& fileSystem, const char* projectId,
                               uint64_t projectGeneration) {
    if (!workspace || !workspace->open || !workspace->hasProject || !projectId ||
        projectGeneration != record.projectGeneration || !equalText(projectId, record.projectId, true)) return false;
    for (uint32_t i = 0; i < record.fileCount; ++i) {
        const RenameUndoFile& file = record.files[i];
        char absolute[kMaxPathBytes] = {};
        if (!absoluteFor(*workspace, file.relativePath, absolute, sizeof(absolute))) return false;
        const int documentIndex = FindOpenDocument(workspace, absolute);
        if (file.openDocument) {
            if (documentIndex < 0) return false;
            const Document& document = workspace->documents[documentIndex];
            if (document.documentId != file.sourceDocumentId || document.buffer.generation != file.afterDocumentGeneration ||
                !undoBufferMatches(document.buffer, record, file)) return false;
        } else {
            if (documentIndex >= 0) return false;
            uint32_t length = 0;
            uint64_t size = 0;
            if (!readClosedFile(fileSystem, absolute, g_renameReadScratch, &length, &size) ||
                length != file.afterLength || hashBytes(g_renameReadScratch, length) != file.afterHash ||
                !TextBufferSet(&g_renameTemporaryBuffer, g_renameReadScratch, length) ||
                !undoBufferMatches(g_renameTemporaryBuffer, record, file)) return false;
        }
    }
    return true;
}

} // namespace

const char* RenameStateName(RenameState state) {
    switch (state) {
    case RenameState::Idle: return "Idle";
    case RenameState::Preview: return "Preview";
    case RenameState::Applying: return "Applying";
    case RenameState::Applied: return "Applied";
    case RenameState::Cancelled: return "Cancelled";
    case RenameState::Failed: return "Failed";
    default: return "Unknown";
    }
}

const char* RenameCandidateStateName(RenameCandidateState state) {
    switch (state) {
    case RenameCandidateState::Selected: return "Selected";
    case RenameCandidateState::Unselected: return "Unselected";
    case RenameCandidateState::Disabled: return "Disabled";
    case RenameCandidateState::Stale: return "Stale";
    case RenameCandidateState::Conflict: return "Conflict";
    default: return "Unknown";
    }
}

const char* RenameConflictSeverityName(RenameConflictSeverity severity) {
    switch (severity) {
    case RenameConflictSeverity::None: return "None";
    case RenameConflictSeverity::Warning: return "Warning";
    case RenameConflictSeverity::Blocking: return "Blocking";
    default: return "Unknown";
    }
}

const char* RenameErrorName(RenameErrorCode error) {
    switch (error) {
    case RenameErrorCode::None: return "RENAME_NONE";
    case RenameErrorCode::NoProject: return "RENAME_NO_PROJECT";
    case RenameErrorCode::NoTarget: return "RENAME_NO_TARGET";
    case RenameErrorCode::TargetAmbiguous: return "RENAME_TARGET_AMBIGUOUS";
    case RenameErrorCode::UnsupportedSymbol: return "RENAME_UNSUPPORTED_SYMBOL";
    case RenameErrorCode::MacroUnsupported: return "RENAME_MACRO_UNSUPPORTED";
    case RenameErrorCode::InvalidIdentifier: return "RENAME_INVALID_IDENTIFIER";
    case RenameErrorCode::EmptyName: return "RENAME_EMPTY_NAME";
    case RenameErrorCode::NameTooLong: return "RENAME_NAME_TOO_LONG";
    case RenameErrorCode::Keyword: return "RENAME_KEYWORD";
    case RenameErrorCode::QualifiedName: return "RENAME_QUALIFIED_NAME";
    case RenameErrorCode::Unchanged: return "RENAME_UNCHANGED";
    case RenameErrorCode::Conflict: return "RENAME_CONFLICT";
    case RenameErrorCode::NoSelectedEdits: return "RENAME_NO_SELECTED_EDITS";
    case RenameErrorCode::FileLimit: return "RENAME_FILE_LIMIT";
    case RenameErrorCode::EditLimit: return "RENAME_EDIT_LIMIT";
    case RenameErrorCode::SnapshotLimit: return "RENAME_SNAPSHOT_LIMIT";
    case RenameErrorCode::PathInvalid: return "RENAME_PATH_INVALID";
    case RenameErrorCode::PathOutsideProject: return "RENAME_PATH_OUTSIDE_PROJECT";
    case RenameErrorCode::ProjectStale: return "RENAME_PROJECT_STALE";
    case RenameErrorCode::DocumentStale: return "RENAME_DOCUMENT_STALE";
    case RenameErrorCode::FileStale: return "RENAME_FILE_STALE";
    case RenameErrorCode::ExpectedTextMismatch: return "RENAME_EXPECTED_TEXT_MISMATCH";
    case RenameErrorCode::Overlap: return "RENAME_OVERLAP";
    case RenameErrorCode::ReadOnly: return "RENAME_READ_ONLY";
    case RenameErrorCode::ReadFailed: return "RENAME_READ_FAILED";
    case RenameErrorCode::WriteFailed: return "RENAME_WRITE_FAILED";
    case RenameErrorCode::RollbackFailed: return "RENAME_ROLLBACK_FAILED";
    case RenameErrorCode::UndoUnavailable: return "RENAME_UNDO_UNAVAILABLE";
    case RenameErrorCode::UndoStale: return "RENAME_UNDO_STALE";
    case RenameErrorCode::UndoFailed: return "RENAME_UNDO_FAILED";
    case RenameErrorCode::Cancelled: return "RENAME_CANCELLED";
    default: return "RENAME_INTERNAL";
    }
}

void RenameModelInit(RenameModel* model) {
    if (!model) return;
    clearBytes(model, sizeof(*model));
    model->state = RenameState::Idle;
    model->error = RenameErrorCode::None;
    ReferenceTargetInit(&model->target);
    setStatus(model, "");
}

void RenameUndoManagerInit(RenameUndoManager* manager) {
    if (!manager) return;
    clearBytes(manager, sizeof(*manager));
}

bool RenameSymbolKindSupported(SymbolKind kind) {
    switch (kind) {
    case SymbolKind::Namespace:
    case SymbolKind::Class:
    case SymbolKind::Struct:
    case SymbolKind::Union:
    case SymbolKind::Enum:
    case SymbolKind::Function:
    case SymbolKind::Method:
    case SymbolKind::Constructor:
    case SymbolKind::Destructor:
    case SymbolKind::GlobalVariable:
    case SymbolKind::StaticVariable:
    case SymbolKind::Typedef:
    case SymbolKind::UsingAlias:
        return true;
    case SymbolKind::Macro:
    default:
        return false;
    }
}

bool RenameValidateNewName(const char* originalName, const char* newName, RenameErrorCode* error) {
    if (error) *error = RenameErrorCode::None;
    if (!newName || newName[0] == '\0') { if (error) *error = RenameErrorCode::EmptyName; return false; }
    uint32_t length = 0;
    while (length <= kRenameMaxIdentifierBytes && newName[length] != '\0') ++length;
    if (length > kRenameMaxIdentifierBytes) { if (error) *error = RenameErrorCode::NameTooLong; return false; }
    if (!((newName[0] >= 'A' && newName[0] <= 'Z') || (newName[0] >= 'a' && newName[0] <= 'z') || newName[0] == '_')) {
        if (error) *error = RenameErrorCode::InvalidIdentifier;
        return false;
    }
    for (uint32_t i = 1; i < length; ++i) {
        const char value = newName[i];
        if (!((value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z') ||
              (value >= '0' && value <= '9') || value == '_')) {
            if (error) *error = (value == ':' || value == '.' || value == '/' || value == '\\' || value == '-' || value == '>')
                ? RenameErrorCode::QualifiedName : RenameErrorCode::InvalidIdentifier;
            return false;
        }
    }
    if (SyntaxIsKeyword(newName)) { if (error) *error = RenameErrorCode::Keyword; return false; }
    if (originalName && equalText(originalName, newName, true)) {
        if (equalText(originalName, newName, false)) { if (error) *error = RenameErrorCode::Unchanged; return false; }
    }
    return true;
}

bool RenameDetectConflict(const SymbolDatabase* database, const ReferenceTarget& target,
                          const char* newName, RenameConflictSeverity* severity,
                          char* message, uint32_t messageSize) {
    if (severity) *severity = RenameConflictSeverity::None;
    if (message && messageSize) message[0] = '\0';
    if (!database || !newName || newName[0] == '\0') return true;
    char qualified[kRenameMaxQualifiedNameBytes + 1u] = {};
    if (!buildQualifiedName(target, newName, qualified, sizeof(qualified))) return false;
    RenameConflictSeverity result = RenameConflictSeverity::None;
    for (uint32_t i = 0; i < database->projectSymbolCount; ++i) {
        const ProjectSymbol& symbol = database->projectSymbols[i];
        const bool sameQualified = qualified[0] != '\0' &&
            equalText(symbol.symbol.qualifiedName, qualified, true);
        const bool sameScopeName = equalText(symbol.symbol.name, newName, true) &&
            equalText(symbol.symbol.container, target.containingScope, true);
        if (!sameQualified && !sameScopeName) continue;
        if (kindRelated(target.kind, symbol.symbol.kind)) {
            if (target.hasSignature && target.signature[0] && symbol.symbol.signature[0] &&
                !equalText(signatureParameters(target.signature), signatureParameters(symbol.symbol.signature), true)) {
                if (result == RenameConflictSeverity::None) result = RenameConflictSeverity::Warning;
            } else {
                result = RenameConflictSeverity::Blocking;
                break;
            }
        } else if (result == RenameConflictSeverity::None) result = RenameConflictSeverity::Warning;
    }
    if (severity) *severity = result;
    if (message && messageSize && result != RenameConflictSeverity::None) {
        copyText(message, messageSize, result == RenameConflictSeverity::Blocking
            ? "A symbol named NewName already exists in this scope."
            : "A nearby or overload symbol may conflict with NewName.");
        const char* marker = result == RenameConflictSeverity::Blocking ? "A symbol named " : "Possible conflict for ";
        copyText(message, messageSize, marker);
        uint32_t used = lengthOf(message, messageSize);
        for (uint32_t i = 0; i < lengthOf(newName, kRenameMaxIdentifierBytes + 1u) && used + 1 < messageSize; ++i)
            message[used++] = newName[i];
        if (used + 1 < messageSize) {
            const char* suffix = result == RenameConflictSeverity::Blocking ? " already exists in this scope." : " may conflict with an existing symbol.";
            for (uint32_t i = 0; suffix[i] != '\0' && used + 1 < messageSize; ++i) message[used++] = suffix[i];
            message[used] = '\0';
        }
    }
    return true;
}

bool RenameModelBuildFromReferences(RenameModel* model, const ReferenceSearchService* references) {
    if (!model || !references) return false;
    const ReferenceSearchOperation* operation = ReferenceSearchOperationInfo(references);
    if (!operation || operation->state != ReferenceSearchState::Completed ||
        operation->lexicalFallback || operation->target.lexicallyAmbiguous) {
        setError(model, RenameErrorCode::NoTarget, "Rename requires an indexed symbol.");
        model->state = RenameState::Failed;
        return false;
    }
    if (!RenameSymbolKindSupported(operation->target.kind)) {
        setError(model, operation->target.kind == SymbolKind::Macro ? RenameErrorCode::MacroUnsupported : RenameErrorCode::UnsupportedSymbol,
                 "Rename is not supported for this symbol kind.");
        model->state = RenameState::Failed;
        return false;
    }
    RenameModelInit(model);
    model->target = operation->target;
    logicalName(model->target.identifier, model->currentName, sizeof(model->currentName));
    copyText(model->newName, sizeof(model->newName), model->currentName);
    for (uint32_t groupIndex = 0; groupIndex < ReferenceSearchResultGroups(references); ++groupIndex) {
        const ReferenceFileGroup* group = ReferenceSearchResultGroupAt(references, groupIndex);
        if (!group) continue;
        for (uint32_t matchIndex = 0; matchIndex < group->matchCount; ++matchIndex) {
            const ReferenceMatch* match = ReferenceSearchResultMatchAt(references, group, matchIndex);
            if (!match) continue;
            bool duplicate = false;
            for (uint32_t prior = 0; prior < model->candidateCount; ++prior) {
                const RenameEditCandidate& existing = model->candidates[prior];
                if (equalText(existing.relativePath, match->relativePath, false) &&
                    existing.byteOffset == match->byteOffset && existing.oldLength == match->identifierLength) {
                    duplicate = true;
                    break;
                }
            }
            if (duplicate) continue;
            if (model->candidateCount >= kRenameMaxTotalEdits) {
                setError(model, RenameErrorCode::EditLimit, "Rename results exceed the bounded edit limit.");
                model->state = RenameState::Failed;
                return false;
            }
            RenameEditCandidate& candidate = model->candidates[model->candidateCount++];
            candidate = RenameEditCandidate();
            candidate.candidateId = model->candidateCount;
            copyText(candidate.relativePath, sizeof(candidate.relativePath), match->relativePath);
            candidate.byteOffset = match->byteOffset;
            candidate.line = match->line;
            candidate.column = match->column;
            candidate.oldLength = match->identifierLength == 0 ? lengthOf(model->target.identifier, sizeof(model->target.identifier)) : match->identifierLength;
            const uint32_t targetLength = lengthOf(model->target.identifier, sizeof(model->target.identifier));
            const bool tilde = model->target.identifier[0] != '~' && candidate.oldLength == targetLength + 1u;
            if (!appendTildeName(candidate.expectedText, sizeof(candidate.expectedText), model->currentName,
                                 model->target.identifier[0] == '~' || tilde)) {
                candidate.state = RenameCandidateState::Disabled;
                candidate.editable = false;
            }
            candidate.referenceKind = match->kind;
    candidate.confidence = match->confidence;
            candidate.previewMatchStart = match->previewMatchStart;
            candidate.previewMatchLength = match->previewMatchLength;
            candidate.documentId = match->sourceDocumentId;
            candidate.documentGeneration = match->sourceDocumentGeneration;
            candidate.fromDirtySnapshot = match->fromDirtySnapshot;
            candidate.stale = match->stale;
            candidate.editable = !candidate.stale && match->confidence != ReferenceConfidence::LexicalOnly;
            if (match->confidence == ReferenceConfidence::Exact) {
                candidate.state = candidate.editable ? RenameCandidateState::Selected : RenameCandidateState::Disabled;
                ++model->exactCount;
            } else if (match->confidence == ReferenceConfidence::Likely) {
                candidate.state = candidate.editable ? RenameCandidateState::Unselected : RenameCandidateState::Disabled;
                ++model->likelyCount;
            } else if (match->confidence == ReferenceConfidence::Ambiguous) {
                candidate.state = candidate.editable ? RenameCandidateState::Unselected : RenameCandidateState::Disabled;
                ++model->ambiguousCount;
            } else {
                candidate.state = RenameCandidateState::Disabled;
                candidate.editable = false;
                ++model->lexicalOnlyCount;
            }
            copyText(candidate.previewBefore, sizeof(candidate.previewBefore), match->previewText);
            setCandidateReplacement(&candidate, model->newName);
            buildPreviewAfter(*match, candidate.replacementText, candidate.previewAfter, sizeof(candidate.previewAfter));
        }
    }
    for (uint32_t i = 0; i < model->candidateCount; ++i) {
        bool found = false;
        for (uint32_t j = 0; j < i; ++j)
            if (equalText(model->candidates[i].relativePath, model->candidates[j].relativePath, false)) found = true;
        if (!found) ++model->fileCount;
    }
    updateSelectedCount(model);
    model->state = RenameState::Preview;
    setStatus(model, model->candidateCount == 0 ? "No references found." : "Preview ready; review selected exact references.");
    return true;
}

bool RenameModelSetNewName(RenameModel* model, const char* newName, const SymbolDatabase* database) {
    if (!model || !newName) return false;
    copyText(model->newName, sizeof(model->newName), newName);
    for (uint32_t i = 0; i < model->candidateCount; ++i) {
        setCandidateReplacement(&model->candidates[i], model->newName);
        buildCandidatePreviewAfter(model->candidates[i], model->candidates[i].replacementText,
                                   model->candidates[i].previewAfter, sizeof(model->candidates[i].previewAfter));
    }
    RenameErrorCode error = RenameErrorCode::None;
    const bool valid = RenameValidateNewName(model->currentName, model->newName, &error);
    model->conflictSeverity = RenameConflictSeverity::None;
    model->conflictMessage[0] = '\0';
    if (!valid) {
        model->error = error;
        setStatus(model, RenameErrorName(error));
        return false;
    }
    RenameDetectConflict(database, model->target, model->newName, &model->conflictSeverity,
                         model->conflictMessage, sizeof(model->conflictMessage));
    model->error = model->conflictSeverity == RenameConflictSeverity::Blocking ? RenameErrorCode::Conflict : RenameErrorCode::None;
    if (model->conflictSeverity != RenameConflictSeverity::None) setStatus(model, model->conflictMessage);
    else setStatus(model, "Preview ready; review selected exact references.");
    return true;
}

bool RenameModelSetCandidateSelected(RenameModel* model, uint32_t index, bool selected) {
    if (!model || index >= model->candidateCount) return false;
    RenameEditCandidate& candidate = model->candidates[index];
    if (!candidate.editable || candidate.stale || candidate.state == RenameCandidateState::Disabled ||
        candidate.confidence == ReferenceConfidence::LexicalOnly) return false;
    candidate.state = selected ? RenameCandidateState::Selected : RenameCandidateState::Unselected;
    updateSelectedCount(model);
    return true;
}

uint32_t RenameModelSelectExact(RenameModel* model) {
    if (!model) return 0;
    for (uint32_t i = 0; i < model->candidateCount; ++i)
        if (model->candidates[i].confidence == ReferenceConfidence::Exact && model->candidates[i].editable && !model->candidates[i].stale)
            model->candidates[i].state = RenameCandidateState::Selected;
    updateSelectedCount(model);
    return model->selectedCount;
}

void RenameModelClearSelection(RenameModel* model) {
    if (!model) return;
    for (uint32_t i = 0; i < model->candidateCount; ++i)
        if (model->candidates[i].state == RenameCandidateState::Selected)
            model->candidates[i].state = RenameCandidateState::Unselected;
    updateSelectedCount(model);
}

uint32_t RenameModelSelectedCount(const RenameModel* model) { return model ? model->selectedCount : 0; }
const char* RenameModelStatus(const RenameModel* model) { return model ? model->status : ""; }

bool RenameModelBuildPlan(RenameModel* model, const WorkspaceModel* workspace,
                          const WorkspaceFileSystem& fileSystem, uint64_t transactionId) {
    if (!model || !workspace || !workspace->open || !workspace->hasProject) {
        if (model) setError(model, RenameErrorCode::NoProject, "Open a project before applying Rename Symbol.");
        return false;
    }
    RenameErrorCode nameError = RenameErrorCode::None;
    if (!RenameValidateNewName(model->currentName, model->newName, &nameError)) {
        setError(model, nameError, RenameErrorName(nameError));
        return false;
    }
    if (model->conflictSeverity == RenameConflictSeverity::Blocking) {
        setError(model, RenameErrorCode::Conflict, model->conflictMessage);
        return false;
    }
    if (model->selectedCount == 0) {
        setError(model, RenameErrorCode::NoSelectedEdits, "Select at least one exact reference before applying.");
        return false;
    }
    model->plan = RenameTransactionPlan();
    model->plan.transactionId = transactionId;
    model->plan.target = model->target;
    copyText(model->plan.newName, sizeof(model->plan.newName), model->newName);
    model->plan.requiresWarnings = model->conflictSeverity == RenameConflictSeverity::Warning;
    for (uint32_t i = 0; i < model->candidateCount; ++i) {
        RenameEditCandidate& candidate = model->candidates[i];
        if (candidate.state != RenameCandidateState::Selected) continue;
        if (!candidate.editable || candidate.stale || !safeRelativePath(candidate.relativePath)) {
            setError(model, RenameErrorCode::PathInvalid, "A selected rename location is invalid or stale.");
            return false;
        }
        int32_t fileIndex = filePlanFor(model->plan, candidate.relativePath);
        if (fileIndex < 0) {
            if (model->plan.fileCount >= kRenameMaxFiles) { setError(model, RenameErrorCode::FileLimit, "Rename affects too many files."); return false; }
            fileIndex = static_cast<int32_t>(model->plan.fileCount++);
            RenameFilePlan& file = model->plan.files[fileIndex];
            file = RenameFilePlan();
            copyText(file.relativePath, sizeof(file.relativePath), candidate.relativePath);
        }
        if (!appendPlanEdit(model, &model->plan.files[fileIndex], candidate)) {
            setError(model, RenameErrorCode::EditLimit, "Rename exceeds the bounded edit limit.");
            return false;
        }
    }
    uint64_t totalBytes = 0;
    for (uint32_t fileIndex = 0; fileIndex < model->plan.fileCount; ++fileIndex) {
        RenameFilePlan& file = model->plan.files[fileIndex];
        char absolute[kMaxPathBytes] = {};
        if (!absoluteFor(*workspace, file.relativePath, absolute, sizeof(absolute))) {
            setError(model, RenameErrorCode::PathOutsideProject, "A rename path is outside the active project.");
            return false;
        }
        const int documentIndex = FindOpenDocument(workspace, absolute);
        const char* data = nullptr;
        uint32_t length = 0;
        if (documentIndex >= 0) {
            const Document& document = workspace->documents[documentIndex];
            file.openDocument = true;
            file.dirtyDocument = document.buffer.dirty;
            file.sourceDocumentId = document.documentId;
            file.sourceDocumentGeneration = document.buffer.generation;
            data = document.buffer.data;
            length = document.buffer.length;
            model->plan.containsDirtyDocuments = model->plan.containsDirtyDocuments || document.buffer.dirty;
        } else {
            uint64_t statSize = 0;
            if (!readClosedFile(fileSystem, absolute, g_renameReadScratch, &length, &statSize)) {
                setError(model, RenameErrorCode::ReadFailed, "A closed project source file could not be read.");
                return false;
            }
            data = g_renameReadScratch;
            file.openDocument = false;
            file.dirtyDocument = false;
            file.sourceDocumentId = 0;
            file.sourceDocumentGeneration = 0;
            model->plan.containsDiskFiles = true;
            file.expectedFileSize = statSize;
        }
        if (length > kProjectSearchMaxFileBytes) { setError(model, RenameErrorCode::SnapshotLimit, "A source file exceeds the rename size limit."); return false; }
        file.expectedFileSize = length;
        file.expectedContentHash = hashBytes(data, length);
        file.expectedModificationToken = file.expectedContentHash;
        totalBytes += length;
        if (totalBytes > 64ull * 1024ull * 1024ull) { setError(model, RenameErrorCode::SnapshotLimit, "Rename snapshots exceed the bounded storage limit."); return false; }
        for (uint32_t i = 0; i < model->candidateCount; ++i) {
            RenameEditCandidate& candidate = model->candidates[i];
            if (candidate.state != RenameCandidateState::Selected || !equalText(candidate.relativePath, file.relativePath, false)) continue;
            if (candidate.documentId != 0 && (candidate.documentId != file.sourceDocumentId ||
                candidate.documentGeneration != file.sourceDocumentGeneration)) {
                setError(model, RenameErrorCode::DocumentStale, "A dirty document changed after the rename preview.");
                candidate.stale = true;
                return false;
            }
            if (candidate.byteOffset > length || candidate.oldLength > length - candidate.byteOffset ||
                !textAt(data, length, candidate.byteOffset, candidate.expectedText, candidate.oldLength)) {
                setError(model, RenameErrorCode::ExpectedTextMismatch, "A selected rename location changed after the preview.");
                return false;
            }
        }
        sortPlanEditsDescending(model, file);
        for (uint32_t i = 1; i < file.editCount; ++i) {
            const RenameTextEdit& prior = model->planEdits[file.firstEditIndex + i - 1];
            const RenameTextEdit& current = model->planEdits[file.firstEditIndex + i];
            if (prior.byteOffset < current.byteOffset + current.oldLength) {
                setError(model, RenameErrorCode::Overlap, "Rename edits overlap in one file.");
                return false;
            }
        }
        uint64_t outputLength = length;
        for (uint32_t i = 0; i < file.editCount; ++i) {
            const RenameTextEdit& edit = model->planEdits[file.firstEditIndex + i];
            const uint32_t replacementLength = lengthOf(edit.replacementText, sizeof(edit.replacementText));
            outputLength = outputLength - edit.oldLength + replacementLength;
        }
        if (outputLength > kProjectSearchMaxFileBytes) { setError(model, RenameErrorCode::SnapshotLimit, "Rename output exceeds the file size limit."); return false; }
        file.outputFileSize = outputLength;
        model->plan.totalBytesBefore += length;
        model->plan.totalBytesAfter += outputLength;
    }
    model->plan.totalEdits = model->selectedCount;
    return true;
}

bool RenameApply(RenameModel* model, WorkspaceModel* workspace,
                 const WorkspaceFileSystem& fileSystem, SymbolDatabase* database,
                 uint64_t projectGeneration, RenameUndoManager* undoManager,
                 uint64_t transactionId) {
    if (!model || !workspace || projectGeneration == 0 || workspace->projectGeneration != projectGeneration ||
        model->target.projectGeneration != projectGeneration) {
        if (model) setError(model, RenameErrorCode::ProjectStale, "The active project changed before Rename Symbol could apply.");
        return false;
    }
    if (!RenameModelBuildPlan(model, workspace, fileSystem, transactionId)) { model->state = RenameState::Failed; return false; }
    model->state = RenameState::Applying;
    if (!model->plan.containsDiskFiles && !model->plan.containsDirtyDocuments) {
        setError(model, RenameErrorCode::NoSelectedEdits, "No editable rename locations remain.");
        model->state = RenameState::Failed;
        return false;
    }
    // BuildPlan performs the immediate pre-apply read/hash/text validation. No
    // document or file is mutated before the entire plan has passed it.
    buildUndoFromPlan(*model, &g_renameWorkingUndo);
    RenameUndoRecord& record = g_renameWorkingUndo;
    bool failed = false;
    for (uint32_t fileIndex = 0; fileIndex < model->plan.fileCount && !failed; ++fileIndex) {
        RenameFilePlan& planFile = model->plan.files[fileIndex];
        RenameUndoFile& undoFile = record.files[fileIndex];
        char absolute[kMaxPathBytes] = {};
        if (!absoluteFor(*workspace, planFile.relativePath, absolute, sizeof(absolute))) { failed = true; break; }
        const int documentIndex = FindOpenDocument(workspace, absolute);
        if (planFile.openDocument) {
            if (documentIndex < 0) { failed = true; model->error = RenameErrorCode::DocumentStale; break; }
            Document& document = workspace->documents[documentIndex];
            undoFile.beforeDocumentGeneration = document.buffer.generation;
            undoFile.beforeCaret = document.buffer.caret;
            undoFile.beforeSelectionAnchor = document.buffer.selectionAnchor;
            undoFile.beforeSelectionActive = document.buffer.selectionActive;
            undoFile.beforeDirty = document.buffer.dirty;
            if (!validatePlanBuffer(*model, planFile, document.buffer.data, document.buffer.length) ||
                !applyForwardToBuffer(&document.buffer, *model, planFile)) { failed = true; model->error = RenameErrorCode::ExpectedTextMismatch; break; }
            document.buffer.caret = transformOffset(undoFile.beforeCaret, *model, planFile);
            document.buffer.selectionAnchor = transformOffset(undoFile.beforeSelectionAnchor, *model, planFile);
            document.buffer.selectionActive = undoFile.beforeSelectionActive && document.buffer.selectionAnchor != document.buffer.caret;
            DocumentUpdateSyntax(&document);
            undoFile.afterDocumentGeneration = document.buffer.generation;
            undoFile.afterCaret = document.buffer.caret;
            undoFile.afterSelectionAnchor = document.buffer.selectionAnchor;
            undoFile.afterSelectionActive = document.buffer.selectionActive;
            undoFile.afterLength = document.buffer.length;
            undoFile.afterHash = hashBytes(document.buffer.data, document.buffer.length);
            undoFile.applied = true;
        } else {
            uint32_t length = 0;
            uint64_t size = 0;
            if (!readClosedFile(fileSystem, absolute, g_renameReadScratch, &length, &size) ||
                !validatePlanBuffer(*model, planFile, g_renameReadScratch, length) ||
                !TextBufferSet(&g_renameTemporaryBuffer, g_renameReadScratch, length) ||
                !applyForwardToBuffer(&g_renameTemporaryBuffer, *model, planFile)) {
                failed = true;
                model->error = RenameErrorCode::ExpectedTextMismatch;
                break;
            }
            uint32_t written = 0;
            if (!fileSystem.write || !fileSystem.write(fileSystem.userData, absolute,
                                                        g_renameTemporaryBuffer.data,
                                                        g_renameTemporaryBuffer.length, &written) ||
                written != g_renameTemporaryBuffer.length) {
                failed = true;
                model->error = RenameErrorCode::WriteFailed;
                break;
            }
            undoFile.afterLength = g_renameTemporaryBuffer.length;
            undoFile.afterHash = hashBytes(g_renameTemporaryBuffer.data, g_renameTemporaryBuffer.length);
            undoFile.applied = true;
        }
    }
    if (failed) {
        bool rollbackFailed = false;
        if (!restoreAppliedRecord(&record, workspace, fileSystem, static_cast<uint32_t>(projectGeneration), &rollbackFailed)) {
            model->error = RenameErrorCode::RollbackFailed;
            setStatus(model, RenameErrorName(model->error));
        } else if (model->error == RenameErrorCode::None) {
            model->error = RenameErrorCode::WriteFailed;
            setStatus(model, RenameErrorName(model->error));
        }
        model->state = RenameState::Failed;
        return false;
    }
    for (uint32_t i = 0; i < model->plan.fileCount; ++i) {
        char absolute[kMaxPathBytes] = {};
        if (!absoluteFor(*workspace, model->plan.files[i].relativePath, absolute, sizeof(absolute))) continue;
        if (model->plan.files[i].openDocument) {
            const int index = FindOpenDocument(workspace, absolute);
            if (index >= 0 && database) SymbolDatabaseIndexDocument(database, absolute,
                workspace->documents[index].documentId, workspace->documents[index].buffer.generation,
                workspace->documents[index].buffer.dirty, workspace->documents[index].buffer.data,
                workspace->documents[index].buffer.length);
        } else if (database) SymbolDatabaseIndexDiskDocument(database, fileSystem, absolute, projectGeneration);
    }
    if (undoManager) pushUndo(undoManager, record);
    model->error = RenameErrorCode::None;
    model->state = RenameState::Applied;
    setStatus(model, "Rename applied. Ctrl+Z undoes the complete workspace operation.");
    return true;
}

bool RenameUndoLast(RenameUndoManager* undoManager, WorkspaceModel* workspace,
                    const WorkspaceFileSystem& fileSystem, SymbolDatabase* database,
                    const char* projectId, uint64_t projectGeneration,
                    RenameErrorCode* error) {
    if (error) *error = RenameErrorCode::None;
    if (!undoManager || !workspace || undoManager->count == 0) { if (error) *error = RenameErrorCode::UndoUnavailable; return false; }
    uint32_t index = undoManager->nextIndex == 0 ? kRenameMaxUndoTransactions - 1u : undoManager->nextIndex - 1u;
    RenameUndoRecord& record = undoManager->records[index];
    if (!record.active || !validateUndoRecord(record, workspace, fileSystem, projectId, projectGeneration)) {
        if (error) *error = RenameErrorCode::UndoStale;
        return false;
    }
    bool appliedAny = false;
    for (uint32_t fileIndex = 0; fileIndex < record.fileCount; ++fileIndex) {
        RenameUndoFile& file = record.files[fileIndex];
        char absolute[kMaxPathBytes] = {};
        if (!absoluteFor(*workspace, file.relativePath, absolute, sizeof(absolute))) { if (error) *error = RenameErrorCode::UndoFailed; return false; }
        const int documentIndex = FindOpenDocument(workspace, absolute);
        bool ok = false;
        if (file.openDocument && documentIndex >= 0) {
            Document& document = workspace->documents[documentIndex];
            ok = applyUndoToBuffer(&document.buffer, record, file);
            if (ok) {
                DocumentUpdateSyntax(&document);
                restoreBufferState(&document.buffer, file.beforeCaret, file.beforeSelectionAnchor,
                                   file.beforeSelectionActive, file.beforeDirty, file.beforeDocumentGeneration);
            }
        } else if (!file.openDocument && documentIndex < 0) {
            uint32_t length = 0;
            uint64_t size = 0;
            ok = readClosedFile(fileSystem, absolute, g_renameReadScratch, &length, &size) &&
                TextBufferSet(&g_renameTemporaryBuffer, g_renameReadScratch, length) &&
                applyUndoToBuffer(&g_renameTemporaryBuffer, record, file);
            if (ok) {
                uint32_t written = 0;
                ok = fileSystem.write && fileSystem.write(fileSystem.userData, absolute,
                    g_renameTemporaryBuffer.data, g_renameTemporaryBuffer.length, &written) &&
                    written == g_renameTemporaryBuffer.length;
            }
        }
        if (!ok) {
            bool rollbackOk = true;
            for (uint32_t prior = fileIndex; prior > 0; --prior) {
                RenameUndoFile& priorFile = record.files[prior - 1];
                if (!priorFile.applied) continue;
                if (!restoreUndoneFile(record, priorFile, workspace, fileSystem)) rollbackOk = false;
                priorFile.applied = false;
            }
            if (error) *error = rollbackOk ? RenameErrorCode::UndoFailed : RenameErrorCode::RollbackFailed;
            return false;
        }
        appliedAny = true;
        file.applied = true;
        if (database) {
            if (file.openDocument && documentIndex >= 0) {
                const Document& document = workspace->documents[documentIndex];
                SymbolDatabaseIndexDocument(database, absolute, document.documentId, document.buffer.generation,
                                            document.buffer.dirty, document.buffer.data, document.buffer.length);
            } else SymbolDatabaseIndexDiskDocument(database, fileSystem, absolute, projectGeneration);
        }
    }
    if (!appliedAny) { if (error) *error = RenameErrorCode::UndoFailed; return false; }
    record.active = false;
    if (undoManager->count > 0) --undoManager->count;
    undoManager->nextIndex = index;
    if (error) *error = RenameErrorCode::None;
    return true;
}

bool RenameUndoAvailable(const RenameUndoManager* manager) {
    return manager && manager->count != 0;
}

} // namespace developer_studio
} // namespace guidexos
