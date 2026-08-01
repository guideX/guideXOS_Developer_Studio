#include "developer_studio_rename.h"

#include <assert.h>
#include <string.h>
#include <string>

using namespace guidexos::developer_studio;

namespace {

struct FakeFile {
    char path[kMaxPathBytes];
    char data[kProjectSearchMaxFileBytes + 1u];
    uint32_t length;
};

struct FakeFileSystem {
    FakeFile files[8];
    uint32_t count;
    bool allowWrite;
};

static FakeFile* findFile(FakeFileSystem* fs, const char* path) {
    if (!fs || !path) return nullptr;
    for (uint32_t i = 0; i < fs->count; ++i) if (strcmp(fs->files[i].path, path) == 0) return &fs->files[i];
    return nullptr;
}

static bool fakeStat(void* userData, const char* path, FileInfo* outInfo) {
    FakeFile* file = findFile(static_cast<FakeFileSystem*>(userData), path);
    if (!file || !outInfo) return false;
    outInfo->kind = FileInfoKind::RegularFile;
    outInfo->size = file->length;
    return true;
}

static bool fakeRead(void* userData, const char* path, char* buffer, uint32_t capacity, uint32_t* outBytes) {
    FakeFile* file = findFile(static_cast<FakeFileSystem*>(userData), path);
    if (!file || !buffer || !outBytes || capacity < file->length) return false;
    memcpy(buffer, file->data, file->length);
    *outBytes = file->length;
    return true;
}

static bool fakeWrite(void* userData, const char* path, const char* buffer, uint32_t bytes, uint32_t* outBytes) {
    FakeFileSystem* fs = static_cast<FakeFileSystem*>(userData);
    FakeFile* file = findFile(fs, path);
    if (!fs || !fs->allowWrite || !file || !buffer || bytes > kProjectSearchMaxFileBytes || !outBytes) return false;
    memcpy(file->data, buffer, bytes);
    file->data[bytes] = '\0';
    file->length = bytes;
    *outBytes = bytes;
    return true;
}

static WorkspaceFileSystem makeFileSystem(FakeFileSystem* fs) {
    WorkspaceFileSystem result = {};
    result.userData = fs;
    result.stat = fakeStat;
    result.read = fakeRead;
    result.write = fakeWrite;
    return result;
}

static void addFile(FakeFileSystem* fs, const char* path, const char* text) {
    FakeFile& file = fs->files[fs->count++];
    strncpy(file.path, path, sizeof(file.path) - 1);
    file.path[sizeof(file.path) - 1] = '\0';
    file.length = static_cast<uint32_t>(strlen(text));
    memcpy(file.data, text, file.length + 1);
}

static void setText(char* output, uint32_t size, const char* value) {
    strncpy(output, value, size - 1);
    output[size - 1] = '\0';
}

static RenameModel g_model;
static RenameUndoManager g_undo;
static WorkspaceModel g_workspace;
static FakeFileSystem g_files;

static void makeReferences(ReferenceSearchService* service) {
    ReferenceSearchServiceInit(service);
    service->operation.state = ReferenceSearchState::Completed;
    service->operation.target = {};
    service->operation.target.targetId = 77;
    service->operation.target.projectGeneration = g_workspace.projectGeneration;
    service->operation.target.projectId = 1;
    setText(service->operation.target.projectIdText, sizeof(service->operation.target.projectIdText), "rename-test");
    setText(service->operation.target.identifier, sizeof(service->operation.target.identifier), "Draw");
    setText(service->operation.target.qualifiedName, sizeof(service->operation.target.qualifiedName), "ns::Draw");
    setText(service->operation.target.containingScope, sizeof(service->operation.target.containingScope), "ns");
    setText(service->operation.target.signature, sizeof(service->operation.target.signature), "void Draw()");
    service->operation.target.kind = SymbolKind::Function;
    service->operation.target.role = SymbolDeclarationRole::Definition;
    service->operation.target.hasQualifiedIdentity = true;
    service->operation.target.hasSignature = true;

    strcpy(service->groups[0].relativePath, "src/open.cpp");
    service->groups[0].firstMatchIndex = 0;
    service->groups[0].matchCount = 2;
    strcpy(service->groups[1].relativePath, "src/closed.cpp");
    service->groups[1].firstMatchIndex = 2;
    service->groups[1].matchCount = 2;
    service->operation.referencesFound = 4;

    ReferenceMatch& exactOpen = service->matches[0];
    exactOpen = {};
    exactOpen.matchId = 1;
    strcpy(exactOpen.relativePath, "src/open.cpp");
    exactOpen.byteOffset = 0;
    exactOpen.line = 1;
    exactOpen.column = 1;
    exactOpen.identifierLength = 4;
    exactOpen.kind = ReferenceKind::Definition;
    exactOpen.confidence = ReferenceConfidence::Exact;
    strcpy(exactOpen.previewText, "Draw();");
    exactOpen.previewMatchLength = 4;
    exactOpen.sourceDocumentId = g_workspace.documents[0].documentId;
    exactOpen.sourceDocumentGeneration = g_workspace.documents[0].buffer.generation;
    exactOpen.fromDirtySnapshot = true;

    ReferenceMatch& likelyOpen = service->matches[1];
    likelyOpen = exactOpen;
    likelyOpen.matchId = 2;
    likelyOpen.byteOffset = 7;
    likelyOpen.line = 2;
    likelyOpen.column = 1;
    likelyOpen.confidence = ReferenceConfidence::Likely;

    ReferenceMatch& exactClosed = service->matches[2];
    exactClosed = {};
    exactClosed.matchId = 3;
    strcpy(exactClosed.relativePath, "src/closed.cpp");
    exactClosed.byteOffset = 5;
    exactClosed.line = 1;
    exactClosed.column = 6;
    exactClosed.identifierLength = 4;
    exactClosed.kind = ReferenceKind::FunctionCall;
    exactClosed.confidence = ReferenceConfidence::Exact;
    strcpy(exactClosed.previewText, "void Draw();");
    exactClosed.previewMatchStart = 5;
    exactClosed.previewMatchLength = 4;

    ReferenceMatch& ambiguous = service->matches[3];
    ambiguous = exactClosed;
    ambiguous.matchId = 4;
    ambiguous.byteOffset = 10;
    ambiguous.line = 2;
    ambiguous.column = 6;
    ambiguous.confidence = ReferenceConfidence::Ambiguous;
}

static void testValidation() {
    RenameErrorCode error = RenameErrorCode::None;
    assert(RenameValidateNewName("Draw", "Paint", &error));
    assert(RenameValidateNewName("Draw", "draw", &error));
    assert(!RenameValidateNewName("Draw", "", &error) && error == RenameErrorCode::EmptyName);
    assert(!RenameValidateNewName("Draw", "9Draw", &error) && error == RenameErrorCode::InvalidIdentifier);
    assert(!RenameValidateNewName("Draw", "two words", &error));
    assert(!RenameValidateNewName("Draw", "ns::Paint", &error) && error == RenameErrorCode::QualifiedName);
    assert(!RenameValidateNewName("Draw", "class", &error) && error == RenameErrorCode::Keyword);
    assert(!RenameValidateNewName("Draw", "Draw", &error) && error == RenameErrorCode::Unchanged);
    std::string oversized(kRenameMaxIdentifierBytes + 1u, 'a');
    assert(!RenameValidateNewName("Draw", oversized.c_str(), &error) && error == RenameErrorCode::NameTooLong);
}

static void testBuildApplyUndo() {
    g_files = {};
    g_files.allowWrite = true;
    addFile(&g_files, "/project/src/closed.cpp", "void Draw();\n");
    addFile(&g_files, "/project/src/open.cpp", "Draw();\nDraw();\n");
    WorkspaceModelInit(&g_workspace);
    assert(WorkspaceModelSetRoot(&g_workspace, "/project", "Rename Test"));
    g_workspace.hasProject = true;
    setText(g_workspace.project.projectId, sizeof(g_workspace.project.projectId), "rename-test");
    ModelErrorCode modelError = ModelErrorCode::None;
    bool duplicate = false;
    assert(WorkspaceModelAddDocument(&g_workspace, "/project/src/open.cpp", g_files.files[1].data,
                                     g_files.files[1].length, &modelError, &duplicate));
    SetCaretOffset(&g_workspace.documents[0].buffer, g_workspace.documents[0].buffer.length);
    assert(TextBufferInsert(&g_workspace.documents[0].buffer, "//dirty\n", 8));

    static ReferenceSearchService references = {};
    makeReferences(&references);
    RenameModelInit(&g_model);
    assert(RenameModelBuildFromReferences(&g_model, &references));
    assert(g_model.exactCount == 2 && g_model.likelyCount == 1 && g_model.ambiguousCount == 1);
    assert(RenameModelSelectedCount(&g_model) == 2);
    assert(g_model.candidates[0].state == RenameCandidateState::Selected);
    assert(g_model.candidates[1].state == RenameCandidateState::Unselected);
    assert(g_model.candidates[3].state == RenameCandidateState::Unselected);
    assert(RenameModelSetNewName(&g_model, "Paint", nullptr));

    SymbolDatabase database = {};
    ProjectSymbol symbols[4] = {};
    SymbolDocument documents[1] = {};
    DocumentSymbol scratch[8] = {};
    SymbolDatabaseInit(&database, symbols, 4, documents, 1, scratch, 8);
    setText(symbols[0].symbol.name, sizeof(symbols[0].symbol.name), "Paint");
    setText(symbols[0].symbol.qualifiedName, sizeof(symbols[0].symbol.qualifiedName), "ns::Paint");
    setText(symbols[0].symbol.signature, sizeof(symbols[0].symbol.signature), "void Paint()");
    symbols[0].symbol.kind = SymbolKind::Function;
    database.projectSymbolCount = 1;
    assert(RenameModelSetNewName(&g_model, "Paint", &database));
    assert(g_model.conflictSeverity == RenameConflictSeverity::Blocking);
    assert(RenameModelSetNewName(&g_model, "Painted", nullptr));

    RenameUndoManagerInit(&g_undo);
    WorkspaceFileSystem fileSystem = makeFileSystem(&g_files);
    assert(RenameApply(&g_model, &g_workspace, fileSystem, &database, g_workspace.projectGeneration,
                       &g_undo, 99));
    assert(strcmp(g_workspace.documents[0].buffer.data, "Painted();\nDraw();\n//dirty\n") == 0);
    assert(strcmp(g_files.files[0].data, "void Painted();\n") == 0);
    assert(RenameUndoAvailable(&g_undo));
    RenameErrorCode undoError = RenameErrorCode::None;
    assert(RenameUndoLast(&g_undo, &g_workspace, fileSystem, &database, "rename-test",
                          g_workspace.projectGeneration, &undoError));
    assert(undoError == RenameErrorCode::None);
    assert(strcmp(g_workspace.documents[0].buffer.data, "Draw();\nDraw();\n//dirty\n") == 0);
    assert(strcmp(g_files.files[0].data, "void Draw();\n") == 0);
}

static void testStaleApply() {
    static ReferenceSearchService references = {};
    makeReferences(&references);
    RenameModelInit(&g_model);
    assert(RenameModelBuildFromReferences(&g_model, &references));
    assert(RenameModelSetNewName(&g_model, "Paint", nullptr));
    SetCaretOffset(&g_workspace.documents[0].buffer, g_workspace.documents[0].buffer.length);
    assert(TextBufferInsert(&g_workspace.documents[0].buffer, "//changed\n", 10));
    WorkspaceFileSystem fileSystem = makeFileSystem(&g_files);
    RenameUndoManagerInit(&g_undo);
    assert(!RenameApply(&g_model, &g_workspace, fileSystem, nullptr, g_workspace.projectGeneration,
                        &g_undo, 100));
    assert(strcmp(g_files.files[0].data, "void Draw();\n") == 0);
    assert(g_model.error == RenameErrorCode::DocumentStale || g_model.error == RenameErrorCode::ExpectedTextMismatch);
}

} // namespace

int main() {
    testValidation();
    testBuildApplyUndo();
    testStaleApply();
    return 0;
}
