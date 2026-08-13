extern "C" void* memset(void* destination, int value, unsigned long size) {
    unsigned char* bytes = static_cast<unsigned char*>(destination);
    for (unsigned long i = 0; i < size; ++i) bytes[i] = static_cast<unsigned char>(value);
    return destination;
}
