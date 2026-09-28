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

struct ApplicationManifestEntry {
    char architecture[32];
    char path[kMaxProjectPathBytes];
    char entryPoint[kMaxNameBytes];
    char abi[kMaxNameBytes];
    char runtime[32];
};

struct ApplicationManifest {
    static const uint32_t kMaxEntries = 4;
    uint32_t schemaVersion;
    char id[kMaxProjectIdBytes];
    char displayName[kMaxProjectDisplayNameBytes];
    char kind[32];
    char architecture[32];
    char path[kMaxProjectPathBytes];
    char entryPoint[kMaxNameBytes];
    char abi[kMaxNameBytes];
    char runtime[32];
    ApplicationManifestEntry entries[kMaxEntries];
    uint32_t entryCount;
    bool hasSchema;
    bool hasId;
    bool hasDisplayName;
    bool hasKind;
    bool hasEntry;
};

struct ManifestValidationGeneration {
    uint64_t requestId;
    uint64_t requestGeneration;
    uint64_t candidateId;
    uint64_t candidateGeneration;
    uint64_t expectedIdentityGeneration;
    uint64_t parsedIdentityGeneration;
};

enum class ManifestIdentityMismatchField {
    None = 0,
    SchemaVersion,
    AppId,
    DisplayName,
    Kind,
    EntryCount,
    Architecture,
    Path,
    EntryPoint,
    Abi,
    Runtime,
    ExpectedGeneration,
    ParsedGeneration,
    CandidateGeneration
};

struct ManifestValidationDiagnostic {
    bool available;
    uint32_t validationCount;
    char validationRole[24];
    char projectMetadataPath[kMaxPathBytes];
    char manifestPath[kMaxPathBytes];
    uint64_t manifestExpectedSize;
    uint64_t manifestBytesRead;
    uint64_t manifestContentHashFnv1a64;
    uint64_t projectMetadataExpectedSize;
    uint64_t projectMetadataBytesRead;
    uint64_t projectMetadataHashFnv1a64;
    ManifestValidationGeneration generation;
    uint32_t expectedSchemaVersion;
    uint32_t parsedSchemaVersion;
    uint32_t expectedEntryCount;
    uint32_t parsedEntryCount;
    char expectedProjectId[kMaxProjectIdBytes];
    char parsedAppId[kMaxProjectIdBytes];
    char expectedDisplayName[kMaxProjectDisplayNameBytes];
    char parsedDisplayName[kMaxProjectDisplayNameBytes];
    char expectedKind[32];
    char parsedKind[32];
    char expectedArchitecture[32];
    char parsedArchitecture[32];
    char expectedPath[kMaxProjectPathBytes];
    char parsedPath[kMaxProjectPathBytes];
    char expectedEntryPoint[kMaxNameBytes];
    char parsedEntryPoint[kMaxNameBytes];
    char expectedAbi[kMaxNameBytes];
    char parsedAbi[kMaxNameBytes];
    char expectedRuntime[32];
    char parsedRuntime[32];
    char mismatchExpected[kMaxPathBytes];
    char mismatchActual[kMaxPathBytes];
    ManifestIdentityMismatchField mismatchField;
    ProjectErrorCode resultCode;
};

/* Bounded per-load scratch. Runtime controllers own one instance so two
 * project transactions cannot alias manifest bytes or parsed identity. */
struct ProjectLoadScratch {
    char normalizedPath[kMaxPathBytes];
    char rootPath[kMaxPathBytes];
    char metadataPath[kMaxPathBytes];
    char metadataBytes[kMaxProjectFileBytes + 1];
    char manifestPath[kMaxPathBytes];
    char manifestBytes[kMaxProjectFileBytes + 1];
    ApplicationManifest manifest;
};

struct ProjectOperationResult {
    bool success;
    bool rollbackAttempted;
    bool rollbackSucceeded;
    ProjectErrorCode error;
    Project project;
    ManifestValidationDiagnostic manifestDiagnostic;
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
uint64_t ComputeManifestContentHashFnv1a64(const char* bytes, uint32_t length);
bool ParseApplicationManifest(const char* bytes, uint32_t length, ApplicationManifest* output, ProjectErrorCode* error);
bool ValidateApplicationManifestIdentity(const ApplicationManifest& manifest, const Project& project,
                                         const ManifestValidationGeneration* generation,
                                         ManifestValidationDiagnostic* diagnostic);
const char* ManifestIdentityMismatchFieldName(ManifestIdentityMismatchField field);
bool SerializeProjectMetadata(const Project& project, char* output, uint32_t outputSize, uint32_t* outBytes, ProjectErrorCode* error);
bool ValidateProjectCreateRequest(const ProjectCreateRequest& request, ProjectErrorCode* error);
bool CreateNativeGuiProject(const ProjectFileSystem& fileSystem, const ProjectCreateRequest& request,
                            ProjectOperationResult* result, ProjectLoadScratch* scratch = nullptr);
bool LoadProject(const ProjectFileSystem& fileSystem, const char* rootOrMetadataPath,
                 ProjectOperationResult* result, ProjectLoadScratch* scratch = nullptr,
                 const ManifestValidationGeneration* generation = nullptr);

} // namespace developer_studio
} // namespace guidexos
