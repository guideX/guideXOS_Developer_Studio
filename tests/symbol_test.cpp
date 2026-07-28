#include "developer_studio_symbols.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>

using namespace guidexos::developer_studio;
namespace fs = std::filesystem;

static bool testStat(void*, const char* path, FileInfo* info) {
    if (!path || !info) return false;
    std::error_code error;
    const fs::file_status status = fs::status(fs::path(path), error);
    if (error) return false;
    if (fs::is_directory(status)) { info->kind = FileInfoKind::Directory; info->size = 0; return true; }
    if (fs::is_regular_file(status)) {
        info->kind = FileInfoKind::RegularFile;
        info->size = fs::file_size(fs::path(path), error);
        return !error;
    }
    info->kind = FileInfoKind::Unknown;
    info->size = 0;
    return true;
}

static bool testList(void*, const char* path, FileListEntry* entries, uint32_t capacity,
                     uint32_t* outCount, bool* outTruncated) {
    if (!path || !entries || !outCount || !outTruncated) return false;
    *outCount = 0;
    *outTruncated = false;
    std::error_code error;
    for (const fs::directory_entry& item : fs::directory_iterator(fs::path(path), error)) {
        if (error) return false;
        if (*outCount >= capacity) { *outTruncated = true; continue; }
        const std::string name = item.path().filename().generic_string();
        if (name.size() >= sizeof(entries[*outCount].name)) continue;
        FileListEntry& entry = entries[(*outCount)++];
        for (uint32_t i = 0; i < sizeof(entry.name); ++i) entry.name[i] = '\0';
        for (uint32_t i = 0; i < name.size(); ++i) entry.name[i] = name[i];
        entry.kind = item.is_directory(error) ? FileInfoKind::Directory : FileInfoKind::RegularFile;
        entry.size = entry.kind == FileInfoKind::RegularFile ? item.file_size(error) : 0;
        if (error) return false;
    }
    return !error;
}

static bool testRead(void*, const char* path, char* buffer, uint32_t capacity, uint32_t* outBytes) {
    if (!path || !buffer || !outBytes) return false;
    std::ifstream input(fs::path(path), std::ios::binary);
    if (!input) return false;
    input.seekg(0, std::ios::end);
    const std::streamoff size = input.tellg();
    if (size < 0 || static_cast<uint64_t>(size) > capacity) return false;
    input.seekg(0, std::ios::beg);
    input.read(buffer, size);
    if (!input && !input.eof()) return false;
    *outBytes = static_cast<uint32_t>(size);
    return true;
}

static void writeText(const fs::path& path, const std::string& text) {
    std::ofstream output(path, std::ios::binary);
    assert(output);
    output << text;
}

static bool hasKindName(const SymbolDatabase& database, SymbolKind kind, const char* name) {
    uint32_t indices[64] = {};
    const uint32_t count = SymbolDatabaseLookupByName(&database, name, true, indices, 64);
    for (uint32_t i = 0; i < count && i < 64; ++i) {
        const ProjectSymbol* symbol = SymbolDatabaseProjectSymbolAt(&database, indices[i]);
        if (symbol && symbol->symbol.kind == kind) return true;
    }
    return false;
}

static uint32_t countKindName(const DocumentSymbol* symbols, uint32_t count, SymbolKind kind, const char* name) {
    uint32_t matches = 0;
    for (uint32_t i = 0; i < count; ++i)
        if (symbols[i].kind == kind && std::string(symbols[i].name) == name) ++matches;
    return matches;
}

