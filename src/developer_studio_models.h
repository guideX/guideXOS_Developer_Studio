#pragma once

#include <stdint.h>

#include "developer_studio_find.h"
#include "developer_studio_syntax.h"

namespace guidexos {
namespace developer_studio {

enum class ProjectKind {
    NativeGuiApplication = 0,
    ConsoleApplication,
    ComponentLibrary,
    WebsiteOrHttpService,
    Game2D,
    SystemComponent,
    ExperimentalOther
};

enum class ProjectLoadState {
    NotLoaded = 0,
    Loaded,
    Invalid
};

enum class ProjectValidationState {
    Unknown = 0,
    Valid,
    Invalid
};

enum class ProjectErrorCode {
    None = 0,
    NullInput,
    ProjectFileTooLarge,
    MalformedJson,
    DuplicateField,
    UnknownField,
    MissingField,
    UnsupportedFormatVersion,
    InvalidProjectId,
    InvalidDisplayName,
    InvalidProjectKind,
    InvalidRelativePath,
    InvalidEntryPoint,
    InvalidAbi,
    InvalidArchitecture,
    UnknownTargetProfile,
    InvalidTargetProfile,
    InvalidOutputName,
    InvalidFolderName,
    InvalidParentPath,
    UnsavedChanges,
    DestinationExists,
    ParentNotFound,
    ParentNotDirectory,
    DirectoryCreateFailed,
    FileWriteFailed,
    FileReadFailed,
    RequiredFileMissing,
    ManifestMalformed,
    ManifestIdentityMismatch,
    ProjectIdCollision,
    RollbackFailed
};

enum class CapabilityMaturity {
    Unavailable = 0,
    Experimental,
    Partial,
    Supported,
    Deprecated
};

static const uint32_t kMaxPathBytes = 768;
static const uint32_t kMaxNameBytes = 128;
#if defined(GXOS_DEVELOPER_STUDIO_BARE_METAL)
// The embedded Phase 27E proof keeps the same model/controller APIs while
// selecting bounded capacities appropriate for a NativeElf application.
static const uint32_t kMaxWorkspaceEntries = 32;
static const uint32_t kMaxOpenDocuments = 2;
static const uint32_t kMaxEditorBytes = 16u * 1024u;
#else
static const uint32_t kMaxWorkspaceEntries = 128;
static const uint32_t kMaxOpenDocuments = 8;
static const uint32_t kMaxEditorBytes = 256u * 1024u;
#endif
static const uint32_t kMaxWorkspaceDepth = 8;
static const uint32_t kMaxProjectFileBytes = 16u * 1024u;
static const uint32_t kMaxProjectIdBytes = 96;
static const uint32_t kMaxProjectDisplayNameBytes = 96;
static const uint32_t kMaxProjectPathBytes = 160;
static const uint32_t kMaxProjectOutputNameBytes = 96;

struct Capability {
    const char* id;
    const char* displayName;
    bool available;
    CapabilityMaturity maturity;
};

struct TargetProfile {
    const char* id;
    const char* displayName;
    const char* architecture;
    const char* abi;
    const char* machine;
    const char* sdk;
    const char* toolchain;
    const char* runner;
    const Capability* capabilities;
    unsigned capabilityCount;
    CapabilityMaturity maturity;
};

struct Project {
    bool loaded;
    bool valid;
    uint32_t formatVersion;
    ProjectKind kind;
    char projectId[kMaxNameBytes];
    char displayName[kMaxNameBytes];
    char rootPath[kMaxPathBytes];
    char sourceRoot[kMaxPathBytes];
    char manifestPath[kMaxPathBytes];
    char targetProfileId[kMaxNameBytes];
    char entryPoint[kMaxNameBytes];
    char abi[kMaxNameBytes];
    char architecture[32];
    char outputName[kMaxNameBytes];
    // Optional deterministic compiler entry. Empty preserves the original
    // project-file shape; when present it is relative to sourceRoot.
    char sourceEntry[kMaxProjectPathBytes];
    ProjectLoadState loadState;
    ProjectValidationState validationState;
    ProjectErrorCode error;
};

enum class WorkspaceEntryKind {
    Directory = 0,
    SupportedTextFile,
    UnsupportedFile
};

enum class ModelErrorCode {
    None = 0,
    WorkspaceNotOpen,
    InvalidPath,
    OutsideWorkspace,
    NotDirectory,
    NotFile,
    UnsupportedFile,
    BinaryFile,
    FileTooLarge,
    TooManyDocuments,
    ReadFailed,
    WriteFailed,
    UnsavedChanges,
    DuplicateDocument,
    DocumentNotFound
};

struct WorkspaceEntry {
    char name[kMaxNameBytes];
    char relativePath[kMaxPathBytes];
    uint64_t size;
    uint32_t depth;
    WorkspaceEntryKind kind;
};

struct TextBuffer {
    char data[kMaxEditorBytes + 1];
    uint32_t length;
    uint32_t caret;
    bool dirty;
    uint32_t generation;
    uint32_t lastMutationStart;
    uint32_t lastMutationFirstLine;
    uint32_t lastMutationInsertedBytes;
    uint32_t lastMutationDeletedBytes;
    int32_t lastMutationLineDelta;
    bool lastMutationFullReplacement;
    bool lastMutationValid;
    uint32_t selectionAnchor;
    bool selectionActive;
};

struct Document {
    bool used;
    uint64_t documentId;
    char path[kMaxPathBytes];
    char name[kMaxNameBytes];
    TextBuffer buffer;
    SyntaxCache syntax;
    FindDocumentState find;
};

struct WorkspaceModel {
    bool open;
    bool hasProject;
    char displayName[kMaxNameBytes];
    char rootPath[kMaxPathBytes];
    char browsePath[kMaxPathBytes];
    Project project;
    WorkspaceEntry entries[kMaxWorkspaceEntries];
    uint32_t entryCount;
    uint32_t selectedEntry;
    Document documents[kMaxOpenDocuments];
    uint32_t activeDocument;
    uint64_t nextDocumentId;
    uint64_t projectGeneration;
    char lastError[96];
};

enum class CloseDecision {
    Save = 0,
    Discard,
    Cancel
};

struct Workspace {
    const char* id;
    const char* displayName;
    const Project* projects;
    unsigned projectCount;
    const char* const* openDocuments;
    unsigned openDocumentCount;
};

const TargetProfile& InitialTargetProfile();
const TargetProfile& BareMetalTargetProfile();
bool IsKnownTargetProfileId(const char* id);
bool IsValidTargetProfile(const TargetProfile& profile);
const char* ToString(ProjectKind kind);
const char* ToString(CapabilityMaturity maturity);

void WorkspaceModelInit(WorkspaceModel* model);
bool NormalizePath(const char* input, char* output, uint32_t outputSize);
bool PathsEqual(const char* left, const char* right);
bool PathContainsTraversal(const char* path);
bool JoinWorkspacePath(const char* root, const char* relative, char* output, uint32_t outputSize);
const char* BaseName(const char* path);
bool IsSupportedTextPath(const char* path);
bool LooksBinary(const char* bytes, uint32_t length);
const char* ModelErrorName(ModelErrorCode code);

bool WorkspaceModelSetRoot(WorkspaceModel* model, const char* normalizedRoot, const char* displayName);
void WorkspaceModelAdvanceProjectGeneration(WorkspaceModel* model);
bool WorkspaceModelSetBrowsePath(WorkspaceModel* model, const char* relativePath);
void WorkspaceModelClearEntries(WorkspaceModel* model);
bool WorkspaceModelAddEntry(WorkspaceModel* model, const WorkspaceEntry& entry);
void WorkspaceModelSortEntries(WorkspaceModel* model);
int FindOpenDocument(const WorkspaceModel* model, const char* normalizedPath);
bool WorkspaceModelAddDocument(WorkspaceModel* model, const char* normalizedPath, const char* bytes, uint32_t length, ModelErrorCode* error, bool* duplicate);
bool WorkspaceModelCloseDocument(WorkspaceModel* model, uint32_t documentIndex, CloseDecision decision, bool saveSucceeded, ModelErrorCode* error);
bool WorkspaceModelMarkSaved(WorkspaceModel* model, uint32_t documentIndex, bool writeSucceeded, ModelErrorCode* error);
bool WorkspaceModelHasDirtyDocuments(const WorkspaceModel* model);

void TextBufferInit(TextBuffer* buffer);
bool TextBufferSet(TextBuffer* buffer, const char* bytes, uint32_t length);
bool TextBufferInsert(TextBuffer* buffer, const char* bytes, uint32_t length);
bool TextBufferBackspace(TextBuffer* buffer);
bool TextBufferDelete(TextBuffer* buffer);
void TextBufferMoveLeft(TextBuffer* buffer);
void TextBufferMoveRight(TextBuffer* buffer);
void TextBufferMoveUp(TextBuffer* buffer);
void TextBufferMoveDown(TextBuffer* buffer);
void TextBufferHome(TextBuffer* buffer);
void TextBufferEnd(TextBuffer* buffer);
uint32_t GetCaretOffset(const TextBuffer* buffer);
bool SetCaretOffset(TextBuffer* buffer, uint64_t offset);
bool SelectTextRange(TextBuffer* buffer, uint64_t start, uint64_t length);
bool ValidateTextRange(const TextBuffer* buffer, uint64_t start, uint64_t length);
uint32_t GetSelectedText(const TextBuffer* buffer, char* output, uint32_t outputSize);
bool ReplaceTextRange(TextBuffer* buffer, uint64_t start, uint64_t length,
                      const char* replacement, uint32_t replacementLength);
bool ReplaceTextRanges(TextBuffer* buffer, const FindMatch* matches, uint32_t matchCount,
                       const char* replacement, uint32_t replacementLength);
bool OffsetToLineColumn(const TextBuffer* buffer, uint64_t offset, uint32_t* outLine, uint32_t* outColumn);
bool LineColumnToOffset(const TextBuffer* buffer, uint32_t line, uint32_t column, uint32_t* outOffset);
uint32_t TextBufferLineCount(const TextBuffer* buffer);
uint32_t TextBufferLineStart(const TextBuffer* buffer, uint32_t line);
uint32_t TextBufferLineEnd(const TextBuffer* buffer, uint32_t line);
void TextBufferClearDirty(TextBuffer* buffer);
void DocumentUpdateSyntax(Document* document);
uint32_t TextBufferVisualColumn(const TextBuffer* buffer, uint32_t lineStart, uint32_t offset, uint32_t tabWidth);
uint32_t TextBufferOffsetForVisualColumn(const TextBuffer* buffer, uint32_t lineStart, uint32_t lineEnd,
                                         uint32_t visualColumn, uint32_t tabWidth);

} // namespace developer_studio
} // namespace guidexos
