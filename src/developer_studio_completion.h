#pragma once

#include <stdint.h>

#include "developer_studio_models.h"
#include "developer_studio_symbols.h"

namespace guidexos {
namespace developer_studio {

// Completion is intentionally lexical. These limits are part of the model
// contract and keep Ctrl+Space independent of project size.
static const uint32_t kCompletionMaxPrefixBytes = 1024u;
static const uint32_t kCompletionMaxQualifierBytes = 2048u;
static const uint32_t kCompletionMaxScopeBytes = 2048u;
static const uint32_t kCompletionMaxContextScanBytes = 8u * 1024u;
static const uint32_t kCompletionMaxCandidateCollection = 5000u;
static const uint32_t kCompletionMaxContextualCollection = 1000u;
static const uint32_t kCompletionMaxRetainedCandidates = 1000u;
static const uint32_t kCompletionMaxVisibleCandidates = 100u;
static const uint32_t kCompletionMaxInsertionBytes = 1024u;
static const uint32_t kCompletionMaxDisplayBytes = 1024u;
static const uint32_t kCompletionMaxQualifiedDisplayBytes = 2048u;
static const uint32_t kCompletionMaxSignatureDisplayBytes = 256u;
static const uint32_t kCompletionMaxDetailDisplayBytes = 512u;
static const uint32_t kCompletionMaxDocumentWordScanBytes = 8u * 1024u * 1024u;
static const uint32_t kCompletionMaxDocumentWords = 20000u;

enum class CompletionCandidateKind {
    Keyword = 0,
    Namespace,
    Class,
    Struct,
    Union,
    Enum,
    Function,
    Method,
    Constructor,
    Destructor,
    Variable,
    StaticVariable,
    Member,
    Typedef,
    UsingAlias,
    DocumentWord,
    Unknown
};

enum class CompletionCandidateSource {
    CurrentScope = 0,
    CurrentDocument,
    ProjectIndex,
    KeywordSet,
    DocumentWordSet
};

enum class CompletionContextKind {
    Identifier = 0,
    QualifiedName,
    NamespaceQualifier,
    TypeQualifier,
    MemberAccessLexical,
    Preprocessor,
    CommentOrString,
    Unsupported
};

enum class CompletionErrorCode {
    None = 0,
    NoProject,
    NoDocument,
    UnsupportedContext,
    InComment,
    InString,
    InCharacter,
    InRawString,
    InPreprocessor,
    PrefixTooLong,
    QualifierTooLong,
    IndexNotReady,
    ProjectStale,
    DocumentStale,
    SessionStale,
    NoResults,
    ResultsTruncated,
    CandidateLimit,
    WordLimit,
    InvalidReplacementRange,
    ExpectedTextMismatch,
    InsertionTooLong,
    InsertionFailed,
    Internal
};

struct CompletionContext {
    uint64_t sessionId;
    uint64_t projectId;
    uint64_t projectGeneration;
    uint64_t documentId;
    uint64_t documentGeneration;
    uint64_t caretByteOffset;
    uint32_t replacementStart;
    uint32_t replacementEnd;
    char prefix[kCompletionMaxPrefixBytes + 1];
    char explicitQualifier[kCompletionMaxQualifierBytes + 1];
    char containingScope[kCompletionMaxScopeBytes + 1];
    CompletionContextKind kind;
    bool manuallyInvoked;
    bool hasExplicitQualifier;
    bool fromSelection;
};

struct DocumentWordEntry {
    char word[kCompletionMaxInsertionBytes + 1];
    uint32_t occurrenceCount;
    uint64_t firstOffset;
};

struct DocumentWordCache {
    DocumentWordEntry* entries;
    uint32_t capacity;
    uint64_t documentId;
    uint64_t documentGeneration;
    uint32_t count;
    bool valid;
    bool truncated;
};

struct CompletionCandidate {
    uint64_t candidateId;
    uint64_t relationshipIdentity;
    char insertionText[kCompletionMaxInsertionBytes + 1];
    char displayText[kCompletionMaxDisplayBytes + 1];
    char qualifiedName[kCompletionMaxQualifiedDisplayBytes + 1];
    char signature[kCompletionMaxSignatureDisplayBytes + 1];
    char detailText[kCompletionMaxDetailDisplayBytes + 1];
    char relativePath[kMaxPathBytes];
    uint32_t line;
    uint32_t column;
    uint32_t overloadCount;
    CompletionCandidateKind kind;
    CompletionCandidateSource source;
    int32_t rankScore;
    bool exactCasePrefix;
    bool caseInsensitivePrefix;
    bool substringMatch;
    bool fromCurrentScope;
    bool fromCurrentDocument;
    bool lexicallyAmbiguous;
};

struct CompletionSession {
    uint64_t sessionId;
    CompletionContext context;
    CompletionCandidate* candidates;
    uint32_t candidateCapacity;
    uint32_t candidateCount;
    uint32_t collectedCount;
    uint32_t selectedIndex;
    uint32_t visibleStart;
    bool active;
    bool truncated;
};

const char* CompletionCandidateKindName(CompletionCandidateKind kind);
const char* CompletionCandidateKindPrefix(CompletionCandidateKind kind);
const char* CompletionCandidateSourceName(CompletionCandidateSource source);
const char* CompletionContextKindName(CompletionContextKind kind);
const char* CompletionErrorName(CompletionErrorCode code);
uint64_t CompletionProjectId(const char* projectId);

void DocumentWordCacheInit(DocumentWordCache* cache, DocumentWordEntry* entries, uint32_t capacity);
bool DocumentWordCacheRefresh(DocumentWordCache* cache, const Document& document,
                              CompletionErrorCode* error);
const DocumentWordEntry* DocumentWordCacheAt(const DocumentWordCache* cache, uint32_t index);

bool CompletionExtractContext(const Document& document, uint64_t projectId,
                              uint64_t projectGeneration, uint64_t sessionId,
                              bool manuallyInvoked, CompletionContext* output,
                              CompletionErrorCode* error);

void CompletionSessionInit(CompletionSession* session, CompletionCandidate* storage,
                           uint32_t capacity);
bool CompletionBuildSession(CompletionSession* session, const Document& document,
                            uint64_t projectId, uint64_t projectGeneration,
                            const SymbolDatabase* database, DocumentWordCache* wordCache,
                            bool manuallyInvoked, CompletionErrorCode* error);
bool CompletionSessionRefresh(CompletionSession* session, const Document& document,
                              uint64_t projectId, uint64_t projectGeneration,
                              const SymbolDatabase* database, DocumentWordCache* wordCache,
                              CompletionErrorCode* error);
const CompletionCandidate* CompletionSessionSelected(const CompletionSession* session);
bool CompletionSessionMove(CompletionSession* session, int32_t delta);
bool CompletionSessionPage(CompletionSession* session, int32_t direction);
bool CompletionSessionHome(CompletionSession* session);
bool CompletionSessionEnd(CompletionSession* session);
void CompletionSessionDismiss(CompletionSession* session);
bool CompletionSessionIsCurrent(const CompletionSession* session, const Document& document,
                                uint64_t projectId, uint64_t projectGeneration,
                                CompletionErrorCode* error);
bool CompletionSessionTextMatches(const CompletionSession* session, const Document& document,
                                  CompletionErrorCode* error);
const char* CompletionStatusText(CompletionErrorCode code);

} // namespace developer_studio
} // namespace guidexos
