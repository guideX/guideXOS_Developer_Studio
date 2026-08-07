#include "developer_studio_types.h"

#include <assert.h>
#include <stdio.h>

using namespace guidexos::developer_studio;

static uint32_t findWord(const char* text, const char* word, uint32_t occurrence = 0) {
    uint32_t seen = 0;
    for (uint32_t i = 0; text && text[i] != '\0'; ++i) {
        uint32_t j = 0;
        while (word[j] != '\0' && text[i + j] != '\0' && text[i + j] == word[j]) ++j;
        if (word[j] == '\0' && (i == 0 || !((text[i - 1] >= 'A' && text[i - 1] <= 'Z') ||
                                             (text[i - 1] >= 'a' && text[i - 1] <= 'z') || text[i - 1] == '_')) &&
            !((text[i + j] >= 'A' && text[i + j] <= 'Z') || (text[i + j] >= 'a' && text[i + j] <= 'z') ||
              (text[i + j] >= '0' && text[i + j] <= '9') || text[i + j] == '_')) {
            if (seen++ == occurrence) return i;
        }
    }
    return UINT32_MAX;
}

static void documentWithText(Document* document, const char* text) {
    *document = Document();
    document->used = true;
    document->documentId = 42;
    for (uint32_t i = 0; i < sizeof(document->path); ++i) document->path[i] = 0;
    const char path[] = "/workspace/fixture.cpp";
    for (uint32_t i = 0; i + 1 < sizeof(document->path) && path[i] != '\0'; ++i) document->path[i] = path[i];
    TextBufferInit(&document->buffer);
    uint32_t length = 0;
    while (text[length] != '\0') ++length;
    assert(TextBufferSet(&document->buffer, text, length));
}

static TypeInspection inspect(TypeDatabase& database, const Document& document, const char* word,
                              uint32_t occurrence = 0) {
    TypeInspection result = {};
    uint32_t offset = findWord(document.buffer.data, word, occurrence);
    assert(offset != UINT32_MAX);
    assert(TypeDatabaseInspectAt(&database, document, 7, offset + 1, &result));
    return result;
}

static void expectExact(TypeDatabase& database, const Document& document, const char* word,
                        const char* spelling, TypeSource source, uint32_t occurrence = 0) {
    TypeInspection result = inspect(database, document, word, occurrence);
    assert(result.available);
    assert(result.state == TypeInspectionState::Exact);
    assert(result.type.source == source);
    assert(result.type.spelling[0] != '\0');
    if (spelling) assert(result.type.spelling[0] == spelling[0]);
}