static void testScanner() {
    const char* source =
        "// namespace Fake { int hidden(); }\n"
        "const char* text = \"class Fake { void no(); }\";\n"
        "#define MACRO_NAME 1\n"
        "#if 0\nclass Disabled { void no(); };\n#endif\n"
        "namespace { class AnonymousType {}; }\n"
        "namespace Outer { namespace Inner {\n"
        "class Widget { public: class Nested {}; Widget(); ~Widget(); void run(); };\n"
        "struct Point { int x; }; enum class Color { Red };\n"
        "typedef int Count; using Alias = Count;\n"
        "static int localState = 0; int globalState = 1;\n"
        "int helper(); int caller() { return helper(); } int initialized = helper();\n"
        "void Duplicate(); void Duplicate();\n"
        "void freeFunction(int value) { (void)value; }\n"
        "} }\n"
        "class Forward; template <typename T> class Box { T value; };\n";
    DocumentSymbol symbols[128] = {};
    SymbolScanResult result = {};
    assert(ScanDocumentSymbols(source, static_cast<uint32_t>(std::string(source).size()), 41, 7,
                               symbols, 128, &result));
    assert(result.success);
    bool anonymous = false, widget = false, nested = false, point = false, color = false, run = false;
    bool ctor = false, dtor = false, count = false, alias = false, global = false, stat = false;
    bool freeFunction = false, box = false, forward = false;
    for (uint32_t i = 0; i < result.symbolCount; ++i) {
        const DocumentSymbol& symbol = symbols[i];
        if (symbol.kind == SymbolKind::Namespace && std::string(symbol.name) == "anonymous") anonymous = true;
        if (symbol.kind == SymbolKind::Class && std::string(symbol.name) == "Widget") widget = true;
        if (symbol.kind == SymbolKind::Class && std::string(symbol.name) == "Nested") nested = true;
        if (symbol.kind == SymbolKind::Struct && std::string(symbol.name) == "Point") point = true;
        if (symbol.kind == SymbolKind::Enum && std::string(symbol.name) == "Color") color = true;
        if (symbol.kind == SymbolKind::Method && std::string(symbol.name) == "run") run = true;
        if (symbol.kind == SymbolKind::Constructor && std::string(symbol.name) == "Widget") ctor = true;
        if (symbol.kind == SymbolKind::Destructor && std::string(symbol.name) == "~Widget") dtor = true;
        if (symbol.kind == SymbolKind::Typedef && std::string(symbol.name) == "Count") count = true;
        if (symbol.kind == SymbolKind::UsingAlias && std::string(symbol.name) == "Alias") alias = true;
        if (symbol.kind == SymbolKind::GlobalVariable && std::string(symbol.name) == "globalState") global = true;
        if (symbol.kind == SymbolKind::StaticVariable && std::string(symbol.name) == "localState") stat = true;
        if (symbol.kind == SymbolKind::Function && std::string(symbol.name) == "freeFunction") freeFunction = true;
        if (symbol.kind == SymbolKind::Class && std::string(symbol.name) == "Box") box = true;
        if (symbol.kind == SymbolKind::Class && std::string(symbol.name) == "Forward") forward = true;
        assert(std::string(symbol.name) != "Disabled");
        assert(std::string(symbol.name) != "Fake");
    }
    assert(anonymous && widget && nested && point && color && run && ctor && dtor && count && alias &&
           global && stat && freeFunction && box && forward);
    assert(countKindName(symbols, result.symbolCount, SymbolKind::Function, "helper") == 1);
    assert(countKindName(symbols, result.symbolCount, SymbolKind::Function, "caller") == 1);
    assert(countKindName(symbols, result.symbolCount, SymbolKind::Function, "Duplicate") == 2);
    for (uint32_t i = 0; i < result.symbolCount; ++i) {
        assert(symbols[i].ordinal == i);
        if (i > 0) assert(symbols[i - 1].location.identifierOffset <= symbols[i].location.identifierOffset);
    }

    const char* malformed = "namespace Broken { class Open { void maybe( ;\n";
    DocumentSymbol malformedSymbols[16] = {};
    SymbolScanResult malformedResult = {};
    assert(ScanDocumentSymbols(malformed, static_cast<uint32_t>(std::string(malformed).size()), 42, 1,
                               malformedSymbols, 16, &malformedResult));
    assert(malformedResult.success);
    assert(countKindName(malformedSymbols, malformedResult.symbolCount, SymbolKind::Namespace, "Broken") == 1);
    assert(countKindName(malformedSymbols, malformedResult.symbolCount, SymbolKind::Class, "Open") == 1);
}

