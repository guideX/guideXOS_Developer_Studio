#pragma once

#include <stdint.h>

namespace guidexos {
namespace developer_studio {

static const uint32_t kDeveloperToolMaxCommands = 4u;

enum class DeveloperToolId : uint32_t {
    None = 0,
    FindInFiles = 1
};

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
