#include "developer_studio_debug_tips.h"

namespace guidexos {
namespace developer_studio {

namespace {

static void copyText(char* output, uint32_t outputSize, const char* input) {
    if (!output || outputSize == 0) return;
    uint32_t index = 0;
    if (input) {
        while (index + 1 < outputSize && input[index] != '\0') {
            output[index] = input[index];
            ++index;
        }
    }
    output[index] = '\0';
}

static bool equalText(const char* left, const char* right) {
    if (!left || !right) return left == right;
    uint32_t index = 0;
    while (left[index] != '\0' || right[index] != '\0') {
        if (left[index] != right[index]) return false;
        ++index;
    }
    return true;
}

static bool spanContains(const SyntaxTokenSpan& span, uint32_t offset) {
    return span.length != 0 && offset >= span.start && offset - span.start < span.length;
}

static bool blockedKind(SyntaxTokenKind kind) {
    return kind == SyntaxTokenKind::Keyword || kind == SyntaxTokenKind::TypeKeyword ||
        kind == SyntaxTokenKind::StringLiteral || kind == SyntaxTokenKind::CharacterLiteral ||
        kind == SyntaxTokenKind::Comment || kind == SyntaxTokenKind::Preprocessor ||
        kind == SyntaxTokenKind::Number || kind == SyntaxTokenKind::Operator ||
        kind == SyntaxTokenKind::Punctuation || kind == SyntaxTokenKind::Invalid;
}

static bool fallbackLexicallyAllowed(const char* text, uint32_t length, uint32_t offset) {
    if (!text || offset > length) return false;
    bool inString = false;
    bool inCharacter = false;
    bool escaped = false;
    bool inBlockComment = false;
    for (uint32_t index = 0; index < offset; ++index) {
        const char value = text[index];
        const char next = index + 1 < length ? text[index + 1] : '\0';
        if (inBlockComment) {
            if (value == '*' && next == '/') {
                inBlockComment = false;
                ++index;
            }
            continue;
        }
        if (!inString && !inCharacter && value == '/' && next == '*') {
            inBlockComment = true;
            ++index;
            continue;
        }
        if (!inString && !inCharacter && value == '/' && next == '/') return false;
        if (inString || inCharacter) {
            if (escaped) {
                escaped = false;
            } else if (value == '\\') {
                escaped = true;
            } else if ((inString && value == '"') || (inCharacter && value == '\'')) {
                inString = false;
                inCharacter = false;
            }
            continue;
        }
        if (value == '"') inString = true;
        else if (value == '\'') inCharacter = true;
    }
    return !inBlockComment && !inString && !inCharacter;
}

} // namespace

bool DebugDataTipIsIdentifierStart(char value) {
    return (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z') || value == '_';
}

bool DebugDataTipIsIdentifierContinue(char value) {
    return DebugDataTipIsIdentifierStart(value) || (value >= '0' && value <= '9');
}

bool DebugDataTipExtractIdentifier(const char* text, uint32_t length, uint32_t offset,
                                   const SyntaxTokenSpan* spans, uint32_t spanCount,
                                   DebugDataTipToken* token) {
    if (!token) return false;
    *token = DebugDataTipToken();
    if (!text || length == 0) return false;
    if (offset == length) --offset;
    if (offset >= length || !DebugDataTipIsIdentifierContinue(text[offset])) return false;
    uint32_t start = offset;
    while (start > 0 && DebugDataTipIsIdentifierContinue(text[start - 1])) --start;
    if (!DebugDataTipIsIdentifierStart(text[start])) return false;
    uint32_t end = offset + 1;
    while (end < length && DebugDataTipIsIdentifierContinue(text[end])) ++end;
    const uint32_t tokenLength = end - start;
    if (tokenLength == 0 || tokenLength > kDebugDataTipMaxIdentifierBytes) return false;

    bool lexicalSpanFound = false;
    for (uint32_t index = 0; index < spanCount; ++index) {
        const SyntaxTokenSpan& span = spans[index];
        if (!spanContains(span, offset)) continue;
        lexicalSpanFound = true;
        if (blockedKind(span.kind)) return false;
        if (span.kind != SyntaxTokenKind::Identifier && span.kind != SyntaxTokenKind::PlainText)
            return false;
        break;
    }
    if (!lexicalSpanFound && !fallbackLexicallyAllowed(text, length, offset)) return false;
    char identifier[kDebugDataTipMaxIdentifierBytes + 1] = {};
    for (uint32_t index = 0; index < tokenLength; ++index) identifier[index] = text[start + index];
    identifier[tokenLength] = '\0';
    if (SyntaxIsKeyword(identifier)) return false;
    token->valid = true;
    token->start = start;
    token->length = tokenLength;
    copyText(token->identifier, sizeof(token->identifier), identifier);
    return true;
}

void DebugDataTipInit(DebugDataTipModel* model) {
    if (!model) return;
    *model = DebugDataTipModel();
    model->state = DebugDataTipState::Empty;
}

void DebugDataTipInvalidate(DebugDataTipModel* model) {
    if (!model) return;
    const int anchorX = model->anchorX;
    const int anchorY = model->anchorY;
    *model = DebugDataTipModel();
    model->anchorX = anchorX;
    model->anchorY = anchorY;
    model->state = DebugDataTipState::Empty;
}

bool DebugDataTipIdentityEqual(const DebugDataTipIdentity& left,
                               const DebugDataTipIdentity& right) {
    return left.valid && right.valid && left.sessionGeneration == right.sessionGeneration &&
        left.stopGeneration == right.stopGeneration &&
        left.selectedFrameIndex == right.selectedFrameIndex && left.documentId == right.documentId &&
        left.documentGeneration == right.documentGeneration && left.line == right.line &&
        left.tokenStart == right.tokenStart && left.tokenLength == right.tokenLength &&
        equalText(left.projectId, right.projectId) && equalText(left.sourcePath, right.sourcePath) &&
        equalText(left.identifier, right.identifier);
}

bool DebugDataTipSetAvailable(DebugDataTipModel* model,
                              const DebugDataTipIdentity& identity,
                              const char* typeDisplay, const char* valueDisplay,
                              int anchorX, int anchorY) {
    if (!model || !identity.valid || !identity.identifier[0]) return false;
    model->visible = true;
    model->state = DebugDataTipState::Available;
    model->identity = identity;
    model->anchorX = anchorX;
    model->anchorY = anchorY;
    copyText(model->name, sizeof(model->name), identity.identifier);
    copyText(model->typeDisplay, sizeof(model->typeDisplay), typeDisplay);
    copyText(model->valueDisplay, sizeof(model->valueDisplay), valueDisplay);
    return true;
}

void DebugDataTipSetState(DebugDataTipModel* model, DebugDataTipState state,
                          const DebugDataTipIdentity* identity) {
    if (!model) return;
    model->visible = false;
    model->state = state;
    if (identity) model->identity = *identity;
}

bool DebugDataTipPlacePopup(DebugDataTipPopupBounds* bounds, int anchorX, int anchorY,
                            int width, int height, int minX, int minY,
                            int maxX, int maxY) {
    if (!bounds || width <= 0 || height <= 0 || maxX <= minX || maxY <= minY) return false;
    const int availableWidth = maxX - minX;
    const int availableHeight = maxY - minY;
    if (width > availableWidth || height > availableHeight) return false;
    int x = anchorX + 8;
    int y = anchorY + 18;
    if (x + width > maxX) x = anchorX - width - 8;
    if (y + height > maxY) y = anchorY - height - 8;
    if (x < minX) x = minX;
    if (y < minY) y = minY;
    if (x + width > maxX) x = maxX - width;
    if (y + height > maxY) y = maxY - height;
    bounds->x = x;
    bounds->y = y;
    bounds->width = width;
    bounds->height = height;
    return true;
}

const char* DebugDataTipStateName(DebugDataTipState state) {
    switch (state) {
    case DebugDataTipState::Empty: return "Empty";
    case DebugDataTipState::Available: return "Available";
    case DebugDataTipState::UnknownIdentifier: return "UnknownIdentifier";
    case DebugDataTipState::NotAvailable: return "NotAvailable";
    case DebugDataTipState::Running: return "Running";
    case DebugDataTipState::SourceMismatch: return "SourceMismatch";
    case DebugDataTipState::StaleSource: return "StaleSource";
    case DebugDataTipState::Unsupported: return "Unsupported";
    }
    return "Unknown";
}

} // namespace developer_studio
} // namespace guidexos
