#include "developer_studio_models.h"

#include <iostream>

using namespace guidexos::developer_studio;

int main() {
    const TargetProfile& target = InitialTargetProfile();
    if (!IsValidTargetProfile(target)) return 1;
    if (!target.id || !target.displayName || !target.architecture || !target.abi ||
        !target.machine || !target.sdk || !target.toolchain || !target.runner) return 2;
    if (target.capabilityCount < 4 || target.maturity != CapabilityMaturity::Experimental) return 3;

    bool sawUnavailableBuild = false;
    bool sawSupportedGui = false;
    for (unsigned i = 0; i < target.capabilityCount; ++i) {
        const Capability& capability = target.capabilities[i];
        if (!capability.id || !capability.displayName) return 4;
        if (capability.id[0] == 'b' && !capability.available && capability.maturity == CapabilityMaturity::Unavailable) {
            sawUnavailableBuild = true;
        }
        if (capability.id[0] == 'n' && capability.available && capability.maturity == CapabilityMaturity::Supported) {
            sawSupportedGui = true;
        }
    }
    if (!sawUnavailableBuild || !sawSupportedGui) return 5;
    if (ToString(ProjectKind::NativeGuiApplication)[0] == '\0') return 6;
    if (ToString(CapabilityMaturity::Experimental)[0] == '\0') return 7;

    const Project project = { "phase1-shell", "Developer Studio shell", ProjectKind::NativeGuiApplication };
    const char* documents[] = { "welcome" };
    const Workspace workspace = { "empty", "No workspace open", &project, 1, documents, 1 };
    if (!workspace.projects || workspace.projectCount != 1 || workspace.openDocumentCount != 1) return 8;

    std::cout << "Developer Studio model test PASS\n";
    return 0;
}
