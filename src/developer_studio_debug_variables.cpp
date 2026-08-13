#include "developer_studio_debug_symbols.h"

namespace guidexos {
namespace developer_studio {
namespace {

static const uint16_t kElfTypeExec = 2;
static const uint16_t kElfMachineAmd64 = 62;
static const uint32_t kSectionTypeString = 3;
static const uint32_t kSectionTypeNoBits = 8;
static const uint16_t kDwarfVersion4 = 4;
static const uint16_t kDwarfVersion5 = 5;

static const uint16_t kTagArrayType = 0x01;
static const uint16_t kTagClassType = 0x02;
static const uint16_t kTagEnumerationType = 0x04;
static const uint16_t kTagFormalParameter = 0x05;
static const uint16_t kTagMember = 0x0d;
static const uint16_t kTagLexicalBlock = 0x0b;
static const uint16_t kTagPointerType = 0x0f;
static const uint16_t kTagTypedef = 0x16;
static const uint16_t kTagStructureType = 0x13;
static const uint16_t kTagUnionType = 0x17;
static const uint16_t kTagInheritance = 0x1c;
static const uint16_t kTagBaseType = 0x24;
static const uint16_t kTagConstType = 0x26;
static const uint16_t kTagSubprogram = 0x2e;
static const uint16_t kTagSubrangeType = 0x21;
static const uint16_t kTagVariable = 0x34;
static const uint16_t kTagVolatileType = 0x35;
static const uint16_t kTagCompileUnit = 0x11;

static const uint16_t kAttributeName = 0x03;
static const uint16_t kAttributeType = 0x49;
static const uint16_t kAttributeLocation = 0x02;
static const uint16_t kAttributeFrameBase = 0x40;
static const uint16_t kAttributeLowPc = 0x11;
static const uint16_t kAttributeHighPc = 0x12;
static const uint16_t kAttributeRanges = 0x55;
static const uint16_t kAttributeByteSize = 0x0b;
static const uint16_t kAttributeEncoding = 0x3e;
static const uint16_t kAttributeDeclFile = 0x3a;
static const uint16_t kAttributeDeclLine = 0x3b;
static const uint16_t kAttributeConstValue = 0x1c;
static const uint16_t kAttributeCount = 0x37;
static const uint16_t kAttributeDataMemberLocation = 0x38;
static const uint16_t kAttributeAccessibility = 0x32;
static const uint16_t kAttributeDeclaration = 0x3c;
static const uint16_t kAttributeLowerBound = 0x22;
static const uint16_t kAttributeUpperBound = 0x2f;
static const uint16_t kAttributeBitOffset = 0x0c;
static const uint16_t kAttributeBitSize = 0x0d;
static const uint16_t kAttributeDataBitOffset = 0x6b;
static const uint16_t kAttributeArtificial = 0x34;
static const uint16_t kAttributeStrOffsetsBase = 0x72;
static const uint16_t kAttributeAddrBase = 0x73;

static const uint16_t kFormAddr = 0x01;
static const uint16_t kFormBlock2 = 0x03;
static const uint16_t kFormBlock4 = 0x04;
static const uint16_t kFormData2 = 0x05;
static const uint16_t kFormData4 = 0x06;
static const uint16_t kFormData8 = 0x07;
static const uint16_t kFormString = 0x08;
static const uint16_t kFormBlock = 0x09;
static const uint16_t kFormBlock1 = 0x0a;
static const uint16_t kFormData1 = 0x0b;
static const uint16_t kFormFlag = 0x0c;
static const uint16_t kFormSdata = 0x0d;
static const uint16_t kFormStrp = 0x0e;
static const uint16_t kFormUdata = 0x0f;
static const uint16_t kFormRefAddr = 0x10;
static const uint16_t kFormRef1 = 0x11;
static const uint16_t kFormRef2 = 0x12;
static const uint16_t kFormRef4 = 0x13;
static const uint16_t kFormRef8 = 0x14;
static const uint16_t kFormRefUdata = 0x15;
static const uint16_t kFormIndirect = 0x16;
static const uint16_t kFormSecOffset = 0x17;
static const uint16_t kFormExprloc = 0x18;
static const uint16_t kFormFlagPresent = 0x19;
static const uint16_t kFormStrx = 0x1a;
static const uint16_t kFormAddrx = 0x1b;
static const uint16_t kFormData16 = 0x1e;
static const uint16_t kFormLineStrp = 0x1f;
static const uint16_t kFormImplicitConst = 0x21;
static const uint16_t kFormLoclistx = 0x22;
static const uint16_t kFormRnglistx = 0x23;
static const uint16_t kFormStrx1 = 0x25;
static const uint16_t kFormStrx2 = 0x26;
static const uint16_t kFormStrx3 = 0x27;
static const uint16_t kFormStrx4 = 0x28;
static const uint16_t kFormAddrx1 = 0x29;
static const uint16_t kFormAddrx2 = 0x2a;
static const uint16_t kFormAddrx3 = 0x2b;
static const uint16_t kFormAddrx4 = 0x2c;

static const uint8_t kOpAddr = 0x03;
static const uint8_t kOpDeref = 0x06;
static const uint8_t kOpConst1u = 0x08;
static const uint8_t kOpConst1s = 0x09;
static const uint8_t kOpConst2u = 0x0a;
static const uint8_t kOpConst2s = 0x0b;
static const uint8_t kOpConst4u = 0x0c;
static const uint8_t kOpConst4s = 0x0d;
static const uint8_t kOpConst8u = 0x0e;
static const uint8_t kOpConst8s = 0x0f;
static const uint8_t kOpConstu = 0x10;
static const uint8_t kOpConsts = 0x11;
static const uint8_t kOpReg0 = 0x50;
static const uint8_t kOpBreg0 = 0x70;
static const uint8_t kOpRegx = 0x90;
static const uint8_t kOpFbreg = 0x91;
static const uint8_t kOpBregx = 0x92;
static const uint8_t kOpPlusUconst = 0x23;
static const uint8_t kOpStackValue = 0x9f;
static const uint8_t kOpAddrx = 0xa1;

struct SectionView {
    const unsigned char* data;
    uint64_t size;
};

struct Cursor {
    const unsigned char* bytes;
    uint64_t position;
    uint64_t end;
};

struct AbbrevAttribute {
    uint16_t attribute;
    uint16_t form;
    bool hasImplicitConst;
    int64_t implicitConst;
};

struct AbbrevDeclaration {
    bool used;
    uint32_t code;
    uint16_t tag;
    bool hasChildren;
    uint32_t attributeCount;
    AbbrevAttribute attributes[kDebugDwarfMaxAttributes];
};

struct FormValue {
    bool number;
    bool signedNumber;
    int64_t signedValue;
    uint64_t unsignedValue;
    bool stringIndex;
    uint64_t stringIndexValue;
    bool addressIndex;
    uint64_t addressIndexValue;
    bool directString;
    char stringValue[kDebugDwarfMaxVariableNameBytes];
    bool block;
    bool list;
    uint32_t blockLength;
    uint8_t blockBytes[kDebugDwarfMaxExpressionBytes];
};

static bool checkedRange(uint64_t offset, uint64_t size, uint64_t total) {
    return offset <= total && size <= total - offset;
}

static uint16_t readU16(const unsigned char* bytes, uint64_t offset) {
    return static_cast<uint16_t>(bytes[offset]) |
        static_cast<uint16_t>(static_cast<uint16_t>(bytes[offset + 1]) << 8);
}

static uint32_t readU32(const unsigned char* bytes, uint64_t offset) {
    return static_cast<uint32_t>(bytes[offset]) |
        (static_cast<uint32_t>(bytes[offset + 1]) << 8) |
        (static_cast<uint32_t>(bytes[offset + 2]) << 16) |
        (static_cast<uint32_t>(bytes[offset + 3]) << 24);
}

static uint64_t readU64(const unsigned char* bytes, uint64_t offset) {
    uint64_t value = 0;
    for (uint32_t i = 0; i < 8; ++i) value |= static_cast<uint64_t>(bytes[offset + i]) << (i * 8);
    return value;
}

static uint32_t textLength(const char* value, uint32_t capacity) {
    if (!value) return 0;
    uint32_t length = 0;
    while (length < capacity && value[length] != '\0') ++length;
    return length;
}

static bool copyText(char* output, uint32_t outputSize, const char* input) {
    if (!output || outputSize == 0) return false;
    if (!input) input = "";
    const uint32_t length = textLength(input, outputSize);
    if (length >= outputSize) { output[0] = '\0'; return false; }
    for (uint32_t i = 0; i < length; ++i) output[i] = input[i];
    output[length] = '\0';
    return true;
}

static void clearFormValue(FormValue* value) {
    if (!value) return;
    *value = FormValue();
}

static bool canRead(const Cursor& cursor, uint64_t count) {
    return cursor.position <= cursor.end && count <= cursor.end - cursor.position;
}

static bool readByte(Cursor& cursor, uint8_t* value) {
    if (!value || !canRead(cursor, 1)) return false;
    *value = cursor.bytes[cursor.position++];
    return true;
}

static bool readU16Cursor(Cursor& cursor, uint16_t* value) {
    if (!value || !canRead(cursor, 2)) return false;
    *value = readU16(cursor.bytes, cursor.position);
    cursor.position += 2;
    return true;
}

static bool readU32Cursor(Cursor& cursor, uint32_t* value) {
    if (!value || !canRead(cursor, 4)) return false;
    *value = readU32(cursor.bytes, cursor.position);
    cursor.position += 4;
    return true;
}

static bool readU64Cursor(Cursor& cursor, uint64_t* value) {
    if (!value || !canRead(cursor, 8)) return false;
    *value = readU64(cursor.bytes, cursor.position);
    cursor.position += 8;
    return true;
}

static bool readULEB(Cursor& cursor, uint64_t* value) {
    if (!value) return false;
    *value = 0;
    for (uint32_t i = 0; i < 10; ++i) {
        uint8_t byte = 0;
        if (!readByte(cursor, &byte)) return false;
        const uint32_t shift = i * 7;
        if (shift >= 64 || (shift == 63 && (byte & 0x7e) != 0)) return false;
        *value |= static_cast<uint64_t>(byte & 0x7f) << shift;
        if ((byte & 0x80) == 0) return true;
    }
    return false;
}

static bool readSLEB(Cursor& cursor, int64_t* value) {
    if (!value) return false;
    *value = 0;
    for (uint32_t i = 0; i < 10; ++i) {
        uint8_t byte = 0;
        if (!readByte(cursor, &byte)) return false;
        const uint32_t shift = i * 7;
        if (shift >= 64 || (shift == 63 && (byte & 0x7e) != 0)) return false;
        *value |= static_cast<int64_t>(static_cast<uint64_t>(byte & 0x7f) << shift);
        if ((byte & 0x80) == 0) {
            if ((byte & 0x40) != 0 && shift + 7 < 64)
                *value |= static_cast<int64_t>(UINT64_MAX << (shift + 7));
            return true;
        }
    }
    return false;
}

static bool readCString(Cursor& cursor, char* output, uint32_t outputSize) {
    if (!output || outputSize == 0) return false;
    uint32_t length = 0;
    while (cursor.position < cursor.end) {
        const char value = static_cast<char>(cursor.bytes[cursor.position++]);
        if (value == '\0') { output[length] = '\0'; return true; }
        if (length + 1 >= outputSize) return false;
        output[length++] = value;
    }
    return false;
}

static bool readSectionString(const SectionView& section, uint64_t offset,
                              char* output, uint32_t outputSize) {
    if (!section.data || offset >= section.size || !output || outputSize == 0) return false;
    Cursor cursor = { section.data, offset, section.size };
    return readCString(cursor, output, outputSize);
}

static bool addSigned(uint64_t base, int64_t offset, uint64_t* result) {
    if (!result) return false;
    if (offset >= 0) {
        const uint64_t amount = static_cast<uint64_t>(offset);
        if (base > UINT64_MAX - amount) return false;
        *result = base + amount;
        return true;
    }
    const uint64_t amount = static_cast<uint64_t>(-(offset + 1)) + 1u;
    if (base < amount) return false;
    *result = base - amount;
    return true;
}

static bool findSection(const unsigned char* bytes, uint64_t size, uint64_t sectionOffset,
                        uint16_t sectionEntrySize, uint16_t sectionCount,
                        const SectionView& sectionNames, const char* wanted,
                        SectionView* out) {
    if (!out || !wanted) return false;
    out->data = nullptr;
    out->size = 0;
    for (uint16_t i = 0; i < sectionCount; ++i) {
        const uint64_t header = sectionOffset + static_cast<uint64_t>(i) * sectionEntrySize;
        const uint32_t nameOffset = readU32(bytes, header);
        char name[96] = {};
        if (!readSectionString(sectionNames, nameOffset, name, sizeof(name))) return false;
        if (textLength(name, sizeof(name)) != textLength(wanted, 96)) continue;
        bool equal = true;
        for (uint32_t j = 0; name[j] || wanted[j]; ++j) if (name[j] != wanted[j]) { equal = false; break; }
        if (!equal) continue;
        const uint32_t type = readU32(bytes, header + 4);
        const uint64_t offset = readU64(bytes, header + 24);
        const uint64_t sectionSize = readU64(bytes, header + 32);
        if (type == kSectionTypeNoBits) return true;
        if (sectionSize > kDebugMapperMaxSectionBytes || !checkedRange(offset, sectionSize, size)) return false;
        out->data = bytes + offset;
        out->size = sectionSize;
        return true;
    }
    return true;
}

static bool collectSections(const unsigned char* bytes, uint64_t size,
                            SectionView* info, SectionView* abbrev, SectionView* strings,
                            SectionView* stringOffsets, SectionView* lineStrings,
                            SectionView* addresses, SectionView* loclists) {
    if (!bytes || size < 64 || bytes[0] != 0x7f || bytes[1] != 'E' || bytes[2] != 'L' ||
        bytes[3] != 'F' || bytes[4] != 2 || bytes[5] != 1 || readU16(bytes, 16) != kElfTypeExec ||
        readU16(bytes, 18) != kElfMachineAmd64) return false;
    const uint64_t sectionOffset = readU64(bytes, 40);
    const uint16_t sectionEntrySize = readU16(bytes, 58);
    const uint16_t sectionCount = readU16(bytes, 60);
    const uint16_t stringIndex = readU16(bytes, 62);
    if (sectionCount == 0 || sectionCount > kDebugMapperMaxSections || sectionEntrySize < 64 ||
        stringIndex >= sectionCount || !checkedRange(sectionOffset,
            static_cast<uint64_t>(sectionEntrySize) * sectionCount, size)) return false;
    const uint64_t stringHeader = sectionOffset + static_cast<uint64_t>(stringIndex) * sectionEntrySize;
    const uint32_t stringType = readU32(bytes, stringHeader + 4);
    const uint64_t stringOffset = readU64(bytes, stringHeader + 24);
    const uint64_t stringSize = readU64(bytes, stringHeader + 32);
    if (stringType != kSectionTypeString || !checkedRange(stringOffset, stringSize, size)) return false;
    SectionView sectionNames = { bytes + stringOffset, stringSize };
    return findSection(bytes, size, sectionOffset, sectionEntrySize, sectionCount, sectionNames, ".debug_info", info) &&
        findSection(bytes, size, sectionOffset, sectionEntrySize, sectionCount, sectionNames, ".debug_abbrev", abbrev) &&
        findSection(bytes, size, sectionOffset, sectionEntrySize, sectionCount, sectionNames, ".debug_str", strings) &&
        findSection(bytes, size, sectionOffset, sectionEntrySize, sectionCount, sectionNames, ".debug_str_offsets", stringOffsets) &&
        findSection(bytes, size, sectionOffset, sectionEntrySize, sectionCount, sectionNames, ".debug_line_str", lineStrings) &&
        findSection(bytes, size, sectionOffset, sectionEntrySize, sectionCount, sectionNames, ".debug_addr", addresses) &&
        findSection(bytes, size, sectionOffset, sectionEntrySize, sectionCount, sectionNames, ".debug_loclists", loclists);
}

static bool parseAbbreviations(const SectionView& section, uint32_t offset,
                               AbbrevDeclaration* declarations, uint32_t* count) {
    if (!declarations || !count || offset >= section.size) return false;
    *count = 0;
    Cursor cursor = { section.data, offset, section.size };
    while (cursor.position < cursor.end) {
        uint64_t codeValue = 0;
        if (!readULEB(cursor, &codeValue) || codeValue > UINT32_MAX) return false;
        if (codeValue == 0) return true;
        if (*count >= kDebugDwarfMaxAbbreviations) return false;
        uint64_t tagValue = 0;
        uint8_t children = 0;
        if (!readULEB(cursor, &tagValue) || tagValue > UINT16_MAX || !readByte(cursor, &children)) return false;
        AbbrevDeclaration& declaration = declarations[(*count)++];
        declaration = AbbrevDeclaration();
        declaration.used = true;
        declaration.code = static_cast<uint32_t>(codeValue);
        declaration.tag = static_cast<uint16_t>(tagValue);
        declaration.hasChildren = children != 0;
        while (true) {
            uint64_t attribute = 0, form = 0;
            if (!readULEB(cursor, &attribute) || !readULEB(cursor, &form)) return false;
            if ((attribute == 0) != (form == 0)) return false;
            if (attribute == 0) break;
            if (attribute > UINT16_MAX || form > UINT16_MAX ||
                declaration.attributeCount >= kDebugDwarfMaxAttributes) return false;
            AbbrevAttribute& pair = declaration.attributes[declaration.attributeCount++];
            pair = AbbrevAttribute();
            pair.attribute = static_cast<uint16_t>(attribute);
            pair.form = static_cast<uint16_t>(form);
            if (pair.form == kFormImplicitConst) {
                if (!readSLEB(cursor, &pair.implicitConst)) return false;
                pair.hasImplicitConst = true;
            }
        }
    }
    return false;
}

static bool readSizedNumber(Cursor& cursor, uint32_t width, uint64_t* value) {
    if (!value || width > 8 || !canRead(cursor, width)) return false;
    *value = 0;
    for (uint32_t i = 0; i < width; ++i) *value |= static_cast<uint64_t>(cursor.bytes[cursor.position + i]) << (i * 8);
    cursor.position += width;
    return true;
}

static bool readForm(Cursor& cursor, uint16_t form, bool hasImplicitConst,
                     int64_t implicitConst, uint8_t addressSize, FormValue* value,
                     uint32_t indirectionDepth = 0) {
    if (!value || indirectionDepth > 4) return false;
    clearFormValue(value);
    if (form == kFormImplicitConst) {
        if (!hasImplicitConst) return false;
        value->number = true;
        value->signedNumber = true;
        value->signedValue = implicitConst;
        value->unsignedValue = static_cast<uint64_t>(implicitConst);
        return true;
    }
    if (form == kFormIndirect) {
        uint64_t indirect = 0;
        if (!readULEB(cursor, &indirect) || indirect > UINT16_MAX) return false;
        return readForm(cursor, static_cast<uint16_t>(indirect), false, 0, addressSize, value,
                        indirectionDepth + 1);
    }
    uint64_t number = 0;
    switch (form) {
    case kFormAddr:
        if (!readSizedNumber(cursor, addressSize, &number)) return false;
        value->number = true; value->unsignedValue = number; return true;
    case kFormData1:
        if (!readSizedNumber(cursor, 1, &number)) return false;
        value->number = true; value->unsignedValue = number; return true;
    case kFormData2:
        if (!readSizedNumber(cursor, 2, &number)) return false;
        value->number = true; value->unsignedValue = number; return true;
    case kFormData4:
        if (!readSizedNumber(cursor, 4, &number)) return false;
        value->number = true; value->unsignedValue = number; return true;
    case kFormData8:
        if (!readSizedNumber(cursor, 8, &number)) return false;
        value->number = true; value->unsignedValue = number; return true;
    case kFormData16:
        if (!canRead(cursor, 16)) return false;
        cursor.position += 16; value->number = true; value->unsignedValue = 0; return true;
    case kFormUdata:
        if (!readULEB(cursor, &number)) return false;
        value->number = true; value->unsignedValue = number; return true;
    case kFormSdata: {
        int64_t signedValue = 0;
        if (!readSLEB(cursor, &signedValue)) return false;
        value->number = true; value->signedNumber = true; value->signedValue = signedValue;
        value->unsignedValue = static_cast<uint64_t>(signedValue); return true;
    }
    case kFormFlag:
        if (!readByte(cursor, reinterpret_cast<uint8_t*>(&number))) return false;
        value->number = true; value->unsignedValue = number != 0; return true;
    case kFormFlagPresent:
        value->number = true; value->unsignedValue = 1; return true;
    case kFormString:
        value->directString = readCString(cursor, value->stringValue, sizeof(value->stringValue));
        return value->directString;
    case kFormStrp:
    case kFormLineStrp:
    case kFormSecOffset:
    case kFormRefAddr:
        if (!readSizedNumber(cursor, 4, &number)) return false;
        value->number = true; value->unsignedValue = number; return true;
    case kFormRef1:
        if (!readSizedNumber(cursor, 1, &number)) return false;
        value->number = true; value->unsignedValue = number; return true;
    case kFormRef2:
        if (!readSizedNumber(cursor, 2, &number)) return false;
        value->number = true; value->unsignedValue = number; return true;
    case kFormRef4:
        if (!readSizedNumber(cursor, 4, &number)) return false;
        value->number = true; value->unsignedValue = number; return true;
    case kFormRef8:
        if (!readSizedNumber(cursor, 8, &number)) return false;
        value->number = true; value->unsignedValue = number; return true;
    case kFormRefUdata:
        if (!readULEB(cursor, &number)) return false;
        value->number = true; value->unsignedValue = number; return true;
    case kFormStrx:
    case kFormAddrx:
        if (!readULEB(cursor, &number)) return false;
        if (form == kFormStrx) { value->stringIndex = true; value->stringIndexValue = number; }
        else { value->addressIndex = true; value->addressIndexValue = number; }
        return true;
    case kFormStrx1:
    case kFormStrx2:
    case kFormStrx3:
    case kFormStrx4: {
        const uint32_t width = form == kFormStrx1 ? 1 : form == kFormStrx2 ? 2 : form == kFormStrx3 ? 3 : 4;
        if (!readSizedNumber(cursor, width, &number)) return false;
        value->stringIndex = true; value->stringIndexValue = number; return true;
    }
    case kFormAddrx1:
    case kFormAddrx2:
    case kFormAddrx3:
    case kFormAddrx4: {
        const uint32_t width = form == kFormAddrx1 ? 1 : form == kFormAddrx2 ? 2 : form == kFormAddrx3 ? 3 : 4;
        if (!readSizedNumber(cursor, width, &number)) return false;
        value->addressIndex = true; value->addressIndexValue = number; return true;
    }
    case kFormExprloc: {
        if (!readULEB(cursor, &number) || number > kDebugDwarfMaxExpressionBytes || !canRead(cursor, number)) return false;
        value->block = true; value->blockLength = static_cast<uint32_t>(number);
        for (uint32_t i = 0; i < value->blockLength; ++i) value->blockBytes[i] = cursor.bytes[cursor.position + i];
        cursor.position += number;
        return true;
    }
    case kFormBlock1:
        if (!readSizedNumber(cursor, 1, &number)) return false;
        break;
    case kFormBlock2:
        if (!readSizedNumber(cursor, 2, &number)) return false;
        break;
    case kFormBlock4:
        if (!readSizedNumber(cursor, 4, &number)) return false;
        break;
    case kFormBlock:
        if (!readULEB(cursor, &number)) return false;
        break;
    case kFormLoclistx:
    case kFormRnglistx:
        if (!readULEB(cursor, &number)) return false;
        value->number = true; value->unsignedValue = number; value->list = true; return true;
    default:
        return false;
    }
    if (number > kDebugDwarfMaxExpressionBytes || !canRead(cursor, number)) return false;
    value->block = true; value->blockLength = static_cast<uint32_t>(number);
    for (uint32_t i = 0; i < value->blockLength; ++i) value->blockBytes[i] = cursor.bytes[cursor.position + i];
    cursor.position += number;
    return true;
}

static bool equalLiteral(const char* left, const char* right) {
    if (!left || !right) return false;
    for (uint32_t i = 0;; ++i) {
        if (left[i] != right[i]) return false;
        if (left[i] == '\0') return true;
    }
}

static int findAbbreviation(const AbbrevDeclaration* declarations, uint32_t count, uint32_t code) {
    for (uint32_t i = 0; i < count; ++i) if (declarations[i].used && declarations[i].code == code) return static_cast<int>(i);
    return -1;
}

static int findDie(const DebugDwarfMapper* mapper, uint64_t offset) {
    if (!mapper) return -1;
    for (uint32_t i = 0; i < mapper->debugInfoDieCount; ++i)
        if (mapper->dies[i].offset == offset) return static_cast<int>(i);
    return -1;
}

static bool resolveString(const SectionView& stringOffsets, const SectionView& strings,
                          uint32_t base, uint64_t index, char* output, uint32_t outputSize) {
    if (!output || outputSize == 0 || !stringOffsets.data || !strings.data ||
        index > (UINT64_MAX - base) / 4u || base + index * 4u > stringOffsets.size - 4u) return false;
    const uint32_t offset = readU32(stringOffsets.data, base + index * 4u);
    return readSectionString(strings, offset, output, outputSize);
}

static bool resolveAddress(const SectionView& addresses, uint32_t base, uint64_t index,
                           uint8_t addressSize, uint64_t* output) {
    if (!output || !addresses.data || addressSize != 8 || index > (UINT64_MAX - base) / 8u ||
        base + index * 8u > addresses.size - 8u) return false;
    *output = readU64(addresses.data, base + index * 8u);
    return true;
}

static void applyAttribute(DebugDwarfDieInfo* die, uint16_t attribute, const FormValue& value,
                           uint16_t form) {
    if (!die) return;
    switch (attribute) {
    case kAttributeName:
        if (value.stringIndex) { die->hasName = true; die->nameIsStringIndex = true; die->nameStringIndex = value.stringIndexValue; }
        else if (value.directString) { die->hasName = true; die->nameIsStringIndex = false; copyText(die->name, sizeof(die->name), value.stringValue); }
        break;
    case kAttributeType:
        if (value.number) { die->hasType = true; die->typeReference = value.unsignedValue; }
        break;
    case kAttributeLowPc:
        if (value.addressIndex) { die->hasLowPc = true; die->lowPcIsAddressIndex = true; die->lowPcIndex = value.addressIndexValue; }
        else if (value.number) { die->hasLowPc = true; die->lowPc = value.unsignedValue; }
        break;
    case kAttributeHighPc:
        if (value.addressIndex) { die->hasHighPc = true; die->highPcOffset = value.addressIndexValue; die->highPcIsAddress = true; die->hasRanges = true; }
        else if (value.number) { die->hasHighPc = true; die->highPcOffset = value.unsignedValue; }
        break;
    case kAttributeRanges:
        if (value.number) { die->hasRanges = true; die->rangesOffset = value.unsignedValue; }
        break;
    case kAttributeFrameBase:
        if (value.block) {
            die->hasFrameBase = true; die->frameBaseLength = value.blockLength;
            for (uint32_t i = 0; i < value.blockLength; ++i) die->frameBase[i] = value.blockBytes[i];
        }
        break;
    case kAttributeLocation:
        if (value.block) {
            die->hasLocation = true; die->locationIsList = false; die->locationLength = value.blockLength;
            for (uint32_t i = 0; i < value.blockLength; ++i) die->location[i] = value.blockBytes[i];
        } else if (value.list || form == kFormSecOffset) {
            die->hasLocation = true; die->locationIsList = true;
        }
        break;
    case kAttributeByteSize:
        if (value.number) { die->hasByteSize = true; die->byteSize = value.unsignedValue; }
        break;
    case kAttributeEncoding:
        if (value.number) { die->hasEncoding = true; die->encoding = value.unsignedValue; }
        break;
    case kAttributeDeclFile:
        if (value.number) { die->hasDeclFile = true; die->declFile = static_cast<uint32_t>(value.unsignedValue); }
        break;
    case kAttributeDeclLine:
        if (value.number) { die->hasDeclLine = true; die->declLine = static_cast<uint32_t>(value.unsignedValue); }
        break;
    case kAttributeConstValue:
        if (value.number) {
            die->hasConstValue = true; die->constValueSigned = value.signedNumber;
            die->constValue = value.signedNumber ? static_cast<uint64_t>(value.signedValue) : value.unsignedValue;
        }
        break;
    case kAttributeCount:
        if (value.number) { die->hasCount = true; die->count = value.unsignedValue; }
        break;
    case kAttributeLowerBound:
        if (value.number) { die->hasLowerBound = true; die->lowerBound = value.signedNumber ? value.signedValue : static_cast<int64_t>(value.unsignedValue); }
        break;
    case kAttributeUpperBound:
        if (value.number) { die->hasUpperBound = true; die->upperBound = value.signedNumber ? value.signedValue : static_cast<int64_t>(value.unsignedValue); }
        break;
    case kAttributeDataMemberLocation:
        if (value.block) {
            die->hasDataMemberLocation = true;
            die->dataMemberLocationIsExpression = true;
            die->dataMemberLocationLength = value.blockLength;
            for (uint32_t i = 0; i < value.blockLength; ++i) die->dataMemberLocationExpression[i] = value.blockBytes[i];
        } else if (value.number) {
            die->hasDataMemberLocation = true;
            die->dataMemberLocationIsExpression = false;
            die->dataMemberLocation = value.signedNumber ? value.signedValue : static_cast<int64_t>(value.unsignedValue);
        }
        break;
    case kAttributeAccessibility:
        if (value.number) { die->hasAccessibility = true; die->accessibility = static_cast<uint8_t>(value.unsignedValue); }
        break;
    case kAttributeDeclaration:
        if (value.number) die->declaration = value.unsignedValue != 0;
        break;
    case kAttributeBitSize:
        if (value.number) { die->hasBitSize = true; die->bitSize = value.unsignedValue; }
        break;
    case kAttributeBitOffset:
        if (value.number) { die->hasBitOffset = true; die->bitOffset = value.unsignedValue; }
        break;
    case kAttributeDataBitOffset:
        if (value.number) { die->hasDataBitOffset = true; die->dataBitOffset = value.unsignedValue; }
        break;
    case kAttributeStrOffsetsBase:
        if (value.number && value.unsignedValue <= UINT32_MAX) { die->hasStrOffsetsBase = true; die->strOffsetsBase = static_cast<uint32_t>(value.unsignedValue); }
        break;
    case kAttributeAddrBase:
        if (value.number && value.unsignedValue <= UINT32_MAX) { die->hasAddrBase = true; die->addrBase = static_cast<uint32_t>(value.unsignedValue); }
        break;
    case kAttributeArtificial:
        if (value.number) die->artificial = value.unsignedValue != 0;
        break;
    default:
        break;
    }
}

static bool resolveDieReferences(DebugDwarfMapper* mapper, const SectionView& stringOffsets,
                                 const SectionView& strings, const SectionView& addresses) {
    for (uint32_t unit = 0; unit < mapper->debugInfoCompilationUnitCount; ++unit) {
        const DebugDwarfCompilationUnitInfo& compilationUnit = mapper->compilationUnits[unit];
        const uint32_t first = compilationUnit.rootDieIndex;
        const uint32_t last = first + compilationUnit.dieCount;
        for (uint32_t i = first; i < last; ++i) {
            DebugDwarfDieInfo& die = mapper->dies[i];
            if (die.nameIsStringIndex && !resolveString(stringOffsets, strings, compilationUnit.strOffsetsBase,
                                                        die.nameStringIndex, die.name, sizeof(die.name))) { copyText(mapper->statusText, sizeof(mapper->statusText), "debug variable string reference failed"); return false; }
            if (die.lowPcIsAddressIndex && !resolveAddress(addresses, compilationUnit.addrBase,
                                                            die.lowPcIndex, compilationUnit.addressSize, &die.lowPc)) { copyText(mapper->statusText, sizeof(mapper->statusText), "debug variable address reference failed"); return false; }
            if (die.hasHighPc && !die.highPcIsAddress) {
                if (!die.hasLowPc || die.lowPc > UINT64_MAX - die.highPcOffset) { copyText(mapper->statusText, sizeof(mapper->statusText), "debug variable high_pc overflow"); return false; }
                die.highPcOffset = die.lowPc + die.highPcOffset;
            } else if (die.hasHighPc && die.highPcIsAddress && die.lowPcIsAddressIndex) {
                if (!resolveAddress(addresses, compilationUnit.addrBase, die.highPcOffset,
                                    compilationUnit.addressSize, &die.highPcOffset)) { copyText(mapper->statusText, sizeof(mapper->statusText), "debug variable high_pc address failed"); return false; }
            }
            if (die.hasLocation && !die.locationIsList && die.locationLength >= 2 && die.location[0] == kOpAddrx) {
                Cursor expression = { die.location, 1, die.locationLength };
                uint64_t index = 0;
                if (!readULEB(expression, &index) || !resolveAddress(addresses, compilationUnit.addrBase,
                                                                      index, compilationUnit.addressSize, &die.locationAddress)) return false;
                die.locationAddressIsIndex = true;
                die.locationAddressIndex = index;
            }
        }
    }
    for (uint32_t i = 0; i < mapper->debugInfoDieCount; ++i) {
        if (mapper->dies[i].hasType && findDie(mapper, mapper->dies[i].typeReference) < 0) { copyText(mapper->statusText, sizeof(mapper->statusText), "debug variable type reference failed"); return false; }
        if (mapper->dies[i].parentIndex != UINT32_MAX && mapper->dies[i].parentIndex >= mapper->debugInfoDieCount) { copyText(mapper->statusText, sizeof(mapper->statusText), "debug variable parent reference failed"); return false; }
    }
    return true;
}

static bool containsPc(const DebugDwarfDieInfo& die, uint64_t pc) {
    if (!die.hasLowPc || !die.hasHighPc || die.highPcOffset <= die.lowPc) return true;
    return pc >= die.lowPc && pc < die.highPcOffset;
}

static bool buildIndexes(DebugDwarfMapper* mapper) {
    for (uint32_t i = 0; i < mapper->debugInfoDieCount; ++i) {
        const DebugDwarfDieInfo& die = mapper->dies[i];
        if (die.tag != kTagSubprogram) continue;
        if (mapper->debugInfoFunctionCount >= kDebugDwarfMaxFunctions) return false;
        DebugDwarfFunctionInfo& function = mapper->debugFunctions[mapper->debugInfoFunctionCount++];
        function = DebugDwarfFunctionInfo();
        function.dieOffset = die.offset;
        function.dieIndex = i;
        function.lowPc = die.lowPc;
        function.highPc = die.highPcOffset;
        function.hasRange = die.hasLowPc && die.hasHighPc && !die.hasRanges && die.highPcOffset > die.lowPc;
        copyText(function.name, sizeof(function.name), die.name);
        function.frameBaseLength = die.frameBaseLength;
        for (uint32_t byte = 0; byte < die.frameBaseLength; ++byte) function.frameBase[byte] = die.frameBase[byte];
    }
    for (uint32_t i = 0; i < mapper->debugInfoDieCount; ++i) {
        const DebugDwarfDieInfo& die = mapper->dies[i];
        if (die.tag != kTagFormalParameter && die.tag != kTagVariable) continue;
        if (!die.name[0]) continue;
        uint32_t parent = die.parentIndex;
        uint32_t functionIndex = UINT32_MAX;
        while (parent != UINT32_MAX) {
            for (uint32_t function = 0; function < mapper->debugInfoFunctionCount; ++function)
                if (mapper->debugFunctions[function].dieIndex == parent) { functionIndex = function; break; }
            if (functionIndex != UINT32_MAX) break;
            parent = mapper->dies[parent].parentIndex;
        }
        // Compilation-unit globals and formal parameters of type DIEs are
        // intentionally outside this first frame-variable index.
        if (functionIndex == UINT32_MAX) continue;
        if (mapper->debugInfoVariableCount >= kDebugDwarfMaxVariables) return false;
        DebugDwarfVariableInfo& variable = mapper->debugVariables[mapper->debugInfoVariableCount++];
        variable = DebugDwarfVariableInfo();
        variable.dieOffset = die.offset;
        variable.dieIndex = i;
        variable.functionIndex = functionIndex;
        variable.kind = die.tag == kTagFormalParameter ? DebugDwarfVariableKind::Argument : DebugDwarfVariableKind::Local;
        variable.scopeDepth = die.depth;
        copyText(variable.name, sizeof(variable.name), die.name);
        variable.typeDieOffset = die.hasType ? die.typeReference : 0;
        variable.declFile = die.hasDeclFile ? die.declFile : 0;
        variable.declLine = die.hasDeclLine ? die.declLine : 0;
        variable.artificial = die.artificial;
    }
    return true;
}

static bool parseCompilationUnit(DebugDwarfMapper* mapper, const SectionView& info,
                                 const SectionView& abbrev, uint64_t compilationUnitOffset,
                                 uint64_t dieStart,
                                 uint64_t unitEnd, uint16_t version, uint8_t unitType,
                                 uint8_t addressSize, uint32_t abbrevOffset,
                                 uint32_t unitIndex) {
    AbbrevDeclaration declarations[kDebugDwarfMaxAbbreviations] = {};
    uint32_t declarationCount = 0;
    if (!parseAbbreviations(abbrev, abbrevOffset, declarations, &declarationCount)) return false;
    if (mapper->debugInfoCompilationUnitCount >= kDebugDwarfMaxCompilationUnits) return false;
    const uint32_t rootIndex = mapper->debugInfoDieCount;
    Cursor cursor = { info.data, dieStart, unitEnd };
    uint32_t parentStack[kDebugDwarfMaxDies > 64 ? 64 : kDebugDwarfMaxDies] = {};
    uint32_t depth = 0;
    bool sawRoot = false;
    while (cursor.position < cursor.end) {
        const uint64_t dieOffset = cursor.position;
        uint64_t codeValue = 0;
        if (!readULEB(cursor, &codeValue) || codeValue > UINT32_MAX) return false;
        if (codeValue == 0) {
            if (depth == 0) break;
            --depth;
            continue;
        }
        const int abbreviationIndex = findAbbreviation(declarations, declarationCount, static_cast<uint32_t>(codeValue));
        if (abbreviationIndex < 0 || mapper->debugInfoDieCount >= kDebugDwarfMaxDies || depth >= sizeof(parentStack) / sizeof(parentStack[0])) return false;
        const AbbrevDeclaration& declaration = declarations[abbreviationIndex];
        DebugDwarfDieInfo& die = mapper->dies[mapper->debugInfoDieCount];
        die = DebugDwarfDieInfo();
        die.offset = dieOffset;
        die.unitIndex = unitIndex;
        die.parentIndex = depth == 0 ? UINT32_MAX : parentStack[depth - 1];
        die.tag = declaration.tag;
        die.depth = static_cast<uint16_t>(depth);
        die.hasChildren = declaration.hasChildren;
        for (uint32_t attribute = 0; attribute < declaration.attributeCount; ++attribute) {
            const AbbrevAttribute& pair = declaration.attributes[attribute];
            FormValue value;
            if (!readForm(cursor, pair.form, pair.hasImplicitConst, pair.implicitConst,
                          addressSize, &value)) return false;
            applyAttribute(&die, pair.attribute, value, pair.form);
        }
        const uint32_t index = mapper->debugInfoDieCount++;
        if (depth == 0 && !sawRoot) {
            if (die.tag != kTagCompileUnit) return false;
            sawRoot = true;
        }
        if (declaration.hasChildren) parentStack[depth++] = index;
    }
    if (!sawRoot || depth != 0 || mapper->debugInfoDieCount == rootIndex) return false;
    DebugDwarfCompilationUnitInfo& compilationUnit = mapper->compilationUnits[mapper->debugInfoCompilationUnitCount++];
    compilationUnit = DebugDwarfCompilationUnitInfo();
    compilationUnit.sectionOffset = compilationUnitOffset;
    compilationUnit.unitEnd = unitEnd;
    compilationUnit.rootDieIndex = rootIndex;
    compilationUnit.dieCount = mapper->debugInfoDieCount - rootIndex;
    compilationUnit.abbrevOffset = abbrevOffset;
    compilationUnit.addressSize = addressSize;
    compilationUnit.version = version;
    compilationUnit.unitType = unitType;
    for (uint32_t i = rootIndex; i < mapper->debugInfoDieCount; ++i)
        if (mapper->dies[i].tag == kTagCompileUnit) {
            compilationUnit.strOffsetsBase = mapper->dies[i].hasStrOffsetsBase ? mapper->dies[i].strOffsetsBase : 0;
            compilationUnit.addrBase = mapper->dies[i].hasAddrBase ? mapper->dies[i].addrBase : 0;
            break;
        }
    // Clang emits DW_FORM_ref4 for these v5 units. The form is CU-relative,
    // while the bounded index stores stable section offsets.
    if (compilationUnitOffset != 0) {
        for (uint32_t i = rootIndex; i < mapper->debugInfoDieCount; ++i) {
            if (mapper->dies[i].hasType) {
                if (mapper->dies[i].typeReference > UINT64_MAX - compilationUnitOffset) return false;
                mapper->dies[i].typeReference += compilationUnitOffset;
            }
        }
    }
    return true;
}

static bool parseDebugInfo(DebugDwarfMapper* mapper, const SectionView& info,
                           const SectionView& abbrev, const SectionView& stringOffsets,
                           const SectionView& strings, const SectionView& addresses) {
    if (!mapper || !info.data || info.size == 0 || !abbrev.data || abbrev.size == 0) return false;
    Cursor cursor = { info.data, 0, info.size };
    while (cursor.position < cursor.end) {
        const uint64_t unitStart = cursor.position;
        uint32_t initialLength = 0;
        if (!readU32Cursor(cursor, &initialLength) || initialLength == 0xffffffffu || initialLength < 8 ||
            !checkedRange(cursor.position, initialLength, cursor.end)) return false;
        const uint64_t unitEnd = cursor.position + initialLength;
        uint16_t version = 0;
        if (!readU16Cursor(cursor, &version) || (version != kDwarfVersion4 && version != kDwarfVersion5)) return false;
        uint32_t abbrevOffset = 0;
        uint8_t unitType = 1;
        uint8_t addressSize = 0;
        if (version >= kDwarfVersion5) {
            if (!readByte(cursor, &unitType) || !readByte(cursor, &addressSize) || !readU32Cursor(cursor, &abbrevOffset)) return false;
        } else {
            if (!readU32Cursor(cursor, &abbrevOffset) || !readByte(cursor, &addressSize)) return false;
        }
        if (unitType != 1 || addressSize != 8 || cursor.position > unitEnd) return false;
        if (!parseCompilationUnit(mapper, info, abbrev, unitStart, cursor.position, unitEnd, version, unitType,
                                  addressSize, abbrevOffset, mapper->debugInfoCompilationUnitCount)) return false;
        cursor.position = unitEnd;
    }
    if (!resolveDieReferences(mapper, stringOffsets, strings, addresses)) return false;
    if (!buildIndexes(mapper)) { copyText(mapper->statusText, sizeof(mapper->statusText), "debug variable index limit or parent failed"); return false; }
    mapper->debugInfoReady = mapper->debugInfoFunctionCount != 0;
    return true;
}

struct TypeDescription {
    bool valid;
    bool scalar;
    bool signedInteger;
    bool unsignedInteger;
    bool boolean;
    bool pointer;
    bool floating;
    bool aggregate;
    bool array;
    uint32_t byteSize;
    uint64_t dieOffset;
    uint64_t elementTypeDieOffset;
    uint64_t elementCount;
    int64_t lowerBound;
    int64_t upperBound;
    bool hasElementCount;
    bool hasBounds;
    DebugDwarfTypeKind kind;
    char display[kDebugDwarfMaxTypeDisplayBytes];
};

static const DebugDwarfDieInfo* dieForOffset(const DebugDwarfMapper* mapper, uint64_t offset) {
    const int index = findDie(mapper, offset);
    return index >= 0 ? &mapper->dies[index] : nullptr;
}

static const DebugDwarfDieInfo* canonicalTypeDie(const DebugDwarfMapper* mapper,
                                                  uint64_t offset, uint64_t* seen,
                                                  uint32_t depth) {
    if (!mapper || !seen || depth >= kDebugDwarfMaxTypeDepth) return nullptr;
    for (uint32_t i = 0; i < depth; ++i) if (seen[i] == offset) return nullptr;
    const DebugDwarfDieInfo* die = dieForOffset(mapper, offset);
    if (!die) return nullptr;
    seen[depth] = offset;
    if ((die->tag == kTagTypedef || die->tag == kTagConstType || die->tag == kTagVolatileType) && die->hasType)
        return canonicalTypeDie(mapper, die->typeReference, seen, depth + 1);
    return die;
}

static bool arrayBounds(const DebugDwarfMapper* mapper, const DebugDwarfDieInfo* array,
                        uint64_t* elementType, uint64_t* count, int64_t* lower,
                        int64_t* upper, bool* haveBounds) {
    if (!mapper || !array || !count || !lower || !upper || !haveBounds) return false;
    const uint32_t arrayIndex = static_cast<uint32_t>(array - mapper->dies);
    for (uint32_t i = 0; i < mapper->debugInfoDieCount; ++i) {
        const DebugDwarfDieInfo& range = mapper->dies[i];
        if (range.parentIndex != arrayIndex || range.tag != kTagSubrangeType) continue;
        if (elementType && array->hasType) *elementType = array->typeReference;
        if (range.hasCount) {
            *count = range.count;
            *lower = range.hasLowerBound ? range.lowerBound : 0;
            if (*count == 0 || *count - 1 > static_cast<uint64_t>(INT64_MAX - *lower)) return false;
            *upper = *lower + static_cast<int64_t>(*count - 1);
            *haveBounds = true;
            return true;
        }
        if (range.hasUpperBound) {
            *lower = range.hasLowerBound ? range.lowerBound : 0;
            *upper = range.upperBound;
            if (*upper < *lower) return false;
            *count = static_cast<uint64_t>(*upper - *lower) + 1u;
            *haveBounds = true;
            return true;
        }
    }
    return false;
}

static bool formatType(const DebugDwarfMapper* mapper, uint64_t offset, char* output,
                       uint32_t outputSize, uint64_t* seen, uint32_t depth);

static bool typeDescription(const DebugDwarfMapper* mapper, uint64_t offset,
                            TypeDescription* description, uint64_t* seen, uint32_t depth) {
    if (!description || !mapper || depth >= kDebugDwarfMaxTypeDepth) return false;
    *description = TypeDescription();
    const DebugDwarfDieInfo* die = dieForOffset(mapper, offset);
    if (!die) return false;
    for (uint32_t i = 0; i < depth; ++i) if (seen[i] == offset) return false;
    seen[depth] = offset;
    description->valid = true;
    description->dieOffset = offset;
    if (die->tag == kTagBaseType) {
        description->byteSize = die->hasByteSize && die->byteSize <= UINT32_MAX ? static_cast<uint32_t>(die->byteSize) : 0;
        description->boolean = die->hasEncoding && die->encoding == 2;
        description->floating = die->hasEncoding && die->encoding == 4;
        description->signedInteger = !description->boolean && !description->floating && die->hasEncoding &&
            (die->encoding == 5 || die->encoding == 6);
        description->unsignedInteger = !description->boolean && !description->floating && !description->signedInteger;
        description->scalar = true;
        description->kind = DebugDwarfTypeKind::Scalar;
    } else if (die->tag == kTagPointerType) {
        description->pointer = true; description->scalar = true; description->kind = DebugDwarfTypeKind::Pointer;
        description->byteSize = die->hasByteSize && die->byteSize <= UINT32_MAX ? static_cast<uint32_t>(die->byteSize) : 8;
    } else if (die->tag == kTagArrayType) {
        description->array = true; description->aggregate = true; description->kind = DebugDwarfTypeKind::Array;
        description->byteSize = die->hasByteSize && die->byteSize <= UINT32_MAX ? static_cast<uint32_t>(die->byteSize) : 0;
        description->elementTypeDieOffset = die->hasType ? die->typeReference : 0;
        uint64_t count = 0; int64_t lower = 0; int64_t upper = -1; bool bounds = false;
        if (arrayBounds(mapper, die, nullptr, &count, &lower, &upper, &bounds)) {
            description->elementCount = count;
            description->lowerBound = lower;
            description->upperBound = upper;
            description->hasElementCount = true;
            description->hasBounds = bounds;
            if (description->byteSize == 0 && die->hasType) {
                uint64_t elementSeen[kDebugDwarfMaxTypeDepth] = {};
                TypeDescription element;
                if (typeDescription(mapper, die->typeReference, &element, elementSeen, depth + 1) &&
                    element.byteSize != 0 && count <= UINT32_MAX / element.byteSize)
                    description->byteSize = static_cast<uint32_t>(count * element.byteSize);
            }
        }
    } else if (die->tag == kTagStructureType || die->tag == kTagClassType || die->tag == kTagUnionType) {
        description->aggregate = true;
        description->kind = die->tag == kTagStructureType ? DebugDwarfTypeKind::Structure :
            die->tag == kTagClassType ? DebugDwarfTypeKind::Class : DebugDwarfTypeKind::Union;
        description->byteSize = die->hasByteSize && die->byteSize <= UINT32_MAX ? static_cast<uint32_t>(die->byteSize) : 0;
    } else if (die->tag == kTagEnumerationType) {
        description->scalar = true; description->signedInteger = true; description->byteSize = die->hasByteSize ? static_cast<uint32_t>(die->byteSize) : 4;
        description->kind = DebugDwarfTypeKind::Enumeration;
        if (die->hasType) {
            TypeDescription underlying;
            if (typeDescription(mapper, die->typeReference, &underlying, seen, depth + 1)) {
                description->signedInteger = underlying.signedInteger;
                description->unsignedInteger = underlying.unsignedInteger;
                description->byteSize = underlying.byteSize;
            }
        }
    } else if (die->tag == kTagTypedef || die->tag == kTagConstType || die->tag == kTagVolatileType) {
        if (!die->hasType || !typeDescription(mapper, die->typeReference, description, seen, depth + 1)) return false;
    } else {
        description->aggregate = true;
        description->kind = DebugDwarfTypeKind::Opaque;
    }
    if (!formatType(mapper, offset, description->display, sizeof(description->display), seen, depth)) return false;
    return true;
}

static bool appendText(char* output, uint32_t outputSize, const char* text) {
    if (!output || outputSize == 0 || !text) return false;
    const uint32_t current = textLength(output, outputSize);
    const uint32_t length = textLength(text, outputSize);
    if (current >= outputSize || length >= outputSize - current) return false;
    for (uint32_t i = 0; i < length; ++i) output[current + i] = text[i];
    output[current + length] = '\0';
    return true;
}

static bool formatType(const DebugDwarfMapper* mapper, uint64_t offset, char* output,
                       uint32_t outputSize, uint64_t* seen, uint32_t depth) {
    if (!output || outputSize == 0 || depth >= kDebugDwarfMaxTypeDepth) return false;
    output[0] = '\0';
    const DebugDwarfDieInfo* die = dieForOffset(mapper, offset);
    if (!die) return false;
    for (uint32_t i = 0; i < depth; ++i) if (seen[i] == offset) return false;
    seen[depth] = offset;
    if (die->tag == kTagTypedef) {
        if (die->name[0]) return copyText(output, outputSize, die->name);
        return die->hasType && formatType(mapper, die->typeReference, output, outputSize, seen, depth + 1);
    }
    if (die->tag == kTagConstType || die->tag == kTagVolatileType) {
        if (!appendText(output, outputSize, die->tag == kTagConstType ? "const " : "volatile ")) return false;
        char inner[kDebugDwarfMaxTypeDisplayBytes] = {};
        return die->hasType && formatType(mapper, die->typeReference, inner, sizeof(inner), seen, depth + 1) && appendText(output, outputSize, inner);
    }
    if (die->tag == kTagPointerType) {
        char inner[kDebugDwarfMaxTypeDisplayBytes] = {};
        if (die->hasType && !formatType(mapper, die->typeReference, inner, sizeof(inner), seen, depth + 1)) return false;
        if (!inner[0]) copyText(inner, sizeof(inner), "void");
        return appendText(output, outputSize, inner) && appendText(output, outputSize, "*");
    }
    if (die->tag == kTagArrayType) {
        char inner[kDebugDwarfMaxTypeDisplayBytes] = {};
        if (die->hasType && !formatType(mapper, die->typeReference, inner, sizeof(inner), seen, depth + 1)) return false;
        if (!inner[0]) copyText(inner, sizeof(inner), "<array>");
        if (!appendText(output, outputSize, inner) || !appendText(output, outputSize, "[")) return false;
        uint64_t count = 0; int64_t lower = 0; int64_t upper = -1; bool haveCount = false;
        haveCount = arrayBounds(mapper, die, nullptr, &count, &lower, &upper, &haveCount);
        char number[24] = {};
        if (haveCount) {
            uint32_t pos = 0; uint64_t value = count;
            char reverse[24] = {}; do { reverse[pos++] = static_cast<char>('0' + value % 10); value /= 10; } while (value && pos < sizeof(reverse));
            for (uint32_t i = 0; i < pos; ++i) number[i] = reverse[pos - i - 1];
            number[pos] = '\0';
        }
        return appendText(output, outputSize, haveCount ? number : "?") && appendText(output, outputSize, "]");
    }
    if (die->tag == kTagStructureType || die->tag == kTagClassType ||
        die->tag == kTagUnionType || die->tag == kTagEnumerationType) {
        if (die->name[0]) return copyText(output, outputSize, die->name);
        return copyText(output, outputSize, die->tag == kTagEnumerationType ? "enum" :
                        die->tag == kTagUnionType ? "<union>" : "<aggregate>");
    }
    if (die->tag == kTagBaseType) return copyText(output, outputSize, die->name[0] ? die->name : "<scalar>");
    if (die->name[0]) return copyText(output, outputSize, die->name);
    return copyText(output, outputSize, "<unsupported type>");
}

static bool readRegister(const DebugDwarfFrameContext& frame, uint16_t registerNumber, uint64_t* value) {
    if (!value) return false;
    if (registerNumber == 6 && frame.frameBaseKnown) { *value = frame.frameBase; return true; }
    if (!frame.registers.valid) return false;
    switch (registerNumber) {
    case 0: *value = frame.registers.rax; return true;
    case 1: *value = frame.registers.rdx; return true;
    case 2: *value = frame.registers.rcx; return true;
    case 3: *value = frame.registers.rbx; return true;
    case 4: *value = frame.registers.rsi; return true;
    case 5: *value = frame.registers.rdi; return true;
    case 6: *value = frame.registers.rbp; return true;
    case 7: *value = frame.registers.rsp; return true;
    case 8: *value = frame.registers.r8; return true;
    case 9: *value = frame.registers.r9; return true;
    case 10: *value = frame.registers.r10; return true;
    case 11: *value = frame.registers.r11; return true;
    case 12: *value = frame.registers.r12; return true;
    case 13: *value = frame.registers.r13; return true;
    case 14: *value = frame.registers.r14; return true;
    case 15: *value = frame.registers.r15; return true;
    case 16: *value = frame.registers.rip; return true;
    case 49: *value = frame.registers.rflags; return true;
    default: return false;
    }
}

struct EvaluatedLocation {
    DebugDwarfLocationKind kind;
    uint64_t value;
    uint16_t registerNumber;
    bool known;
    char status[kDebugDwarfMaxTypeDisplayBytes];
};

static void locationStatus(EvaluatedLocation* location, DebugDwarfLocationKind kind, const char* status) {
    if (!location) return;
    location->kind = kind;
    copyText(location->status, sizeof(location->status), status);
}

static bool evaluateLocation(const DebugDwarfDieInfo& die, const DebugDwarfFrameContext& frame,
                             EvaluatedLocation* location) {
    if (!location) return false;
    *location = EvaluatedLocation();
    if (die.locationIsList || !die.hasLocation || die.locationLength == 0) {
        locationStatus(location, DebugDwarfLocationKind::Unavailable, die.locationIsList ?
                       "unsupported location expression" : "<unavailable>");
        return false;
    }
    Cursor cursor = { die.location, 0, die.locationLength };
    uint64_t stack[kDebugDwarfMaxExpressionStack] = {};
    uint32_t stackCount = 0;
    bool stackValue = false;
    uint32_t operations = 0;
    while (cursor.position < cursor.end) {
        if (++operations > kDebugDwarfMaxExpressionOperations) { locationStatus(location, DebugDwarfLocationKind::Unsupported, "expression operation limit"); return false; }
        const uint8_t op = cursor.bytes[cursor.position++];
        if (op >= 0x30 && op <= 0x4f) {
            if (stackCount >= kDebugDwarfMaxExpressionStack) return false;
            stack[stackCount++] = op - 0x30; continue;
        }
        uint16_t registerNumber = 0;
        bool registerLocation = false;
        if (op >= kOpReg0 && op < kOpReg0 + 32) { registerNumber = op - kOpReg0; registerLocation = true; }
        else if (op == kOpRegx) {
            uint64_t number = 0; if (!readULEB(cursor, &number) || number > UINT16_MAX) { locationStatus(location, DebugDwarfLocationKind::Malformed, "malformed register expression"); return false; }
            registerNumber = static_cast<uint16_t>(number); registerLocation = true;
        }
        if (registerLocation) {
            uint64_t value = 0;
            if (!readRegister(frame, registerNumber, &value)) {
                location->registerNumber = registerNumber; location->known = false;
                locationStatus(location, DebugDwarfLocationKind::Register, "unavailable in caller frame"); return false;
            }
            location->registerNumber = registerNumber; location->value = value; location->known = true;
            locationStatus(location, DebugDwarfLocationKind::Register, ""); return true;
        }
        if (op >= kOpBreg0 && op < kOpBreg0 + 32) {
            registerNumber = op - kOpBreg0;
            int64_t offset = 0; uint64_t base = 0;
            if (!readSLEB(cursor, &offset) || !readRegister(frame, registerNumber, &base) ||
                !addSigned(base, offset, &location->value)) { locationStatus(location, DebugDwarfLocationKind::Unavailable, "unavailable in caller frame"); return false; }
            location->kind = DebugDwarfLocationKind::MemoryAddress; location->known = true; location->registerNumber = registerNumber;
            return true;
        }
        if (op == kOpFbreg) {
            int64_t offset = 0;
            if (!readSLEB(cursor, &offset) || !frame.frameBaseKnown || !addSigned(frame.frameBase, offset, &location->value)) {
                locationStatus(location, DebugDwarfLocationKind::Unavailable, "frame base unavailable"); return false;
            }
            location->kind = DebugDwarfLocationKind::MemoryAddress; location->known = true; return true;
        }
        if (op == kOpBregx) {
            uint64_t number = 0; int64_t offset = 0; uint64_t base = 0;
            if (!readULEB(cursor, &number) || number > UINT16_MAX || !readSLEB(cursor, &offset) ||
                !readRegister(frame, static_cast<uint16_t>(number), &base) || !addSigned(base, offset, &location->value)) {
                locationStatus(location, DebugDwarfLocationKind::Unavailable, "unavailable in caller frame"); return false;
            }
            location->kind = DebugDwarfLocationKind::MemoryAddress; location->known = true; location->registerNumber = static_cast<uint16_t>(number); return true;
        }
        if (op == kOpAddr) {
            if (!readU64Cursor(cursor, &location->value)) { locationStatus(location, DebugDwarfLocationKind::Malformed, "truncated address expression"); return false; }
            location->kind = DebugDwarfLocationKind::MemoryAddress; location->known = true; return true;
        }
        if (op == kOpAddrx) {
            locationStatus(location, DebugDwarfLocationKind::Unsupported, "unresolved address expression"); return false;
        }
        if (op == kOpPlusUconst) {
            uint64_t amount = 0;
            if (!readULEB(cursor, &amount) || stackCount == 0 || stack[stackCount - 1] > UINT64_MAX - amount) { locationStatus(location, DebugDwarfLocationKind::Malformed, "location arithmetic overflow"); return false; }
            stack[stackCount - 1] += amount; continue;
        }
        if (op == kOpStackValue) { stackValue = true; continue; }
        if (op == kOpDeref) { locationStatus(location, DebugDwarfLocationKind::Unsupported, "dereference location expression"); return false; }
        if (op == kOpConstu) {
            uint64_t value = 0; if (!readULEB(cursor, &value) || stackCount >= kDebugDwarfMaxExpressionStack) return false;
            stack[stackCount++] = value; continue;
        }
        if (op == kOpConsts) {
            int64_t value = 0; if (!readSLEB(cursor, &value) || stackCount >= kDebugDwarfMaxExpressionStack) return false;
            stack[stackCount++] = static_cast<uint64_t>(value); continue;
        }
        if (op >= kOpConst1u && op <= kOpConst8s) {
            const uint8_t width = static_cast<uint8_t>(1u << ((op - kOpConst1u) / 2u));
            uint64_t value = 0; if (!readSizedNumber(cursor, width, &value) || stackCount >= kDebugDwarfMaxExpressionStack) return false;
            if (((op - kOpConst1u) & 1u) != 0 && width < 8 && (value & (1ull << (width * 8u - 1u)))) value |= UINT64_MAX << (width * 8u);
            stack[stackCount++] = value; continue;
        }
        locationStatus(location, DebugDwarfLocationKind::Unsupported, "unsupported location expression");
        return false;
    }
    if (stackCount == 0) { locationStatus(location, DebugDwarfLocationKind::Malformed, "location stack underflow"); return false; }
    location->value = stack[stackCount - 1]; location->known = true;
    location->kind = stackValue ? DebugDwarfLocationKind::ImmediateValue : DebugDwarfLocationKind::MemoryAddress;
    return true;
}

static uint64_t readLittle(const uint8_t* bytes, uint32_t count) {
    uint64_t value = 0;
    const uint32_t width = count > 8 ? 8 : count;
    for (uint32_t i = 0; i < width; ++i) value |= static_cast<uint64_t>(bytes[i]) << (i * 8);
    return value;
}

static void appendUnsigned(char* output, uint32_t outputSize, uint64_t value) {
    if (!output || outputSize == 0) return;
    char reverse[32] = {}; uint32_t count = 0;
    do { reverse[count++] = static_cast<char>('0' + value % 10); value /= 10; } while (value && count < sizeof(reverse));
    for (uint32_t i = 0; i < count && i + 1 < outputSize; ++i) output[i] = reverse[count - i - 1];
    output[count < outputSize ? count : outputSize - 1] = '\0';
}

static void appendHex(char* output, uint32_t outputSize, uint64_t value) {
    if (!output || outputSize < 3) return;
    static const char digits[] = "0123456789abcdef";
    output[0] = '0'; output[1] = 'x';
    for (uint32_t i = 0; i < 16 && i + 3 < outputSize; ++i) output[2 + i] = digits[(value >> (60 - i * 4)) & 0xfu];
    output[18 < outputSize ? 18 : outputSize - 1] = '\0';
}

static void fillValue(DebugDwarfVariable* variable, const TypeDescription& type,
                       const EvaluatedLocation& location, const DebugDwarfDieInfo& die,
                       const DebugDwarfFrameContext& frame, DebugDwarfReadMemoryFn readMemory,
                       void* userData) {
    if (!variable) return;
    variable->locationKind = location.kind;
    variable->address = location.kind == DebugDwarfLocationKind::MemoryAddress ? location.value : 0;
    variable->registerNumber = location.registerNumber;
    if (!type.valid) { variable->state = DebugDwarfVariableState::Unavailable; copyText(variable->valueDisplay, sizeof(variable->valueDisplay), "<unavailable>"); return; }
    variable->valueKind = type.boolean ? DebugDwarfValueKind::Boolean : type.pointer ? DebugDwarfValueKind::Pointer :
        type.signedInteger ? DebugDwarfValueKind::SignedInteger : type.unsignedInteger ? DebugDwarfValueKind::UnsignedInteger :
        type.floating ? DebugDwarfValueKind::FloatingPoint : type.array ? DebugDwarfValueKind::Array : DebugDwarfValueKind::Aggregate;
    if (type.aggregate || type.array) {
        if (location.kind == DebugDwarfLocationKind::MemoryAddress && location.known && location.value != 0) {
            variable->state = DebugDwarfVariableState::Available;
            copyText(variable->valueDisplay, sizeof(variable->valueDisplay), type.array ? "<array>" : "<aggregate>");
        } else {
            variable->state = DebugDwarfVariableState::UnsupportedLocation;
            copyText(variable->valueDisplay, sizeof(variable->valueDisplay), "<aggregate location unsupported>");
            copyText(variable->status, sizeof(variable->status), "aggregate is not memory-backed");
        }
        return;
    }
    if (type.byteSize == 0 || type.byteSize > kDebugDwarfMaxVariableValueBytes) {
        variable->state = DebugDwarfVariableState::Unavailable; copyText(variable->valueDisplay, sizeof(variable->valueDisplay), "<unavailable>"); return;
    }
    uint8_t raw[kDebugDwarfMaxVariableValueBytes] = {};
    if (location.kind == DebugDwarfLocationKind::Register || location.kind == DebugDwarfLocationKind::ImmediateValue) {
        if (!location.known) { variable->state = DebugDwarfVariableState::Unavailable; copyText(variable->valueDisplay, sizeof(variable->valueDisplay), location.status); return; }
        for (uint32_t i = 0; i < type.byteSize; ++i) raw[i] = static_cast<uint8_t>(location.value >> (i * 8));
    } else if (location.kind == DebugDwarfLocationKind::MemoryAddress) {
        uint32_t returned = 0;
        if (!readMemory || !readMemory(userData, frame.sessionGeneration, frame.processId, frame.nativeRuntimeId,
                                       frame.threadId, frame.stopGeneration, location.value, raw, type.byteSize, &returned) || returned != type.byteSize) {
            variable->state = DebugDwarfVariableState::ReadFailure; copyText(variable->valueDisplay, sizeof(variable->valueDisplay), "<unavailable>");
            copyText(variable->status, sizeof(variable->status), "target memory read failed"); return;
        }
    } else {
        variable->state = DebugDwarfVariableState::Unavailable; copyText(variable->valueDisplay, sizeof(variable->valueDisplay), location.status[0] ? location.status : "<unavailable>"); return;
    }
    variable->rawByteCount = type.byteSize;
    for (uint32_t i = 0; i < type.byteSize; ++i) variable->rawBytes[i] = raw[i];
    const uint64_t value = readLittle(raw, type.byteSize);
    variable->scalarValue = value;
    variable->state = DebugDwarfVariableState::Available;
    if (type.boolean) copyText(variable->valueDisplay, sizeof(variable->valueDisplay), value ? "true" : "false");
    else if (type.pointer) { if (value == 0) copyText(variable->valueDisplay, sizeof(variable->valueDisplay), "nullptr"); else appendHex(variable->valueDisplay, sizeof(variable->valueDisplay), value); }
    else if (type.signedInteger) {
        int64_t signedValue = static_cast<int64_t>(value);
        if (type.byteSize < 8 && (value & (1ull << (type.byteSize * 8u - 1u)))) signedValue = static_cast<int64_t>(value | (UINT64_MAX << (type.byteSize * 8u)));
        if (signedValue < 0) { variable->valueDisplay[0] = '-'; appendUnsigned(variable->valueDisplay + 1, sizeof(variable->valueDisplay) - 1, static_cast<uint64_t>(-(signedValue + 1)) + 1u); }
        else appendUnsigned(variable->valueDisplay, sizeof(variable->valueDisplay), static_cast<uint64_t>(signedValue));
    } else if (type.unsignedInteger) appendUnsigned(variable->valueDisplay, sizeof(variable->valueDisplay), value);
    else { variable->state = DebugDwarfVariableState::Unavailable; copyText(variable->valueDisplay, sizeof(variable->valueDisplay), "<unavailable>"); copyText(variable->status, sizeof(variable->status), "unsupported scalar type"); }
    (void)die;
}

static bool variableVisible(const DebugDwarfMapper* mapper, const DebugDwarfVariableInfo& variable, uint64_t pc) {
    if (!mapper || variable.dieIndex >= mapper->debugInfoDieCount || variable.functionIndex >= mapper->debugInfoFunctionCount) return false;
    const DebugDwarfDieInfo* die = &mapper->dies[variable.dieIndex];
    uint32_t parent = die->parentIndex;
    while (parent != UINT32_MAX) {
        const DebugDwarfDieInfo& ancestor = mapper->dies[parent];
        if ((ancestor.tag == kTagLexicalBlock || ancestor.tag == kTagSubprogram) &&
            ancestor.hasLowPc && ancestor.hasHighPc && !containsPc(ancestor, pc)) return false;
        parent = ancestor.parentIndex;
    }
    return true;
}

static bool selectFunction(const DebugDwarfMapper* mapper, uint64_t address, uint32_t* index) {
    if (!mapper || !index) return false;
    bool found = false; uint64_t bestSize = UINT64_MAX;
    for (uint32_t i = 0; i < mapper->debugInfoFunctionCount; ++i) {
        const DebugDwarfFunctionInfo& function = mapper->debugFunctions[i];
        if (!function.hasRange || address < function.lowPc || address >= function.highPc) continue;
        const uint64_t size = function.highPc - function.lowPc;
        if (!found || size < bestSize) { found = true; bestSize = size; *index = i; }
    }
    return found;
}

static bool canonicalAddress(uint64_t address) {
    const uint64_t upper = address >> 48;
    return address != 0 && (upper == 0 || upper == 0xffffu);
}

static bool memberLocationExpression(const DebugDwarfDieInfo& die, int64_t* offset) {
    if (!offset || !die.hasDataMemberLocation || !die.dataMemberLocationIsExpression ||
        die.dataMemberLocationLength == 0) return false;
    Cursor cursor = { die.dataMemberLocationExpression, 0, die.dataMemberLocationLength };
    int64_t stack[kDebugDwarfMaxExpressionStack] = {};
    uint32_t stackCount = 0;
    uint32_t operations = 0;
    while (cursor.position < cursor.end) {
        if (++operations > kDebugDwarfMaxExpressionOperations) return false;
        const uint8_t op = cursor.bytes[cursor.position++];
        if (op >= 0x30 && op <= 0x4f) {
            if (stackCount >= kDebugDwarfMaxExpressionStack) return false;
            stack[stackCount++] = static_cast<int64_t>(op - 0x30);
            continue;
        }
        if (op == kOpConstu) {
            uint64_t value = 0;
            if (!readULEB(cursor, &value) || value > INT64_MAX || stackCount >= kDebugDwarfMaxExpressionStack) return false;
            stack[stackCount++] = static_cast<int64_t>(value);
            continue;
        }
        if (op == kOpConsts) {
            int64_t value = 0;
            if (!readSLEB(cursor, &value) || stackCount >= kDebugDwarfMaxExpressionStack) return false;
            stack[stackCount++] = value;
            continue;
        }
        if (op >= kOpConst1u && op <= kOpConst8s) {
            const uint8_t width = static_cast<uint8_t>(1u << ((op - kOpConst1u) / 2u));
            uint64_t value = 0;
            if (!readSizedNumber(cursor, width, &value) || stackCount >= kDebugDwarfMaxExpressionStack) return false;
            if (((op - kOpConst1u) & 1u) != 0 && width < 8 && (value & (1ull << (width * 8u - 1u))))
                value |= UINT64_MAX << (width * 8u);
            if (value > static_cast<uint64_t>(INT64_MAX) && (value & (1ull << 63)) == 0) return false;
            stack[stackCount++] = static_cast<int64_t>(value);
            continue;
        }
        if (op == kOpPlusUconst) {
            uint64_t amount = 0;
            if (!readULEB(cursor, &amount) || stackCount == 0 || amount > static_cast<uint64_t>(INT64_MAX) ||
                stack[stackCount - 1] > INT64_MAX - static_cast<int64_t>(amount)) return false;
            stack[stackCount - 1] += static_cast<int64_t>(amount);
            continue;
        }
        if (op == kOpStackValue) continue;
        return false;
    }
    if (stackCount != 1) return false;
    *offset = stack[0];
    return true;
}

static bool memberInfoInternal(const DebugDwarfMapper* mapper, uint64_t parentTypeDieOffset,
                               uint32_t memberIndex, DebugDwarfMemberInfo* member) {
    if (!mapper || !member) return false;
    *member = DebugDwarfMemberInfo();
    uint64_t seen[kDebugDwarfMaxTypeDepth] = {};
    const DebugDwarfDieInfo* parent = canonicalTypeDie(mapper, parentTypeDieOffset, seen, 0);
    if (!parent || (parent->tag != kTagStructureType && parent->tag != kTagClassType &&
                    parent->tag != kTagUnionType)) return false;
    const uint32_t parentIndex = static_cast<uint32_t>(parent - mapper->dies);
    uint32_t current = 0;
    for (uint32_t i = 0; i < mapper->debugInfoDieCount; ++i) {
        const DebugDwarfDieInfo& die = mapper->dies[i];
        if (die.parentIndex != parentIndex || die.tag != kTagMember) continue;
        if (current++ != memberIndex) continue;
        member->parentTypeDieOffset = parent->offset;
        member->dieOffset = die.offset;
        member->typeDieOffset = die.hasType ? die.typeReference : 0;
        member->declarationFile = die.hasDeclFile ? die.declFile : 0;
        member->declarationLine = die.hasDeclLine ? die.declLine : 0;
        member->accessibility = die.hasAccessibility ? die.accessibility : 0;
        member->declaration = die.declaration;
        member->artificial = die.artificial;
        member->bitField = die.hasBitSize || die.hasBitOffset || die.hasDataBitOffset;
        copyText(member->name, sizeof(member->name), die.name[0] ? die.name : "<anonymous member>");
        if (die.hasDataMemberLocation) {
            member->byteOffsetIsExpression = die.dataMemberLocationIsExpression;
            if (die.dataMemberLocationIsExpression)
                member->hasByteOffset = memberLocationExpression(die, &member->byteOffset);
            else {
                member->hasByteOffset = true;
                member->byteOffset = die.dataMemberLocation;
            }
        }
        return true;
    }
    return false;
}

static bool appendNode(DebugDwarfVariableView* view, const DebugDwarfValueNode& source,
                       uint64_t* nodeId) {
    if (!view || !nodeId || view->nodeCount >= kDebugDwarfMaxValueNodes) return false;
    DebugDwarfValueNode& node = view->nodes[view->nodeCount];
    node = source;
    node.nodeId = static_cast<uint64_t>(view->nodeCount + 1u);
    *nodeId = node.nodeId;
    ++view->nodeCount;
    view->materializedNodeCount = view->nodeCount;
    return true;
}

static DebugDwarfValueNode* nodeForId(DebugDwarfVariableView* view, uint64_t nodeId) {
    if (!view || nodeId == 0 || nodeId > view->nodeCount) return nullptr;
    DebugDwarfValueNode& node = view->nodes[nodeId - 1u];
    return node.nodeId == nodeId ? &node : nullptr;
}

static const DebugDwarfValueNode* nodeForId(const DebugDwarfVariableView* view, uint64_t nodeId) {
    if (!view || nodeId == 0 || nodeId > view->nodeCount) return nullptr;
    const DebugDwarfValueNode& node = view->nodes[nodeId - 1u];
    return node.nodeId == nodeId ? &node : nullptr;
}

static bool linkChild(DebugDwarfVariableView* view, DebugDwarfValueNode* parent, uint64_t childId) {
    if (!view || !parent || parent->childCount >= kDebugDwarfMaxValueChildren) {
        if (parent) parent->truncated = true;
        return false;
    }
    parent->childNodeIds[parent->childCount++] = childId;
    return true;
}

static bool appendDiagnostic(DebugDwarfVariableView* view, DebugDwarfValueNode* parent,
                             const char* text, DebugDwarfVariableState state) {
    if (!view || !parent || parent->childCount >= kDebugDwarfMaxValueChildren) {
        if (parent) parent->truncated = true;
        return false;
    }
    DebugDwarfValueNode diagnostic = DebugDwarfValueNode();
    diagnostic.parentNodeId = parent->nodeId;
    diagnostic.typeDieOffset = 0;
    diagnostic.depth = parent->depth + 1u;
    diagnostic.kind = DebugDwarfValueNodeKind::Diagnostic;
    diagnostic.state = state;
    diagnostic.locationKind = DebugDwarfLocationKind::Unavailable;
    diagnostic.frameIndex = parent->frameIndex;
    diagnostic.sessionGeneration = parent->sessionGeneration;
    diagnostic.stopGeneration = parent->stopGeneration;
    diagnostic.artifactGeneration = parent->artifactGeneration;
    copyText(diagnostic.name, sizeof(diagnostic.name), text);
    copyText(diagnostic.valueDisplay, sizeof(diagnostic.valueDisplay), text);
    copyText(diagnostic.status, sizeof(diagnostic.status), text);
    uint64_t childId = 0;
    return appendNode(view, diagnostic, &childId) && linkChild(view, parent, childId);
}

static DebugDwarfValueNodeKind nodeKind(const TypeDescription& type) {
    if (!type.valid) return DebugDwarfValueNodeKind::Unavailable;
    if (type.pointer) return DebugDwarfValueNodeKind::Pointer;
    if (type.array) return DebugDwarfValueNodeKind::Array;
    if (type.kind == DebugDwarfTypeKind::Structure) return DebugDwarfValueNodeKind::Structure;
    if (type.kind == DebugDwarfTypeKind::Class) return DebugDwarfValueNodeKind::Class;
    if (type.kind == DebugDwarfTypeKind::Union) return DebugDwarfValueNodeKind::Union;
    if (type.scalar) return DebugDwarfValueNodeKind::Scalar;
    return DebugDwarfValueNodeKind::OpaqueAggregate;
}

static const DebugDwarfDieInfo* pointedTypeDie(const DebugDwarfMapper* mapper,
                                               uint64_t typeDieOffset) {
    uint64_t seen[kDebugDwarfMaxTypeDepth] = {};
    const DebugDwarfDieInfo* pointer = canonicalTypeDie(mapper, typeDieOffset, seen, 0);
    if (!pointer || pointer->tag != kTagPointerType || !pointer->hasType) return nullptr;
    uint64_t pointedSeen[kDebugDwarfMaxTypeDepth] = {};
    return canonicalTypeDie(mapper, pointer->typeReference, pointedSeen, 0);
}

static void populateNodeFromVariable(DebugDwarfValueNode* node, const DebugDwarfVariable& variable,
                                     const TypeDescription& type, const DebugDwarfFrameContext& frame,
                                     uint64_t artifactGeneration) {
    if (!node) return;
    *node = DebugDwarfValueNode();
    node->dieOffset = variable.dieOffset;
    node->typeDieOffset = variable.typeDieOffset;
    node->address = variable.address;
    node->scalarValue = variable.scalarValue;
    node->targetAddress = type.pointer ? variable.scalarValue : 0;
    node->parentNodeId = 0;
    node->frameIndex = frame.frameIndex;
    node->depth = 0;
    node->sessionGeneration = frame.sessionGeneration;
    node->stopGeneration = frame.stopGeneration;
    node->artifactGeneration = artifactGeneration;
    node->kind = nodeKind(type);
    node->state = variable.state;
    node->locationKind = variable.locationKind;
    copyText(node->name, sizeof(node->name), variable.name);
    copyText(node->typeDisplay, sizeof(node->typeDisplay), variable.typeDisplay);
    copyText(node->valueDisplay, sizeof(node->valueDisplay), variable.valueDisplay);
    copyText(node->status, sizeof(node->status), variable.status);
}

static bool addValueChild(DebugDwarfVariableView* view, const DebugDwarfMapper* mapper,
                          const DebugDwarfFrameContext& frame, DebugDwarfReadMemoryFn readMemory,
                          void* userData, DebugDwarfValueNode* parent, const char* name,
                          uint64_t typeDieOffset, uint64_t address, uint64_t dieOffset) {
    if (!view || !mapper || !parent || !name || typeDieOffset == 0) return false;
    if (parent->childCount >= kDebugDwarfMaxValueChildren) { parent->truncated = true; return false; }
    uint64_t seen[kDebugDwarfMaxTypeDepth] = {};
    TypeDescription type;
    if (!typeDescription(mapper, typeDieOffset, &type, seen, 0))
        return appendDiagnostic(view, parent, "<missing member type>", DebugDwarfVariableState::MalformedDebugInfo);
    DebugDwarfValueNode child = DebugDwarfValueNode();
    child.dieOffset = dieOffset;
    child.typeDieOffset = typeDieOffset;
    child.parentNodeId = parent->nodeId;
    child.address = address;
    child.frameIndex = frame.frameIndex;
    child.depth = parent->depth + 1u;
    child.sessionGeneration = frame.sessionGeneration;
    child.stopGeneration = frame.stopGeneration;
    child.artifactGeneration = view->artifactGeneration;
    child.kind = nodeKind(type);
    child.locationKind = DebugDwarfLocationKind::MemoryAddress;
    copyText(child.name, sizeof(child.name), name);
    copyText(child.typeDisplay, sizeof(child.typeDisplay), type.display);
    EvaluatedLocation location = EvaluatedLocation();
    location.kind = DebugDwarfLocationKind::MemoryAddress;
    location.value = address;
    location.known = canonicalAddress(address);
    DebugDwarfVariable temporary = DebugDwarfVariable();
    temporary.dieOffset = dieOffset;
    temporary.typeDieOffset = typeDieOffset;
    copyText(temporary.name, sizeof(temporary.name), name);
    if (location.known) {
        DebugDwarfDieInfo memberDie = DebugDwarfDieInfo();
        fillValue(&temporary, type, location, memberDie, frame, readMemory, userData);
    } else {
        temporary.state = DebugDwarfVariableState::ReadFailure;
        temporary.locationKind = DebugDwarfLocationKind::MemoryAddress;
        copyText(temporary.valueDisplay, sizeof(temporary.valueDisplay), "<unreadable>");
        copyText(temporary.status, sizeof(temporary.status), "target address is not canonical");
    }
    child.state = temporary.state;
    child.locationKind = temporary.locationKind;
    child.address = temporary.address;
    child.scalarValue = temporary.scalarValue;
    copyText(child.valueDisplay, sizeof(child.valueDisplay), temporary.valueDisplay);
    copyText(child.status, sizeof(child.status), temporary.status);
    if (child.kind == DebugDwarfValueNodeKind::Pointer)
        child.targetAddress = child.scalarValue;
    child.expandable = child.state == DebugDwarfVariableState::Available &&
        ((child.kind == DebugDwarfValueNodeKind::Pointer && child.targetAddress != 0 &&
          pointedTypeDie(mapper, typeDieOffset) != nullptr) ||
         (child.kind != DebugDwarfValueNodeKind::Scalar && child.kind != DebugDwarfValueNodeKind::Unavailable &&
          child.locationKind == DebugDwarfLocationKind::MemoryAddress));
    uint64_t childId = 0;
    if (!appendNode(view, child, &childId)) { parent->truncated = true; return false; }
    return linkChild(view, parent, childId);
}

static bool activePathContains(const DebugDwarfVariableView* view, const DebugDwarfMapper* mapper,
                               const DebugDwarfValueNode& node, uint64_t targetAddress,
                               uint64_t targetTypeDieOffset) {
    uint64_t currentId = node.nodeId;
    while (currentId != 0) {
        const DebugDwarfValueNode* current = nodeForId(view, currentId);
        if (!current) return false;
        uint64_t seen[kDebugDwarfMaxTypeDepth] = {};
        const DebugDwarfDieInfo* type = canonicalTypeDie(mapper, current->typeDieOffset, seen, 0);
        const uint64_t currentType = type ? type->offset : 0;
        const uint64_t currentAddress = current->kind == DebugDwarfValueNodeKind::Pointer ?
            current->targetAddress : current->address;
        if (currentType == targetTypeDieOffset && currentAddress == targetAddress) return true;
        currentId = current->parentNodeId;
    }
    return false;
}

static bool probeTarget(const DebugDwarfFrameContext& frame, DebugDwarfReadMemoryFn readMemory,
                        void* userData, uint64_t address, uint32_t* readCount) {
    if (!canonicalAddress(address) || !readMemory) return false;
    uint8_t byte = 0;
    uint32_t returned = 0;
    const bool ok = readMemory(userData, frame.sessionGeneration, frame.processId, frame.nativeRuntimeId,
                               frame.threadId, frame.stopGeneration, address, &byte, 1, &returned) && returned == 1;
    if (ok && readCount) ++*readCount;
    return ok;
}

static bool addMemberChildren(DebugDwarfVariableView* view, const DebugDwarfMapper* mapper,
                              const DebugDwarfFrameContext& frame, DebugDwarfReadMemoryFn readMemory,
                              void* userData, DebugDwarfValueNode* parent,
                              const DebugDwarfDieInfo& typeDie, uint64_t baseAddress) {
    (void)baseAddress;
    const uint32_t parentIndex = static_cast<uint32_t>(&typeDie - mapper->dies);
    uint32_t memberIndex = 0;
    for (uint32_t i = 0; i < mapper->debugInfoDieCount; ++i) {
        const DebugDwarfDieInfo& candidate = mapper->dies[i];
        if (candidate.parentIndex != parentIndex || candidate.tag != kTagMember) continue;
        if (memberIndex >= kDebugDwarfMaxValueChildren) { parent->truncated = true; break; }
        DebugDwarfMemberInfo member = DebugDwarfMemberInfo();
        if (!memberInfoInternal(mapper, typeDie.offset, memberIndex++, &member) || !member.hasByteOffset) {
            DebugDwarfValueNode diagnostic = DebugDwarfValueNode();
            diagnostic.parentNodeId = parent->nodeId;
            diagnostic.depth = parent->depth + 1u;
            diagnostic.kind = DebugDwarfValueNodeKind::Diagnostic;
            diagnostic.state = DebugDwarfVariableState::UnsupportedLocation;
            diagnostic.locationKind = DebugDwarfLocationKind::Unsupported;
            diagnostic.frameIndex = frame.frameIndex;
            diagnostic.sessionGeneration = frame.sessionGeneration;
            diagnostic.stopGeneration = frame.stopGeneration;
            diagnostic.artifactGeneration = view->artifactGeneration;
            copyText(diagnostic.name, sizeof(diagnostic.name), candidate.name[0] ? candidate.name : "<anonymous member>");
            copyText(diagnostic.valueDisplay, sizeof(diagnostic.valueDisplay), "<unsupported layout>");
            copyText(diagnostic.status, sizeof(diagnostic.status), "<unsupported layout>");
            uint64_t diagnosticId = 0;
            if (!appendNode(view, diagnostic, &diagnosticId) || !linkChild(view, parent, diagnosticId)) return false;
            continue;
        }
        if (member.bitField) {
            appendDiagnostic(view, parent, "<bit-field unsupported>", DebugDwarfVariableState::UnsupportedLocation);
            continue;
        }
        uint64_t memberTypeSeen[kDebugDwarfMaxTypeDepth] = {};
        TypeDescription memberType;
        if (!typeDescription(mapper, member.typeDieOffset, &memberType, memberTypeSeen, 0)) {
            appendDiagnostic(view, parent, member.name, DebugDwarfVariableState::MalformedDebugInfo);
            continue;
        }
        const uint64_t aggregateBase = parent->kind == DebugDwarfValueNodeKind::Pointer ?
            parent->targetAddress : parent->address;
        if (member.byteOffset < 0) {
            appendDiagnostic(view, parent, "<unsupported layout>", DebugDwarfVariableState::UnsupportedLocation);
            continue;
        }
        const uint64_t memberOffset = static_cast<uint64_t>(member.byteOffset);
        if (typeDie.hasByteSize && (memberOffset > typeDie.byteSize ||
            (memberType.byteSize != 0 && memberType.byteSize > typeDie.byteSize - memberOffset))) {
            appendDiagnostic(view, parent, "<unsupported layout>", DebugDwarfVariableState::UnsupportedLocation);
            continue;
        }
        uint64_t memberAddress = 0;
        if (!addSigned(aggregateBase, member.byteOffset, &memberAddress) || !canonicalAddress(memberAddress)) {
            appendDiagnostic(view, parent, "<unsupported layout>", DebugDwarfVariableState::UnsupportedLocation);
            continue;
        }
        if (!addValueChild(view, mapper, frame, readMemory, userData, parent, member.name,
                           member.typeDieOffset, memberAddress, member.dieOffset)) return false;
    }
    return true;
}

static bool addArrayChildren(DebugDwarfVariableView* view, const DebugDwarfMapper* mapper,
                             const DebugDwarfFrameContext& frame, DebugDwarfReadMemoryFn readMemory,
                             void* userData, DebugDwarfValueNode* parent,
                             const DebugDwarfDieInfo& arrayDie, uint64_t baseAddress) {
    uint64_t elementTypeOffset = arrayDie.hasType ? arrayDie.typeReference : 0;
    uint64_t count = 0;
    int64_t lower = 0;
    int64_t upper = -1;
    bool haveBounds = false;
    if (!arrayBounds(mapper, &arrayDie, &elementTypeOffset, &count, &lower, &upper, &haveBounds) ||
        !haveBounds || elementTypeOffset == 0) {
        appendDiagnostic(view, parent, "<unsupported array bounds>", DebugDwarfVariableState::UnsupportedLocation);
        return true;
    }
    uint64_t elementSeen[kDebugDwarfMaxTypeDepth] = {};
    TypeDescription elementType;
    if (!typeDescription(mapper, elementTypeOffset, &elementType, elementSeen, 0) || elementType.byteSize == 0) {
        appendDiagnostic(view, parent, "<unsupported array element>", DebugDwarfVariableState::UnsupportedLocation);
        return true;
    }
    const uint64_t elementSize = elementType.byteSize;
    const uint64_t emitted = count < kDebugDwarfMaxArrayElements ? count : kDebugDwarfMaxArrayElements;
    for (uint64_t index = 0; index < emitted; ++index) {
        if (index > UINT64_MAX / elementSize) {
            appendDiagnostic(view, parent, "<array address overflow>", DebugDwarfVariableState::MalformedDebugInfo);
            return false;
        }
        const uint64_t byteOffset = index * elementSize;
        uint64_t elementAddress = 0;
        if (!addSigned(baseAddress, byteOffset > static_cast<uint64_t>(INT64_MAX) ? INT64_MAX :
                       static_cast<int64_t>(byteOffset), &elementAddress)) {
            appendDiagnostic(view, parent, "<array address overflow>", DebugDwarfVariableState::MalformedDebugInfo);
            return false;
        }
        char name[kDebugDwarfMaxVariableNameBytes] = {};
        const int64_t logicalIndex = lower + static_cast<int64_t>(index);
        name[0] = '[';
        char number[32] = {};
        if (logicalIndex < 0) {
            name[1] = '-';
            appendUnsigned(number, sizeof(number), static_cast<uint64_t>(-(logicalIndex + 1)) + 1u);
            copyText(name + 2, sizeof(name) - 2, number);
        } else {
            appendUnsigned(number, sizeof(number), static_cast<uint64_t>(logicalIndex));
            copyText(name + 1, sizeof(name) - 1, number);
        }
        const uint32_t length = textLength(name, sizeof(name));
        if (length + 1 < sizeof(name)) name[length] = ']';
        if (length + 2 < sizeof(name)) name[length + 1] = '\0';
        if (!addValueChild(view, mapper, frame, readMemory, userData, parent, name,
                           elementTypeOffset, elementAddress, 0)) return false;
    }
    if (count > emitted) {
        char truncated[kDebugDwarfMaxTypeDisplayBytes] = {};
        copyText(truncated, sizeof(truncated), "<");
        appendUnsigned(truncated, sizeof(truncated), count - emitted);
        appendText(truncated, sizeof(truncated), " more elements>");
        appendDiagnostic(view, parent, truncated, DebugDwarfVariableState::Unavailable);
        parent->truncated = true;
    }
    (void)upper;
    return true;
}

static bool addPointedScalarChild(DebugDwarfVariableView* view, const DebugDwarfMapper* mapper,
                                  const DebugDwarfFrameContext& frame, DebugDwarfReadMemoryFn readMemory,
                                  void* userData, DebugDwarfValueNode* parent,
                                  uint64_t typeDieOffset, uint64_t targetAddress) {
    return addValueChild(view, mapper, frame, readMemory, userData, parent, "*",
                         typeDieOffset, targetAddress, 0);
}

static bool expandValueNodeInternal(const DebugDwarfMapper* mapper,
                                    const DebugDwarfFrameContext& frame,
                                    DebugDwarfReadMemoryFn readMemory, void* userData,
                                    DebugDwarfVariableView* view, DebugDwarfValueNode* node) {
    if (!mapper || !view || !node) return false;
    if (node->expanded) return true;
    node->expanded = true;
    if (node->depth >= kDebugDwarfMaxValueDepth - 1u) {
        appendDiagnostic(view, node, "<maximum expansion depth>", DebugDwarfVariableState::UnsupportedLocation);
        node->expandable = false;
        return true;
    }
    uint64_t seen[kDebugDwarfMaxTypeDepth] = {};
    const DebugDwarfDieInfo* die = canonicalTypeDie(mapper, node->typeDieOffset, seen, 0);
    if (!die) {
        copyText(node->valueDisplay, sizeof(node->valueDisplay), "<unsupported type>");
        node->state = DebugDwarfVariableState::MalformedDebugInfo;
        node->expandable = false;
        return true;
    }
    if (die->tag == kTagPointerType) {
        if (node->targetAddress == 0) {
            copyText(node->valueDisplay, sizeof(node->valueDisplay), "nullptr");
            node->expandable = false;
            return true;
        }
        const DebugDwarfDieInfo* targetType = pointedTypeDie(mapper, node->typeDieOffset);
        if (!targetType) {
            copyText(node->valueDisplay, sizeof(node->valueDisplay), "<unsupported pointee>");
            node->state = DebugDwarfVariableState::UnsupportedLocation;
            node->expandable = false;
            return true;
        }
        if (activePathContains(view, mapper, *node, node->targetAddress, targetType->offset)) {
            appendDiagnostic(view, node, "<cycle>", DebugDwarfVariableState::Unavailable);
            node->expandable = false;
            return true;
        }
        if (!probeTarget(frame, readMemory, userData, node->targetAddress, &view->targetMemoryReadCount)) {
            copyText(node->valueDisplay, sizeof(node->valueDisplay), "<unreadable>");
            copyText(node->status, sizeof(node->status), "pointer target read failed");
            node->state = DebugDwarfVariableState::ReadFailure;
            node->expandable = false;
            return true;
        }
        if (targetType->tag == kTagStructureType || targetType->tag == kTagClassType || targetType->tag == kTagUnionType)
            return addMemberChildren(view, mapper, frame, readMemory, userData, node, *targetType, node->targetAddress);
        if (targetType->tag == kTagArrayType)
            return addArrayChildren(view, mapper, frame, readMemory, userData, node, *targetType, node->targetAddress);
        return addPointedScalarChild(view, mapper, frame, readMemory, userData, node,
                                     targetType->offset, node->targetAddress);
    }
    if (!canonicalAddress(node->address)) {
        node->state = DebugDwarfVariableState::ReadFailure;
        copyText(node->valueDisplay, sizeof(node->valueDisplay), "<unreadable>");
        node->expandable = false;
        return true;
    }
    if (die->tag == kTagStructureType || die->tag == kTagClassType || die->tag == kTagUnionType)
        return addMemberChildren(view, mapper, frame, readMemory, userData, node, *die, node->address);
    if (die->tag == kTagArrayType)
        return addArrayChildren(view, mapper, frame, readMemory, userData, node, *die, node->address);
    node->expandable = false;
    return true;
}

} // namespace

const char* DebugDwarfVariableKindName(DebugDwarfVariableKind kind) {
    return kind == DebugDwarfVariableKind::Argument ? "Argument" : "Local";
}

const char* DebugDwarfVariableStateName(DebugDwarfVariableState state) {
    switch (state) {
    case DebugDwarfVariableState::Available: return "Available";
    case DebugDwarfVariableState::Unavailable: return "Unavailable";
    case DebugDwarfVariableState::UnsupportedLocation: return "UnsupportedLocation";
    case DebugDwarfVariableState::OutOfScope: return "OutOfScope";
    case DebugDwarfVariableState::Stale: return "Stale";
    case DebugDwarfVariableState::ReadFailure: return "ReadFailure";
    case DebugDwarfVariableState::MalformedDebugInfo: return "MalformedDebugInfo";
    }
    return "Unknown";
}

const char* DebugDwarfValueKindName(DebugDwarfValueKind kind) {
    switch (kind) {
    case DebugDwarfValueKind::SignedInteger: return "SignedInteger";
    case DebugDwarfValueKind::UnsignedInteger: return "UnsignedInteger";
    case DebugDwarfValueKind::Boolean: return "Boolean";
    case DebugDwarfValueKind::Pointer: return "Pointer";
    case DebugDwarfValueKind::FloatingPoint: return "FloatingPoint";
    case DebugDwarfValueKind::Address: return "Address";
    case DebugDwarfValueKind::Aggregate: return "Aggregate";
    case DebugDwarfValueKind::Array: return "Array";
    case DebugDwarfValueKind::Bytes: return "Bytes";
    case DebugDwarfValueKind::Unavailable: return "Unavailable";
    }
    return "Unknown";
}

const char* DebugDwarfLocationKindName(DebugDwarfLocationKind kind) {
    switch (kind) {
    case DebugDwarfLocationKind::Register: return "Register";
    case DebugDwarfLocationKind::MemoryAddress: return "MemoryAddress";
    case DebugDwarfLocationKind::ImmediateValue: return "ImmediateValue";
    case DebugDwarfLocationKind::Unsupported: return "Unsupported";
    case DebugDwarfLocationKind::Malformed: return "Malformed";
    case DebugDwarfLocationKind::Unavailable: return "Unavailable";
    }
    return "Unknown";
}

const char* DebugDwarfTypeKindName(DebugDwarfTypeKind kind) {
    switch (kind) {
    case DebugDwarfTypeKind::Scalar: return "Scalar";
    case DebugDwarfTypeKind::Pointer: return "Pointer";
    case DebugDwarfTypeKind::Structure: return "Structure";
    case DebugDwarfTypeKind::Class: return "Class";
    case DebugDwarfTypeKind::Union: return "Union";
    case DebugDwarfTypeKind::Array: return "Array";
    case DebugDwarfTypeKind::Enumeration: return "Enumeration";
    case DebugDwarfTypeKind::Opaque: return "Opaque";
    case DebugDwarfTypeKind::Unknown: break;
    }
    return "Unknown";
}

const char* DebugDwarfValueNodeKindName(DebugDwarfValueNodeKind kind) {
    switch (kind) {
    case DebugDwarfValueNodeKind::Scalar: return "Scalar";
    case DebugDwarfValueNodeKind::Pointer: return "Pointer";
    case DebugDwarfValueNodeKind::Structure: return "Structure";
    case DebugDwarfValueNodeKind::Class: return "Class";
    case DebugDwarfValueNodeKind::Union: return "Union";
    case DebugDwarfValueNodeKind::Array: return "Array";
    case DebugDwarfValueNodeKind::OpaqueAggregate: return "OpaqueAggregate";
    case DebugDwarfValueNodeKind::Diagnostic: return "Diagnostic";
    case DebugDwarfValueNodeKind::Unavailable: return "Unavailable";
    }
    return "Unknown";
}

bool DebugDwarfDescribeType(const DebugDwarfMapper* mapper, uint64_t typeDieOffset,
                            DebugDwarfTypeInfo* type) {
    if (!type) return false;
    *type = DebugDwarfTypeInfo();
    if (!mapper) return false;
    uint64_t seen[kDebugDwarfMaxTypeDepth] = {};
    const DebugDwarfDieInfo* die = canonicalTypeDie(mapper, typeDieOffset, seen, 0);
    uint64_t descriptionSeen[kDebugDwarfMaxTypeDepth] = {};
    TypeDescription description;
    if (!die || !typeDescription(mapper, typeDieOffset, &description, descriptionSeen, 0)) return false;
    type->dieOffset = die->offset;
    type->byteSize = description.byteSize;
    type->elementTypeDieOffset = description.elementTypeDieOffset;
    type->elementCount = description.elementCount;
    type->lowerBound = description.lowerBound;
    type->upperBound = description.upperBound;
    type->kind = description.kind;
    type->complete = description.valid &&
        (!(description.aggregate || description.array) || description.byteSize != 0);
    type->hasElementCount = description.hasElementCount;
    type->hasBounds = description.hasBounds;
    copyText(type->name, sizeof(type->name), description.display);
    copyText(type->display, sizeof(type->display), description.display);
    if (die->tag == kTagStructureType || die->tag == kTagClassType || die->tag == kTagUnionType) {
        const uint32_t parentIndex = static_cast<uint32_t>(die - mapper->dies);
        for (uint32_t i = 0; i < mapper->debugInfoDieCount; ++i)
            if (mapper->dies[i].parentIndex == parentIndex && mapper->dies[i].tag == kTagMember) ++type->memberCount;
        type->visibleMemberCount = type->memberCount;
    }
    return true;
}

bool DebugDwarfDescribeMember(const DebugDwarfMapper* mapper, uint64_t parentTypeDieOffset,
                              uint32_t memberIndex, DebugDwarfMemberInfo* member) {
    return memberInfoInternal(mapper, parentTypeDieOffset, memberIndex, member);
}

bool DebugDwarfParseVariables(DebugDwarfMapper* mapper, const unsigned char* elfBytes, uint64_t elfSize) {
    if (!mapper || !elfBytes || elfSize == 0) return false;
    SectionView info = {}, abbrev = {}, strings = {}, stringOffsets = {}, lineStrings = {}, addresses = {}, loclists = {};
    if (!collectSections(elfBytes, elfSize, &info, &abbrev, &strings, &stringOffsets, &lineStrings, &addresses, &loclists)) {
        mapper->error = DebugDwarfError::MalformedDwarf;
        return false;
    }
    mapper->debugInfoSectionBytes = info.size;
    mapper->debugAbbrevSectionBytes = abbrev.size;
    mapper->debugStringSectionBytes = strings.size;
    mapper->debugStringOffsetsSectionBytes = stringOffsets.size;
    mapper->debugLineStringSectionBytes = lineStrings.size;
    mapper->debugAddressSectionBytes = addresses.size;
    mapper->debugLocationListsSectionBytes = loclists.size;
    if (!info.data || info.size == 0) return true;
    if (!abbrev.data || !strings.data || !stringOffsets.data || !addresses.data) {
        mapper->error = DebugDwarfError::UnsupportedForm;
        return false;
    }
    if (!parseDebugInfo(mapper, info, abbrev, stringOffsets, strings, addresses)) {
        mapper->error = DebugDwarfError::MalformedDwarf;
        return false;
    }
    return true;
}

bool DebugDwarfMapperLookupDebugFunction(const DebugDwarfMapper* mapper, uint64_t address,
                                         uint32_t* functionIndex, DebugDwarfError* error) {
    if (error) *error = DebugDwarfError::None;
    if (functionIndex) *functionIndex = 0;
    if (!mapper || !functionIndex || !mapper->debugInfoReady) {
        if (error) *error = mapper && mapper->state == DebugDwarfMapperState::Stale ? DebugDwarfError::ArtifactChanged : DebugDwarfError::NoDebugInfo;
        return false;
    }
    if (!selectFunction(mapper, address, functionIndex)) { if (error) *error = DebugDwarfError::LineNotMapped; return false; }
    return true;
}

bool DebugDwarfInspectVariables(const DebugDwarfMapper* mapper,
                                const DebugDwarfFrameContext& frame,
                                DebugDwarfReadMemoryFn readMemory, void* userData,
                                DebugDwarfVariableView* view) {
    if (!view) return false;
    *view = DebugDwarfVariableView();
    view->frameIndex = frame.frameIndex;
    view->sessionGeneration = frame.sessionGeneration;
    view->stopGeneration = frame.stopGeneration;
    view->frameInstructionAddress = frame.instructionAddress;
    if (!mapper || mapper->state != DebugDwarfMapperState::Ready || !mapper->debugInfoReady) {
        copyText(view->status, sizeof(view->status), "Locals unavailable: no bounded DWARF variable index");
        view->stale = true;
        return false;
    }
    view->artifactGeneration = mapper->identity.mapperGeneration;
    copyText(view->artifactSha256, sizeof(view->artifactSha256), mapper->identity.sha256);
    uint32_t functionIndex = 0;
    if (!selectFunction(mapper, frame.instructionAddress, &functionIndex)) {
        copyText(view->status, sizeof(view->status), "Variables unavailable: PC is not in a DWARF subprogram");
        return false;
    }
    view->functionIndex = functionIndex;
    copyText(view->functionName, sizeof(view->functionName), mapper->debugFunctions[functionIndex].name);
    const DebugDwarfFunctionInfo& function = mapper->debugFunctions[functionIndex];
    if (function.frameBaseLength == 0 || function.frameBase[0] != kOpReg0 + 6 || !frame.frameBaseKnown) {
        copyText(view->status, sizeof(view->status), "Variables unavailable: unsupported frame base expression");
        return false;
    }
    for (uint32_t i = 0; i < mapper->debugInfoVariableCount; ++i) {
        const DebugDwarfVariableInfo& info = mapper->debugVariables[i];
        if (info.functionIndex != functionIndex || !variableVisible(mapper, info, frame.instructionAddress)) continue;
        DebugDwarfVariable variable = DebugDwarfVariable();
        variable.dieOffset = info.dieOffset;
        variable.kind = info.kind;
        variable.state = DebugDwarfVariableState::Unavailable;
        variable.typeDieOffset = info.typeDieOffset;
        variable.scopeDepth = info.scopeDepth;
        copyText(variable.name, sizeof(variable.name), info.name);
        uint64_t seen[kDebugDwarfMaxTypeDepth] = {};
        TypeDescription type;
        if (!typeDescription(mapper, info.typeDieOffset, &type, seen, 0)) {
            copyText(variable.typeDisplay, sizeof(variable.typeDisplay), "<unknown>");
            copyText(variable.valueDisplay, sizeof(variable.valueDisplay), "<unavailable>");
            copyText(variable.status, sizeof(variable.status), "missing or cyclic type");
        } else {
            copyText(variable.typeDisplay, sizeof(variable.typeDisplay), type.display);
            const DebugDwarfDieInfo& die = mapper->dies[info.dieIndex];
            EvaluatedLocation location;
            bool locationOk = false;
            if (die.hasConstValue && !die.hasLocation) {
                location.kind = DebugDwarfLocationKind::ImmediateValue;
                location.value = die.constValue;
                location.known = true;
                location.status[0] = '\0';
                locationOk = true;
            } else locationOk = evaluateLocation(die, frame, &location);
            if (locationOk) fillValue(&variable, type, location, die, frame, readMemory, userData);
            else {
                variable.locationKind = location.kind;
                variable.registerNumber = location.registerNumber;
                variable.state = location.kind == DebugDwarfLocationKind::Unsupported ? DebugDwarfVariableState::UnsupportedLocation : DebugDwarfVariableState::Unavailable;
                copyText(variable.valueDisplay, sizeof(variable.valueDisplay), location.status[0] ? location.status : "<unavailable>");
                copyText(variable.status, sizeof(variable.status), location.status[0] ? location.status : "<unavailable>");
            }
        }
        copyText(variable.locationDisplay, sizeof(variable.locationDisplay), DebugDwarfLocationKindName(variable.locationKind));
        int replacement = -1;
        for (uint32_t existing = 0; existing < view->variableCount; ++existing)
            if (equalLiteral(view->variables[existing].name, variable.name) && view->variables[existing].scopeDepth < variable.scopeDepth) replacement = static_cast<int>(existing);
        if (replacement >= 0) view->variables[replacement] = variable;
        else if (view->variableCount < kDebugDwarfMaxDisplayedVariables) view->variables[view->variableCount++] = variable;
    }
    for (uint32_t i = 0; i < view->variableCount; ++i) {
        if (view->variables[i].kind == DebugDwarfVariableKind::Argument) ++view->argumentCount;
        else ++view->localCount;
    }
    for (uint32_t i = 0; i < view->variableCount; ++i) {
        DebugDwarfVariable& variable = view->variables[i];
        uint64_t seen[kDebugDwarfMaxTypeDepth] = {};
        TypeDescription type;
        DebugDwarfValueNode node = DebugDwarfValueNode();
        if (typeDescription(mapper, variable.typeDieOffset, &type, seen, 0)) {
            populateNodeFromVariable(&node, variable, type, frame, view->artifactGeneration);
            if (node.kind == DebugDwarfValueNodeKind::Pointer)
                node.expandable = variable.state == DebugDwarfVariableState::Available && variable.scalarValue != 0 &&
                    pointedTypeDie(mapper, variable.typeDieOffset) != nullptr;
            else node.expandable = variable.state == DebugDwarfVariableState::Available &&
                node.locationKind == DebugDwarfLocationKind::MemoryAddress && canonicalAddress(node.address) &&
                (node.kind == DebugDwarfValueNodeKind::Structure || node.kind == DebugDwarfValueNodeKind::Class ||
                 node.kind == DebugDwarfValueNodeKind::Union || node.kind == DebugDwarfValueNodeKind::Array);
        } else {
            node = DebugDwarfValueNode();
            node.dieOffset = variable.dieOffset;
            node.typeDieOffset = variable.typeDieOffset;
            node.frameIndex = frame.frameIndex;
            node.sessionGeneration = frame.sessionGeneration;
            node.stopGeneration = frame.stopGeneration;
            node.artifactGeneration = view->artifactGeneration;
            node.state = DebugDwarfVariableState::MalformedDebugInfo;
            node.kind = DebugDwarfValueNodeKind::Unavailable;
            copyText(node.name, sizeof(node.name), variable.name);
            copyText(node.typeDisplay, sizeof(node.typeDisplay), variable.typeDisplay);
            copyText(node.valueDisplay, sizeof(node.valueDisplay), "<unavailable>");
            copyText(node.status, sizeof(node.status), "missing or cyclic type");
        }
        uint64_t nodeId = 0;
        if (appendNode(view, node, &nodeId)) variable.nodeId = nodeId;
        else {
            variable.nodeId = 0;
            copyText(variable.status, sizeof(variable.status), "value node limit");
        }
    }
    view->valid = true;
    copyText(view->status, sizeof(view->status), view->variableCount ? "Live stopped values" : "No variables in selected frame");
    return true;
}

bool DebugDwarfExpandValue(const DebugDwarfMapper* mapper,
                           const DebugDwarfFrameContext& frame,
                           DebugDwarfReadMemoryFn readMemory, void* userData,
                           DebugDwarfVariableView* view, uint64_t nodeId) {
    if (!mapper || !view || !view->valid || view->stale || mapper->state != DebugDwarfMapperState::Ready ||
        !mapper->debugInfoReady || view->artifactGeneration != mapper->identity.mapperGeneration ||
        !equalLiteral(view->artifactSha256, mapper->identity.sha256) ||
        view->sessionGeneration != frame.sessionGeneration || view->stopGeneration != frame.stopGeneration ||
        view->frameIndex != frame.frameIndex || view->frameInstructionAddress != frame.instructionAddress) return false;
    DebugDwarfValueNode* node = nodeForId(view, nodeId);
    if (!node || node->sessionGeneration != frame.sessionGeneration ||
        node->stopGeneration != frame.stopGeneration || node->frameIndex != frame.frameIndex ||
        node->artifactGeneration != mapper->identity.mapperGeneration) return false;
    return expandValueNodeInternal(mapper, frame, readMemory, userData, view, node);
}

} // namespace developer_studio
} // namespace guidexos
