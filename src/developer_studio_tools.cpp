#include "developer_studio_tools.h"

namespace guidexos {
namespace developer_studio {
namespace {

static uint32_t findIndex(const DeveloperToolRegistry* registry, DeveloperToolId id) {
    if (!registry || id == DeveloperToolId::None) return kDeveloperToolMaxCommands;
    for (uint32_t i = 0; i < registry->count; ++i)
        if (registry->entries[i].command.id == id) return i;
    return kDeveloperToolMaxCommands;
}

} // namespace

void DeveloperToolRegistryInit(DeveloperToolRegistry* registry) {
    if (!registry) return;
    registry->count = 0;
    registry->activeId = DeveloperToolId::None;
    for (uint32_t i = 0; i < kDeveloperToolMaxCommands; ++i) {
        registry->entries[i].command.id = DeveloperToolId::None;
        registry->entries[i].command.label = nullptr;
        registry->entries[i].command.shortcut = nullptr;
        registry->entries[i].command.activate = nullptr;
        registry->entries[i].command.close = nullptr;
        registry->entries[i].active = false;
    }
}

bool DeveloperToolRegistryRegister(DeveloperToolRegistry* registry,
                                   const DeveloperToolCommand* command) {
    if (!registry || !command || command->id == DeveloperToolId::None ||
        !command->label || command->label[0] == '\0' || !command->activate || !command->close ||
        registry->count >= kDeveloperToolMaxCommands ||
        findIndex(registry, command->id) != kDeveloperToolMaxCommands) return false;
    registry->entries[registry->count].command = *command;
    registry->entries[registry->count].active = false;
    ++registry->count;
    return true;
}

const DeveloperToolCommand* DeveloperToolRegistryFind(const DeveloperToolRegistry* registry,
                                                       DeveloperToolId id) {
    const uint32_t index = findIndex(registry, id);
    return index < kDeveloperToolMaxCommands ? &registry->entries[index].command : nullptr;
}

const DeveloperToolCommand* DeveloperToolRegistryAt(const DeveloperToolRegistry* registry,
                                                     uint32_t index) {
    return registry && index < registry->count ? &registry->entries[index].command : nullptr;
}

bool DeveloperToolRegistryIsActive(const DeveloperToolRegistry* registry, DeveloperToolId id) {
    const uint32_t index = findIndex(registry, id);
    return index < kDeveloperToolMaxCommands && registry->entries[index].active;
}

bool DeveloperToolRegistryClose(DeveloperToolRegistry* registry, DeveloperToolId id,
                                void* userData) {
    const uint32_t index = findIndex(registry, id);
    if (index >= kDeveloperToolMaxCommands || registry->activeId != id ||
        !registry->entries[index].active) return true;
    DeveloperToolRegistryEntry& entry = registry->entries[index];
    if (!entry.command.close(userData)) return false;
    entry.active = false;
    registry->activeId = DeveloperToolId::None;
    return true;
}

bool DeveloperToolRegistryCloseActive(DeveloperToolRegistry* registry, void* userData) {
    return !registry || registry->activeId == DeveloperToolId::None ? true :
        DeveloperToolRegistryClose(registry, registry->activeId, userData);
}

bool DeveloperToolRegistryActivate(DeveloperToolRegistry* registry, DeveloperToolId id,
                                   void* userData) {
    const uint32_t index = findIndex(registry, id);
    if (index >= kDeveloperToolMaxCommands) return false;
    if (registry->activeId != DeveloperToolId::None && registry->activeId != id &&
        !DeveloperToolRegistryCloseActive(registry, userData)) return false;
    DeveloperToolRegistryEntry& entry = registry->entries[index];
    if (!entry.command.activate(userData)) return false;
    entry.active = true;
    registry->activeId = id;
    return true;
}

} // namespace developer_studio
} // namespace guidexos
