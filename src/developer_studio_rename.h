#pragma once

#include <stdint.h>

#include "developer_studio_references.h"
#include "developer_studio_workspace.h"

namespace guidexos {
namespace developer_studio {

// Rename deliberately uses the existing Find All References bounds.  The
// current editor's reference service retains at most 256 files and 4,096
// matches, so Rename never creates a larger second scan or transaction.
static const uint32_t kRenameMaxFiles = kReferenceMaxResultFiles;
static const uint32_t kRenameMaxTotalEdits = kReferenceMaxTotalMatches;
static const uint32_t kRenameMaxEditsPerFile = kReferenceMaxMatchesPerFile;
static const uint32_t kRenameMaxIdentifierBytes = kDefinitionMaxIdentifierBytes;
static const uint32_t kRenameMaxTextBytes = kRenameMaxIdentifierBytes + 2u;
static const uint32_t kRenameMaxQualifiedNameBytes = kReferenceMaxQualifiedNameBytes;
static const uint32_t kRenameMaxPreviewBytes = kReferenceMaxPreviewBytes;
static const uint32_t kRenameMaxUndoTransactions = 16u;

enum class RenameState {
    Idle = 0,
    Preview,
    Applying,
    Applied,
    Cancelled,
    Failed
};

enum class RenameCandidateState {
    Selected = 0,
    Unselected,
    Disabled,
    Stale,
    Conflict
};

enum class RenameConflictSeverity {
    None = 0,
    Warning,
    Blocking
};

enum class RenameErrorCode {
    None = 0,
    NoProject,
    NoTarget,
    TargetAmbiguous,
    UnsupportedSymbol,
    MacroUnsupported,
    InvalidIdentifier,
    EmptyName,
    NameTooLong,
    Keyword,
    QualifiedName,
    Unchanged,
    Conflict,
    NoSelectedEdits,
    FileLimit,
    EditLimit,
    SnapshotLimit,
    PathInvalid,
    PathOutsideProject,
    ProjectStale,
    DocumentStale,
    FileStale,
    ExpectedTextMismatch,
    Overlap,
    ReadOnly,
    ReadFailed,
    WriteFailed,
    RollbackFailed,
    UndoUnavailable,
    UndoStale,
    UndoFailed,
    Cancelled,
    Internal
};

struct RenameEditCandidate {
    uint64_t candidateId;
    char relativePath[kMaxPathBytes];
    uint64_t byteOffset;
    uint32_t line;
    uint32_t column;
    uint32_t oldLength;
    char expectedText[kRenameMaxTextBytes];
    char replacementText[kRenameMaxTextBytes];
    ReferenceKind referenceKind;
    ReferenceConfidence confidence;
    RenameCandidateState state;
    char previewBefore[kRenameMaxPreviewBytes + 1u];
    char previewAfter[kRenameMaxPreviewBytes + 1u];
    uint32_t previewMatchStart;
    uint32_t previewMatchLength;
    uint64_t documentId;
    uint64_t documentGeneration;
    bool fromDirtySnapshot;
    bool editable;
    bool stale;
};

struct RenameTextEdit {
    uint64_t byteOffset;
    uint32_t oldLength;
    char expectedText[kRenameMaxTextBytes];
    char replacementText[kRenameMaxTextBytes];
};

struct RenameFilePlan {
    char relativePath[kMaxPathBytes];
    uint64_t sourceDocumentId;
    uint64_t sourceDocumentGeneration;
    bool openDocument;
    bool dirtyDocument;
    uint32_t firstEditIndex;
    uint32_t editCount;
    uint64_t expectedFileSize;
    uint64_t expectedModificationToken;
    uint64_t expectedContentHash;
    uint64_t outputFileSize;
    uint64_t outputContentHash;
};

struct RenameTransactionPlan {
    uint64_t transactionId;
    ReferenceTarget target;
    char newName[kRenameMaxIdentifierBytes + 1u];
    RenameFilePlan files[kRenameMaxFiles];
    uint32_t fileCount;
    uint32_t totalEdits;
    uint64_t totalBytesBefore;
    uint64_t totalBytesAfter;
    bool containsDirtyDocuments;
    bool containsDiskFiles;
    bool requiresWarnings;
};

struct RenameModel {
    RenameState state;
    RenameErrorCode error;
    ReferenceTarget target;
    char currentName[kRenameMaxIdentifierBytes + 1u];
    char newName[kRenameMaxIdentifierBytes + 1u];
    RenameConflictSeverity conflictSeverity;
    char conflictMessage[160];
    RenameEditCandidate candidates[kRenameMaxTotalEdits];
    uint32_t candidateCount;
    uint32_t exactCount;
    uint32_t likelyCount;
    uint32_t ambiguousCount;
    uint32_t lexicalOnlyCount;
    uint32_t selectedCount;
    uint32_t fileCount;
    RenameTransactionPlan plan;
    RenameTextEdit planEdits[kRenameMaxTotalEdits];
    char status[160];
};

struct RenameUndoEdit {
    uint64_t byteOffset;
    uint32_t oldLength;
    uint32_t newLength;
    bool tildePrefixed;
};

struct RenameUndoFile {
    char relativePath[kMaxPathBytes];
    uint64_t sourceDocumentId;
    uint32_t beforeDocumentGeneration;
    uint32_t afterDocumentGeneration;
    bool openDocument;
    bool beforeDirty;
    uint32_t beforeCaret;
    uint32_t beforeSelectionAnchor;
    bool beforeSelectionActive;
    uint32_t afterCaret;
    uint32_t afterSelectionAnchor;
    bool afterSelectionActive;
    uint32_t beforeLength;
    uint32_t afterLength;
    uint32_t firstEditIndex;
    uint32_t editCount;
    uint64_t beforeHash;
    uint64_t afterHash;
    bool applied;
};

struct RenameUndoRecord {
    bool active;
    uint64_t transactionId;
    uint64_t projectGeneration;
    uint64_t targetId;
    char projectId[kMaxProjectIdBytes];
    char oldName[kRenameMaxIdentifierBytes + 1u];
    char newName[kRenameMaxIdentifierBytes + 1u];
    RenameUndoFile files[kRenameMaxFiles];
    uint32_t fileCount;
    RenameUndoEdit edits[kRenameMaxTotalEdits];
    uint32_t editCount;
};

struct RenameUndoManager {
    RenameUndoRecord records[kRenameMaxUndoTransactions];
    uint32_t nextIndex;
    uint32_t count;
};

const char* RenameStateName(RenameState state);
const char* RenameCandidateStateName(RenameCandidateState state);
const char* RenameConflictSeverityName(RenameConflictSeverity severity);
const char* RenameErrorName(RenameErrorCode error);

void RenameModelInit(RenameModel* model);
void RenameUndoManagerInit(RenameUndoManager* manager);
bool RenameSymbolKindSupported(SymbolKind kind);
bool RenameValidateNewName(const char* originalName, const char* newName, RenameErrorCode* error);
bool RenameDetectConflict(const SymbolDatabase* database, const ReferenceTarget& target,
                          const char* newName, RenameConflictSeverity* severity,
                          char* message, uint32_t messageSize);
bool RenameModelBuildFromReferences(RenameModel* model, const ReferenceSearchService* references);
bool RenameModelSetNewName(RenameModel* model, const char* newName,
                           const SymbolDatabase* database);
bool RenameModelSetCandidateSelected(RenameModel* model, uint32_t index, bool selected);
uint32_t RenameModelSelectExact(RenameModel* model);
void RenameModelClearSelection(RenameModel* model);
uint32_t RenameModelSelectedCount(const RenameModel* model);
const char* RenameModelStatus(const RenameModel* model);

bool RenameModelBuildPlan(RenameModel* model, const WorkspaceModel* workspace,
                          const WorkspaceFileSystem& fileSystem, uint64_t transactionId);
bool RenameApply(RenameModel* model, WorkspaceModel* workspace,
                 const WorkspaceFileSystem& fileSystem, SymbolDatabase* database,
                 uint64_t projectGeneration, RenameUndoManager* undoManager,
                 uint64_t transactionId);
bool RenameUndoLast(RenameUndoManager* undoManager, WorkspaceModel* workspace,
                    const WorkspaceFileSystem& fileSystem, SymbolDatabase* database,
                    const char* projectId, uint64_t projectGeneration,
                    RenameErrorCode* error);
bool RenameUndoAvailable(const RenameUndoManager* manager);

} // namespace developer_studio
} // namespace guidexos
