#pragma once

#include "developer_studio_models.h"

namespace guidexos {
namespace developer_studio {

enum class FileInfoKind {
    Unknown = 0,
    RegularFile,
    Directory
};

struct FileInfo {
    FileInfoKind kind;
    uint64_t size;
};

struct FileListEntry {
    char name[kMaxNameBytes];
    FileInfoKind kind;
    uint64_t size;
};

struct WorkspaceFileSystem {
    void* userData;
    bool (*stat)(void* userData, const char* path, FileInfo* outInfo);
    uint32_t (*list)(void* userData, const char* path, FileListEntry* entries, uint32_t capacity, bool* outTruncated);
    bool (*read)(void* userData, const char* path, char* buffer, uint32_t capacity, uint32_t* outBytes);
    bool (*write)(void* userData, const char* path, const char* buffer, uint32_t bytes, uint32_t* outBytes);
};

struct WorkspaceController {
    WorkspaceModel model;
    WorkspaceFileSystem fileSystem;
    bool listingTruncated;
    ModelErrorCode lastError;
};

void WorkspaceControllerInit(WorkspaceController* controller, const WorkspaceFileSystem& fileSystem);
bool WorkspaceControllerOpenWorkspace(WorkspaceController* controller, const char* path);
bool WorkspaceControllerRefresh(WorkspaceController* controller);
bool WorkspaceControllerEnterSelected(WorkspaceController* controller);
bool WorkspaceControllerGoUp(WorkspaceController* controller);
bool WorkspaceControllerOpenDocument(WorkspaceController* controller, const char* path);
bool WorkspaceControllerSaveDocument(WorkspaceController* controller, uint32_t documentIndex);
bool WorkspaceControllerSaveActive(WorkspaceController* controller);
bool WorkspaceControllerSaveAll(WorkspaceController* controller);
bool WorkspaceControllerCloseDocument(WorkspaceController* controller, uint32_t documentIndex, CloseDecision decision);
bool WorkspaceControllerCloseWorkspace(WorkspaceController* controller, CloseDecision decision);
Document* WorkspaceControllerActiveDocument(WorkspaceController* controller);
const Document* WorkspaceControllerActiveDocumentConst(const WorkspaceController* controller);
const char* WorkspaceControllerError(const WorkspaceController* controller);

} // namespace developer_studio
} // namespace guidexos