int main() {
    static TypeRecord records[kTypeMaxRecords] = {};
    static TypeDocument documents[32] = {};
    TypeDatabase database = {};
    TypeDatabaseInit(&database, records, kTypeMaxRecords, documents, 32);
    const char fixture[] =
        "// struct FakeComment { int no; }\n"
        "const char* fakeString = \"struct FakeString { int no; }\";\n"
        "int count;\n"
        "const unsigned long flags;\n"
        "struct Renderer {};\n"
        "struct Widget {};\n"
        "struct Frame {};\n"
        "struct Window { int width; Renderer* renderer; };\n"
        "typedef unsigned int LegacyId;\n"
        "using WindowId = uint32_t;\n"
        "using PrimaryWindowId = WindowId;\n"
        "PrimaryWindowId id;\n"
        "using AliasA = uint32_t;\n"
        "using AliasB = AliasA;\n"
        "using AliasC = AliasB;\n"
        "using CycleA = CycleB;\n"
        "using CycleB = CycleA;\n"
        "struct Result {};\n"
        "Result build();\n"
        "Result build(int mode);\n"
        "Result makeResult();\n"
        "void draw(Renderer* renderer, const Frame& frame) { int inner = 1; int other = inner; }\n"
        "int value;\n"
        "void shadow(int value) { int other = value; }\n"
        "void shadowLocal() { int value = 5; int other = value; }\n"
        "auto literal = 42;\n"
        "auto result = makeResult();\n"
        "auto ambiguous = build();\n"
        "auto unknown = ;\n";
    static Document document = {};
    documentWithText(&document, fixture);
    assert(TypeDatabaseIndexDocument(&database, "/workspace", document.path, document.documentId,
                                     document.buffer.generation, 7, document.buffer.data, document.buffer.length));
    assert(database.current);

    expectExact(database, document, "count", "int", TypeSource::GlobalDeclaration);
    TypeInspection flags = inspect(database, document, "flags");
    assert(flags.available && flags.type.constQualified && flags.type.baseName[0] != '\0');

    TypeInspection renderer = inspect(database, document, "renderer", 0);
    assert(renderer.available && renderer.type.pointerDepth == 1);
    TypeInspection frame = inspect(database, document, "frame");
    assert(frame.available && frame.type.referenceKind == TypeReferenceKind::LValue);
    TypeInspection width = inspect(database, document, "width");
    assert(width.available && width.declarationKind == TypeDeclarationKind::Member);

    TypeInspection parameter = inspect(database, document, "value", 2);
    assert(parameter.available && parameter.declarationKind == TypeDeclarationKind::Parameter);
    TypeInspection local = inspect(database, document, "value", 4);
    assert(local.available && local.declarationKind == TypeDeclarationKind::LocalVariable);

    TypeInspection alias = inspect(database, document, "PrimaryWindowId");
    assert(alias.available && alias.type.aliasName[0] != '\0' && alias.type.resolvedAlias[0] != '\0');
    TypeInspection aliasVariable = inspect(database, document, "id");
    assert(aliasVariable.available && aliasVariable.type.aliasName[0] != '\0' && aliasVariable.type.resolvedAlias[0] != '\0');
    TypeInspection chain = inspect(database, document, "AliasC");
    assert(chain.available && chain.type.resolvedAlias[0] != '\0');

    TypeInspection returnType = inspect(database, document, "makeResult");
    assert(returnType.available && returnType.declarationKind == TypeDeclarationKind::Function);
    assert(returnType.type.source == TypeSource::FunctionReturn);

    TypeInspection literal = inspect(database, document, "literal");
    assert(literal.available && literal.state == TypeInspectionState::Conservative && literal.type.baseName[0] == 'i');
    TypeInspection call = inspect(database, document, "result");
    assert(call.available && call.state == TypeInspectionState::Conservative);
    assert(call.type.source == TypeSource::FunctionReturnInference);
    TypeInspection ambiguous = inspect(database, document, "ambiguous");
    assert(ambiguous.state == TypeInspectionState::Ambiguous || ambiguous.state == TypeInspectionState::Unknown);

    TypeInspection cycle = inspect(database, document, "CycleA");
    assert(cycle.state == TypeInspectionState::Unknown || cycle.state == TypeInspectionState::Ambiguous);

    const char* comment = "FakeComment";
    uint32_t commentOffset = findWord(document.buffer.data, comment);
    assert(commentOffset != UINT32_MAX);
    TypeInspection ignored = {};
    assert(!TypeDatabaseInspectAt(&database, document, 7, commentOffset + 1, &ignored));
    const char* stringWord = "FakeString";
    uint32_t stringOffset = findWord(document.buffer.data, stringWord);
    assert(stringOffset != UINT32_MAX);
    assert(!TypeDatabaseInspectAt(&database, document, 7, stringOffset + 1, &ignored));

    assert(!TypeDatabaseInspectAt(&database, document, 8, findWord(document.buffer.data, "count") + 1, &ignored));
    assert(ignored.state == TypeInspectionState::Stale);

    static TypeRecord smallRecords[2] = {};
    static TypeDocument smallDocuments[2] = {};
    TypeDatabase small = {};
    TypeDatabaseInit(&small, smallRecords, 2, smallDocuments, 2);
    assert(TypeDatabaseIndexDocument(&small, "/workspace", document.path, document.documentId,
                                     document.buffer.generation, 7, document.buffer.data, document.buffer.length));
    assert(TypeDatabaseIsTruncated(&small));

    static char oversized[kTypeMaxParserDocumentBytes + 2] = {};
    for (uint32_t i = 0; i < kTypeMaxParserDocumentBytes + 1; ++i) oversized[i] = ' ';
    assert(TypeDatabaseIndexDocument(&database, "/workspace", document.path, document.documentId,
                                     document.buffer.generation + 1, 7, oversized, kTypeMaxParserDocumentBytes + 1));
    assert(TypeDatabaseIsTruncated(&database));

    return 0;
}
