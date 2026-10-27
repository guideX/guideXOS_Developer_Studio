#include "developer_studio_tools.h"

#include <cassert>
#include <cstring>
#include <iostream>

using namespace guidexos::developer_studio;

struct ToolLifecycle {
    uint32_t activateCount;
    uint32_t closeCount;
    bool activationSucceeds;
    bool closeSucceeds;
};

static bool activateFindInFiles(void* userData) {
    ToolLifecycle* lifecycle = static_cast<ToolLifecycle*>(userData);
    ++lifecycle->activateCount;
    return lifecycle->activationSucceeds;
}

static bool closeFindInFiles(void* userData) {
    ToolLifecycle* lifecycle = static_cast<ToolLifecycle*>(userData);
    ++lifecycle->closeCount;
    return lifecycle->closeSucceeds;
}

int main() {
    DeveloperToolRegistry registry = {};
    DeveloperToolRegistryInit(&registry);
    ToolLifecycle lifecycle = { 0, 0, true, true };
    const DeveloperToolCommand findInFiles = {
        DeveloperToolId::FindInFiles, "Find in Files", "Ctrl+Shift+F",
        activateFindInFiles, closeFindInFiles
    };

    assert(DeveloperToolRegistryRegister(&registry, &findInFiles));
    assert(!DeveloperToolRegistryRegister(&registry, &findInFiles));
    assert(DeveloperToolRegistryAt(&registry, 0) != nullptr);
    assert(DeveloperToolRegistryAt(&registry, 1) == nullptr);
    const DeveloperToolCommand* registered =
        DeveloperToolRegistryFind(&registry, DeveloperToolId::FindInFiles);
    assert(registered && std::strcmp(registered->label, "Find in Files") == 0);
    assert(std::strcmp(registered->shortcut, "Ctrl+Shift+F") == 0);

    // The Tools menu and Ctrl+Shift+F both dispatch through this same command
    // activation path. Re-activation preserves an already-open utility.
    assert(DeveloperToolRegistryActivate(&registry, DeveloperToolId::FindInFiles, &lifecycle));
    assert(DeveloperToolRegistryActivate(&registry, DeveloperToolId::FindInFiles, &lifecycle));
    assert(lifecycle.activateCount == 2);
    assert(DeveloperToolRegistryIsActive(&registry, DeveloperToolId::FindInFiles));

    lifecycle.closeSucceeds = false;
    assert(!DeveloperToolRegistryClose(&registry, DeveloperToolId::FindInFiles, &lifecycle));
    assert(DeveloperToolRegistryIsActive(&registry, DeveloperToolId::FindInFiles));
    lifecycle.closeSucceeds = true;
    assert(DeveloperToolRegistryCloseActive(&registry, &lifecycle));
    assert(!DeveloperToolRegistryIsActive(&registry, DeveloperToolId::FindInFiles));
    const uint32_t closesAfterActive = lifecycle.closeCount;
    assert(DeveloperToolRegistryClose(&registry, DeveloperToolId::FindInFiles, &lifecycle));
    assert(lifecycle.closeCount == closesAfterActive);

    lifecycle.activationSucceeds = false;
    assert(!DeveloperToolRegistryActivate(&registry, DeveloperToolId::FindInFiles, &lifecycle));
    assert(!DeveloperToolRegistryIsActive(&registry, DeveloperToolId::FindInFiles));
    std::cout << "Developer Studio static tools registry PASS\n";
    return 0;
}
