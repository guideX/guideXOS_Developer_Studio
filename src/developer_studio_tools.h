#pragma once

#include "developer_studio_models.h"

#include <stdint.h>

namespace guidexos {
namespace developer_studio {

static const uint32_t kDeveloperToolMaxCommands = 4u;
static const uint32_t kDeveloperToolMaxFileQueryBytes = 128u;
#if defined(GXOS_DEVELOPER_STUDIO_BARE_METAL)
static const uint32_t kDeveloperToolMaxIndexedFiles = 32u;
#else
static const uint32_t kDeveloperToolMaxIndexedFiles = 2048u;
#endif
static const uint32_t kDeveloperToolMaxFileResults = 64u;
static const uint32_t kDeveloperToolMaxLineInputBytes = 10u;

enum class DeveloperToolId : uint32_t {
    None = 0,
    FindInFiles = 1,
    GoToFile = 2,
    GoToLine = 3,
    ReplaceInFiles = 4
};

struct DeveloperToolFileSearchResult {
    char relativePath[kMaxPathBytes];
    bool fileNameMatch;
};

struct DeveloperToolFileSearchResults {
    DeveloperToolFileSearchResult results[kDeveloperToolMaxFileResults];
    uint32_t count;
    uint32_t totalMatches;
    bool truncated;
};

// Search a bounded snapshot of absolute project paths. Results own their
// relative paths so a later selection never depends on an inventory pointer.
bool DeveloperToolBuildFileSearchResults(const char* projectRoot,
                                         const char* const* indexedAbsolutePaths,
                                         uint32_t indexedPathCount,
                                         const char* query,
                                         DeveloperToolFileSearchResults* output);

bool DeveloperToolProjectIdentityMatches(const char* expectedProjectId,
                                         uint64_t expectedGeneration,
                                         const char* actualProjectId,
                                         uint64_t actualGeneration);

DeveloperToolId DeveloperToolIdFromShortcut(uint32_t keyCode, bool control,
                                            bool shift, bool alt);

bool DeveloperToolParsePositiveLineNumber(const char* input, uint32_t* outLine);

// On success, places the caret at the requested 1-based line start and clears
// the selection. Invalid lines leave all caller-owned editor state untouched.
bool DeveloperToolPositionAtLine(const char* text, uint32_t length, uint32_t line,
                                uint32_t* caret, uint32_t* selectionAnchor,
                                bool* selectionActive);

typedef bool (*DeveloperToolActivateCallback)(void* userData);
typedef bool (*DeveloperToolCloseCallback)(void* userData);

struct DeveloperToolCommand {
    DeveloperToolId id;
    const char* label;
    const char* shortcut;
    DeveloperToolActivateCallback activate;
    DeveloperToolCloseCallback close;
};

struct DeveloperToolRegistryEntry {
    DeveloperToolCommand command;
    bool active;
};

// This registry stores a small, explicit command table. Commands are copied
// into fixed storage; it is not a plugin loader and performs no allocation.
struct DeveloperToolRegistry {
    DeveloperToolRegistryEntry entries[kDeveloperToolMaxCommands];
    uint32_t count;
    DeveloperToolId activeId;
};

void DeveloperToolRegistryInit(DeveloperToolRegistry* registry);
bool DeveloperToolRegistryRegister(DeveloperToolRegistry* registry,
                                   const DeveloperToolCommand* command);
const DeveloperToolCommand* DeveloperToolRegistryFind(const DeveloperToolRegistry* registry,
                                                       DeveloperToolId id);
const DeveloperToolCommand* DeveloperToolRegistryAt(const DeveloperToolRegistry* registry,
                                                     uint32_t index);
bool DeveloperToolRegistryIsActive(const DeveloperToolRegistry* registry, DeveloperToolId id);
bool DeveloperToolRegistryActivate(DeveloperToolRegistry* registry, DeveloperToolId id,
                                   void* userData);
bool DeveloperToolRegistryClose(DeveloperToolRegistry* registry, DeveloperToolId id,
                                void* userData);
bool DeveloperToolRegistryCloseActive(DeveloperToolRegistry* registry, void* userData);

} // namespace developer_studio
} // namespace guidexos
