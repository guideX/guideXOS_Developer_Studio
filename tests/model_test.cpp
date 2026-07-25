#include "developer_studio_models.h"

#include <cassert>
#include <cstdio>
#include <cstring>

using namespace guidexos::developer_studio;

static WorkspaceEntry entry(const char* name, WorkspaceEntryKind kind) {
    WorkspaceEntry value = {};
    std::strncpy(value.name, name, sizeof(value.name) - 1);
    std::strncpy(value.relativePath, name, sizeof(value.relativePath) - 1);
    value.kind = kind;
    return value;
}

int main() {
    const TargetProfile& target = InitialTargetProfile();
    assert(IsValidTargetProfile(target));
    assert(std::strcmp(target.architecture, "amd64") == 0);

    char normalized[kMaxPathBytes];
    assert(NormalizePath("D:\\work\\guidexos\\.\\studio", normalized, sizeof(normalized)));
    assert(std::strcmp(normalized, "d:/work/guidexos/studio") == 0);
    assert(PathsEqual("/workspace/src/./sample.cpp", "/workspace/src/sample.cpp"));
    assert(PathContainsTraversal("sub/../sample.cpp"));
    assert(!JoinWorkspacePath("/workspace", "../outside.txt", normalized, sizeof(normalized)));
    assert(JoinWorkspacePath("/workspace", "src/sample.cpp", normalized, sizeof(normalized)));
    assert(std::strcmp(normalized, "/workspace/src/sample.cpp") == 0);

    assert(IsSupportedTextPath("sample.cpp"));
    assert(IsSupportedTextPath("README.MD"));
    assert(!IsSupportedTextPath("image.png"));
    assert(LooksBinary("abc\0def", 7));
    assert(!LooksBinary("abc\ndef", 7));

    static WorkspaceModel model;
    WorkspaceModelInit(&model);
    assert(WorkspaceModelSetRoot(&model, "/workspace", "workspace"));
    WorkspaceModelAddEntry(&model, entry("zeta.txt", WorkspaceEntryKind::SupportedTextFile));
    WorkspaceModelAddEntry(&model, entry("src", WorkspaceEntryKind::Directory));
    WorkspaceModelAddEntry(&model, entry("Alpha.cpp", WorkspaceEntryKind::SupportedTextFile));
    WorkspaceModelSortEntries(&model);
    assert(std::strcmp(model.entries[0].name, "src") == 0);
    assert(std::strcmp(model.entries[1].name, "Alpha.cpp") == 0);
    assert(std::strcmp(model.entries[2].name, "zeta.txt") == 0);

    const char* original = "one\ntwo\n";
    ModelErrorCode error = ModelErrorCode::None;
    bool duplicate = false;
    assert(WorkspaceModelAddDocument(&model, "/workspace/sample.cpp", original, 8, &error, &duplicate));
    assert(!duplicate);
    assert(model.activeDocument < kMaxOpenDocuments);
    assert(WorkspaceModelAddDocument(&model, "/workspace/./sample.cpp", original, 8, &error, &duplicate));
    assert(duplicate);
    assert(FindOpenDocument(&model, "/workspace/sample.cpp") == static_cast<int>(model.activeDocument));

    TextBuffer& buffer = model.documents[model.activeDocument].buffer;
    TextBufferEnd(&buffer);
    assert(TextBufferInsert(&buffer, "tail", 4));
    assert(buffer.dirty);
    TextBufferHome(&buffer);
    assert(TextBufferInsert(&buffer, "X", 1));
    TextBufferMoveRight(&buffer);
    assert(TextBufferBackspace(&buffer));
    assert(buffer.dirty);
    assert(!WorkspaceModelMarkSaved(&model, model.activeDocument, false, &error));
    assert(buffer.dirty);
    assert(WorkspaceModelMarkSaved(&model, model.activeDocument, true, &error));
    assert(!buffer.dirty);

    for (uint32_t i = 1; i < kMaxOpenDocuments; ++i) {
        char path[kMaxPathBytes];
        std::snprintf(path, sizeof(path), "/workspace/file%u.txt", i);
        assert(WorkspaceModelAddDocument(&model, path, "x", 1, &error, &duplicate));
    }
    assert(model.activeDocument < kMaxOpenDocuments);
    assert(!WorkspaceModelAddDocument(&model, "/workspace/overflow.txt", "x", 1, &error, &duplicate));
    assert(error == ModelErrorCode::TooManyDocuments);

    WorkspaceModelInit(&model);
    assert(WorkspaceModelSetRoot(&model, "/workspace", "workspace"));
    assert(!WorkspaceModelAddDocument(&model, "/outside.txt", "x", 1, &error, &duplicate));
    assert(error == ModelErrorCode::OutsideWorkspace);
    assert(!WorkspaceModelAddDocument(&model, "/workspace/large.txt", "x", kMaxEditorBytes + 1, &error, &duplicate));
    assert(error == ModelErrorCode::FileTooLarge);
    assert(!WorkspaceModelAddDocument(&model, "/workspace/binary.txt", "a\0b", 3, &error, &duplicate));
    assert(error == ModelErrorCode::BinaryFile);

    assert(WorkspaceModelAddDocument(&model, "/workspace/close.txt", "x", 1, &error, &duplicate));
    uint32_t closeIndex = model.activeDocument;
    assert(TextBufferInsert(&model.documents[closeIndex].buffer, "!", 1));
    assert(!WorkspaceModelCloseDocument(&model, closeIndex, CloseDecision::Cancel, false, &error));
    assert(model.documents[closeIndex].used);
    assert(!WorkspaceModelCloseDocument(&model, closeIndex, CloseDecision::Save, false, &error));
    assert(model.documents[closeIndex].used);
    assert(WorkspaceModelCloseDocument(&model, closeIndex, CloseDecision::Discard, false, &error));
    assert(!model.documents[closeIndex].used);

    return 0;
}
