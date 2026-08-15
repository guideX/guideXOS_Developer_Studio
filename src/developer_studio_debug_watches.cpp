#include "developer_studio_debug_watches.h"

namespace guidexos {
namespace developer_studio {
namespace {

static uint32_t textLength(const char* text, uint32_t limit) {
    if (!text) return 0;
    uint32_t length = 0;
    while (length < limit && text[length] != '\0') ++length;
    return length;
}

static void copyText(char* output, uint32_t outputSize, const char* input) {
    if (!output || outputSize == 0) return;
    uint32_t i = 0;
    if (input) while (i + 1 < outputSize && input[i] != '\0') {
        output[i] = input[i];
        ++i;
    }
    output[i] = '\0';
}

static bool equalText(const char* left, const char* right) {
    if (!left || !right) return false;
    uint32_t index = 0;
    while (left[index] != '\0' && right[index] != '\0') {
        if (left[index] != right[index]) return false;
        ++index;
    }
    return left[index] == right[index];
}

static void formatHexAddress(char* output, uint32_t outputSize, uint64_t address) {
    if (!output || outputSize == 0) return;
    const char digits[] = "0123456789abcdef";
    copyText(output, outputSize, "0x");
    uint32_t length = textLength(output, outputSize);
    for (int shift = 60; shift >= 0 && length + 1 < outputSize; shift -= 4)
        output[length++] = digits[(address >> shift) & 0xfu];
    output[length] = '\0';
}

static bool isIdentifierStart(char value) {
    return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') || value == '_';
}

static bool isIdentifierPart(char value) {
    return isIdentifierStart(value) || (value >= '0' && value <= '9');
}

static bool isHexDigit(char value, uint64_t* digit) {
    if (value >= '0' && value <= '9') { if (digit) *digit = static_cast<uint64_t>(value - '0'); return true; }
    if (value >= 'a' && value <= 'f') { if (digit) *digit = static_cast<uint64_t>(value - 'a' + 10); return true; }
    if (value >= 'A' && value <= 'F') { if (digit) *digit = static_cast<uint64_t>(value - 'A' + 10); return true; }
    return false;
}

struct Token {
    DebugExpressionTokenKind kind;
    uint32_t offset;
    uint32_t length;
    uint64_t integerValue;
};

class Parser {
public:
    Parser(const char* source, DebugExpressionAst* output)
        : source_(source), ast_(output), tokenCount_(0), cursor_(0), depth_(0), memberChain_(0),
          indexDepth_(0), unaryDepth_(0) {}

    bool run() {
        if (!source_ || !ast_) return false;
        *ast_ = DebugExpressionAst();
        const uint32_t length = textLength(source_, kDebugWatchMaxExpressionBytes + 1);
        ast_->sourceLength = length;
        if (length == 0) return fail(DebugExpressionParseState::ParseError, 0, "empty expression");
        if (length > kDebugWatchMaxExpressionBytes)
            return fail(DebugExpressionParseState::TooLong, kDebugWatchMaxExpressionBytes,
                        "expression is too long");
        if (!tokenize(length)) return false;
        ast_->tokenCount = tokenCount_;
        const uint16_t root = parseExpression();
        if (root == 0xffffu) return false;
        const Token& trailing = tokens_[cursor_];
        if (trailing.kind != DebugExpressionTokenKind::End) {
            if (trailing.kind == DebugExpressionTokenKind::LeftParen)
                return fail(DebugExpressionParseState::UnsupportedExpression, trailing.offset,
                            "function calls are not supported");
            return fail(DebugExpressionParseState::ParseError, trailing.offset,
                        "unexpected trailing token");
        }
        ast_->rootNode = root;
        ast_->valid = true;
        ast_->state = DebugExpressionParseState::Valid;
        ast_->diagnostic[0] = '\0';
        return true;
    }

private:
    bool fail(DebugExpressionParseState state, uint32_t offset, const char* message) {
        ast_->valid = false;
        ast_->state = state;
        ast_->errorOffset = static_cast<uint16_t>(offset > 0xffffu ? 0xffffu : offset);
        copyText(ast_->diagnostic, sizeof(ast_->diagnostic), message);
        return false;
    }

    bool pushToken(DebugExpressionTokenKind kind, uint32_t offset, uint32_t length,
                   uint64_t value = 0) {
        if (tokenCount_ + 1 >= kDebugWatchMaxTokens)
            return fail(DebugExpressionParseState::TooManyTokens, offset, "token limit exceeded");
        tokens_[tokenCount_++] = { kind, offset, length, value };
        return true;
    }

    bool tokenize(uint32_t length) {
        uint32_t offset = 0;
        while (offset < length) {
            const char value = source_[offset];
            if (value == ' ' || value == '\t' || value == '\r' || value == '\n') { ++offset; continue; }
            if (isIdentifierStart(value)) {
                const uint32_t start = offset++;
                while (offset < length && isIdentifierPart(source_[offset])) ++offset;
                if (offset - start >= kDebugWatchMaxIdentifierBytes)
                    return fail(DebugExpressionParseState::ParseError, start, "identifier is too long");
                if (!pushToken(DebugExpressionTokenKind::Identifier, start, offset - start)) return false;
                continue;
            }
            if (value >= '0' && value <= '9') {
                const uint32_t start = offset;
                bool hexadecimal = false;
                if (value == '0' && offset + 1 < length &&
                    (source_[offset + 1] == 'x' || source_[offset + 1] == 'X')) {
                    hexadecimal = true;
                    offset += 2;
                    if (offset >= length || !isHexDigit(source_[offset], nullptr))
                        return fail(DebugExpressionParseState::ParseError, start, "malformed integer literal");
                }
                uint64_t parsed = 0;
                uint32_t digitCount = 0;
                while (offset < length) {
                    uint64_t digit = 0;
                    const bool validDigit = hexadecimal ? isHexDigit(source_[offset], &digit) :
                        (source_[offset] >= '0' && source_[offset] <= '9' &&
                         (digit = static_cast<uint64_t>(source_[offset] - '0'), true));
                    if (!validDigit) break;
                    if (digitCount >= kDebugWatchMaxNumericLiteralBytes - 2u)
                        return fail(DebugExpressionParseState::ParseError, start, "integer literal is too long");
                    const uint64_t base = hexadecimal ? 16u : 10u;
                    if (parsed > (UINT64_MAX - digit) / base)
                        return fail(DebugExpressionParseState::ParseError, start, "integer literal overflows");
                    parsed = parsed * base + digit;
                    ++digitCount;
                    ++offset;
                }
                if (offset < length && isIdentifierStart(source_[offset]))
                    return fail(DebugExpressionParseState::ParseError, offset, "malformed integer literal");
                if (!pushToken(DebugExpressionTokenKind::IntegerLiteral, start, offset - start, parsed)) return false;
                continue;
            }
            if (value == '-' && offset + 1 < length && source_[offset + 1] == '>') {
                if (!pushToken(DebugExpressionTokenKind::Arrow, offset, 2)) return false;
                offset += 2;
                continue;
            }
            if (value == '=' && offset + 1 < length && source_[offset + 1] == '=') {
                if (!pushToken(DebugExpressionTokenKind::EqualEqual, offset, 2)) return false;
                offset += 2;
                continue;
            }
            if (value == '!' && offset + 1 < length && source_[offset + 1] == '=') {
                if (!pushToken(DebugExpressionTokenKind::NotEqual, offset, 2)) return false;
                offset += 2;
                continue;
            }
            if (value == '<' && offset + 1 < length && source_[offset + 1] == '=') {
                if (!pushToken(DebugExpressionTokenKind::LessEqual, offset, 2)) return false;
                offset += 2;
                continue;
            }
            if (value == '>' && offset + 1 < length && source_[offset + 1] == '=') {
                if (!pushToken(DebugExpressionTokenKind::GreaterEqual, offset, 2)) return false;
                offset += 2;
                continue;
            }
            DebugExpressionTokenKind kind;
            switch (value) {
            case '.': kind = DebugExpressionTokenKind::Dot; break;
            case '*': kind = DebugExpressionTokenKind::Star; break;
            case '&': kind = DebugExpressionTokenKind::Ampersand; break;
            case '[': kind = DebugExpressionTokenKind::LeftBracket; break;
            case ']': kind = DebugExpressionTokenKind::RightBracket; break;
            case '(': kind = DebugExpressionTokenKind::LeftParen; break;
            case ')': kind = DebugExpressionTokenKind::RightParen; break;
            case '<': kind = DebugExpressionTokenKind::Less; break;
            case '>': kind = DebugExpressionTokenKind::Greater; break;
            default:
                return fail(DebugExpressionParseState::UnsupportedExpression, offset,
                            "operator is not supported");
            }
            if (!pushToken(kind, offset, 1)) return false;
            ++offset;
        }
        if (!pushToken(DebugExpressionTokenKind::End, length, 0)) return false;
        return true;
    }

