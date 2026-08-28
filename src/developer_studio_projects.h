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

/* The project service uses the same small filesystem boundary as the
 * workspace controller. Directory creation/removal are exact-path,
 * non-recursive operations used only by the rollback-aware generator. */
struct WorkspaceFileSystem {
    void* userData;
    bool (*stat)(void* userData, const char* path, FileInfo* outInfo);
    bool (*list)(void* userData, const char* path, FileListEntry* entries, uint32_t capacity, uint32_t* outCount, bool* outTruncated);
    bool (*read)(void* userData, const char* path, char* buffer, uint32_t capacity, uint32_t* outBytes);
    bool (*write)(void* userData, const char* path, const char* buffer, uint32_t bytes, uint32_t* outBytes);
    bool (*createDirectory)(void* userData, const char* path);
    bool (*removePath)(void* userData, const char* path);
};

using ProjectFileSystem = WorkspaceFileSystem;

struct ProjectCreateRequest {
    char parentPath[kMaxPathBytes];
    char folderName[kMaxNameBytes];
    char projectId[kMaxProjectIdBytes];
    char displayName[kMaxProjectDisplayNameBytes];
    ProjectKind kind;
    // Empty selects the existing hosted target. Bare-metal project creation
    // supplies the append-only target id when that capability is advertised.
    char targetProfileId[kMaxNameBytes];
};

struct ProjectOperationResult {
    bool success;
    bool rollbackAttempted;
    bool rollbackSucceeded;
    ProjectErrorCode error;
    Project project;
};

const char* ProjectErrorName(ProjectErrorCode code);
bool IsSupportedProjectKind(ProjectKind kind);
bool ValidateProjectDisplayName(const char* value);
bool ValidateProjectId(const char* value);
bool ValidateProjectFolderName(const char* value);
bool DeriveProjectFolderName(const char* displayName, char* output, uint32_t outputSize);
bool DeriveProjectOutputName(const char* folderName, char* output, uint32_t outputSize);
bool ValidateProjectMetadata(const Project& project, ProjectErrorCode* error);
bool ParseProjectMetadata(const char* bytes, uint32_t length, Project* output, ProjectErrorCode* error);
bool SerializeProjectMetadata(const Project& project, char* output, uint32_t outputSize, uint32_t* outBytes, ProjectErrorCode* error);
bool ValidateProjectCreateRequest(const ProjectCreateRequest& request, ProjectErrorCode* error);
bool CreateNativeGuiProject(const ProjectFileSystem& fileSystem, const ProjectCreateRequest& request, ProjectOperationResult* result);
bool LoadProject(const ProjectFileSystem& fileSystem, const char* rootOrMetadataPath, ProjectOperationResult* result);

} // namespace developer_studio
} // namespace guidexos
