#include "developer_studio_app_activation.h"

#include <cstring>
#include <iostream>

using guidexos::developer_studio::AppModelActivationPathStatus;
using guidexos::developer_studio::CopyAppModelActivationPath;

namespace {

char g_activationPath[] = "/Projects/Phase15/src/main.cpp";
uint32_t g_callbackCalls = 0;
uint32_t g_callbackMode = 0;

gx_result GX_CALL get_document_activation_path(gx_app_context*, char* path,
                                                uint32_t capacity, uint32_t* requiredBytes) {
    ++g_callbackCalls;
    if (!requiredBytes) return GX_ERROR_INVALID_ARGUMENT;
    if (g_callbackMode == 1) {
        *requiredBytes = 0;
        if (capacity && path) path[0] = '\0';
        return GX_OK;
    }
    if (g_callbackMode == 2) {
        *requiredBytes = 0;
        return GX_ERROR_UNSUPPORTED;
    }
    const uint32_t required = static_cast<uint32_t>(sizeof(g_activationPath));
    *requiredBytes = required;
    if (!path || capacity < required) {
        if (capacity && path) path[0] = '\0';
        return GX_ERROR_INVALID_ARGUMENT;
    }
    if (g_callbackMode == 3) {
        std::memset(path, 'x', required);
        path[required - 1] = 'x';
        return GX_OK;
    }
    if (g_callbackMode == 4) {
        std::memcpy(path, g_activationPath, required);
        path[3] = '\0';
        return GX_OK;
    }
    std::memcpy(path, g_activationPath, required);
    return GX_OK;
}

} // namespace

int main() {
    uint32_t checks = 0;
    const auto check = [&checks](bool value, const char* label) {
        ++checks;
        if (!value) std::cerr << "FAIL: " << label << '\n';
        return value;
    };
    bool passed = true;
    char path[128];
    std::memset(path, 'x', sizeof(path));

    gx_host_calls host = {};
    gx_app_context context = {};
    host.size = sizeof(host);
    context.host = &host;
    AppModelActivationPathStatus status;

    g_callbackCalls = 0;
    status = CopyAppModelActivationPath(&context, path, sizeof(path)).status;
    passed &= check(status == AppModelActivationPathStatus::Unavailable && path[0] == '\0' &&
                    g_callbackCalls == 0, "null optional callback is explicitly unavailable");

    host.get_document_activation_path = get_document_activation_path;
    host.size = static_cast<uint32_t>(offsetof(gx_host_calls, get_document_activation_path));
    status = CopyAppModelActivationPath(&context, path, sizeof(path)).status;
    passed &= check(status == AppModelActivationPathStatus::Unavailable && path[0] == '\0' &&
                    g_callbackCalls == 0, "short legacy host table does not expose the tail callback");

    host.size = sizeof(host);
    g_callbackMode = 0;
    const auto valid = CopyAppModelActivationPath(&context, path, sizeof(path));
    passed &= check(valid.status == AppModelActivationPathStatus::Ready &&
                    valid.requiredBytes == sizeof(g_activationPath) &&
                    std::memcmp(path, g_activationPath, sizeof(g_activationPath)) == 0 &&
                    path[valid.requiredBytes - 1] == '\0',
                    "valid callback returns exact caller-owned terminated path bytes");
    g_activationPath[0] = 'X';
    passed &= check(path[0] == '/', "copied activation path does not retain a borrowed provider pointer");
    g_activationPath[0] = '/';

    g_callbackMode = 1;
    status = CopyAppModelActivationPath(&context, path, sizeof(path)).status;
    passed &= check(status == AppModelActivationPathStatus::NoDocument && path[0] == '\0',
                    "ordinary application launch is explicitly distinguished from unavailable");

    g_callbackMode = 2;
    status = CopyAppModelActivationPath(&context, path, sizeof(path)).status;
    passed &= check(status == AppModelActivationPathStatus::Unavailable && path[0] == '\0',
                    "provider unsupported result is an explicit bounded fallback");

    g_callbackMode = 0;
    char shortPath[8];
    std::memset(shortPath, 'x', sizeof(shortPath));
    const auto tooLong = CopyAppModelActivationPath(&context, shortPath, sizeof(shortPath));
    passed &= check(tooLong.status == AppModelActivationPathStatus::PathTooLong && shortPath[0] == '\0' &&
                    tooLong.requiredBytes == sizeof(g_activationPath),
                    "short destination reports required length without retaining truncated data");

    g_callbackMode = 3;
    status = CopyAppModelActivationPath(&context, path, sizeof(path)).status;
    passed &= check(status == AppModelActivationPathStatus::MalformedPath && path[0] == '\0',
                    "unterminated provider path is rejected");

    g_callbackMode = 4;
    status = CopyAppModelActivationPath(&context, path, sizeof(path)).status;
    passed &= check(status == AppModelActivationPathStatus::MalformedPath && path[0] == '\0',
                    "embedded NUL provider path is rejected");

    std::cout << "developerStudioDocumentActivationAbiChecks=" << (passed ? checks : 0)
              << "/" << checks << "\n";
    return passed ? 0 : 1;
}
