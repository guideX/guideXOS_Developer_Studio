#pragma once

#include <stdint.h>

#include "developer_studio_models.h"
#include "developer_studio_symbols.h"

namespace guidexos {
namespace developer_studio {

// Lightweight Type Intelligence is intentionally a bounded declaration model,
// not a C++ compiler frontend.  The limits are part of the embedded contract.
static const uint32_t kTypeMaxDeclaratorBytes = 512u;
static const uint32_t kTypeMaxTypeSpellingBytes = 192u;
static const uint32_t kTypeMaxCanonicalNameBytes = 192u;
static const uint32_t kTypeMaxAliasDepth = 8u;
static const uint32_t kTypeMaxLookupCandidates = 64u;
static const uint32_t kTypeMaxScopeNesting = 64u;
static const uint32_t kTypeMaxQualifiedNameBytes = 256u;
static const uint32_t kTypeMaxInitializerBytes = 256u;
static const uint32_t kTypeMaxParserTokens = 32768u;
static const uint32_t kTypeMaxParserDocumentBytes = kMaxEditorBytes;
static const uint32_t kTypeMaxHoverRows = 8u;
static const uint32_t kTypeMaxHoverTextBytes = 1024u;
static const uint32_t kTypeMaxRecords = 4096u;
static const uint32_t kTypeMaxDocuments = 2048u;

enum class TypeBaseKind {
    Unknown = 0,
    Primitive,
    Named,
    NullptrLiteral
};

enum class TypeReferenceKind {
    None = 0,
    LValue,
    RValue
};

enum class TypeConfidence {
    Exact = 0,
    Conservative,
    Ambiguous,
    Unknown
};

enum class TypeSource {
    None = 0,
    LocalDeclaration,
    FunctionParameter,
    MemberDeclaration,
    GlobalDeclaration,
    FunctionReturn,
    Typedef,
    UsingAlias,
    LiteralInference,
    FunctionReturnInference,
    UnknownInference
};

enum class TypeDeclarationKind {
    Unknown = 0,
    LocalVariable,
    Parameter,
    Member,
    GlobalVariable,
    Function,
    Class,
    Struct,
    Enum,
    Union,
    Typedef,
    UsingAlias
};

enum class TypeInspectionState {
    Exact = 0,
    Conservative,
    Ambiguous,
    Unknown,
    Stale
};

struct TypeLocation {
    uint64_t documentId;
    uint32_t documentGeneration;
    uint64_t projectGeneration;
    uint32_t line;
    uint32_t column;
    uint32_t identifierOffset;
    uint32_t identifierLength;
    char relativePath[kMaxPathBytes];
};

struct TypeInfo {
    char spelling[kTypeMaxTypeSpellingBytes + 1];
    char canonicalName[kTypeMaxCanonicalNameBytes + 1];
    char baseName[kTypeMaxCanonicalNameBytes + 1];
    char aliasName[kTypeMaxCanonicalNameBytes + 1];
    char resolvedAlias[kTypeMaxCanonicalNameBytes + 1];
    TypeBaseKind baseKind;
    uint16_t pointerDepth;
    TypeReferenceKind referenceKind;
    bool constQualified;
    bool volatileQualified;
    bool hasArrayExtent;
    uint32_t arrayExtent;
    bool aliasCycle;
    bool truncated;
    TypeConfidence confidence;
    TypeSource source;
    TypeLocation declarationLocation;
};

struct TypeRecord {
    bool used;
    TypeDeclarationKind kind;
    uint32_t ordinal;
    uint32_t documentIndex;
    uint64_t documentId;
    uint32_t documentGeneration;
    uint32_t declarationOffset;
    uint32_t scopeStart;
    uint32_t scopeEnd;
    uint16_t scopeDepth;
    uint16_t flags;
    uint32_t ownerOffset;
    char name[kTypeMaxCanonicalNameBytes + 1];
    char qualifiedName[kTypeMaxQualifiedNameBytes + 1];
    char container[kTypeMaxQualifiedNameBytes + 1];
    char aliasTarget[kTypeMaxCanonicalNameBytes + 1];
    char initializer[kTypeMaxInitializerBytes + 1];
    bool hasInitializer;
    TypeInfo type;
};

struct TypeDocument {
    bool used;
    uint64_t documentId;
    uint32_t generation;
    uint32_t recordCount;
    char relativePath[kMaxPathBytes];
    char absolutePath[kMaxPathBytes];
};

struct TypeDatabase {
    TypeRecord* records;
    TypeDocument* documents;
    uint32_t recordCapacity;
    uint32_t documentCapacity;
    uint32_t recordCount;
    uint32_t documentCount;
    uint32_t droppedRecords;
    uint32_t droppedDocuments;
    uint64_t projectGeneration;
    uint64_t symbolDatabaseGeneration;
    uint64_t buildSequence;
    char rootPath[kMaxPathBytes];
    bool current;
    bool truncated;
};

struct TypeInspection {
    bool available;
    bool truncated;
    TypeInspectionState state;
    TypeDeclarationKind declarationKind;
    char identifier[kTypeMaxCanonicalNameBytes + 1];
    char displayType[kTypeMaxTypeSpellingBytes + 1];
    char detail[kTypeMaxHoverTextBytes + 1];
    TypeInfo type;
};

const char* TypeConfidenceName(TypeConfidence confidence);
const char* TypeSourceName(TypeSource source);
const char* TypeDeclarationKindName(TypeDeclarationKind kind);
const char* TypeInspectionStateName(TypeInspectionState state);
const char* TypeBaseKindName(TypeBaseKind kind);
const char* TypeReferenceKindName(TypeReferenceKind kind);

void TypeDatabaseInit(TypeDatabase* database, TypeRecord* recordStorage, uint32_t recordCapacity,
                      TypeDocument* documentStorage, uint32_t documentCapacity);
void TypeDatabaseClear(TypeDatabase* database);
bool TypeDatabaseIndexDocument(TypeDatabase* database, const char* rootPath, const char* path,
                               uint64_t documentId, uint32_t documentGeneration,
                               uint64_t projectGeneration, const char* text, uint32_t length);
bool TypeDatabaseIndexProject(TypeDatabase* database, const WorkspaceFileSystem& fileSystem,
                              const char* rootPath, const Document* dirtyDocuments,
                              uint32_t dirtyDocumentCount, uint64_t projectGeneration,
                              const SymbolDatabase* symbolDatabase);
bool TypeDatabaseIsCurrent(const TypeDatabase* database, uint64_t projectGeneration,
                           uint64_t symbolDatabaseGeneration);
bool TypeDatabaseIsTruncated(const TypeDatabase* database);
uint32_t TypeDatabaseRecordCount(const TypeDatabase* database);
const TypeRecord* TypeDatabaseRecordAt(const TypeDatabase* database, uint32_t index);

bool TypeDatabaseInspectAt(const TypeDatabase* database, const Document& document,
                           uint64_t projectGeneration, uint32_t caretOffset,
                           TypeInspection* output);
bool TypeInspectionIsCurrent(const TypeInspection* inspection, const Document& document,
                             uint64_t projectGeneration);

} // namespace developer_studio
} // namespace guidexos
