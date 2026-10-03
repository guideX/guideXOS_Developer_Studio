#pragma once

#include <guidexos/app.h>

namespace guidexos {
namespace developer_studio {

enum class AppModelActivationPathStatus {
    Ready = 0,
    NoDocument,
    Unavailable,
    InvalidContext,
    InvalidBuffer,
    PathTooLong,
    MalformedPath,
    HostFailure
};

struct AppModelActivationPathResult {
    AppModelActivationPathStatus status;
    gx_result hostResult;
    uint32_t requiredBytes;
};

AppModelActivationPathResult CopyAppModelActivationPath(const gx_app_context* context,
                                                        char* path,
                                                        uint32_t pathCapacity);

} // namespace developer_studio
} // namespace guidexos
