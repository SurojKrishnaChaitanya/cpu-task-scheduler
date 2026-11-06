#include "json_mini.h"
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace mj {

const Value* Value::find(const std::string& key) const {
    if (type != Object) return nullptr;
    for (size_t k = keys.size(); k-- > 0;)
        if (keys[k] == key) return &vals[k];
    return nullptr;
}

namespace {

const int MAX_DEPTH = 32;

bool isDigit(char c) { return c >= '0' && c <= '9'; }

int hexVal(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

void appendUtf8(std::string& out, unsigned cp) {
    if (cp < 0x80) out += static_cast<char>(cp);
    else if (cp < 0x800) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
}

// Recursive-descent parser: one function per kind of JSON value.
struct Parser {
    const std::string& t;
    size_t i = 0;
    explicit Parser(const std::string& text) : t(text) {}

    void ws() {
        while (i < t.size() && (t[i] == ' ' || t[i] == '\t' || t[i] == '\n' || t[i] == '\r')) ++i;
    }
    bool literal(const char* word) {
        size_t n = std::strlen(word);
        if (t.compare(i, n, word) != 0) return false;
        i += n;
        return true;
    }

    bool value(Value& v, int depth) {
        if (depth > MAX_DEPTH) return false;
        ws();
        if (i >= t.size()) return false;
        char c = t[i];
        if (c == '{') return object(v, depth);
        if (c == '[') return array(v, depth);
        if (c == '"') { v.type = Value::String; return string(v.s); }
        if (c == 't') { v.type = Value::Bool; v.b = true;  return literal("true"); }
        if (c == 'f') { v.type = Value::Bool; v.b = false; return literal("false"); }
        if (c == 'n') { v.type = Value::Null; return literal("null"); }
        if (c == '-' || isDigit(c)) return number(v);
        return false;
    }

    bool number(Value& v) {
        size_t start = i;
        bool neg = false, real = false;
        if (t[i] == '-') { neg = true; ++i; }
        if (i >= t.size() || !isDigit(t[i])) return false;
        if (t[i] == '0') ++i;                                   // no leading zeros
        else while (i < t.size() && isDigit(t[i])) ++i;
        if (i < t.size() && t[i] == '.') {
            real = true; ++i;
            if (i >= t.size() || !isDigit(t[i])) return false;
            while (i < t.size() && isDigit(t[i])) ++i;
        }
        if (i < t.size() && (t[i] == 'e' || t[i] == 'E')) {
            real = true; ++i;
            if (i < t.size() && (t[i] == '+' || t[i] == '-')) ++i;
            if (i >= t.size() || !isDigit(t[i])) return false;
            while (i < t.size() && isDigit(t[i])) ++i;
        }
        std::string tok = t.substr(start, i - start);
        if (real) { v.type = Value::Real; v.d = std::strtod(tok.c_str(), nullptr); return true; }
        v.type = Value::Int;
        size_t digits = tok.size() - (neg ? 1 : 0);
        if (digits > 18) v.i = neg ? -LLONG_MAX : LLONG_MAX;    // saturate; callers range-check
        else v.i = std::strtoll(tok.c_str(), nullptr, 10);
        return true;
    }

    bool string(std::string& out) {
        ++i;   // opening quote
        while (i < t.size()) {
            unsigned char c = static_cast<unsigned char>(t[i++]);
            if (c == '"') return true;
            if (c < 0x20) return false;                         // raw control characters are illegal
            if (c != '\\') { out += static_cast<char>(c); continue; }
            if (i >= t.size()) return false;
            char e = t[i++];
            switch (e) {
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                case '/': out += '/'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case 'u': {
                    if (i + 4 > t.size()) return false;
                    unsigned cp = 0;
                    for (int k = 0; k < 4; ++k) {
                        int h = hexVal(t[i++]);
                        if (h < 0) return false;
                        cp = cp * 16 + static_cast<unsigned>(h);
                    }
                    appendUtf8(out, cp);
                    break;
                }
                default: return false;
            }
        }
        return false;   // ran off the end: unterminated string
    }

    bool array(Value& v, int depth) {
        v.type = Value::Array;
        ++i;   // '['
        ws();
        if (i < t.size() && t[i] == ']') { ++i; return true; }
        while (true) {
            Value item;
            if (!value(item, depth + 1)) return false;
            v.arr.push_back(std::move(item));
            ws();
            if (i >= t.size()) return false;
            if (t[i] == ',') { ++i; continue; }
            if (t[i] == ']') { ++i; return true; }
            return false;
        }
    }

    bool object(Value& v, int depth) {
        v.type = Value::Object;
        ++i;   // '{'
        ws();
        if (i < t.size() && t[i] == '}') { ++i; return true; }
        while (true) {
            ws();
            if (i >= t.size() || t[i] != '"') return false;
            std::string key;
            if (!string(key)) return false;
            ws();
            if (i >= t.size() || t[i] != ':') return false;
            ++i;
            Value item;
            if (!value(item, depth + 1)) return false;
            v.keys.push_back(std::move(key));
            v.vals.push_back(std::move(item));
            ws();
            if (i >= t.size()) return false;
            if (t[i] == ',') { ++i; continue; }
            if (t[i] == '}') { ++i; return true; }
            return false;
        }
    }
};

}  // namespace

bool parse(const std::string& text, Value& out) {
    Value tmp;
    Parser p(text);
    if (!p.value(tmp, 0)) return false;
    p.ws();
    if (p.i != text.size()) return false;   // trailing garbage
    out = std::move(tmp);
    return true;
}

std::string quote(const std::string& s) {
    std::string out = "\"";
    for (unsigned char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) { char buf[8]; std::snprintf(buf, sizeof buf, "\\u%04x", c); out += buf; }
                else out += static_cast<char>(c);
        }
    }
    return out + "\"";
}

}