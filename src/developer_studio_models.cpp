#include "developer_studio_models.h"

namespace guidexos {
namespace developer_studio {
namespace {

static const Capability kInitialCapabilities[] = {
    { "native-gui", "Native GUI application", true, CapabilityMaturity::Supported },
    { "app-model-manifest", "App Model manifest registration", true, CapabilityMaturity::Supported },
    { "hosted-native-runner", "Hosted Native ELF runner", true, CapabilityMaturity::Experimental },
    { "workspace-model", "Workspace and project model", true, CapabilityMaturity::Partial },
    { "build-integration", "Project build integration", false, CapabilityMaturity::Unavailable }
};

static const TargetProfile kInitialTargetProfile = {
    "guidexos.amd64.hosted.native",
    "guideXOS AMD64 Hosted — Native",
    "amd64",
    "guidexos-c-abi-v1",
    "Windows hosted guideXOS Server",
    "guideXOS Native SDK v1",
    "LLVM/LLD Native ELF",
    "guideXOS Hosted Native ELF Runtime",
    kInitialCapabilities,
    sizeof(kInitialCapabilities) / sizeof(kInitialCapabilities[0]),
    CapabilityMaturity::Experimental
};

} // namespace

const TargetProfile& InitialTargetProfile() {
    return kInitialTargetProfile;
}

bool IsValidTargetProfile(const TargetProfile& profile) {
    return profile.id && profile.displayName;
}

const char* ToString(ProjectKind kind) {
    switch (kind) {
    case ProjectKind::NativeGuiApplication: return "Native GUI application";
    case ProjectKind::ConsoleApplication: return "Console application";
    case ProjectKind::ComponentLibrary: return "Component/library";
    case ProjectKind::WebsiteOrHttpService: return "Website or HTTP service";
    case ProjectKind::Game2D: return "2D game";
    case ProjectKind::SystemComponent: return "System component";
    case ProjectKind::ExperimentalOther: return "Experimental/other";
    default: return "Unknown project kind";
    }
}

const char* ToString(CapabilityMaturity maturity) {
    switch (maturity) {
    case CapabilityMaturity::Unavailable: return "unavailable";
    case CapabilityMaturity::Experimental: return "experimental";
    case CapabilityMaturity::Partial: return "partial";
    case CapabilityMaturity::Supported: return "supported";
    case CapabilityMaturity::Deprecated: return "deprecated";
    default: return "unknown";
    }
}

} // namespace developer_studio
} // namespace guidexos
