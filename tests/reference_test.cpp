#include "developer_studio_references.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

using namespace guidexos::developer_studio;
namespace fs = std::filesystem;

static bool statFile(void*, const char* path, FileInfo* info) {
    std::error_code ec;
    const fs::file_status status = fs::symlink_status(fs::path(path), ec);
    if (ec || !fs::exists(status)) return false;
    info->kind = fs::is_directory(status) ? FileInfoKind::Directory :
        (fs::is_regular_file(status) ? FileInfoKind::RegularFile : FileInfoKind::Unknown);
    info->size = info->kind == FileInfoKind::RegularFile ? fs::file_size(fs::path(path), ec) : 0;
    return !ec;
}

static bool listFiles(void*, const char* path, FileListEntry* entries, uint32_t capacity,
                      uint32_t* outCount, bool* outTruncated) {
    *outCount = 0;
    *outTruncated = false;
    std::error_code ec;
    for (const fs::directory_entry& item : fs::directory_iterator(fs::path(path), ec)) {
        if (ec) return false;
        if (*outCount >= capacity) { *outTruncated = true; break; }
        const std::string name = item.path().filename().generic_string();
        if (name.size() >= sizeof(entries[*outCount].name)) continue;
        FileListEntry& entry = entries[(*outCount)++];
        for (uint32_t i = 0; i < sizeof(entry.name); ++i) entry.name[i] = '\0';
        for (uint32_t i = 0; i < name.size(); ++i) entry.name[i] = name[i];
        const fs::file_status status = item.symlink_status(ec);
        if (ec) return false;
        entry.kind = fs::is_directory(status) ? FileInfoKind::Directory :
            (fs::is_regular_file(status) ? FileInfoKind::RegularFile : FileInfoKind::Unknown);
        entry.size = entry.kind == FileInfoKind::RegularFile ? item.file_size(ec) : 0;
        if (ec) return false;
    }
    return true;
}

static bool readFile(void*, const char* path, char* buffer, uint32_t capacity, uint32_t* outBytes) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return false;
    input.seekg(0, std::ios::end);
    const std::streamoff size = input.tellg();
    if (size < 0 || static_cast<uint64_t>(size) > capacity) return false;
    input.seekg(0, std::ios::beg);
    input.read(buffer, size);
    if (!input && size > 0) return false;
    *outBytes = static_cast<uint32_t>(size);
    return true;
}

static void writeFile(const fs::path& path, const std::string& contents) {
    std::ofstream output(path, std::ios::binary);
    assert(output);
    output << contents;
}

static void setText(char* output, uint32_t capacity, const char* value) {
    uint32_t i = 0;
    while (i + 1 < capacity && value && value[i] != '\0') { output[i] = value[i]; ++i; }
    output[i] = '\0';
}

static ReferenceSearchRequest requestFor(const fs::path& root, const ReferenceTarget& target,
                                         const WorkspaceFileSystem& fileSystem) {
    ReferenceSearchRequest request = {};
    request.target = target;
    setText(request.projectId, sizeof(request.projectId), "com.example.references");
    request.projectGeneration = 7;
    setText(request.rootPath, sizeof(request.rootPath), root.generic_string().c_str());
    ProjectSearchOptionsInit(&request.scanOptions);
    setText(request.scanOptions.includePattern, sizeof(request.scanOptions.includePattern), "*.cpp;*.h");
    request.fileSystem = fileSystem;
    request.includeDeclarations = true;
    request.includeAmbiguous = true;
    return request;
}