static void testDatabase() {
    static ProjectSymbol projectStorage[1024];
    static SymbolDocument documentStorage[16];
    static DocumentSymbol scratchStorage[256];
    SymbolDatabase database = {};
    SymbolDatabaseInit(&database, projectStorage, 1024, documentStorage, 16, scratchStorage, 256);
    const char* first = "namespace A { class Alpha {}; void build() {} }\n";
    const char* second = "struct Beta {}; static int state;\n";
    assert(SymbolDatabaseIndexDocument(&database, "C:/symbols/first.cpp", 10, 1, false,
                                      first, static_cast<uint32_t>(std::string(first).size())));
    assert(SymbolDatabaseIndexDocument(&database, "C:/symbols/second.cpp", 11, 1, true,
                                      second, static_cast<uint32_t>(std::string(second).size())));
    assert(SymbolDatabaseDocumentCount(&database) == 2);
    assert(SymbolDatabaseProjectSymbolCount(&database) > 0);
    assert(hasKindName(database, SymbolKind::Class, "Alpha"));
    assert(hasKindName(database, SymbolKind::StaticVariable, "state"));
    uint32_t matches[100] = {};
    assert(SymbolDatabaseLookupByKind(&database, SymbolKind::Class, matches, 100) >= 1);
    assert(SymbolDatabaseFindSymbols(&database, "build", false, matches, 100) >= 1);
    assert(SymbolDatabaseFindSymbols(&database, "ALP", false, matches, 100) >= 1);
    assert(SymbolDatabaseLookupByPrefix(&database, "Al", true, matches, 100) >= 1);
    const uint32_t before = database.incrementalIndexCount;
    const char* changed = "namespace A { class Alpha {}; void rebuild() {} }\n";
    assert(SymbolDatabaseIndexDocument(&database, "C:/symbols/first.cpp", 10, 2, true,
                                      changed, static_cast<uint32_t>(std::string(changed).size())));
    assert(database.incrementalIndexCount == before + 1);
    assert(SymbolDatabaseFindSymbols(&database, "rebuild", true, matches, 100) == 1);
    assert(SymbolDatabaseLookupByName(&database, "build", true, matches, 100) == 0);
    assert(SymbolDatabaseRemoveDocument(&database, "C:/symbols/second.cpp"));
    assert(SymbolDatabaseDocumentCount(&database) == 1);
}

