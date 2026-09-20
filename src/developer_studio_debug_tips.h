#pragma once

#include <stdint.h>

#include "developer_studio_syntax.h"

namespace guidexos {
namespace developer_studio {

// Data tips deliberately keep a smaller contract than Watches.  They accept
// one lexical identifier and retain only the presentation needed for one
// bounded popup.
static const uint32_t kDebugDataTipMaxIdentifierBytes = 64u;
static const uint32_t kDebugDataTipMaxTextBytes = 128u;

enum class DebugDataTipState {
    Empty = 0,
    Available,
    UnknownIdentifier,
    NotAvailable,
    Running,
    SourceMismatch,
    StaleSource,
    Unsupported
};

struct DebugDataTipToken {
    bool valid;
    uint32_t start;
    uint32_t length;
    char identifier[kDebugDataTipMaxIdentifierBytes + 1];
};

struct DebugDataTipIdentity {
    bool valid;
    uint64_t sessionGeneration;
    uint64_t stopGeneration;
    uint32_t selectedFrameIndex;
    uint64_t documentId;
    uint32_t documentGeneration;
    uint32_t line;
    uint32_t tokenStart;
    uint32_t tokenLength;
    char projectId[96];
    char sourcePath[160];
    char identifier[kDebugDataTipMaxIdentifierBytes + 1];
};

struct DebugDataTipModel {
    bool visible;
    DebugDataTipState state;
    DebugDataTipIdentity identity;
    int anchorX;
    int anchorY;
    char name[kDebugDataTipMaxIdentifierBytes + 1];
    char typeDisplay[kDebugDataTipMaxTextBytes];
    char valueDisplay[kDebugDataTipMaxTextBytes];
};

struct DebugDataTipPopupBounds {
    int x;
    int y;
    int width;
    int height;
};

bool DebugDataTipIsIdentifierStart(char value);
bool DebugDataTipIsIdentifierContinue(char value);

// offset is a byte offset into one source line.  Lexical spans are optional;
// when present they suppress comments, strings, character literals, and
// preprocessor text.  The small fallback scanner is intentionally bounded and
// does not attempt to parse a C/C++ expression.
bool DebugDataTipExtractIdentifier(const char* text, uint32_t length, uint32_t offset,
                                   const SyntaxTokenSpan* spans, uint32_t spanCount,
                                   DebugDataTipToken* token);

void DebugDataTipInit(DebugDataTipModel* model);
void DebugDataTipInvalidate(DebugDataTipModel* model);
bool DebugDataTipIdentityEqual(const DebugDataTipIdentity& left,
                               const DebugDataTipIdentity& right);
bool DebugDataTipSetAvailable(DebugDataTipModel* model,
                              const DebugDataTipIdentity& identity,
                              const char* typeDisplay, const char* valueDisplay,
                              int anchorX, int anchorY);
void DebugDataTipSetState(DebugDataTipModel* model, DebugDataTipState state,
                          const DebugDataTipIdentity* identity = nullptr);

// Places a popup below/right of the anchor and flips it when needed so the
// result stays inside the supplied editor/window bounds.
bool DebugDataTipPlacePopup(DebugDataTipPopupBounds* bounds, int anchorX, int anchorY,
                            int width, int height, int minX, int minY,
                            int maxX, int maxY);

const char* DebugDataTipStateName(DebugDataTipState state);

} // namespace developer_studio
} // namespace guidexos
