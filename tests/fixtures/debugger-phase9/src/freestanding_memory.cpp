#include <stdint.h>

extern "C" void* memcpy(void* destination, const void* source, uint64_t bytes) {
    unsigned char* out = static_cast<unsigned char*>(destination);
    const unsigned char* in = static_cast<const unsigned char*>(source);
    for (uint64_t i = 0; i < bytes; ++i) out[i] = in[i];
    return destination;
}

extern "C" void* memset(void* destination, int value, uint64_t bytes) {
    unsigned char* out = static_cast<unsigned char*>(destination);
    for (uint64_t i = 0; i < bytes; ++i) out[i] = static_cast<unsigned char>(value);
    return destination;
}
