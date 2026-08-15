#pragma once

#include "developer_studio_debug_symbols.h"

namespace guidexos {
namespace developer_studio {

// Phase 11 deliberately describes a debugger-only expression language.  The
// limits are part of the model contract; they also apply to the Native ELF
// build where no general-purpose parser/runtime is available.
static const uint32_t kDebugWatchMaxExpressionBytes = 256;
static const uint32_t kDebugWatchMaxTokens = 96;
static const uint32_t kDebugWatchMaxIdentifierBytes = 64;
static const uint32_t kDebugWatchMaxNumericLiteralBytes = 32;
static const uint32_t kDebugWatchMaxAstNodes = 64;
static const uint32_t kDebugWatchMaxParserDepth = 16;
static const uint32_t kDebugWatchMaxMemberChain = 16;
static const uint32_t kDebugWatchMaxIndexDepth = 8;
static const uint32_t kDebugWatchMaxUnaryDepth = 8;
static const uint32_t kDebugWatchMaxWatches = 32;
static const uint32_t kDebugWatchMaxDiagnosticBytes = 96;

enum class DebugExpressionTokenKind {
    Identifier = 0,
    IntegerLiteral,
    Dot,
    Arrow,
    Star,
    Ampersand,
    LeftBracket,
    RightBracket,
    LeftParen,
    RightParen,
    End,
    EqualEqual,
    NotEqual,
    Less,
    LessEqual,
    Greater,
    GreaterEqual
};

enum class DebugExpressionNodeKind {
    Identifier = 0,
    IntegerLiteral,
    MemberAccess,
    PointerMemberAccess,
    ArrayIndex,
    Dereference,
    AddressOf,
    Comparison
};

enum class DebugExpressionComparisonKind {
    Equal = 0,
    NotEqual,
    Less,
    LessEqual,
    Greater,
    GreaterEqual
};

enum class DebugExpressionParseState {
    Empty = 0,
    Valid,
    ParseError,
    UnsupportedExpression,
    TooLong,
    TooManyTokens,
    TooManyNodes,
    TooDeep
};

struct DebugExpressionNode {
    DebugExpressionNodeKind kind;
    uint16_t left;
    uint16_t right;
    uint16_t sourceOffset;
    uint16_t sourceLength;
    uint64_t integerValue;
    char identifier[kDebugWatchMaxIdentifierBytes];
};

struct DebugExpressionAst {
    bool valid;
    uint8_t reserved[3];
    uint32_t sourceLength;
    uint32_t tokenCount;
    uint32_t nodeCount;
    uint16_t rootNode;
    uint16_t errorOffset;
    DebugExpressionParseState state;
    char diagnostic[kDebugWatchMaxDiagnosticBytes];
    DebugExpressionNode nodes[kDebugWatchMaxAstNodes];
};

enum class DebugWatchState {
    Empty = 0,
    Available,
    ParseError,
    UnsupportedExpression,
    UnknownIdentifier,
    TypeMismatch,
    NotPointer,
    NotAggregate,
    MemberNotFound,
    NullPointer,
    UnreadableTarget,
    IndexOutOfRange,
    AddressUnavailable,
    UnavailableInCallerFrame,
    Stale,
    Running,
    MalformedDebugInfo
};

struct DebugWatchResult {
    uint64_t watchId;
    DebugWatchState state;
    DebugDwarfValueKind valueKind;
    DebugDwarfLocationKind locationKind;
    uint64_t nodeId;
    uint64_t dieOffset;
    uint64_t typeDieOffset;
    uint64_t address;
    uint64_t scalarValue;
    uint64_t sessionGeneration;
    uint64_t stopGeneration;
    uint64_t artifactGeneration;
    uint32_t frameIndex;
    bool hasAddress;
    bool hasScalar;
    bool structured;
    char typeDisplay[kDebugDwarfMaxTypeDisplayBytes];
    char valueDisplay[kDebugDwarfMaxTypeDisplayBytes];
    char diagnostic[kDebugWatchMaxDiagnosticBytes];
};

struct DebugWatchItem {
    bool used;
    uint8_t reserved[3];
    uint64_t id;
    char expression[kDebugWatchMaxExpressionBytes + 1];
    DebugExpressionParseState parseState;
    char parseDiagnostic[kDebugWatchMaxDiagnosticBytes];
    DebugWatchResult result;
};

struct DebugWatchEvaluationContext {
    const DebugDwarfMapper* mapper;
    DebugDwarfFrameContext frame;
    DebugDwarfReadMemoryFn readMemory;
    void* userData;
};

struct DebugWatchCollection {
    uint64_t nextId;
    uint32_t count;
    bool treeValid;
    bool treeStale;
    DebugDwarfVariableView tree;
    DebugWatchItem items[kDebugWatchMaxWatches];
};

const char* DebugExpressionTokenKindName(DebugExpressionTokenKind kind);
const char* DebugExpressionNodeKindName(DebugExpressionNodeKind kind);
const char* DebugExpressionParseStateName(DebugExpressionParseState state);
const char* DebugWatchStateName(DebugWatchState state);

bool DebugExpressionParse(const char* expression, DebugExpressionAst* ast);

bool DebugWatchCollectionInit(DebugWatchCollection* collection);
bool DebugWatchCollectionAdd(DebugWatchCollection* collection, const char* expression,
                             uint64_t* outWatchId);
bool DebugWatchCollectionEdit(DebugWatchCollection* collection, uint64_t watchId,
                              const char* expression);
bool DebugWatchCollectionRemove(DebugWatchCollection* collection, uint64_t watchId);
const DebugWatchItem* DebugWatchCollectionAt(const DebugWatchCollection* collection,
                                             uint32_t index);
DebugWatchItem* DebugWatchCollectionFind(DebugWatchCollection* collection, uint64_t watchId);
const DebugWatchItem* DebugWatchCollectionFindConst(const DebugWatchCollection* collection,
                                                    uint64_t watchId);

bool DebugWatchEvaluateExpression(const DebugExpressionAst& ast, const char* expression,
                                  const DebugWatchEvaluationContext& context,
                                  DebugDwarfVariableView* valueTree,
                                  DebugWatchResult* result);
// Converts an already-evaluated Phase 11 value to the deliberately minimal
// Phase 12 condition truth value. This never reads memory or changes target
// state; pointer validation is limited to the canonical-address check.
bool DebugWatchConvertToTruth(const DebugWatchResult& result, bool* truth,
                              char* diagnostic, uint32_t diagnosticSize);
bool DebugWatchCollectionRefresh(DebugWatchCollection* collection,
                                 const DebugWatchEvaluationContext& context);
bool DebugWatchCollectionExpand(DebugWatchCollection* collection,
                                const DebugWatchEvaluationContext& context,
                                uint64_t watchId);
void DebugWatchCollectionMarkRunning(DebugWatchCollection* collection);
void DebugWatchCollectionMarkStale(DebugWatchCollection* collection);

} // namespace developer_studio
} // namespace guidexos
