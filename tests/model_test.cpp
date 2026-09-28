#include "developer_studio_models.h"

#include "test_check.h"
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
    TEST_CHECK(IsValidTargetProfile(target));
#if defined(GXOS_DEVELOPER_STUDIO_AARCH64)
    TEST_CHECK(std::strcmp(target.architecture, "arm64") == 0);
#else
    TEST_CHECK(std::strcmp(target.architecture, "amd64") == 0);
#endif

    char normalized[kMaxPathBytes];
    TEST_CHECK(NormalizePath("D:\\work\\guidexos\\.\\studio", normalized, sizeof(normalized)));
    TEST_CHECK(std::strcmp(normalized, "d:/work/guidexos/studio") == 0);
    TEST_CHECK(NormalizePath("D:\\work\\guide XOS\\src\\\\main.cpp", normalized, sizeof(normalized)));
    TEST_CHECK(std::strcmp(normalized, "d:/work/guide XOS/src/main.cpp") == 0);
    TEST_CHECK(PathsEqual("D:/work/guide XOS/src/./main.cpp", "d:/WORK/guide XOS/src/main.cpp"));
    TEST_CHECK(PathsEqual("/workspace/src/./sample.cpp", "/workspace/src/sample.cpp"));
    TEST_CHECK(PathContainsTraversal("sub/../sample.cpp"));
    TEST_CHECK(!JoinWorkspacePath("/workspace", "../outside.txt", normalized, sizeof(normalized)));
    TEST_CHECK(JoinWorkspacePath("/workspace", "src/sample.cpp", normalized, sizeof(normalized)));
    TEST_CHECK(std::strcmp(normalized, "/workspace/src/sample.cpp") == 0);
    TEST_CHECK(JoinWorkspacePath("D:/work/guide XOS", "src/main.cpp", normalized, sizeof(normalized)));
    TEST_CHECK(std::strcmp(normalized, "d:/work/guide XOS/src/main.cpp") == 0);
    char longPath[kMaxPathBytes] = "D:/work/guide XOS";
    for (uint32_t i = 0; i < 15; ++i) {
        char segment[16] = {};
        std::snprintf(segment, sizeof(segment), "/segment%02u", i);
        std::strcat(longPath, segment);
    }
    TEST_CHECK(std::strlen(longPath) > 160 && std::strlen(longPath) < kMaxPathBytes);
    TEST_CHECK(NormalizePath(longPath, normalized, sizeof(normalized)));

    TEST_CHECK(IsSupportedTextPath("sample.cpp"));
    TEST_CHECK(IsSupportedTextPath("README.MD"));
    TEST_CHECK(!IsSupportedTextPath("image.png"));
    TEST_CHECK(LooksBinary("abc\0def", 7));
    TEST_CHECK(!LooksBinary("abc\ndef", 7));

    static WorkspaceModel model;
    WorkspaceModelInit(&model);
    TEST_CHECK(WorkspaceModelSetRoot(&model, "/workspace", "workspace"));
    WorkspaceModelAddEntry(&model, entry("zeta.txt", WorkspaceEntryKind::SupportedTextFile));
    WorkspaceModelAddEntry(&model, entry("src", WorkspaceEntryKind::Directory));
    WorkspaceModelAddEntry(&model, entry("Alpha.cpp", WorkspaceEntryKind::SupportedTextFile));
    WorkspaceModelSortEntries(&model);
    TEST_CHECK(std::strcmp(model.entries[0].name, "src") == 0);
    TEST_CHECK(std::strcmp(model.entries[1].name, "Alpha.cpp") == 0);
    TEST_CHECK(std::strcmp(model.entries[2].name, "zeta.txt") == 0);

    const char* original = "one\ntwo\n";
    ModelErrorCode error = ModelErrorCode::None;
    bool duplicate = false;
    TEST_CHECK(WorkspaceModelAddDocument(&model, "/workspace/sample.cpp", original, 8, &error, &duplicate));
    TEST_CHECK(!duplicate);
    TEST_CHECK(model.activeDocument < kMaxOpenDocuments);
    TEST_CHECK(WorkspaceModelAddDocument(&model, "/workspace/./sample.cpp", original, 8, &error, &duplicate));
    TEST_CHECK(duplicate);
    TEST_CHECK(FindOpenDocument(&model, "/workspace/sample.cpp") == static_cast<int>(model.activeDocument));

    TextBuffer& buffer = model.documents[model.activeDocument].buffer;
    TextBufferEnd(&buffer);
    TEST_CHECK(TextBufferInsert(&buffer, "tail", 4));
    TEST_CHECK(buffer.dirty);
    TextBufferHome(&buffer);
    TEST_CHECK(TextBufferInsert(&buffer, "X", 1));
    TextBufferMoveRight(&buffer);
    TEST_CHECK(TextBufferBackspace(&buffer));
    TEST_CHECK(buffer.dirty);
    TEST_CHECK(!WorkspaceModelMarkSaved(&model, model.activeDocument, false, &error));
    TEST_CHECK(buffer.dirty);
    TEST_CHECK(WorkspaceModelMarkSaved(&model, model.activeDocument, true, &error));
    TEST_CHECK(!buffer.dirty);

    for (uint32_t i = 1; i < kMaxOpenDocuments; ++i) {
        char path[kMaxPathBytes];
        std::snprintf(path, sizeof(path), "/workspace/file%u.txt", i);
        TEST_CHECK(WorkspaceModelAddDocument(&model, path, "x", 1, &error, &duplicate));
    }
    TEST_CHECK(model.activeDocument < kMaxOpenDocuments);
    TEST_CHECK(!WorkspaceModelAddDocument(&model, "/workspace/overflow.txt", "x", 1, &error, &duplicate));
    TEST_CHECK(error == ModelErrorCode::TooManyDocuments);

    WorkspaceModelInit(&model);
    TEST_CHECK(WorkspaceModelSetRoot(&model, "/workspace", "workspace"));
    TEST_CHECK(!WorkspaceModelAddDocument(&model, "/outside.txt", "x", 1, &error, &duplicate));
    TEST_CHECK(error == ModelErrorCode::OutsideWorkspace);
    TEST_CHECK(!WorkspaceModelAddDocument(&model, "/workspace/large.txt", "x", kMaxEditorBytes + 1, &error, &duplicate));
    TEST_CHECK(error == ModelErrorCode::FileTooLarge);
    TEST_CHECK(!WorkspaceModelAddDocument(&model, "/workspace/binary.txt", "a\0b", 3, &error, &duplicate));
    TEST_CHECK(error == ModelErrorCode::BinaryFile);

    TEST_CHECK(WorkspaceModelAddDocument(&model, "/workspace/close.txt", "x", 1, &error, &duplicate));
    uint32_t closeIndex = model.activeDocument;
    TEST_CHECK(TextBufferInsert(&model.documents[closeIndex].buffer, "!", 1));
    TEST_CHECK(!WorkspaceModelCloseDocument(&model, closeIndex, CloseDecision::Cancel, false, &error));
    TEST_CHECK(model.documents[closeIndex].used);
    TEST_CHECK(!WorkspaceModelCloseDocument(&model, closeIndex, CloseDecision::Save, false, &error));
    TEST_CHECK(model.documents[closeIndex].used);
    TEST_CHECK(WorkspaceModelCloseDocument(&model, closeIndex, CloseDecision::Discard, false, &error));
    TEST_CHECK(!model.documents[closeIndex].used);

    return 0;
}