static void testProjectIndexAndBounds() {
    const fs::path root = fs::temp_directory_path() / "guidexos-symbol-index-test";
    std::error_code cleanupError;
    fs::remove_all(root, cleanupError);
    fs::create_directories(root / "src");
    writeText(root / "src" / "disk.cpp", "void DiskOnly() {}\n");
    writeText(root / "src" / "dirty.cpp", "void DiskStale() {}\n");
    WorkspaceFileSystem fileSystem = { nullptr, testStat, testList, testRead, nullptr, nullptr, nullptr };

    static ProjectSymbol projectStorage[512];
    static SymbolDocument documentStorage[16];
    static DocumentSymbol scratchStorage[256];
    SymbolDatabase database = {};
    SymbolDatabaseInit(&database, projectStorage, 512, documentStorage, 16, scratchStorage, 256);

    static Document dirty = {};
    dirty.used = true;
    dirty.documentId = 501;
    NormalizePath((root / "src" / "dirty.cpp").generic_string().c_str(), dirty.path, sizeof(dirty.path));
    TextBufferInit(&dirty.buffer);
    const char* dirtyText = "void DirtyOnly() {}\n";
    assert(TextBufferSet(&dirty.buffer, dirtyText, static_cast<uint32_t>(std::string(dirtyText).size())));
    dirty.buffer.dirty = true;
    dirty.buffer.generation = 17;
    assert(SymbolDatabaseIndexProject(&database, fileSystem, root.generic_string().c_str(), &dirty, 1, 77));
    assert(database.fullIndexCount == 1);
    assert(hasKindName(database, SymbolKind::Function, "DirtyOnly"));
    assert(!hasKindName(database, SymbolKind::Function, "DiskStale"));
    assert(hasKindName(database, SymbolKind::Function, "DiskOnly"));
    uint32_t indices[64] = {};
    assert(SymbolDatabaseLookupByFile(&database, dirty.path, indices, 64) >= 1);
    const ProjectSymbol* dirtySymbol = SymbolDatabaseProjectSymbolAt(&database, indices[0]);
    assert(dirtySymbol && dirtySymbol->symbol.location.documentId == dirty.documentId);
    assert(dirtySymbol->symbol.location.generation == dirty.buffer.generation);
    assert(dirtySymbol->symbol.location.line == 1);
    assert(dirtySymbol->symbol.location.column == 6);
    const uint32_t previousIncremental = database.incrementalIndexCount;
    const char* changed = "void DirtyChanged() {}\n";
    assert(SymbolDatabaseIndexDocument(&database, dirty.path, dirty.documentId, 18, true,
                                      changed, static_cast<uint32_t>(std::string(changed).size())));
    assert(database.incrementalIndexCount == previousIncremental + 1);
    assert(hasKindName(database, SymbolKind::Function, "DirtyChanged"));
    assert(!hasKindName(database, SymbolKind::Function, "DirtyOnly"));

    TextBuffer selected = {};
    TextBufferInit(&selected);
    assert(TextBufferSet(&selected, changed, static_cast<uint32_t>(std::string(changed).size())));
    const uint32_t changedCount = SymbolDatabaseLookupByName(&database, "DirtyChanged", true, indices, 64);
    assert(changedCount == 1);
    const ProjectSymbol* changedSymbol = SymbolDatabaseProjectSymbolAt(&database, indices[0]);
    assert(changedSymbol);
    const uint32_t changedLength = static_cast<uint32_t>(std::string(changedSymbol->symbol.name).size());
    assert(SelectTextRange(&selected, changedSymbol->symbol.location.identifierOffset, changedLength));
    char selectedText[32] = {};
    assert(GetSelectedText(&selected, selectedText, sizeof(selectedText)) == changedLength);
    assert(std::string(selectedText) == "DirtyChanged");

    static ProjectSymbol tinyProjectStorage[2];
    static SymbolDocument tinyDocumentStorage[2];
    static DocumentSymbol tinyScratchStorage[32];
    SymbolDatabase tiny = {};
    SymbolDatabaseInit(&tiny, tinyProjectStorage, 2, tinyDocumentStorage, 2, tinyScratchStorage, 32);
    const char* many = "namespace N { class A {}; struct B {}; enum E { X }; void f() {} }\n";
    assert(SymbolDatabaseIndexDocument(&tiny, "C:/tiny.cpp", 1, 1, false, many,
                                      static_cast<uint32_t>(std::string(many).size())));
    assert(SymbolDatabaseIsTruncated(&tiny));
    assert(SymbolDatabaseProjectSymbolCount(&tiny) <= 2);
    char longQuery[kSymbolMaxQueryBytes + 2] = {};
    for (uint32_t i = 0; i < kSymbolMaxQueryBytes + 1; ++i) longQuery[i] = 'a';
    assert(SymbolDatabaseFindSymbols(&database, longQuery, false, indices, 64) == 0);

    const fs::path largeRoot = fs::temp_directory_path() / "guidexos-symbol-index-large-test";
    fs::remove_all(largeRoot, cleanupError);
    fs::create_directories(largeRoot);
    for (uint32_t i = 0; i < 128; ++i) {
        std::ostringstream fileName;
        fileName << "file" << (i < 10 ? "0" : "") << i << ".cpp";
        std::ostringstream source;
        source << "void ProjectSymbol" << (i < 10 ? "0" : "") << i << "() {}\n";
        writeText(largeRoot / fileName.str(), source.str());
    }
    static ProjectSymbol largeProjectStorage[256];
    static SymbolDocument largeDocumentStorage[160];
    static DocumentSymbol largeScratchStorage[64];
    SymbolDatabase large = {};
    SymbolDatabaseInit(&large, largeProjectStorage, 256, largeDocumentStorage, 160, largeScratchStorage, 64);
    assert(SymbolDatabaseIndexProject(&large, fileSystem, largeRoot.generic_string().c_str(), nullptr, 0, 88));
    assert(SymbolDatabaseDocumentCount(&large) == 128);
    assert(SymbolDatabaseProjectSymbolCount(&large) == 128);
    assert(std::string(SymbolDatabaseProjectSymbolAt(&large, 0)->symbol.name) == "ProjectSymbol00");
    uint32_t visible[100] = {};
    assert(SymbolDatabaseFindSymbols(&large, "", false, visible, 100) == 100);
    assert(SymbolDatabaseLookupByPrefix(&large, "ProjectSymbol", true, visible, 100) == 128);
    fs::remove_all(largeRoot, cleanupError);
    fs::remove_all(root, cleanupError);
}

int main() {
    testScanner();
    testDatabase();
    testProjectIndexAndBounds();
    return 0;
}