    const Token& current() const { return tokens_[cursor_]; }
    bool accept(DebugExpressionTokenKind kind) {
        if (current().kind != kind) return false;
        ++cursor_;
        return true;
    }

    uint16_t addNode(DebugExpressionNodeKind kind, const Token& token, uint16_t left = 0xffffu,
                     uint16_t right = 0xffffu, const char* identifier = nullptr,
                     uint64_t integerValue = 0, uint32_t endOffset = 0) {
        if (ast_->nodeCount >= kDebugWatchMaxAstNodes) {
            fail(DebugExpressionParseState::TooManyNodes, token.offset, "AST node limit exceeded");
            return 0xffffu;
        }
        DebugExpressionNode& node = ast_->nodes[ast_->nodeCount];
        node = DebugExpressionNode();
        node.kind = kind;
        node.left = left;
        node.right = right;
        node.sourceOffset = static_cast<uint16_t>(token.offset > 0xffffu ? 0xffffu : token.offset);
        const uint32_t end = endOffset ? endOffset : token.offset + token.length;
        node.sourceLength = static_cast<uint16_t>(end > token.offset + 0xffffu ? 0xffffu : end - token.offset);
        node.integerValue = integerValue;
        if (identifier) copyText(node.identifier, sizeof(node.identifier), identifier);
        return static_cast<uint16_t>(ast_->nodeCount++);
    }

    bool enterDepth(uint32_t offset) {
        if (++depth_ > kDebugWatchMaxParserDepth)
            return fail(DebugExpressionParseState::TooDeep, offset, "parser nesting limit exceeded");
        return true;
    }
    void leaveDepth() { if (depth_ > 0) --depth_; }

    static bool isComparisonToken(DebugExpressionTokenKind kind) {
        return kind == DebugExpressionTokenKind::EqualEqual ||
            kind == DebugExpressionTokenKind::NotEqual ||
            kind == DebugExpressionTokenKind::Less ||
            kind == DebugExpressionTokenKind::LessEqual ||
            kind == DebugExpressionTokenKind::Greater ||
            kind == DebugExpressionTokenKind::GreaterEqual;
    }

    static DebugExpressionComparisonKind comparisonKind(DebugExpressionTokenKind kind) {
        switch (kind) {
        case DebugExpressionTokenKind::EqualEqual: return DebugExpressionComparisonKind::Equal;
        case DebugExpressionTokenKind::NotEqual: return DebugExpressionComparisonKind::NotEqual;
        case DebugExpressionTokenKind::Less: return DebugExpressionComparisonKind::Less;
        case DebugExpressionTokenKind::LessEqual: return DebugExpressionComparisonKind::LessEqual;
        case DebugExpressionTokenKind::Greater: return DebugExpressionComparisonKind::Greater;
        case DebugExpressionTokenKind::GreaterEqual: return DebugExpressionComparisonKind::GreaterEqual;
        default: return DebugExpressionComparisonKind::Equal;
        }
    }

    uint16_t parseExpression() { return parseComparison(); }

    uint16_t parseComparison() {
        const uint16_t left = parsePostfix();
        if (left == 0xffffu || !isComparisonToken(current().kind)) return left;
        const Token operation = current();
        ++cursor_;
        const uint16_t right = parsePostfix();
        if (right == 0xffffu) return right;
        if (isComparisonToken(current().kind)) {
            fail(DebugExpressionParseState::UnsupportedExpression, current().offset,
                 "chained comparisons are not supported; parenthesize each comparison");
            return 0xffffu;
        }
        return addNode(DebugExpressionNodeKind::Comparison, operation, left, right, nullptr,
                       static_cast<uint64_t>(comparisonKind(operation.kind)));
    }

    uint16_t parsePostfix() {
        uint16_t value = parseUnary();
        if (value == 0xffffu) return value;
        while (current().kind == DebugExpressionTokenKind::Dot ||
               current().kind == DebugExpressionTokenKind::Arrow ||
               current().kind == DebugExpressionTokenKind::LeftBracket) {
            const Token operation = current();
            if (operation.kind == DebugExpressionTokenKind::LeftBracket) {
                if (++indexDepth_ > kDebugWatchMaxIndexDepth) {
                    fail(DebugExpressionParseState::TooDeep, operation.offset, "index nesting limit exceeded");
                    return 0xffffu;
                }
                ++cursor_;
                const uint16_t index = parseExpression();
                if (index == 0xffffu) { --indexDepth_; return index; }
                if (!accept(DebugExpressionTokenKind::RightBracket)) {
                    fail(DebugExpressionParseState::ParseError, current().offset, "missing closing bracket");
                    --indexDepth_;
                    return 0xffffu;
                }
                const Token start = tokens_[cursor_ - 1u];
                value = addNode(DebugExpressionNodeKind::ArrayIndex, operation, value, index,
                                nullptr, 0, current().offset);
                --indexDepth_;
                if (value == 0xffffu) return value;
                (void)start;
                continue;
            }
            if (++memberChain_ > kDebugWatchMaxMemberChain) {
                fail(DebugExpressionParseState::TooDeep, operation.offset, "member chain limit exceeded");
                return 0xffffu;
            }
            ++cursor_;
            if (current().kind != DebugExpressionTokenKind::Identifier) {
                fail(DebugExpressionParseState::ParseError, current().offset,
                     operation.kind == DebugExpressionTokenKind::Arrow ? "arrow needs a member" : "dot needs a member");
                return 0xffffu;
            }
            const Token nameToken = current();
            char name[kDebugWatchMaxIdentifierBytes] = {};
            copyText(name, sizeof(name), source_ + nameToken.offset);
            name[nameToken.length < sizeof(name) ? nameToken.length : sizeof(name) - 1u] = '\0';
            ++cursor_;
            value = addNode(operation.kind == DebugExpressionTokenKind::Arrow ?
                                DebugExpressionNodeKind::PointerMemberAccess : DebugExpressionNodeKind::MemberAccess,
                            operation, value, 0xffffu, name, 0, nameToken.offset + nameToken.length);
            if (value == 0xffffu) return value;
        }
        return value;
    }

    uint16_t parseUnary() {
        const Token token = current();
        if (token.kind == DebugExpressionTokenKind::Star || token.kind == DebugExpressionTokenKind::Ampersand) {
            if (++unaryDepth_ > kDebugWatchMaxUnaryDepth) {
                fail(DebugExpressionParseState::TooDeep, token.offset, "unary nesting limit exceeded");
                return 0xffffu;
            }
            ++cursor_;
            const uint16_t child = parseUnary();
            --unaryDepth_;
            if (child == 0xffffu) return child;
            return addNode(token.kind == DebugExpressionTokenKind::Star ?
                               DebugExpressionNodeKind::Dereference : DebugExpressionNodeKind::AddressOf,
                           token, child);
        }
        if (!enterDepth(token.offset)) return 0xffffu;
        uint16_t result = 0xffffu;
        if (accept(DebugExpressionTokenKind::Identifier)) {
            const Token identifier = tokens_[cursor_ - 1u];
            char name[kDebugWatchMaxIdentifierBytes] = {};
            for (uint32_t i = 0; i < identifier.length && i + 1 < sizeof(name); ++i)
                name[i] = source_[identifier.offset + i];
            result = addNode(DebugExpressionNodeKind::Identifier, identifier, 0xffffu, 0xffffu,
                             name);
        } else if (accept(DebugExpressionTokenKind::IntegerLiteral)) {
            const Token literal = tokens_[cursor_ - 1u];
            result = addNode(DebugExpressionNodeKind::IntegerLiteral, literal, 0xffffu, 0xffffu,
                             nullptr, literal.integerValue);
        } else if (accept(DebugExpressionTokenKind::LeftParen)) {
            result = parseExpression();
            if (result != 0xffffu && !accept(DebugExpressionTokenKind::RightParen)) {
                fail(DebugExpressionParseState::ParseError, current().offset, "missing closing parenthesis");
                result = 0xffffu;
            }
        } else {
            fail(DebugExpressionParseState::ParseError, current().offset, "expected identifier or integer");
        }
        leaveDepth();
        return result;
    }

