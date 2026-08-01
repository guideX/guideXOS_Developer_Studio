#pragma once

#include <stdint.h>

#include "developer_studio_models.h"
#include "developer_studio_symbols.h"

namespace guidexos {
namespace developer_studio {

// Signature Help is deliberately lexical. These limits are part of the
// embedded model contract; the caller supplies storage for retained results.
static const uint32_t kSignatureMaxContextScanBytes = 64u * 1024u;
static const uint32_t kSignatureMaxNestingDepth = 256u;
static const uint32_t kSignatureMaxCallableBytes = 1024u;
static const uint32_t kSignatureMaxQualifierBytes = 2048u;
static const uint32_t kSignatureMaxReceiverHintBytes = 1024u;
static const uint32_t kSignatureMaxContainingScopeBytes = 2048u;
static const uint32_t kSignatureMaxCandidateCollection = 2000u;
static const uint32_t kSignatureMaxRetainedCandidates = 256u;
static const uint32_t kSignatureMaxParameters = 64u;
static const uint32_t kSignatureMaxParameterDisplayBytes = 256u;
static const uint32_t kSignatureMaxParameterNameBytes = 128u;
static const uint32_t kSignatureMaxParameterTypeBytes = 256u;
static const uint32_t kSignatureMaxDisplayBytes = 1024u;
static const uint32_t kSignatureMaxDetailBytes = 512u;

enum class SignatureContextKind {
    FunctionCall = 0,
    QualifiedFunctionCall,
    MethodCallLexical,
    ConstructorCall,
    FunctionPointerCall,
    Unsupported
};

enum class SignatureErrorCode {
    None = 0,
    NoProject,
    NoDocument,
    NoActiveCall,
    UnsupportedContext,
    InComment,
    InString,
    InCharacter,
    InRawString,
    InPreprocessor,
    ContextTooLarge,
    NestingLimit,
    CallableTooLong,
    QualifierTooLong,
    IndexNotReady,
    ProjectStale,
    DocumentStale,
    SessionStale,
    NoCandidates,
    ResultsTruncated,
    CandidateLimit,
    ParseApproximate,
    ParameterParseFailed,
    ReceiverUnresolved,
    Internal
};

struct SignatureInvocationContext {
    uint64_t sessionId;
    uint64_t projectId;
    uint64_t projectGeneration;
    uint64_t documentId;
    uint64_t documentGeneration;
    uint64_t caretByteOffset;
    uint64_t openParenByteOffset;
    uint32_t activeArgumentIndex;
    uint32_t nestingDepth;
    char callableName[kSignatureMaxCallableBytes + 1];
    char explicitQualifier[kSignatureMaxQualifierBytes + 1];
    char containingScope[kSignatureMaxContainingScopeBytes + 1];
    char receiverExpressionHint[kSignatureMaxReceiverHintBytes + 1];
    SignatureContextKind kind;
    bool hasExplicitQualifier;
    bool receiverTypeResolved;
    bool parameterPositionApproximate;
};

struct SignatureParameter {
    char displayText[kSignatureMaxParameterDisplayBytes + 1];
    char name[kSignatureMaxParameterNameBytes + 1];
    char typeText[kSignatureMaxParameterTypeBytes + 1];
    uint32_t displayStart;
    uint32_t displayLength;
    bool hasDefaultValue;
    bool variadic;
    bool displayTruncated;
};

struct SignatureCandidate {
    uint64_t candidateId;
    char callableName[kSignatureMaxCallableBytes + 1];
    char qualifiedName[kSymbolMaxQualifiedNameBytes];
    char displaySignature[kSignatureMaxDisplayBytes + 1];
    char returnType[kSignatureMaxParameterTypeBytes + 1];
    char detailText[kSignatureMaxDetailBytes + 1];
    char relativePath[kMaxPathBytes];
    uint32_t parameterStart;
    uint32_t parameterCount;
    uint32_t line;
    uint32_t column;
    uint32_t byteOffset;
    SymbolKind symbolKind;
    SymbolDeclarationRole declarationRole;
    int32_t rankScore;
    bool exactQualifiedMatch;
    bool sameScope;
    bool sameDocument;
    bool lexicallyAmbiguous;
    bool truncatedParameters;
    bool parameterParseFailed;
};

struct SignatureHelpSession {
    uint64_t sessionId;
    SignatureInvocationContext context;
    SignatureCandidate* candidates;
    SignatureParameter* parameters;
    uint32_t candidateCapacity;
    uint32_t parameterCapacity;
    uint32_t candidateCount;
    uint32_t parameterCount;
    uint32_t collectedCount;
    uint32_t selectedSignatureIndex;
    uint32_t activeParameterIndex;
    bool active;
    bool truncated;
    bool contextStale;
};

const char* SignatureContextKindName(SignatureContextKind kind);
const char* SignatureErrorName(SignatureErrorCode code);
const char* SignatureStatusText(SignatureErrorCode code);
uint64_t SignatureProjectId(const char* projectId);

void SignatureHelpSessionInit(SignatureHelpSession* session,
                              SignatureCandidate* candidateStorage,
                              uint32_t candidateCapacity,
                              SignatureParameter* parameterStorage,
                              uint32_t parameterCapacity);

bool SignatureExtractInvocationContext(const Document& document, uint64_t projectId,
                                       uint64_t projectGeneration, uint64_t sessionId,
                                       SignatureInvocationContext* output,
                                       SignatureErrorCode* error);

bool SignatureHelpBuildSession(SignatureHelpSession* session, const Document& document,
                               uint64_t projectId, uint64_t projectGeneration,
                               const SymbolDatabase* database,
                               SignatureErrorCode* error);
bool SignatureHelpSessionRefresh(SignatureHelpSession* session, const Document& document,
                                 uint64_t projectId, uint64_t projectGeneration,
                                 const SymbolDatabase* database,
                                 SignatureErrorCode* error);

const SignatureCandidate* SignatureHelpSessionSelected(const SignatureHelpSession* session);
const SignatureParameter* SignatureHelpSessionActiveParameter(const SignatureHelpSession* session);
bool SignatureHelpSessionMove(SignatureHelpSession* session, int32_t delta);
bool SignatureHelpSessionPage(SignatureHelpSession* session, int32_t direction);
bool SignatureHelpSessionHome(SignatureHelpSession* session);
bool SignatureHelpSessionEnd(SignatureHelpSession* session);
void SignatureHelpSessionDismiss(SignatureHelpSession* session);
bool SignatureHelpSessionIsCurrent(const SignatureHelpSession* session, const Document& document,
                                   uint64_t projectId, uint64_t projectGeneration,
                                   SignatureErrorCode* error);

} // namespace developer_studio
} // namespace guidexos
