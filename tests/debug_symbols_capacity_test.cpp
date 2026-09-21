#include "developer_studio_debug_symbols.h"

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

static bool readFile(const char* path, std::vector<unsigned char>* bytes) {
    if (!path || !bytes) return false;
    std::ifstream input(path, std::ios::binary);
    if (!input) return false;
    input.seekg(0, std::ios::end);
    const std::streamoff size = input.tellg();
    if (size <= 0) return false;
    input.seekg(0, std::ios::beg);
    bytes->resize(static_cast<size_t>(size));
    input.read(reinterpret_cast<char*>(&(*bytes)[0]), size);
    return input.good() || input.eof();
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: debug_symbols_capacity_test <debugger-phase3b.elf>\n";
        return 2;
    }

    std::vector<unsigned char> elf;
    if (!readFile(argv[1], &elf)) {
        std::cerr << "unable to read fixture: " << argv[1] << "\n";
        return 2;
    }

    char sha256[65] = {};
    if (!guidexos::developer_studio::DebugDwarfComputeSha256(
            &elf[0], elf.size(), sha256, sizeof(sha256))) {
        std::cerr << "unable to hash fixture\n";
        return 1;
    }

    guidexos::developer_studio::DebugDwarfMapper mapper = {};
    guidexos::developer_studio::DebugDwarfError error =
        guidexos::developer_studio::DebugDwarfError::None;
    const bool loaded = guidexos::developer_studio::DebugDwarfMapperLoad(
        &mapper,
        "D:/dev/guideXOS_Developer_Studio/tests/fixtures/debugger-phase3b",
        "debugger-phase3b", "native", "amd64", argv[1], elf.size(), sha256,
        1, &elf[0], elf.size(), 1, &error);
    if (!loaded || mapper.debugInfoDieCount <= 256u) {
        std::cerr << "bare DWARF capacity regression: loaded=" << (loaded ? 1 : 0)
                  << " error="
                  << guidexos::developer_studio::DebugDwarfErrorName(error)
                  << " dies=" << mapper.debugInfoDieCount << "\n";
        return 1;
    }

    std::cout << "Developer Studio bare DWARF capacity PASS dies="
              << mapper.debugInfoDieCount << "\n";
    return 0;
}
