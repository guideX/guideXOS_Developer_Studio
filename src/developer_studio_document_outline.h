#pragma once

#include "developer_studio_models.h"

namespace guidexos {
namespace developer_studio {

#if defined(GXOS_DEVELOPER_STUDIO_BARE_METAL)
static const uint32_t kDocumentOutlineMaxSymbols = 48u;
#else
static const uint32_t kDocumentOutlineMaxSymbols = 128u;
#endif
static const uint32_t kDocumentOutlineMaxIdentifierBytes = 64u;
static const uint32_t kDocumentOutlineMaxSignatureBytes = 128u;
static const uint32_t kDocumentOutlineMaxLookaheadTokens = 96u;
static const uint32_t kDocumentOutlineMaxLookaheadBytes = 1024u;
static const uint32_t kDocumentOutlineMaxNestingDepth = 32u;
static const uint32_t kDocumentOutlineMaxInputBytes = kMaxEditorBytes;

enum class DocumentOutlineCategory : uint8_t {
    Function = 0,
    Method,
    Class,
    Struct,
    Enum,
    Namespace
};

enum class DocumentOutlineError : uint8_t {
    None = 0,
    InvalidInput,
    UnsupportedLanguage,
    DocumentTooLarge
};

struct DocumentOutlineSymbol {
    char name[kDocumentOutlineMaxIdentifierBytes + 1u];
    char signature[kDocumentOutlineMaxSignatureBytes + 1u];
    DocumentOutlineCategory category;
    uint32_t line;
    uint32_t offset;
};

struct DocumentOutline {
    DocumentOutlineSymbol symbols[kDocumentOutlineMaxSymbols];
    uint32_t count;
    uint32_t totalCount;
    bool truncated;
    DocumentOutlineError error;
    uint64_t documentId;
    uint64_t projectGeneration;
    uint32_t documentGeneration;
    char documentPath[kMaxPathBytes];
};

void DocumentOutlineInit(DocumentOutline* outline);
bool DocumentOutlineScan(const char* path, const char* data, uint32_t length,
                         DocumentOutline* outline);
void DocumentOutlineSetIdentity(DocumentOutline* outline, uint64_t documentId,
                                uint64_t projectGeneration, uint32_t documentGeneration,
                                const char* path);
bool DocumentOutlineIdentityMatches(const DocumentOutline* outline,
                                    uint64_t documentId, uint64_t projectGeneration,
                                    uint32_t documentGeneration, const char* path);
bool DocumentOutlineCanNavigate(const DocumentOutline* outline, uint32_t symbolIndex,
                                uint64_t documentId, uint64_t projectGeneration,
                                uint32_t documentGeneration, const char* path,
                                const char* currentData, uint32_t currentLength,
                                const DocumentOutlineSymbol** outSymbol);
const char* DocumentOutlineCategoryName(DocumentOutlineCategory category);
const char* DocumentOutlineErrorName(DocumentOutlineError error);

} // namespace developer_studio
} // namespace guidexos