static void testTokenScanAndClassification(const fs::path& root,
                                           const WorkspaceFileSystem& fileSystem) {
    const std::string source =
        "int BuildProject();\n"
        "int BuildProject() { return 1; }\n"
        "void use() { BuildProject(); BuildProject2(); }\n"
        "// BuildProject\n"
        "const char* text = \"BuildProject\";\n"
        "const char* raw = R\"(BuildProject)\";\n"
        "#if 0\nBuildProject();\n#endif\n";
    writeFile(root / "a.cpp", source);
    writeFile(root / "b.cpp", "void other() { BuildProject(); RebuildProject(); }\n");

    static ProjectSymbol projectSymbols[128];
    static SymbolDocument documents[8];
    static DocumentSymbol scratch[128];
    SymbolDatabase database = {};
    SymbolDatabaseInit(&database, projectSymbols, 128, documents, 8, scratch, 128);
    const std::string absolute = (root / "a.cpp").generic_string();
    assert(SymbolDatabaseIndexDocument(&database, absolute.c_str(), 41, 3, false,
                                      source.c_str(), static_cast<uint32_t>(source.size())));
    uint32_t indices[32] = {};
    const uint32_t count = SymbolDatabaseLookupByName(&database, "BuildProject", true, indices, 32);
    assert(count >= 2);
    const ProjectSymbol* definition = nullptr;
    for (uint32_t i = 0; i < count; ++i) {
        const ProjectSymbol* symbol = SymbolDatabaseProjectSymbolAt(&database, indices[i]);
        if (symbol && symbol->symbol.declarationRole == SymbolDeclarationRole::Definition) {
            definition = symbol;
            break;
        }
    }
    assert(definition);
    DefinitionCandidate candidate = {};
    candidate.symbol = *definition;
    setText(candidate.relativePath, sizeof(candidate.relativePath), "a.cpp");
    ReferenceTarget target = {};
    assert(ReferenceTargetFromDefinitionCandidate(candidate, "com.example.references", 7, &target));
    assert(target.hasQualifiedIdentity);

    ReferenceSearchRequest request = requestFor(root, target, fileSystem);
    request.symbolDatabase = &database;
    static ReferenceSearchService service = {};
    ReferenceSearchServiceInit(&service);
    uint64_t operationId = 0;
    ReferenceSearchErrorCode error = ReferenceSearchErrorCode::None;
    assert(ReferenceSearchStart(&service, &request, 0, &operationId, &error));
    for (uint32_t i = 0; i < 1000 && ReferenceSearchIsActive(&service); ++i)
        assert(ReferenceSearchPoll(&service, operationId, 16, i));
    const ReferenceSearchOperation* operation = ReferenceSearchOperationInfo(&service);
    assert(operation && operation->state == ReferenceSearchState::Completed);
    assert(operation->referencesFound >= 4);
    assert(ReferenceSearchResultGroups(&service) == 2);
    bool definitionFound = false, declarationFound = false, callFound = false;
    for (uint32_t g = 0; g < ReferenceSearchResultGroups(&service); ++g) {
        const ReferenceFileGroup* group = ReferenceSearchResultGroupAt(&service, g);
        assert(group);
        for (uint32_t m = 0; m < group->matchCount; ++m) {
            const ReferenceMatch* match = ReferenceSearchResultMatchAt(&service, group, m);
            assert(match);
            assert(match->confidence != ReferenceConfidence::LexicalOnly);
            if (match->kind == ReferenceKind::Definition) definitionFound = true;
            if (match->kind == ReferenceKind::Declaration) declarationFound = true;
            if (match->kind == ReferenceKind::FunctionCall) callFound = true;
            assert(match->previewMatchLength > 0);
        }
    }
    assert(definitionFound && declarationFound && callFound);

    const ReferenceFileGroup* first = ReferenceSearchResultGroupAt(&service, 0);
    assert(first && std::string(first->relativePath) == "a.cpp");
}

