#include "developer_studio_document_outline.h"

#include "test_check.h"

#include <cstring>
#include <string>

using namespace guidexos::developer_studio;

static uint32_t findSymbol(const DocumentOutline& outline, const char* name,
                          DocumentOutlineCategory category) {
    for (uint32_t i = 0; i < outline.count; ++i)
        if (outline.symbols[i].category == category &&
            std::strcmp(outline.symbols[i].name, name) == 0) return i;
    return 0xFFFFFFFFu;
}

static uint32_t countSymbols(const DocumentOutline& outline, const char* name,
                             DocumentOutlineCategory category) {
    uint32_t count = 0;
    for (uint32_t i = 0; i < outline.count; ++i)
        if (outline.symbols[i].category == category &&
            std::strcmp(outline.symbols[i].name, name) == 0) ++count;
    return count;
}

int main() {
    static DocumentOutline outline;

    const char* functions =
        "int first(int value);\n"
        "unsigned long second(\n"
        "    const char* text,\n"
        "    int count) { return count; }\n"
        "void third() {}\n";
    TEST_CHECK(DocumentOutlineScan("src/main.cpp", functions,
        static_cast<uint32_t>(std::strlen(functions)), &outline));
    TEST_CHECK(outline.count == 3 && outline.totalCount == 3 && !outline.truncated);
    TEST_CHECK(findSymbol(outline, "first", DocumentOutlineCategory::Function) != 0xFFFFFFFFu);
    TEST_CHECK(findSymbol(outline, "second", DocumentOutlineCategory::Function) != 0xFFFFFFFFu);
    TEST_CHECK(findSymbol(outline, "third", DocumentOutlineCategory::Function) != 0xFFFFFFFFu);
    TEST_CHECK(outline.symbols[1].line == 2);
    TEST_CHECK(std::strstr(outline.symbols[1].signature, "second") != nullptr);

    const char* types =
        "namespace sample {\n"
        "struct Point {\n"
        "  int x;\n"
        "  int getX() const { if (x) { helper(); } return x; }\n"
        "  Point();\n"
        "};\n"
        "enum class Color : unsigned { Red, Blue };\n"
        "class Worker { public:\n"
        "  void run(\n"
        "      int value) const;\n"
        "};\n"
        "}\n"
        "Point translate(Point p) { return p; }\n"
        "int Point::getX() const { return 0; }\n"
        "int sample::utility() { return 0; }\n";
    TEST_CHECK(DocumentOutlineScan("src/types.hpp", types,
        static_cast<uint32_t>(std::strlen(types)), &outline));
    TEST_CHECK(findSymbol(outline, "sample", DocumentOutlineCategory::Namespace) != 0xFFFFFFFFu);
    TEST_CHECK(findSymbol(outline, "Point", DocumentOutlineCategory::Struct) != 0xFFFFFFFFu);
    TEST_CHECK(findSymbol(outline, "Color", DocumentOutlineCategory::Enum) != 0xFFFFFFFFu);
    TEST_CHECK(findSymbol(outline, "Worker", DocumentOutlineCategory::Class) != 0xFFFFFFFFu);
    TEST_CHECK(findSymbol(outline, "getX", DocumentOutlineCategory::Method) != 0xFFFFFFFFu);
    TEST_CHECK(findSymbol(outline, "Point", DocumentOutlineCategory::Method) != 0xFFFFFFFFu);
    TEST_CHECK(findSymbol(outline, "run", DocumentOutlineCategory::Method) != 0xFFFFFFFFu);
    TEST_CHECK(findSymbol(outline, "translate", DocumentOutlineCategory::Function) != 0xFFFFFFFFu);
    TEST_CHECK(findSymbol(outline, "getX", DocumentOutlineCategory::Method) != 0xFFFFFFFFu);
    TEST_CHECK(countSymbols(outline, "getX", DocumentOutlineCategory::Method) == 2u);
    TEST_CHECK(findSymbol(outline, "utility", DocumentOutlineCategory::Function) != 0xFFFFFFFFu);
    TEST_CHECK(findSymbol(outline, "helper", DocumentOutlineCategory::Function) == 0xFFFFFFFFu);

    const char* falsePositives =
        "// int line_comment() {}\n"
        "/* class BlockComment {}; */\n"
        "const char* ordinary = \"int quoted() {}\";\n"
        "char character = '}';\n"
        "const char* raw = u8R\"tag(\n"
        "  int raw_false_positive() {}\n"
        "  /* class also_raw {}; */\n"
        ")tag\";\n"
        "#define FAKE_FUNCTION() int macro_fake() {}\n"
        "Class CaseSensitiveFake {};\n"
        "INT caseSensitive() {}\n"
        "int visible() { nested_call(); return 0; }\n";
    TEST_CHECK(DocumentOutlineScan("src/false.cpp", falsePositives,
        static_cast<uint32_t>(std::strlen(falsePositives)), &outline));
    TEST_CHECK(outline.count == 1 && std::strcmp(outline.symbols[0].name, "visible") == 0);

    const char* unsupported =
        "template <typename T> class Box { T value; };\n"
        "template <typename T> T identity(T value) { return value; }\n"
        "struct Vec { Vec operator+(const Vec&) const; void supported(); };\n";
    TEST_CHECK(DocumentOutlineScan("src/unsupported.cpp", unsupported,
        static_cast<uint32_t>(std::strlen(unsupported)), &outline));
    TEST_CHECK(findSymbol(outline, "Box", DocumentOutlineCategory::Class) == 0xFFFFFFFFu);
    TEST_CHECK(findSymbol(outline, "identity", DocumentOutlineCategory::Function) == 0xFFFFFFFFu);
    TEST_CHECK(findSymbol(outline, "operator", DocumentOutlineCategory::Method) == 0xFFFFFFFFu);
    TEST_CHECK(findSymbol(outline, "supported", DocumentOutlineCategory::Method) != 0xFFFFFFFFu);

    TEST_CHECK(DocumentOutlineScan("src/empty.cpp", "", 0, &outline));
    TEST_CHECK(outline.count == 0 && outline.totalCount == 0 && !outline.truncated);
    TEST_CHECK(!DocumentOutlineScan("notes.txt", "int hidden() {}", 15, &outline));
    TEST_CHECK(outline.error == DocumentOutlineError::UnsupportedLanguage);
    std::string oversized(kDocumentOutlineMaxInputBytes + 1u, ' ');
    TEST_CHECK(!DocumentOutlineScan("src/large.cpp", oversized.data(),
        static_cast<uint32_t>(oversized.size()), &outline));
    TEST_CHECK(outline.error == DocumentOutlineError::DocumentTooLarge && outline.truncated);

    std::string manySymbols;
    const uint32_t requested = kDocumentOutlineMaxSymbols + 7u;
    for (uint32_t i = 0; i < requested; ++i) {
        manySymbols += "int function_";
        manySymbols += static_cast<char>('A' + (i % 26u));
        manySymbols += "() {}\n";
    }
    TEST_CHECK(DocumentOutlineScan("src/capacity.cpp", manySymbols.data(),
        static_cast<uint32_t>(manySymbols.size()), &outline));
    TEST_CHECK(outline.count == kDocumentOutlineMaxSymbols);
    TEST_CHECK(outline.totalCount == requested && outline.truncated);

    const char* navigationSource = "int target() {}\n";
    TEST_CHECK(DocumentOutlineScan("src/nav.cpp", navigationSource,
        static_cast<uint32_t>(std::strlen(navigationSource)), &outline));
    DocumentOutlineSetIdentity(&outline, 41, 7, 3, "C:/project/src/nav.cpp");
    const uint32_t target = findSymbol(outline, "target", DocumentOutlineCategory::Function);
    const DocumentOutlineSymbol* selected = nullptr;
    TEST_CHECK(target != 0xFFFFFFFFu);
    TEST_CHECK(DocumentOutlineCanNavigate(&outline, target, 41, 7, 3,
        "C:/project/src/nav.cpp", navigationSource,
        static_cast<uint32_t>(std::strlen(navigationSource)), &selected));
    TEST_CHECK(selected && std::strcmp(selected->name, "target") == 0);
    TEST_CHECK(!DocumentOutlineCanNavigate(&outline, target, 41, 7, 4,
        "C:/project/src/nav.cpp", navigationSource,
        static_cast<uint32_t>(std::strlen(navigationSource)), &selected));
    TEST_CHECK(!DocumentOutlineCanNavigate(&outline, target, 41, 7, 3,
        "C:/project/src/other.cpp", navigationSource,
        static_cast<uint32_t>(std::strlen(navigationSource)), &selected));
    const char* changedSource = "int changed() {}\n";
    TEST_CHECK(!DocumentOutlineCanNavigate(&outline, target, 41, 7, 3,
        "C:/project/src/nav.cpp", changedSource,
        static_cast<uint32_t>(std::strlen(changedSource)), &selected));
    TEST_CHECK(!DocumentOutlineCanNavigate(&outline, outline.count, 41, 7, 3,
        "C:/project/src/nav.cpp", navigationSource,
        static_cast<uint32_t>(std::strlen(navigationSource)), &selected));

    return 0;
}
