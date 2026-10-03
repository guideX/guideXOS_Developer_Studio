#include "developer_studio_app_activation.h"

#include <stddef.h>

namespace guidexos {
namespace developer_studio {

AppModelActivationPathResult CopyAppModelActivationPath(const gx_app_context* context,
                                                        char* path,
                                                        uint32_t pathCapacity) {
    AppModelActivationPathResult result = {
        AppModelActivationPathStatus::InvalidContext,
        GX_ERROR_INVALID_ARGUMENT,
        0
    };
    if (!context || !context->host) return result;
    if (!path || pathCapacity == 0) {
        result.status = AppModelActivationPathStatus::InvalidBuffer;
        return result;
    }

    path[0] = '\0';
    const gx_host_calls* host = context->host;
    const size_t callbackEnd = offsetof(gx_host_calls, get_document_activation_path) +
        sizeof(host->get_document_activation_path);
    if (host->size < callbackEnd || !host->get_document_activation_path) {
        result.status = AppModelActivationPathStatus::Unavailable;
        result.hostResult = GX_ERROR_UNSUPPORTED;
        return result;
    }

    uint32_t requiredBytes = 0;
    const gx_result hostResult = host->get_document_activation_path(
        const_cast<gx_app_context*>(context), path, pathCapacity, &requiredBytes);
    result.hostResult = hostResult;
    result.requiredBytes = requiredBytes;
    if (hostResult == GX_OK && requiredBytes == 0) {
        result.status = AppModelActivationPathStatus::NoDocument;
        return result;
    }
    if (hostResult == GX_ERROR_UNSUPPORTED || hostResult == GX_ERROR_NOT_IMPLEMENTED) {
        path[0] = '\0';
        result.status = AppModelActivationPathStatus::Unavailable;
        return result;
    }
    if (requiredBytes > pathCapacity) {
        path[0] = '\0';
        result.status = AppModelActivationPathStatus::PathTooLong;
        return result;
    }
    if (hostResult != GX_OK) {
        path[0] = '\0';
        result.status = AppModelActivationPathStatus::HostFailure;
        return result;
    }
    if (requiredBytes < 2 || path[requiredBytes - 1] != '\0') {
        path[0] = '\0';
        result.status = AppModelActivationPathStatus::MalformedPath;
        return result;
    }
    for (uint32_t i = 0; i + 1 < requiredBytes; ++i) {
        if (path[i] == '\0') {
            path[0] = '\0';
            result.status = AppModelActivationPathStatus::MalformedPath;
            return result;
        }
    }

    result.status = AppModelActivationPathStatus::Ready;
    return result;
}

} // namespace developer_studio
} // namespace guidexos
