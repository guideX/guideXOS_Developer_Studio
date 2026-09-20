#include "developer_studio_debug_tips.h"

#include <cassert>
#include <cstring>
#include <iostream>

using namespace guidexos::developer_studio;

static SyntaxTokenSpan span(uint32_t start, uint32_t length, SyntaxTokenKind kind) {
    SyntaxTokenSpan value = {};
    value.start = start;
    value.length = length;
    value.kind = kind;
    return value;
}

static DebugDataTipIdentity identity(const char* name, uint32_t tokenStart = 4) {
    DebugDataTipIdentity value = {};
    value.valid = true;
    value.sessionGeneration = 7;
    value.stopGeneration = 3;
    value.selectedFrameIndex = 1;
    value.documentId = 11;
    value.documentGeneration = 4;
    value.line = 9;
    value.tokenStart = tokenStart;
    value.tokenLength = static_cast<uint32_t>(std::strlen(name));
    std::strcpy(value.projectId, "demo");
    std::strcpy(value.sourcePath, "src/helper.cpp");
    std::strcpy(value.identifier, name);
    return value;
}

int main() {
    DebugDataTipToken token = {};
    const char* line = "  adjusted = value;";
    SyntaxTokenSpan spans[] = {
        span(2, 8, SyntaxTokenKind::Identifier),
        span(13, 5, SyntaxTokenKind::Identifier)
    };
    assert(DebugDataTipExtractIdentifier(line, 19, 4, spans, 2, &token));
    assert(std::strcmp(token.identifier, "adjusted") == 0);
    assert(token.start == 2 && token.length == 8);
    assert(DebugDataTipExtractIdentifier(line, 19, 9, spans, 2, &token)); // token end
    assert(!DebugDataTipExtractIdentifier(line, 19, 0, spans, 2, &token));

    const char* underscore = "_value2";
    SyntaxTokenSpan underscoreSpan[] = { span(0, 7, SyntaxTokenKind::Identifier) };
    assert(DebugDataTipExtractIdentifier(underscore, 7, 6, underscoreSpan, 1, &token));
    assert(std::strcmp(token.identifier, "_value2") == 0);

    const char* keyword = "return";
    SyntaxTokenSpan keywordSpan[] = { span(0, 6, SyntaxTokenKind::Keyword) };
    assert(!DebugDataTipExtractIdentifier(keyword, 6, 2, keywordSpan, 1, &token));

    const char* comment = "// adjusted";
    SyntaxTokenSpan commentSpan[] = { span(0, 11, SyntaxTokenKind::Comment) };
    assert(!DebugDataTipExtractIdentifier(comment, 11, 5, commentSpan, 1, &token));
    const char* string = "\"adjusted\"";
    SyntaxTokenSpan stringSpan[] = { span(0, 10, SyntaxTokenKind::StringLiteral) };
    assert(!DebugDataTipExtractIdentifier(string, 10, 4, stringSpan, 1, &token));
    const char* character = "'x'";
    SyntaxTokenSpan characterSpan[] = { span(0, 3, SyntaxTokenKind::CharacterLiteral) };
    assert(!DebugDataTipExtractIdentifier(character, 3, 1, characterSpan, 1, &token));

    const char* fallbackComment = "value // adjusted";
    assert(!DebugDataTipExtractIdentifier(fallbackComment, 17, 13, nullptr, 0, &token));

    DebugDataTipModel model = {};
    DebugDataTipInit(&model);
    DebugDataTipIdentity first = identity("value");
    assert(DebugDataTipSetAvailable(&model, first, "int32", "33", 100, 120));
    assert(model.visible && model.state == DebugDataTipState::Available);
    assert(DebugDataTipIdentityEqual(model.identity, first));
    DebugDataTipIdentity caller = first;
    caller.selectedFrameIndex = 0;
    caller.stopGeneration = 4;
    assert(!DebugDataTipIdentityEqual(model.identity, caller));
    DebugDataTipSetState(&model, DebugDataTipState::SourceMismatch, &caller);
    assert(!model.visible && model.state == DebugDataTipState::SourceMismatch);
    DebugDataTipInvalidate(&model);
    assert(!model.visible && model.state == DebugDataTipState::Empty);

    DebugDataTipPopupBounds bounds = {};
    assert(DebugDataTipPlacePopup(&bounds, 920, 620, 240, 76, 0, 0, 960, 700));
    assert(bounds.x >= 0 && bounds.y >= 0);
    assert(bounds.x + bounds.width <= 960 && bounds.y + bounds.height <= 700);
    assert(DebugDataTipPlacePopup(&bounds, 280, 90, 240, 76, 270, 48, 960, 520));
    assert(bounds.x >= 270 && bounds.y >= 48);
    assert(bounds.x + bounds.width <= 960 && bounds.y + bounds.height <= 520);

    std::cout << "Developer Studio debugger data tips model PASS\n";
    return 0;
}
