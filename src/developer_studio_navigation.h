#pragma once

#include <stdint.h>

#include "developer_studio_models.h"
#include "developer_studio_symbols.h"

namespace guidexos {
namespace developer_studio {

static const uint32_t kDefinitionMaxIdentifierBytes = 1024u;
static const uint32_t kDefinitionMaxQualifierBytes = 2048u;
static const uint32_t kDefinitionMaxScopeBytes = 2048u;
static const uint32_t kDefinitionMaxCandidates = 1000u;
static const uint32_t kDefinitionMaxVisibleCandidates = 100u;
static const uint32_t kDefinitionMaxSignatureDisplayBytes = 256u;
static const uint32_t kDefinitionMaxQualifiedDisplayBytes = 512u;
static const uint32_t kNavigationHistoryCapacity = 256u;

struct DefinitionIdentifier {
    bool valid;
    bool fromSelection;
    bool tooLong;
    uint32_t start;
    uint32_t length;
    char text[kDefinitionMaxIdentifierBytes + 1];
};

struct DefinitionQuery {
    uint64_t queryId;
    char projectId[kMaxProjectIdBytes];
    uint64_t projectGeneration;
    uint64_t documentId;
    uint64_t documentGeneration;
    char projectRoot[kMaxPathBytes];
    char relativePath[kMaxPathBytes];
    uint64_t caretByteOffset;
    char identifier[kDefinitionMaxIdentifierBytes + 1];
    char lexicalQualifier[kDefinitionMaxQualifierBytes + 1];
    char containingScope[kDefinitionMaxScopeBytes + 1];
    char objectHint[kDefinitionMaxIdentifierBytes + 1];
    SymbolKind likelyKind;
    bool invokedFromSelection;
};

enum DefinitionCandidateReasonFlag {
    DefinitionReasonExactQualifiedName = 1u << 0,
    DefinitionReasonSameScope = 1u << 1,
    DefinitionReasonSameDocument = 1u << 2,
    DefinitionReasonExactCase = 1u << 3,
    DefinitionReasonDefinitionPreferred = 1u << 4,
    DefinitionReasonDeclarationFallback = 1u << 5,
    DefinitionReasonMatchingKind = 1u << 6,
    DefinitionReasonNearbyScope = 1u << 7,
    DefinitionReasonProjectWideNameMatch = 1u << 8
};

struct DefinitionCandidate {
    uint64_t candidateId;
    ProjectSymbol symbol;
    char relativePath[kMaxPathBytes];
    int32_t rankScore;
    bool isDefinition;
    bool isDeclaration;
    bool isForwardDeclaration;
    bool stale;
    uint32_t reasonFlags;
};

enum class DefinitionResolutionKind {
    None = 0,
    Direct,
    Multiple,
    Stale,
    Failed
};

struct DefinitionResolution {
    uint64_t queryId;
    DefinitionResolutionKind kind;
    DefinitionCandidate* candidates;
    uint32_t candidateCount;
    uint32_t visibleCandidateCount;
    bool truncated;
    bool declarationsOnly;
    char statusCode[64];
};

bool ExtractDefinitionIdentifier(const char* text, uint32_t length, uint32_t caret,
                                 bool selectionActive, uint32_t selectionAnchor,
                                 uint32_t selectionCaret, DefinitionIdentifier* output);
bool BuildDefinitionQuery(const Document& document, const char* projectId,
                          uint64_t projectGeneration, const char* projectRoot,
                          const char* relativePath, uint64_t queryId,
                          DefinitionQuery* output);
bool ResolveDefinition(const SymbolDatabase* database, const DefinitionQuery& query,
                       DefinitionCandidate* candidates, uint32_t candidateCapacity,
                       DefinitionResolution* output);
const char* DefinitionResolutionKindName(DefinitionResolutionKind kind);

struct NavigationLocation {
    char projectId[kMaxProjectIdBytes];
    uint64_t projectGeneration;
    uint64_t documentId;
    uint64_t documentGeneration;
    char relativePath[kMaxPathBytes];
    uint64_t caretByteOffset;
    uint64_t selectionStart;
    uint64_t selectionEnd;
    uint32_t line;
    uint32_t column;
    uint32_t viewportTopLine;
};

struct NavigationHistory {
    NavigationLocation back[kNavigationHistoryCapacity];
    NavigationLocation forward[kNavigationHistoryCapacity];
    uint32_t backCount;
    uint32_t forwardCount;
};

void NavigationHistoryInit(NavigationHistory* history);
bool NavigationLocationEqual(const NavigationLocation& left, const NavigationLocation& right);
void NavigationHistoryPush(NavigationHistory* history, const NavigationLocation& location);
bool NavigationHistoryBack(NavigationHistory* history, const NavigationLocation& current,
                           NavigationLocation* destination);
bool NavigationHistoryForward(NavigationHistory* history, const NavigationLocation& current,
                              NavigationLocation* destination);
void NavigationHistoryClear(NavigationHistory* history);

} // namespace developer_studio
} // namespace guidexos