    const char* source_;
    DebugExpressionAst* ast_;
    Token tokens_[kDebugWatchMaxTokens];
    uint32_t tokenCount_;
    uint32_t cursor_;
    uint32_t depth_;
    uint32_t memberChain_;
    uint32_t indexDepth_;
    uint32_t unaryDepth_;
};

static const DebugDwarfValueNode* nodeAt(const DebugDwarfVariableView* view, uint64_t nodeId) {
    if (!view || nodeId == 0 || nodeId > view->nodeCount) return nullptr;
    const DebugDwarfValueNode& node = view->nodes[nodeId - 1u];
    return node.nodeId == nodeId ? &node : nullptr;
}

static DebugDwarfValueNode* nodeAt(DebugDwarfVariableView* view, uint64_t nodeId) {
    if (!view || nodeId == 0 || nodeId > view->nodeCount) return nullptr;
    DebugDwarfValueNode& node = view->nodes[nodeId - 1u];
    return node.nodeId == nodeId ? &node : nullptr;
}

static bool canonicalAddress(uint64_t address) {
    const uint64_t upper = address >> 48;
    return address != 0 && (upper == 0 || upper == 0xffffu);
}

static bool findNodeByName(const DebugDwarfVariableView* view, uint64_t parentId,
                           const char* name, uint64_t* nodeId) {
    const DebugDwarfValueNode* parent = nodeAt(view, parentId);
    if (!parent || !name || !nodeId) return false;
    for (uint32_t i = 0; i < parent->childCount; ++i) {
        const DebugDwarfValueNode* child = nodeAt(view, parent->childNodeIds[i]);
        if (child && equalText(child->name, name)) { *nodeId = child->nodeId; return true; }
    }
    return false;
}

static const DebugDwarfVariable* findVariable(const DebugDwarfVariableView* view, const char* name) {
    if (!view || !name) return nullptr;
    for (uint32_t i = 0; i < view->variableCount; ++i)
        if (equalText(view->variables[i].name, name)) return &view->variables[i];
    return nullptr;
}

static const DebugDwarfDieInfo* dieAt(const DebugDwarfMapper* mapper, uint64_t offset) {
    if (!mapper || offset == 0) return nullptr;
    for (uint32_t i = 0; i < mapper->debugInfoDieCount; ++i)
        if (mapper->dies[i].offset == offset) return &mapper->dies[i];
    return nullptr;
}

static bool pointeeType(const DebugDwarfMapper* mapper, uint64_t pointerType,
                        uint64_t* typeOffset) {
    if (!mapper || !typeOffset) return false;
    uint64_t current = pointerType;
    for (uint32_t depth = 0; depth < kDebugDwarfMaxTypeDepth; ++depth) {
        const DebugDwarfDieInfo* die = dieAt(mapper, current);
        if (!die) return false;
        if (die->tag == 0x0fu) {
            if (!die->hasType) return false;
            *typeOffset = die->typeReference;
            return true;
        }
        if ((die->tag == 0x16u || die->tag == 0x26u || die->tag == 0x35u) && die->hasType) {
            current = die->typeReference;
            continue;
        }
        return false;
    }
    return false;
}

static bool isAggregate(DebugDwarfValueNodeKind kind) {
    return kind == DebugDwarfValueNodeKind::Structure || kind == DebugDwarfValueNodeKind::Class ||
        kind == DebugDwarfValueNodeKind::Union || kind == DebugDwarfValueNodeKind::Array ||
        kind == DebugDwarfValueNodeKind::OpaqueAggregate;
}

enum class EvalCategory {
    Invalid = 0,
    SignedInteger,
    UnsignedInteger,
    Boolean,
    Pointer,
    Aggregate,
    FloatingPoint
};

struct EvalTypeInfo {
    EvalCategory category;
    uint32_t bitWidth;
};

struct EvalValue {
    bool valid;
    bool fromNode;
    bool literal;
    bool logicalDereference;
    bool aggregate;
    bool pointer;
    bool scalar;
    uint64_t nodeId;
    uint64_t typeDieOffset;
    uint64_t address;
    uint64_t scalarValue;
    DebugDwarfValueKind valueKind;
    DebugDwarfLocationKind locationKind;
    char typeDisplay[kDebugDwarfMaxTypeDisplayBytes];
    char valueDisplay[kDebugDwarfMaxTypeDisplayBytes];
};

static bool classifyDwarfType(const DebugDwarfMapper* mapper, uint64_t typeOffset,
                              EvalTypeInfo* info) {
    if (!mapper || !info || typeOffset == 0) return false;
    *info = EvalTypeInfo();
    uint64_t current = typeOffset;
    for (uint32_t depth = 0; depth < kDebugDwarfMaxTypeDepth; ++depth) {
        const DebugDwarfDieInfo* die = dieAt(mapper, current);
        if (!die) return false;
        if ((die->tag == 0x16u || die->tag == 0x26u || die->tag == 0x35u) && die->hasType) {
            current = die->typeReference;
            continue;
        }
        if (die->tag == 0x0fu) {
            info->category = EvalCategory::Pointer;
            info->bitWidth = die->hasByteSize && die->byteSize <= 8u ?
                static_cast<uint32_t>(die->byteSize * 8u) : 64u;
            return info->bitWidth != 0;
        }
        if (die->tag == 0x24u) {
            if (!die->hasEncoding || !die->hasByteSize || die->byteSize == 0 || die->byteSize > 8u)
                return false;
            info->bitWidth = static_cast<uint32_t>(die->byteSize * 8u);
            if (die->encoding == 2u) info->category = EvalCategory::Boolean;
            else if (die->encoding == 4u) info->category = EvalCategory::FloatingPoint;
            else if (die->encoding == 5u || die->encoding == 6u)
                info->category = EvalCategory::SignedInteger;
            else info->category = EvalCategory::UnsignedInteger;
            return true;
        }
        if (die->tag == 0x04u) {
            if (die->hasType) {
                current = die->typeReference;
                continue;
            }
            info->category = EvalCategory::SignedInteger;
            info->bitWidth = die->hasByteSize && die->byteSize <= 8u ?
                static_cast<uint32_t>(die->byteSize * 8u) : 32u;
            return true;
        }
        info->category = EvalCategory::Aggregate;
        info->bitWidth = 0;
        return true;
    }
    return false;
}

static bool classifyEvalValue(const DebugDwarfMapper* mapper, const EvalValue& value,
                              EvalTypeInfo* info) {
    if (!info || !value.valid) return false;
    *info = EvalTypeInfo();
    if (value.pointer || value.valueKind == DebugDwarfValueKind::Pointer ||
        value.valueKind == DebugDwarfValueKind::Address) {
        info->category = EvalCategory::Pointer;
        info->bitWidth = 64;
        return true;
    }
    if (value.aggregate || !value.scalar) {
        info->category = EvalCategory::Aggregate;
        return true;
    }
    if (!value.fromNode) {
        switch (value.valueKind) {
        case DebugDwarfValueKind::Boolean: info->category = EvalCategory::Boolean; break;
        case DebugDwarfValueKind::SignedInteger: info->category = EvalCategory::SignedInteger; break;
        case DebugDwarfValueKind::UnsignedInteger: info->category = EvalCategory::UnsignedInteger; break;
        case DebugDwarfValueKind::FloatingPoint: info->category = EvalCategory::FloatingPoint; break;
        default: info->category = EvalCategory::Invalid; break;
        }
        info->bitWidth = value.valueKind == DebugDwarfValueKind::Boolean ? 1u : 64u;
        return info->category != EvalCategory::Invalid;
    }
    return classifyDwarfType(mapper, value.typeDieOffset, info);
}

static EvalValue evalValueFromNode(const DebugDwarfValueNode& node) {
    EvalValue value = EvalValue();
    value.valid = true;
    value.fromNode = true;
    value.nodeId = node.nodeId;
    value.typeDieOffset = node.typeDieOffset;
    value.address = node.address;
    value.scalarValue = node.scalarValue;
    value.pointer = node.kind == DebugDwarfValueNodeKind::Pointer;
    value.aggregate = isAggregate(node.kind);
    value.scalar = node.kind == DebugDwarfValueNodeKind::Scalar || value.pointer;
    value.valueKind = value.pointer ? DebugDwarfValueKind::Pointer :
        value.aggregate ? (node.kind == DebugDwarfValueNodeKind::Array ? DebugDwarfValueKind::Array : DebugDwarfValueKind::Aggregate) :
        DebugDwarfValueKind::SignedInteger;
    value.locationKind = node.locationKind;
    copyText(value.typeDisplay, sizeof(value.typeDisplay), node.typeDisplay);
    copyText(value.valueDisplay, sizeof(value.valueDisplay), node.valueDisplay);
    return value;
}

static bool expandNode(const DebugWatchEvaluationContext& context, DebugDwarfVariableView* view,
                       EvalValue* value) {
    if (!view || !value || !value->fromNode || value->nodeId == 0) return false;
    if (!DebugDwarfExpandValue(context.mapper, context.frame, context.readMemory, context.userData,
                               view, value->nodeId)) return false;
    const DebugDwarfValueNode* node = nodeAt(view, value->nodeId);
    if (!node) return false;
    return true;
}

static DebugWatchState nodeFailureState(const DebugDwarfValueNode& node) {
    if (node.valueDisplay[0] != '\0' && equalText(node.valueDisplay, "nullptr"))
        return DebugWatchState::NullPointer;
    if (node.state == DebugDwarfVariableState::MalformedDebugInfo)
        return DebugWatchState::MalformedDebugInfo;
    if (node.state == DebugDwarfVariableState::ReadFailure ||
        equalText(node.valueDisplay, "<unreadable>"))
        return DebugWatchState::UnreadableTarget;
    return DebugWatchState::Stale;
}

static bool evaluateNode(uint16_t nodeIndex, const DebugExpressionAst& ast,
                         const DebugWatchEvaluationContext& context, DebugDwarfVariableView* view,
                         EvalValue* value, DebugWatchResult* result);

static bool evaluateMember(const DebugExpressionNode& expressionNode,
                           const DebugExpressionAst& ast, const DebugWatchEvaluationContext& context,
                           DebugDwarfVariableView* view, EvalValue* value, DebugWatchResult* result) {
    if (!evaluateNode(expressionNode.left, ast, context, view, value, result)) return false;
    if (!value->aggregate || value->pointer) {
        result->state = value->pointer ? DebugWatchState::TypeMismatch : DebugWatchState::NotAggregate;
        copyText(result->diagnostic, sizeof(result->diagnostic), value->pointer ? "member access needs an aggregate" : "value is not an aggregate");
        return false;
    }
    if (!expandNode(context, view, value)) {
        result->state = DebugWatchState::Stale;
        copyText(result->diagnostic, sizeof(result->diagnostic), "stopped value is stale");
        return false;
    }
    const DebugDwarfValueNode* parent = nodeAt(view, value->nodeId);
    if (!parent || parent->state != DebugDwarfVariableState::Available) {
        if (parent) result->state = nodeFailureState(*parent);
        copyText(result->diagnostic, sizeof(result->diagnostic), "aggregate is unavailable");
        return false;
    }
    uint64_t childId = 0;
    if (!findNodeByName(view, value->nodeId, expressionNode.identifier, &childId)) {
        result->state = DebugWatchState::MemberNotFound;
        copyText(result->diagnostic, sizeof(result->diagnostic), "member was not found");
        return false;
    }
    const DebugDwarfValueNode* child = nodeAt(view, childId);
    if (!child) { result->state = DebugWatchState::MalformedDebugInfo; return false; }
    *value = evalValueFromNode(*child);
    return true;
}

static bool evaluatePointerMember(const DebugExpressionNode& expressionNode,
                                  const DebugExpressionAst& ast, const DebugWatchEvaluationContext& context,
                                  DebugDwarfVariableView* view, EvalValue* value, DebugWatchResult* result) {
    if (!evaluateNode(expressionNode.left, ast, context, view, value, result)) return false;
    if (!value->pointer || !value->fromNode) {
        result->state = DebugWatchState::NotPointer;
        copyText(result->diagnostic, sizeof(result->diagnostic), "arrow access needs a pointer");
        return false;
    }
    if (value->scalarValue == 0) {
        result->state = DebugWatchState::NullPointer;
        copyText(result->diagnostic, sizeof(result->diagnostic), "null pointer");
        return false;
    }
    if (!canonicalAddress(value->scalarValue)) {
        result->state = DebugWatchState::UnreadableTarget;
        copyText(result->diagnostic, sizeof(result->diagnostic), "pointer target is not canonical");
        return false;
    }
    uint64_t pointee = 0;
    if (!pointeeType(context.mapper, value->typeDieOffset, &pointee)) {
        result->state = DebugWatchState::TypeMismatch;
        copyText(result->diagnostic, sizeof(result->diagnostic), "pointer has no supported pointee");
        return false;
    }
    DebugDwarfTypeInfo type = {};
    if (!DebugDwarfDescribeType(context.mapper, pointee, &type)) {
        result->state = DebugWatchState::MalformedDebugInfo;
        copyText(result->diagnostic, sizeof(result->diagnostic), "pointee type is unavailable");
        return false;
    }
    if (type.kind == DebugDwarfTypeKind::Array || type.kind == DebugDwarfTypeKind::Structure ||
        type.kind == DebugDwarfTypeKind::Class || type.kind == DebugDwarfTypeKind::Union) {
        value->aggregate = true;
    } else {
        result->state = DebugWatchState::NotAggregate;
        copyText(result->diagnostic, sizeof(result->diagnostic), "pointee is not an aggregate");
        return false;
    }
    if (!expandNode(context, view, value)) {
        result->state = DebugWatchState::UnreadableTarget;
        copyText(result->diagnostic, sizeof(result->diagnostic), "pointer target is unreadable");
        return false;
    }
    const DebugDwarfValueNode* pointerNode = nodeAt(view, value->nodeId);
    if (!pointerNode) { result->state = DebugWatchState::MalformedDebugInfo; return false; }
    uint64_t childId = 0;
    if (!findNodeByName(view, value->nodeId, expressionNode.identifier, &childId)) {
        result->state = pointerNode->state == DebugDwarfVariableState::ReadFailure ?
            DebugWatchState::UnreadableTarget : DebugWatchState::MemberNotFound;
        copyText(result->diagnostic, sizeof(result->diagnostic), result->state == DebugWatchState::UnreadableTarget ? "pointer target is unreadable" : "member was not found");
        return false;
    }
    const DebugDwarfValueNode* child = nodeAt(view, childId);
    if (!child) { result->state = DebugWatchState::MalformedDebugInfo; return false; }
    *value = evalValueFromNode(*child);
    return true;
}

static bool evaluateArrayIndex(const DebugExpressionNode& expressionNode,
                               const DebugExpressionAst& ast, const DebugWatchEvaluationContext& context,
                               DebugDwarfVariableView* view, EvalValue* value, DebugWatchResult* result) {
    if (!evaluateNode(expressionNode.left, ast, context, view, value, result)) return false;
    if (!value->fromNode || !value->aggregate || value->pointer) {
        result->state = DebugWatchState::TypeMismatch;
        copyText(result->diagnostic, sizeof(result->diagnostic), "indexing needs an array");
        return false;
    }
    EvalValue indexValue = {};
    if (!evaluateNode(expressionNode.right, ast, context, view, &indexValue, result)) return false;
    if (!indexValue.scalar || indexValue.pointer) {
        result->state = DebugWatchState::TypeMismatch;
        copyText(result->diagnostic, sizeof(result->diagnostic), "array index must be an integer");
        return false;
    }
    DebugDwarfTypeInfo type = {};
    if (!DebugDwarfDescribeType(context.mapper, value->typeDieOffset, &type) || type.kind != DebugDwarfTypeKind::Array || !type.hasBounds) {
        result->state = DebugWatchState::MalformedDebugInfo;
        copyText(result->diagnostic, sizeof(result->diagnostic), "array bounds are unavailable");
        return false;
    }
    if (indexValue.scalarValue > static_cast<uint64_t>(INT64_MAX) ||
        static_cast<int64_t>(indexValue.scalarValue) < type.lowerBound ||
        static_cast<int64_t>(indexValue.scalarValue) > type.upperBound) {
        result->state = DebugWatchState::IndexOutOfRange;
        copyText(result->diagnostic, sizeof(result->diagnostic), "array index is out of range");
        return false;
    }
    if (!expandNode(context, view, value)) {
        result->state = DebugWatchState::UnreadableTarget;
        copyText(result->diagnostic, sizeof(result->diagnostic), "array is unavailable");
        return false;
    }
    char indexName[kDebugDwarfMaxVariableNameBytes] = {};
    indexName[0] = '[';
    char number[24] = {};
    uint64_t index = indexValue.scalarValue;
    uint32_t digits = 0;
    do { number[digits++] = static_cast<char>('0' + (index % 10u)); index /= 10u; } while (index != 0 && digits + 1 < sizeof(number));
    for (uint32_t i = 0; i < digits; ++i) indexName[i + 1] = number[digits - i - 1u];
    indexName[digits + 1u] = ']';
    indexName[digits + 2u] = '\0';
    uint64_t childId = 0;
    if (!findNodeByName(view, value->nodeId, indexName, &childId)) {
        result->state = DebugWatchState::UnreadableTarget;
        copyText(result->diagnostic, sizeof(result->diagnostic), "array element is not materialized");
        return false;
    }
    const DebugDwarfValueNode* child = nodeAt(view, childId);
    if (!child) { result->state = DebugWatchState::MalformedDebugInfo; return false; }
    *value = evalValueFromNode(*child);
    return true;
}

static bool isEqualityComparison(DebugExpressionComparisonKind kind) {
    return kind == DebugExpressionComparisonKind::Equal ||
        kind == DebugExpressionComparisonKind::NotEqual;
}

static bool applyComparison(DebugExpressionComparisonKind kind, int comparison) {
    switch (kind) {
    case DebugExpressionComparisonKind::Equal: return comparison == 0;
    case DebugExpressionComparisonKind::NotEqual: return comparison != 0;
    case DebugExpressionComparisonKind::Less: return comparison < 0;
    case DebugExpressionComparisonKind::LessEqual: return comparison <= 0;
    case DebugExpressionComparisonKind::Greater: return comparison > 0;
    case DebugExpressionComparisonKind::GreaterEqual: return comparison >= 0;
    }
    return false;
}

static int compareUnsigned(uint64_t left, uint64_t right) {
    return left < right ? -1 : left > right ? 1 : 0;
}

static int64_t signedValue(uint64_t value, uint32_t bitWidth) {
    if (bitWidth == 0 || bitWidth >= 64u) return static_cast<int64_t>(value);
    const uint64_t mask = (1ull << bitWidth) - 1u;
    value &= mask;
    const uint64_t sign = 1ull << (bitWidth - 1u);
    if (value & sign) value |= ~mask;
    return static_cast<int64_t>(value);
}

static int compareIntegers(const EvalValue& left, const EvalTypeInfo& leftType,
                           const EvalValue& right, const EvalTypeInfo& rightType) {
    const bool leftSigned = leftType.category == EvalCategory::SignedInteger;
    const bool rightSigned = rightType.category == EvalCategory::SignedInteger;
    if (leftSigned && rightSigned) {
        const int64_t leftNumber = signedValue(left.scalarValue, leftType.bitWidth);
        const int64_t rightNumber = signedValue(right.scalarValue, rightType.bitWidth);
        return leftNumber < rightNumber ? -1 : leftNumber > rightNumber ? 1 : 0;
    }
    if (!leftSigned && !rightSigned) return compareUnsigned(left.scalarValue, right.scalarValue);

    // Mixed signed/unsigned comparison is explicit and mathematical within the
    // bounded 64-bit model: a negative signed value is below every unsigned
    // value; a non-negative signed value is compared as uint64_t. This avoids
    // host compiler promotions and preserves unsigned maxima.
    if (leftSigned) {
        const int64_t leftNumber = signedValue(left.scalarValue, leftType.bitWidth);
        if (leftNumber < 0) return -1;
        return compareUnsigned(static_cast<uint64_t>(leftNumber), right.scalarValue);
    }
    const int64_t rightNumber = signedValue(right.scalarValue, rightType.bitWidth);
    if (rightNumber < 0) return 1;
    return compareUnsigned(left.scalarValue, static_cast<uint64_t>(rightNumber));
}

static bool comparisonFailure(DebugWatchResult* result, DebugWatchState state,
                              const char* diagnostic) {
    if (!result) return false;
    result->state = state;
    copyText(result->diagnostic, sizeof(result->diagnostic), diagnostic);
    return false;
}

static bool evaluateComparison(const DebugExpressionNode& expressionNode,
                               const DebugExpressionAst& ast,
                               const DebugWatchEvaluationContext& context,
                               DebugDwarfVariableView* view, EvalValue* value,
                               DebugWatchResult* result) {
    EvalValue left = {};
    EvalValue right = {};
    if (!evaluateNode(expressionNode.left, ast, context, view, &left, result) ||
        !evaluateNode(expressionNode.right, ast, context, view, &right, result)) return false;

    const DebugExpressionComparisonKind kind = static_cast<DebugExpressionComparisonKind>(expressionNode.integerValue);
    EvalTypeInfo leftType = {};
    EvalTypeInfo rightType = {};
    if (!classifyEvalValue(context.mapper, left, &leftType) ||
        !classifyEvalValue(context.mapper, right, &rightType))
        return comparisonFailure(result, DebugWatchState::TypeMismatch,
                                 "comparison type is unavailable");

    const bool leftPointer = leftType.category == EvalCategory::Pointer;
    const bool rightPointer = rightType.category == EvalCategory::Pointer;
    if (leftPointer || rightPointer) {
        if (!isEqualityComparison(kind))
            return comparisonFailure(result, DebugWatchState::TypeMismatch,
                                     "relational pointer comparison is not supported");
        const bool rightIsZeroLiteral = right.literal && right.scalarValue == 0;
        const bool leftIsZeroLiteral = left.literal && left.scalarValue == 0;
        if (leftPointer && rightPointer) {
            if (left.scalarValue != 0 && !canonicalAddress(left.scalarValue))
                return comparisonFailure(result, DebugWatchState::UnreadableTarget,
                                         "left pointer is not canonical");
            if (right.scalarValue != 0 && !canonicalAddress(right.scalarValue))
                return comparisonFailure(result, DebugWatchState::UnreadableTarget,
                                         "right pointer is not canonical");
        } else if (leftPointer && rightIsZeroLiteral) {
            if (left.scalarValue != 0 && !canonicalAddress(left.scalarValue))
                return comparisonFailure(result, DebugWatchState::UnreadableTarget,
                                         "pointer is not canonical");
        } else if (rightPointer && leftIsZeroLiteral) {
            if (right.scalarValue != 0 && !canonicalAddress(right.scalarValue))
                return comparisonFailure(result, DebugWatchState::UnreadableTarget,
                                         "pointer is not canonical");
        } else {
            return comparisonFailure(result, DebugWatchState::TypeMismatch,
                                     "pointer equality needs a pointer or integer literal zero");
        }
        *value = EvalValue();
        value->valid = true;
        value->scalar = true;
        value->valueKind = DebugDwarfValueKind::Boolean;
        value->locationKind = DebugDwarfLocationKind::ImmediateValue;
        value->scalarValue = applyComparison(kind, compareUnsigned(left.scalarValue, right.scalarValue)) ? 1u : 0u;
        copyText(value->typeDisplay, sizeof(value->typeDisplay), "bool");
        copyText(value->valueDisplay, sizeof(value->valueDisplay), value->scalarValue ? "true" : "false");
        return true;
    }

    const bool leftInteger = leftType.category == EvalCategory::SignedInteger ||
        leftType.category == EvalCategory::UnsignedInteger || leftType.category == EvalCategory::Boolean;
    const bool rightInteger = rightType.category == EvalCategory::SignedInteger ||
        rightType.category == EvalCategory::UnsignedInteger || rightType.category == EvalCategory::Boolean;
    if (!leftInteger || !rightInteger)
        return comparisonFailure(result, DebugWatchState::TypeMismatch,
                                 "comparison needs compatible integral scalar values");

    const int ordering = compareIntegers(left, leftType, right, rightType);
    *value = EvalValue();
    value->valid = true;
    value->scalar = true;
    value->valueKind = DebugDwarfValueKind::Boolean;
    value->locationKind = DebugDwarfLocationKind::ImmediateValue;
    value->scalarValue = applyComparison(kind, ordering) ? 1u : 0u;
    copyText(value->typeDisplay, sizeof(value->typeDisplay), "bool");
    copyText(value->valueDisplay, sizeof(value->valueDisplay), value->scalarValue ? "true" : "false");
    return true;
}

static bool evaluateNode(uint16_t nodeIndex, const DebugExpressionAst& ast,
                         const DebugWatchEvaluationContext& context, DebugDwarfVariableView* view,
                         EvalValue* value, DebugWatchResult* result) {
    if (!value || !result || nodeIndex >= ast.nodeCount) {
        if (result) { result->state = DebugWatchState::MalformedDebugInfo; copyText(result->diagnostic, sizeof(result->diagnostic), "invalid expression node"); }
        return false;
    }
    const DebugExpressionNode& expressionNode = ast.nodes[nodeIndex];
    switch (expressionNode.kind) {
    case DebugExpressionNodeKind::Identifier: {
        const DebugDwarfVariable* variable = findVariable(view, expressionNode.identifier);
        if (!variable || variable->nodeId == 0) {
            result->state = DebugWatchState::UnknownIdentifier;
            copyText(result->diagnostic, sizeof(result->diagnostic), "unknown identifier");
            return false;
        }
        const DebugDwarfValueNode* node = nodeAt(view, variable->nodeId);
        if (!node) { result->state = DebugWatchState::MalformedDebugInfo; return false; }
        if (variable->state != DebugDwarfVariableState::Available) {
            result->state = context.frame.frameIndex > 0 ? DebugWatchState::UnavailableInCallerFrame :
                variable->state == DebugDwarfVariableState::MalformedDebugInfo ? DebugWatchState::MalformedDebugInfo :
                DebugWatchState::Stale;
            copyText(result->diagnostic, sizeof(result->diagnostic), context.frame.frameIndex > 0 ?
                     "unavailable in caller frame" : "variable is unavailable");
            return false;
        }
        *value = evalValueFromNode(*node);
        return true;
    }
    case DebugExpressionNodeKind::IntegerLiteral: {
        *value = EvalValue();
        value->valid = true;
        value->literal = true;
        value->scalar = true;
        value->scalarValue = expressionNode.integerValue;
        value->valueKind = expressionNode.integerValue <= static_cast<uint64_t>(INT64_MAX) ?
            DebugDwarfValueKind::SignedInteger : DebugDwarfValueKind::UnsignedInteger;
        value->locationKind = DebugDwarfLocationKind::ImmediateValue;
        copyText(value->typeDisplay, sizeof(value->typeDisplay),
                 value->valueKind == DebugDwarfValueKind::SignedInteger ? "int" : "uint64");
        char number[24] = {};
        uint64_t numberValue = expressionNode.integerValue;
        uint32_t digits = 0;
        do { number[digits++] = static_cast<char>('0' + (numberValue % 10u)); numberValue /= 10u; } while (numberValue != 0 && digits + 1 < sizeof(number));
        for (uint32_t i = 0; i < digits; ++i) value->valueDisplay[i] = number[digits - i - 1u];
        value->valueDisplay[digits] = '\0';
        return true;
    }
    case DebugExpressionNodeKind::MemberAccess:
        return evaluateMember(expressionNode, ast, context, view, value, result);
    case DebugExpressionNodeKind::PointerMemberAccess:
        return evaluatePointerMember(expressionNode, ast, context, view, value, result);
    case DebugExpressionNodeKind::ArrayIndex:
        return evaluateArrayIndex(expressionNode, ast, context, view, value, result);
    case DebugExpressionNodeKind::Dereference: {
        if (!evaluateNode(expressionNode.left, ast, context, view, value, result)) return false;
        if (!value->pointer || !value->fromNode) {
            result->state = DebugWatchState::NotPointer;
            copyText(result->diagnostic, sizeof(result->diagnostic), "dereference needs a pointer");
            return false;
        }
        if (value->scalarValue == 0) {
            result->state = DebugWatchState::NullPointer;
            copyText(result->diagnostic, sizeof(result->diagnostic), "null pointer");
            return false;
        }
        if (!canonicalAddress(value->scalarValue)) {
            result->state = DebugWatchState::UnreadableTarget;
            copyText(result->diagnostic, sizeof(result->diagnostic), "pointer target is not canonical");
            return false;
        }
        uint64_t pointee = 0;
        if (!pointeeType(context.mapper, value->typeDieOffset, &pointee)) {
            result->state = DebugWatchState::MalformedDebugInfo;
            copyText(result->diagnostic, sizeof(result->diagnostic), "pointee type is unavailable");
            return false;
        }
        DebugDwarfTypeInfo type = {};
        if (!DebugDwarfDescribeType(context.mapper, pointee, &type)) {
            result->state = DebugWatchState::MalformedDebugInfo;
            return false;
        }
        if (!expandNode(context, view, value)) {
            result->state = DebugWatchState::UnreadableTarget;
            copyText(result->diagnostic, sizeof(result->diagnostic), "pointer target is unreadable");
            return false;
        }
        uint64_t childId = 0;
        const DebugDwarfValueNode* pointerNode = nodeAt(view, value->nodeId);
        if (pointerNode && findNodeByName(view, value->nodeId, "*", &childId)) {
            const DebugDwarfValueNode* child = nodeAt(view, childId);
            if (!child) { result->state = DebugWatchState::MalformedDebugInfo; return false; }
            *value = evalValueFromNode(*child);
            return true;
        }
        if (!pointerNode || !isAggregate(type.kind == DebugDwarfTypeKind::Array ? DebugDwarfValueNodeKind::Array :
                                           type.kind == DebugDwarfTypeKind::Structure ? DebugDwarfValueNodeKind::Structure :
                                           type.kind == DebugDwarfTypeKind::Class ? DebugDwarfValueNodeKind::Class :
                                           type.kind == DebugDwarfTypeKind::Union ? DebugDwarfValueNodeKind::Union :
                                           DebugDwarfValueNodeKind::OpaqueAggregate)) {
            result->state = pointerNode ? nodeFailureState(*pointerNode) : DebugWatchState::MalformedDebugInfo;
            return false;
        }
        value->logicalDereference = true;
        value->aggregate = true;
        value->pointer = false;
        value->scalar = false;
        value->typeDieOffset = pointee;
        value->address = value->scalarValue;
        copyText(value->typeDisplay, sizeof(value->typeDisplay), type.display);
        copyText(value->valueDisplay, sizeof(value->valueDisplay), "{...}");
        return true;
    }
    case DebugExpressionNodeKind::AddressOf: {
        if (!evaluateNode(expressionNode.left, ast, context, view, value, result)) return false;
        if (!value->fromNode || !value->address || value->locationKind != DebugDwarfLocationKind::MemoryAddress) {
            result->state = DebugWatchState::AddressUnavailable;
            copyText(result->diagnostic, sizeof(result->diagnostic), "address is unavailable");
            return false;
        }
        value->valid = true;
        value->pointer = true;
        value->scalar = true;
        value->aggregate = false;
        value->scalarValue = value->address;
        value->valueKind = DebugDwarfValueKind::Pointer;
        value->locationKind = DebugDwarfLocationKind::ImmediateValue;
        char pointerType[kDebugDwarfMaxTypeDisplayBytes] = {};
        copyText(pointerType, sizeof(pointerType), value->typeDisplay);
        uint32_t length = textLength(pointerType, sizeof(pointerType));
        if (length + 1 < sizeof(pointerType)) { pointerType[length] = '*'; pointerType[length + 1] = '\0'; }
        copyText(value->typeDisplay, sizeof(value->typeDisplay), pointerType);
        formatHexAddress(value->valueDisplay, sizeof(value->valueDisplay), value->scalarValue);
        value->nodeId = 0;
        return true;
    }
    case DebugExpressionNodeKind::Comparison:
        return evaluateComparison(expressionNode, ast, context, view, value, result);
    }
    return false;
}

static void clearResult(DebugWatchResult* result, uint64_t id) {
    if (!result) return;
    *result = DebugWatchResult();
    result->watchId = id;
    result->state = DebugWatchState::Stale;
    copyText(result->valueDisplay, sizeof(result->valueDisplay), "<stale>");
}

static void setParseResult(DebugWatchResult* result, uint64_t id, const DebugExpressionAst& ast) {
    if (!result) return;
    clearResult(result, id);
    result->state = ast.state == DebugExpressionParseState::UnsupportedExpression ?
        DebugWatchState::UnsupportedExpression : DebugWatchState::ParseError;
    copyText(result->valueDisplay, sizeof(result->valueDisplay), ast.diagnostic[0] ? ast.diagnostic : "invalid expression");
    copyText(result->diagnostic, sizeof(result->diagnostic), ast.diagnostic);
}

static void setParseResultText(DebugWatchResult* result, uint64_t id,
                               DebugExpressionParseState state, const char* diagnostic) {
    if (!result) return;
    clearResult(result, id);
    result->state = state == DebugExpressionParseState::UnsupportedExpression ?
        DebugWatchState::UnsupportedExpression : DebugWatchState::ParseError;
    copyText(result->valueDisplay, sizeof(result->valueDisplay), diagnostic && diagnostic[0] ? diagnostic : "invalid expression");
    copyText(result->diagnostic, sizeof(result->diagnostic), diagnostic);
}

} // namespace

const char* DebugExpressionTokenKindName(DebugExpressionTokenKind kind) {
    switch (kind) {
    case DebugExpressionTokenKind::Identifier: return "Identifier";
    case DebugExpressionTokenKind::IntegerLiteral: return "IntegerLiteral";
    case DebugExpressionTokenKind::Dot: return "Dot";
    case DebugExpressionTokenKind::Arrow: return "Arrow";
    case DebugExpressionTokenKind::Star: return "Star";
    case DebugExpressionTokenKind::Ampersand: return "Ampersand";
    case DebugExpressionTokenKind::LeftBracket: return "LeftBracket";
    case DebugExpressionTokenKind::RightBracket: return "RightBracket";
    case DebugExpressionTokenKind::LeftParen: return "LeftParen";
    case DebugExpressionTokenKind::RightParen: return "RightParen";
    case DebugExpressionTokenKind::End: return "End";
    case DebugExpressionTokenKind::EqualEqual: return "EqualEqual";
    case DebugExpressionTokenKind::NotEqual: return "NotEqual";
    case DebugExpressionTokenKind::Less: return "Less";
    case DebugExpressionTokenKind::LessEqual: return "LessEqual";
    case DebugExpressionTokenKind::Greater: return "Greater";
    case DebugExpressionTokenKind::GreaterEqual: return "GreaterEqual";
    }
    return "Unknown";
}

const char* DebugExpressionNodeKindName(DebugExpressionNodeKind kind) {
    switch (kind) {
    case DebugExpressionNodeKind::Identifier: return "Identifier";
    case DebugExpressionNodeKind::IntegerLiteral: return "IntegerLiteral";
    case DebugExpressionNodeKind::MemberAccess: return "MemberAccess";
    case DebugExpressionNodeKind::PointerMemberAccess: return "PointerMemberAccess";
    case DebugExpressionNodeKind::ArrayIndex: return "ArrayIndex";
    case DebugExpressionNodeKind::Dereference: return "Dereference";
    case DebugExpressionNodeKind::AddressOf: return "AddressOf";
    case DebugExpressionNodeKind::Comparison: return "Comparison";
    }
    return "Unknown";
}

const char* DebugExpressionParseStateName(DebugExpressionParseState state) {
    switch (state) {
    case DebugExpressionParseState::Empty: return "Empty";
    case DebugExpressionParseState::Valid: return "Valid";
    case DebugExpressionParseState::ParseError: return "ParseError";
    case DebugExpressionParseState::UnsupportedExpression: return "UnsupportedExpression";
    case DebugExpressionParseState::TooLong: return "TooLong";
    case DebugExpressionParseState::TooManyTokens: return "TooManyTokens";
    case DebugExpressionParseState::TooManyNodes: return "TooManyNodes";
    case DebugExpressionParseState::TooDeep: return "TooDeep";
    }
    return "Unknown";
}

const char* DebugWatchStateName(DebugWatchState state) {
    switch (state) {
    case DebugWatchState::Empty: return "Empty";
    case DebugWatchState::Available: return "Available";
    case DebugWatchState::ParseError: return "ParseError";
    case DebugWatchState::UnsupportedExpression: return "UnsupportedExpression";
    case DebugWatchState::UnknownIdentifier: return "UnknownIdentifier";
    case DebugWatchState::TypeMismatch: return "TypeMismatch";
    case DebugWatchState::NotPointer: return "NotPointer";
    case DebugWatchState::NotAggregate: return "NotAggregate";
    case DebugWatchState::MemberNotFound: return "MemberNotFound";
    case DebugWatchState::NullPointer: return "NullPointer";
    case DebugWatchState::UnreadableTarget: return "UnreadableTarget";
    case DebugWatchState::IndexOutOfRange: return "IndexOutOfRange";
    case DebugWatchState::AddressUnavailable: return "AddressUnavailable";
    case DebugWatchState::UnavailableInCallerFrame: return "UnavailableInCallerFrame";
    case DebugWatchState::Stale: return "Stale";
    case DebugWatchState::Running: return "Running";
    case DebugWatchState::MalformedDebugInfo: return "MalformedDebugInfo";
    }
    return "Unknown";
}

bool DebugExpressionParse(const char* expression, DebugExpressionAst* ast) {
    if (!ast) return false;
    *ast = DebugExpressionAst();
    if (!expression) {
        ast->state = DebugExpressionParseState::ParseError;
        copyText(ast->diagnostic, sizeof(ast->diagnostic), "null expression");
        return false;
    }
    Parser parser(expression, ast);
    return parser.run();
}

bool DebugWatchCollectionInit(DebugWatchCollection* collection) {
    if (!collection) return false;
    *collection = DebugWatchCollection();
    collection->nextId = 1;
    return true;
}

DebugWatchItem* DebugWatchCollectionFind(DebugWatchCollection* collection, uint64_t watchId) {
    if (!collection || watchId == 0) return nullptr;
    for (uint32_t i = 0; i < kDebugWatchMaxWatches; ++i)
        if (collection->items[i].used && collection->items[i].id == watchId) return &collection->items[i];
    return nullptr;
}

const DebugWatchItem* DebugWatchCollectionFindConst(const DebugWatchCollection* collection, uint64_t watchId) {
    return DebugWatchCollectionFind(const_cast<DebugWatchCollection*>(collection), watchId);
}

const DebugWatchItem* DebugWatchCollectionAt(const DebugWatchCollection* collection, uint32_t index) {
    if (!collection) return nullptr;
    uint32_t seen = 0;
    for (uint32_t i = 0; i < kDebugWatchMaxWatches; ++i) {
        if (!collection->items[i].used) continue;
        if (seen++ == index) return &collection->items[i];
    }
    return nullptr;
}

bool DebugWatchCollectionAdd(DebugWatchCollection* collection, const char* expression,
                             uint64_t* outWatchId) {
    if (outWatchId) *outWatchId = 0;
    if (!collection || !expression || collection->count >= kDebugWatchMaxWatches) return false;
    for (uint32_t i = 0; i < kDebugWatchMaxWatches; ++i) if (!collection->items[i].used) {
        DebugWatchItem& item = collection->items[i];
        item = DebugWatchItem();
        item.used = true;
        item.id = collection->nextId++;
        if (item.id == 0) item.id = collection->nextId++;
        copyText(item.expression, sizeof(item.expression), expression);
        DebugExpressionAst parsed = {};
        DebugExpressionParse(expression, &parsed);
        item.parseState = parsed.state;
        copyText(item.parseDiagnostic, sizeof(item.parseDiagnostic), parsed.diagnostic);
        clearResult(&item.result, item.id);
        if (item.parseState != DebugExpressionParseState::Valid)
            setParseResultText(&item.result, item.id, item.parseState, item.parseDiagnostic);
        ++collection->count;
        if (outWatchId) *outWatchId = item.id;
        return true;
    }
    return false;
}

bool DebugWatchCollectionEdit(DebugWatchCollection* collection, uint64_t watchId,
                              const char* expression) {
    DebugWatchItem* item = DebugWatchCollectionFind(collection, watchId);
    if (!item || !expression) return false;
    copyText(item->expression, sizeof(item->expression), expression);
    DebugExpressionAst parsed = {};
    DebugExpressionParse(expression, &parsed);
    item->parseState = parsed.state;
    copyText(item->parseDiagnostic, sizeof(item->parseDiagnostic), parsed.diagnostic);
    clearResult(&item->result, item->id);
    if (item->parseState != DebugExpressionParseState::Valid)
        setParseResultText(&item->result, item->id, item->parseState, item->parseDiagnostic);
    collection->treeStale = true;
    return true;
}

bool DebugWatchCollectionRemove(DebugWatchCollection* collection, uint64_t watchId) {
    DebugWatchItem* item = DebugWatchCollectionFind(collection, watchId);
    if (!item) return false;
    *item = DebugWatchItem();
    if (collection->count > 0) --collection->count;
    collection->treeStale = true;
    return true;
}

bool DebugWatchEvaluateExpression(const DebugExpressionAst& ast, const char* expression,
                                  const DebugWatchEvaluationContext& context,
                                  DebugDwarfVariableView* valueTree,
                                  DebugWatchResult* result) {
    if (!result) return false;
    const uint64_t id = result->watchId;
    clearResult(result, id);
    result->sessionGeneration = context.frame.sessionGeneration;
    result->stopGeneration = context.frame.stopGeneration;
    result->frameIndex = context.frame.frameIndex;
    result->artifactGeneration = context.mapper ? context.mapper->identity.mapperGeneration : 0;
    if (!ast.valid) { setParseResult(result, id, ast); return false; }
    if (!context.mapper || !valueTree || !valueTree->valid || valueTree->stale ||
        !DebugDwarfMapperIsReady(context.mapper)) {
        result->state = DebugWatchState::Stale;
        copyText(result->valueDisplay, sizeof(result->valueDisplay), "<stale>");
        copyText(result->diagnostic, sizeof(result->diagnostic), "stopped value is stale");
        return false;
    }
    EvalValue value = {};
    if (!evaluateNode(ast.rootNode, ast, context, valueTree, &value, result)) {
        if (result->valueDisplay[0] == '\0') copyText(result->valueDisplay, sizeof(result->valueDisplay), "<unavailable>");
        return false;
    }
    result->state = DebugWatchState::Available;
    result->valueKind = value.valueKind;
    result->locationKind = value.locationKind;
    result->nodeId = value.nodeId;
    result->typeDieOffset = value.typeDieOffset;
    result->address = value.address;
    result->scalarValue = value.scalarValue;
    result->hasAddress = value.address != 0 && value.fromNode &&
        (value.locationKind == DebugDwarfLocationKind::MemoryAddress || value.logicalDereference);
    result->hasScalar = value.scalar || value.pointer;
    result->structured = value.aggregate;
    copyText(result->typeDisplay, sizeof(result->typeDisplay), value.typeDisplay);
    copyText(result->valueDisplay, sizeof(result->valueDisplay), value.valueDisplay);
    if (value.nodeId != 0) {
        const DebugDwarfValueNode* node = nodeAt(valueTree, value.nodeId);
        if (node) {
            result->dieOffset = node->dieOffset;
            result->sessionGeneration = node->sessionGeneration;
            result->stopGeneration = node->stopGeneration;
            result->frameIndex = node->frameIndex;
            if (!value.logicalDereference) result->artifactGeneration = node->artifactGeneration;
        }
    }
    copyText(result->diagnostic, sizeof(result->diagnostic), expression ? "" : "missing expression");
    return true;
}

bool DebugWatchConvertToTruth(const DebugWatchResult& result, bool* truth,
                              char* diagnostic, uint32_t diagnosticSize) {
    if (truth) *truth = false;
    if (diagnostic && diagnosticSize > 0) diagnostic[0] = '\0';
    if (result.state != DebugWatchState::Available) {
        if (diagnostic && diagnosticSize > 0)
            copyText(diagnostic, diagnosticSize, result.diagnostic[0] ? result.diagnostic : "condition value is unavailable");
        return false;
    }
    if (result.structured || !result.hasScalar) {
        if (diagnostic && diagnosticSize > 0)
            copyText(diagnostic, diagnosticSize, "condition value is not a supported scalar");
        return false;
    }
    switch (result.valueKind) {
    case DebugDwarfValueKind::SignedInteger:
    case DebugDwarfValueKind::UnsignedInteger:
    case DebugDwarfValueKind::Boolean:
        if (truth) *truth = result.scalarValue != 0;
        return true;
    case DebugDwarfValueKind::Pointer:
    case DebugDwarfValueKind::Address:
        if (result.scalarValue == 0) {
            if (truth) *truth = false;
            return true;
        }
        if (!canonicalAddress(result.scalarValue)) {
            if (diagnostic && diagnosticSize > 0)
                copyText(diagnostic, diagnosticSize, "condition pointer is not canonical");
            return false;
        }
        if (truth) *truth = true;
        return true;
    default:
        if (diagnostic && diagnosticSize > 0)
            copyText(diagnostic, diagnosticSize, "condition value is not a supported scalar");
        return false;
    }
}

bool DebugWatchCollectionRefresh(DebugWatchCollection* collection,
                                 const DebugWatchEvaluationContext& context) {
    if (!collection || !context.mapper || context.frame.sessionGeneration == 0 ||
        context.frame.stopGeneration == 0 || !DebugDwarfMapperIsReady(context.mapper)) {
        if (collection) DebugWatchCollectionMarkStale(collection);
        return false;
    }
    collection->tree = DebugDwarfVariableView();
    if (!DebugDwarfInspectVariables(context.mapper, context.frame, context.readMemory,
                                    context.userData, &collection->tree)) {
        collection->treeValid = false;
        collection->treeStale = true;
        for (uint32_t i = 0; i < kDebugWatchMaxWatches; ++i)
            if (collection->items[i].used && collection->items[i].parseState == DebugExpressionParseState::Valid) {
                clearResult(&collection->items[i].result, collection->items[i].id);
                collection->items[i].result.state = context.frame.frameIndex > 0 ?
                    DebugWatchState::UnavailableInCallerFrame : DebugWatchState::Stale;
                copyText(collection->items[i].result.valueDisplay, sizeof(collection->items[i].result.valueDisplay), "<unavailable>");
            }
        return false;
    }
    collection->treeValid = true;
    collection->treeStale = false;
    DebugWatchEvaluationContext current = context;
    for (uint32_t i = 0; i < kDebugWatchMaxWatches; ++i) {
        DebugWatchItem& item = collection->items[i];
        if (!item.used) continue;
        if (item.parseState != DebugExpressionParseState::Valid) {
            setParseResultText(&item.result, item.id, item.parseState, item.parseDiagnostic);
            continue;
        }
        DebugExpressionAst ast = {};
        DebugExpressionParse(item.expression, &ast);
        if (!ast.valid) {
            item.parseState = ast.state;
            copyText(item.parseDiagnostic, sizeof(item.parseDiagnostic), ast.diagnostic);
            setParseResult(&item.result, item.id, ast);
            continue;
        }
        item.result.watchId = item.id;
        DebugWatchEvaluateExpression(ast, item.expression, current, &collection->tree, &item.result);
    }
    return true;
}

bool DebugWatchCollectionExpand(DebugWatchCollection* collection,
                                const DebugWatchEvaluationContext& context,
                                uint64_t watchId) {
    if (!collection || !collection->treeValid || collection->treeStale) return false;
    DebugWatchItem* item = DebugWatchCollectionFind(collection, watchId);
    if (!item || item->result.state != DebugWatchState::Available || item->result.nodeId == 0) return false;
    if (!DebugDwarfExpandValue(context.mapper, context.frame, context.readMemory, context.userData,
                               &collection->tree, item->result.nodeId)) return false;
    const DebugDwarfValueNode* node = nodeAt(&collection->tree, item->result.nodeId);
    if (node && node->state == DebugDwarfVariableState::ReadFailure) {
        item->result.state = DebugWatchState::UnreadableTarget;
        copyText(item->result.valueDisplay, sizeof(item->result.valueDisplay), node->valueDisplay);
        copyText(item->result.diagnostic, sizeof(item->result.diagnostic), node->status);
        return false;
    }
    return node != nullptr;
}

void DebugWatchCollectionMarkRunning(DebugWatchCollection* collection) {
    if (!collection) return;
    collection->treeValid = false;
    collection->treeStale = true;
    collection->tree.stale = true;
    for (uint32_t i = 0; i < kDebugWatchMaxWatches; ++i) if (collection->items[i].used) {
        if (collection->items[i].parseState != DebugExpressionParseState::Valid) {
            setParseResultText(&collection->items[i].result, collection->items[i].id,
                               collection->items[i].parseState, collection->items[i].parseDiagnostic);
            continue;
        }
        clearResult(&collection->items[i].result, collection->items[i].id);
        collection->items[i].result.state = DebugWatchState::Running;
        copyText(collection->items[i].result.valueDisplay, sizeof(collection->items[i].result.valueDisplay), "<running>");
    }
}

void DebugWatchCollectionMarkStale(DebugWatchCollection* collection) {
    if (!collection) return;
    collection->treeValid = false;
    collection->treeStale = true;
    collection->tree.stale = true;
    for (uint32_t i = 0; i < kDebugWatchMaxWatches; ++i) if (collection->items[i].used) {
        if (collection->items[i].parseState != DebugExpressionParseState::Valid) {
            setParseResultText(&collection->items[i].result, collection->items[i].id,
                               collection->items[i].parseState, collection->items[i].parseDiagnostic);
            continue;
        }
        clearResult(&collection->items[i].result, collection->items[i].id);
    }
}

} // namespace developer_studio
} // namespace guidexos