static void testLexicalFallbackAndDirtySnapshot(const fs::path& root,
                                                const WorkspaceFileSystem& fileSystem) {
    writeFile(root / "dirty.cpp", "disk text without the unsaved identifier\n");
    ReferenceTarget target = {};
    ReferenceTargetInit(&target);
    setText(target.projectIdText, sizeof(target.projectIdText), "com.example.references");
    target.projectId = 1;
    target.projectGeneration = 7;
    setText(target.identifier, sizeof(target.identifier), "UnsavedThing");
    target.lexicallyAmbiguous = true;
    target.targetId = 55;

    ProjectSearchDocumentSnapshot snapshot = {};
    setText(snapshot.relativePath, sizeof(snapshot.relativePath), "dirty.cpp");
    setText(snapshot.data, sizeof(snapshot.data), "UnsavedThing(); // UnsavedThing\n");
    snapshot.length = static_cast<uint32_t>(std::string(snapshot.data).size());
    snapshot.documentId = 91;
    snapshot.documentGeneration = 12;

    ReferenceSearchRequest request = requestFor(root, target, fileSystem);
    request.dirtyDocuments = &snapshot;
    request.dirtyDocumentCount = 1;
    request.lexicalFallback = true;
    static ReferenceSearchService service = {};
    ReferenceSearchServiceInit(&service);
    uint64_t operationId = 0;
    ReferenceSearchErrorCode error = ReferenceSearchErrorCode::None;
    assert(ReferenceSearchStart(&service, &request, 0, &operationId, &error));
    for (uint32_t i = 0; i < 1000 && ReferenceSearchIsActive(&service); ++i)
        ReferenceSearchPoll(&service, operationId, 16, i);
    const ReferenceSearchOperation* operation = ReferenceSearchOperationInfo(&service);
    assert(operation && operation->state == ReferenceSearchState::Completed);
    assert(operation->lexicalFallback);
    assert(operation->referencesFound == 1);
    const ReferenceFileGroup* group = ReferenceSearchResultGroupAt(&service, 0);
    assert(group && group->sourceKind == ProjectSearchSourceKind::DirtyDocument);
    const ReferenceMatch* match = ReferenceSearchResultMatchAt(&service, group, 0);
    assert(match && match->fromDirtySnapshot && match->confidence == ReferenceConfidence::LexicalOnly);
    assert(match->sourceDocumentId == 91 && match->sourceDocumentGeneration == 12);

    uint64_t firstOperation = 0;
    uint64_t secondOperation = 0;
    assert(ReferenceSearchStart(&service, &request, operationId, &firstOperation, &error));
    assert(ReferenceSearchStart(&service, &request, firstOperation, &secondOperation, &error));
    assert(firstOperation != secondOperation);
    assert(!ReferenceSearchPoll(&service, firstOperation, 16, 0));
    assert(ReferenceSearchCancel(&service, secondOperation));
    assert(ReferenceSearchPoll(&service, secondOperation, 16, 1));
    operation = ReferenceSearchOperationInfo(&service);
    assert(operation && operation->state == ReferenceSearchState::Cancelled);
}

static void testResolutionAndSupersession() {
    DefinitionQuery query = {};
    query.queryId = 33;
    query.identifier[0] = 'M'; query.identifier[1] = 'i'; query.identifier[2] = 's';
    query.identifier[3] = 's'; query.identifier[4] = 'i'; query.identifier[5] = 'n';
    query.identifier[6] = 'g'; query.identifier[7] = '\0';
    DefinitionCandidate candidates[4] = {};
    static ProjectSymbol projectSymbols[4];
    static SymbolDocument documents[2];
    static DocumentSymbol scratch[4];
    SymbolDatabase database = {};
    SymbolDatabaseInit(&database, projectSymbols, 4, documents, 2, scratch, 4);
    ReferenceTargetResolution resolution = {};
    assert(ResolveReferenceTarget(&database, query, candidates, 4, true, &resolution));
    assert(resolution.kind == ReferenceTargetResolutionKind::LexicalFallback);
    assert(resolution.lexicalFallback);
}

int main() {
    const fs::path root = fs::temp_directory_path() / "guidexos-reference-test";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root, ec);
    WorkspaceFileSystem fileSystem = { nullptr, statFile, listFiles, readFile, nullptr, nullptr, nullptr };
    testTokenScanAndClassification(root, fileSystem);
    testLexicalFallbackAndDirtySnapshot(root, fileSystem);
    testResolutionAndSupersession();
    fs::remove_all(root, ec);
    return 0;
}
