#pragma once

#include <stdint.h>

#include "developer_studio_navigation.h"
#include "developer_studio_project_search.h"
#include "developer_studio_relationships.h"

namespace guidexos {
namespace developer_studio {

// Find All References is deliberately lexical and bounded. It shares the
// existing project scanner and syntax tokenizer; these limits do not imply
// semantic completeness.
static const uint32_t kReferenceMaxQualifiedNameBytes = 512u;
static const uint32_t kReferenceMaxContainingScopeBytes = 256u;
static const uint32_t kReferenceMaxSignatureBytes = kSymbolMaxSignatureBytes;
static const uint32_t kReferenceMaxCandidates = kDefinitionMaxCandidates;
static const uint32_t kReferenceMaxVisibleCandidates = kDefinitionMaxVisibleCandidates;
static const uint32_t kReferenceMaxResultFiles = kProjectSearchMaxResultFiles;
static const uint32_t kReferenceMaxTotalMatches = kProjectSearchMaxTotalMatches;
static const uint32_t kReferenceMaxMatchesPerFile = kProjectSearchMaxMatchesPerFile;
static const uint32_t kReferenceMaxPreviewBytes = kProjectSearchMaxPreviewBytes;
static const uint32_t kReferenceMaxDeclarationHints = 512u;
static const uint32_t kReferenceMaxTokensPerOperation = 4000000u;

enum class ReferenceSearchState {
    Idle = 0,
    ResolvingTarget,
    Enumerating,
    Searching,
    Cancelling,
    Completed,
    Cancelled,
    Failed
};

enum class ReferenceKind {
    Definition = 0,
    Declaration,
    ForwardDeclaration,
    AliasDeclaration,
    FunctionCall,
    MethodCall,
    TypeUse,
    NamespaceUse,
    MemberAccess,
    VariableUse,
    PossibleWrite,
    PossibleRead,
    AddressUse,
    LexicalMatch,
    Unknown
};

enum class ReferenceConfidence {
    Exact = 0,
    Likely,
    Ambiguous,
    LexicalOnly
};

enum class ReferenceSearchErrorCode {
    None = 0,
    NoProject,
    NoDocument,
    NoIdentifier,
    IdentifierTooLong,
    TargetNotFound,
    TargetAmbiguous,
    LexicalFallback,
    IndexNotReady,
    ProjectStale,
    DocumentStale,
    EnumerationFailed,
    PathInvalid,
    PathOutsideProject,
    FileReadFailed,
    FileTooLarge,
    BinaryFile,
    ByteLimit,
    TokenLimit,
    FileLimit,
    ResultLimit,
    Timeout,
    Cancelled,
    OperationStale,
    NoResults,
    ActivationStale,
    ActivationFailed,
    Internal
};

struct ReferenceTarget {
    uint64_t targetId;
    uint64_t relationshipSymbolId;
    uint64_t projectId;
    uint64_t projectGeneration;
    uint64_t sourceDocumentId;
    uint64_t sourceDocumentGeneration;
    char projectIdText[kMaxProjectIdBytes];
    char identifier[kDefinitionMaxIdentifierBytes + 1];
    char qualifiedName[kReferenceMaxQualifiedNameBytes + 1];
    char containingScope[kReferenceMaxContainingScopeBytes + 1];
    char signature[kReferenceMaxSignatureBytes + 1];
    SymbolKind kind;
    SymbolDeclarationRole role;
    char declarationPath[kMaxPathBytes];
    uint64_t declarationByteOffset;
    uint32_t declarationLine;
    uint32_t declarationColumn;
    bool hasQualifiedIdentity;
    bool hasSignature;
    bool lexicallyAmbiguous;
};

enum class ReferenceTargetResolutionKind {
    None = 0,
    Direct,
    Multiple,
    Stale,
    LexicalFallback,
    Failed
};

struct ReferenceTargetResolution {
    uint64_t queryId;
    ReferenceTargetResolutionKind kind;
    uint32_t candidateCount;
    uint32_t visibleCandidateCount;
    bool truncated;
    bool lexicalFallback;
    char statusCode[64];
};

struct ReferenceMatch {
    uint64_t matchId;
    char relativePath[kMaxPathBytes];
    uint64_t byteOffset;
    uint32_t line;
    uint32_t column;
    uint32_t identifierLength;
    ReferenceKind kind;
    ReferenceConfidence confidence;
    char previewText[kReferenceMaxPreviewBytes + 1];
    uint32_t previewMatchStart;
    uint32_t previewMatchLength;
    bool previewLeftTruncated;
    bool previewRightTruncated;
    uint64_t sourceDocumentId;
    uint64_t sourceDocumentGeneration;
    bool fromDirtySnapshot;
    bool stale;
};

struct ReferenceFileGroup {
    char relativePath[kMaxPathBytes];
    uint32_t firstMatchIndex;
    uint32_t matchCount;
    ProjectSearchSourceKind sourceKind;
    uint64_t fileSize;
    uint64_t documentId;
    uint64_t documentGeneration;
};

struct ReferenceSearchOperation {
    uint64_t operationId;
    ReferenceTarget target;
    ReferenceSearchState state;
    uint64_t filesEnumerated;
    uint64_t filesSearched;
    uint64_t bytesScanned;
    uint64_t tokensExamined;
    uint64_t referencesFound;
    bool cancellationRequested;
    bool truncated;
    bool lexicalFallback;
    ReferenceSearchErrorCode error;
    char notice[128];
};

struct ReferenceSearchRequest {
    ReferenceTarget target;
    char projectId[kMaxProjectIdBytes];
    uint64_t projectGeneration;
    char rootPath[kMaxPathBytes];
    ProjectSearchOptions scanOptions;
    WorkspaceFileSystem fileSystem;
    const ProjectSearchDocumentSnapshot* dirtyDocuments;
    uint32_t dirtyDocumentCount;
    // Used only during Start to copy matching declaration records into the
    // operation. The service never retains this pointer.
    const SymbolDatabase* symbolDatabase;
    const SymbolRelationshipGraph* relationshipGraph;
    bool includeDeclarations;
    bool includeAmbiguous;
    bool lexicalFallback;
};

struct ReferenceSymbolHint {
    uint64_t relationshipSymbolId;
    char relativePath[kMaxPathBytes];
    char identifier[kSymbolMaxNameBytes];
    char qualifiedName[kSymbolMaxQualifiedNameBytes];
    char container[kSymbolMaxContainerBytes];
    char signature[kSymbolMaxSignatureBytes];
    uint64_t documentId;
    uint32_t generation;
    uint32_t identifierOffset;
    uint32_t line;
    uint32_t column;
    SymbolKind kind;
    SymbolDeclarationRole role;
};

struct ReferenceSearchService {
    ProjectSearchService scanner;
    ReferenceSearchOperation operation;
    ReferenceFileGroup groups[kReferenceMaxResultFiles];
    ReferenceMatch matches[kReferenceMaxTotalMatches];
    ReferenceSymbolHint declarationHints[kReferenceMaxDeclarationHints];
    SyntaxTokenSpan tokenScratch[kSyntaxMaxTokensPerLine];
    uint32_t declarationHintCount;
    bool includeDeclarations;
    bool includeAmbiguous;
    const SymbolRelationshipGraph* relationshipGraph;
    uint64_t startedAtMs;
    bool terminalReported;
};

const char* ReferenceSearchStateName(ReferenceSearchState state);
const char* ReferenceKindName(ReferenceKind kind);
const char* ReferenceConfidenceName(ReferenceConfidence confidence);
const char* ReferenceSearchErrorName(ReferenceSearchErrorCode error);
const char* ReferenceTargetResolutionKindName(ReferenceTargetResolutionKind kind);

void ReferenceTargetInit(ReferenceTarget* target);
bool ReferenceTargetFromDefinitionCandidate(const DefinitionCandidate& candidate,
                                            const char* projectId,
                                            uint64_t projectGeneration,
                                            ReferenceTarget* output);
bool ReferenceTargetFromDefinitionQuery(const DefinitionQuery& query,
                                        const char* projectId,
                                        uint64_t projectGeneration,
                                        ReferenceTarget* output);
bool ReferenceTargetIsValid(const ReferenceTarget* target);
bool ResolveReferenceTarget(const SymbolDatabase* database, const DefinitionQuery& query,
                            DefinitionCandidate* candidates, uint32_t candidateCapacity,
                            bool allowLexicalFallback,
                            ReferenceTargetResolution* output);

void ReferenceSearchServiceInit(ReferenceSearchService* service);
bool ReferenceSearchStart(ReferenceSearchService* service, const ReferenceSearchRequest* request,
                          uint64_t nowMs, uint64_t* outOperationId,
                          ReferenceSearchErrorCode* outError);
bool ReferenceSearchPoll(ReferenceSearchService* service, uint64_t operationId,
                         uint32_t workBudget, uint64_t nowMs);
bool ReferenceSearchCancel(ReferenceSearchService* service, uint64_t operationId);
bool ReferenceSearchRelease(ReferenceSearchService* service, uint64_t operationId);
bool ReferenceSearchIsActive(const ReferenceSearchService* service);
const ReferenceSearchOperation* ReferenceSearchOperationInfo(const ReferenceSearchService* service);
uint32_t ReferenceSearchResultGroups(const ReferenceSearchService* service);
const ReferenceFileGroup* ReferenceSearchResultGroupAt(const ReferenceSearchService* service,
                                                       uint32_t index);
const ReferenceMatch* ReferenceSearchResultMatchAt(const ReferenceSearchService* service,
                                                   const ReferenceFileGroup* group,
                                                   uint32_t matchIndex);

} // namespace developer_studio
} // namespace guidexos
