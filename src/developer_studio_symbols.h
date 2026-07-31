#pragma once

#include <stdint.h>

#include "developer_studio_projects.h"

namespace guidexos {
namespace developer_studio {

// Symbol indexing is deliberately lexical.  These limits are part of the
// embedded model contract; callers may provide smaller storage for tests.
static const uint32_t kSymbolMaxProjectSymbols = 100000u;
static const uint32_t kSymbolMaxDocumentSymbols = 20000u;
static const uint32_t kSymbolMaxDocuments = 2048u;
static const uint32_t kSymbolMaxNameBytes = 96u;
static const uint32_t kSymbolMaxContainerBytes = 192u;
static const uint32_t kSymbolMaxQualifiedNameBytes = 512u;
static const uint32_t kSymbolMaxSignatureBytes = 256u;
static const uint32_t kSymbolMaxQueryBytes = 1024u;
static const uint32_t kSymbolMaxVisibleResults = 100u;

enum class SymbolKind {
    Namespace = 0,
    Class,
    Struct,
    Enum,
    Function,
    Method,
    Constructor,
    Destructor,
    GlobalVariable,
    StaticVariable,
    Typedef,
    UsingAlias,
    Union,
    Macro
};

enum class SymbolDeclarationRole {
    Definition = 0,
    Declaration,
    ForwardDeclaration,
    Alias,
    Unknown
};

struct SymbolLocation {
    // documentId and generation make a location safe to compare with an
    // in-memory Document without retaining an editor pointer.
    uint64_t documentId;
    uint32_t generation;
    uint32_t line;
    uint32_t column;
    uint32_t identifierOffset;
    uint32_t identifierLength;
};

struct DocumentSymbol {
    SymbolKind kind;
    SymbolDeclarationRole declarationRole;
    uint32_t ordinal;
    uint16_t depth;
    uint16_t flags;
    char name[kSymbolMaxNameBytes];
    char container[kSymbolMaxContainerBytes];
    char qualifiedName[kSymbolMaxQualifiedNameBytes];
    char signature[kSymbolMaxSignatureBytes];
    SymbolLocation location;
};

struct SymbolDocument {
    bool used;
    bool dirty;
    uint64_t documentId;
    uint32_t generation;
    uint32_t symbolStart;
    uint32_t symbolCount;
    char path[kMaxPathBytes];
};

struct ProjectSymbol {
    uint32_t documentIndex;
    DocumentSymbol symbol;
};

struct SymbolScanResult {
    bool success;
    bool truncated;
    uint32_t symbolCount;
    uint32_t tokenCount;
};

// Storage is supplied by the owner so the model can be tested with small
// arrays while the Native ELF application provides the full bounded limits.
struct SymbolDatabase {
    ProjectSymbol* projectSymbols;
    SymbolDocument* documents;
    DocumentSymbol* scratchSymbols;
    uint32_t projectCapacity;
    uint32_t documentCapacity;
    uint32_t scratchCapacity;
    uint32_t projectSymbolCount;
    uint32_t documentCount;
    uint32_t droppedSymbols;
    uint32_t droppedDocuments;
    uint64_t projectGeneration;
    uint64_t lastIndexedDocumentId;
    uint32_t lastIndexedSymbolCount;
    uint32_t fullIndexCount;
    uint32_t incrementalIndexCount;
    bool projectIndexActive;
    bool truncated;
};

const char* SymbolKindName(SymbolKind kind);
const char* SymbolKindPrefix(SymbolKind kind);
const char* SymbolDeclarationRoleName(SymbolDeclarationRole role);
bool IsSymbolSourcePath(const char* path);

bool ScanDocumentSymbols(const char* text, uint32_t length,
                         uint64_t documentId, uint32_t generation,
                         DocumentSymbol* output, uint32_t capacity,
                         SymbolScanResult* result);

void SymbolDatabaseInit(SymbolDatabase* database,
                        ProjectSymbol* projectStorage, uint32_t projectCapacity,
                        SymbolDocument* documentStorage, uint32_t documentCapacity,
                        DocumentSymbol* scratchStorage, uint32_t scratchCapacity);
void SymbolDatabaseClear(SymbolDatabase* database);

bool SymbolDatabaseIndexDocument(SymbolDatabase* database, const char* path,
                                  uint64_t documentId, uint32_t generation, bool dirty,
                                  const char* text, uint32_t length);
bool SymbolDatabaseIndexDiskDocument(SymbolDatabase* database, const WorkspaceFileSystem& fileSystem,
                                     const char* path, uint64_t projectGeneration);
bool SymbolDatabaseIndexProject(SymbolDatabase* database, const WorkspaceFileSystem& fileSystem,
                                const char* rootPath, const Document* dirtyDocuments,
                                uint32_t dirtyDocumentCount, uint64_t projectGeneration);
bool SymbolDatabaseRemoveDocument(SymbolDatabase* database, const char* path);

uint32_t SymbolDatabaseProjectSymbolCount(const SymbolDatabase* database);
const ProjectSymbol* SymbolDatabaseProjectSymbolAt(const SymbolDatabase* database, uint32_t index);
uint32_t SymbolDatabaseDocumentCount(const SymbolDatabase* database);
const SymbolDocument* SymbolDatabaseDocumentAt(const SymbolDatabase* database, uint32_t index);
const DocumentSymbol* SymbolDatabaseDocumentSymbolAt(const SymbolDatabase* database,
                                                      uint32_t documentIndex, uint32_t symbolIndex);
int32_t SymbolDatabaseFindDocumentByPath(const SymbolDatabase* database, const char* path);
int32_t SymbolDatabaseFindDocumentById(const SymbolDatabase* database, uint64_t documentId);
const char* SymbolDatabaseDocumentPath(const SymbolDatabase* database, uint32_t documentIndex);

uint32_t SymbolDatabaseLookupByFile(const SymbolDatabase* database, const char* path,
                                    uint32_t* indices, uint32_t capacity);
uint32_t SymbolDatabaseLookupByKind(const SymbolDatabase* database, SymbolKind kind,
                                    uint32_t* indices, uint32_t capacity);
uint32_t SymbolDatabaseLookupByName(const SymbolDatabase* database, const char* name,
                                    bool caseSensitive, uint32_t* indices, uint32_t capacity);
uint32_t SymbolDatabaseLookupByPrefix(const SymbolDatabase* database, const char* prefix,
                                      bool caseSensitive, uint32_t* indices, uint32_t capacity);
uint32_t SymbolDatabaseFindSymbols(const SymbolDatabase* database, const char* query,
                                   bool caseSensitive, uint32_t* indices, uint32_t capacity);

bool SymbolDatabaseIsTruncated(const SymbolDatabase* database);

} // namespace developer_studio
} // namespace guidexos
