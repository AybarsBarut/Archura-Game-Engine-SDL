#pragma once

// Small bounded JSON DOM shared by both legacy project formats. Parsing is
// independent of whitespace; duplicate keys and trailing input are rejected.
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <locale>
#include <map>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace Archura::Serialization {
struct Json {
    enum class Kind { Null, Boolean, Number, String, Array, Object };
    Kind kind = Kind::Null;
    bool boolean = false;
    double number = 0;
    std::string string;
    std::vector<Json> array;
    std::map<std::string, Json> object;
    const Json* Find(const std::string& key) const {
        const auto it = object.find(key);
        return kind == Kind::Object && it != object.end() ? &it->second : nullptr;
    }
};

class JsonParser {
public:
    explicit JsonParser(std::string_view input) : input(input) {}
    bool Parse(Json& result) {
        if (input.size() > 32u * 1024u * 1024u) return false;
        Json parsed;
        if (!Value(parsed, 0)) return false;
        Space();
        if (pos != input.size()) return false;
        result = std::move(parsed);
        return true;
    }
private:
    std::string_view input;
    size_t pos = 0;
    size_t nodes = 0;
    void Space() { while (pos < input.size() && (input[pos] == ' ' || input[pos] == '\t' || input[pos] == '\r' || input[pos] == '\n')) ++pos; }
    bool Take(char c) { Space(); if (pos == input.size() || input[pos] != c) return false; ++pos; return true; }
    bool Hex(uint32_t& value) {
        value = 0;
        for (int i = 0; i < 4; ++i) {
            if (pos == input.size()) return false;
            const char c = input[pos++];
            unsigned d;
            if (c >= '0' && c <= '9') d = c - '0';
            else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
            else return false;
            value = value * 16 + d;
        }
        return true;
    }
    static void Utf8(std::string& s, uint32_t c) {
        if (c < 0x80) s += static_cast<char>(c);
        else if (c < 0x800) { s += static_cast<char>(0xc0 | (c >> 6)); s += static_cast<char>(0x80 | (c & 63)); }
        else if (c < 0x10000) { s += static_cast<char>(0xe0 | (c >> 12)); s += static_cast<char>(0x80 | ((c >> 6) & 63)); s += static_cast<char>(0x80 | (c & 63)); }
        else { s += static_cast<char>(0xf0 | (c >> 18)); s += static_cast<char>(0x80 | ((c >> 12) & 63)); s += static_cast<char>(0x80 | ((c >> 6) & 63)); s += static_cast<char>(0x80 | (c & 63)); }
    }
    bool String(std::string& result) {
        if (!Take('"')) return false;
        while (pos < input.size()) {
            const unsigned char c = static_cast<unsigned char>(input[pos++]);
            if (c == '"') return true;
            if (c < 0x20) return false;
            if (c != '\\') {
                if (c < 0x80) { result += static_cast<char>(c); continue; }
                // Reject invalid UTF-8, overlong sequences and encoded surrogates.
                unsigned count = c >= 0xc2 && c <= 0xdf ? 1 : c >= 0xe0 && c <= 0xef ? 2 : c >= 0xf0 && c <= 0xf4 ? 3 : 0;
                if (!count || pos + count > input.size()) return false;
                uint32_t cp = c & (count == 1 ? 31 : count == 2 ? 15 : 7);
                for (unsigned i = 0; i < count; ++i) {
                    unsigned char next = static_cast<unsigned char>(input[pos++]);
                    if ((next & 0xc0) != 0x80) return false;
                    cp = (cp << 6) | (next & 63);
                }
                if ((count == 1 && cp < 0x80) || (count == 2 && cp < 0x800) || (count == 3 && cp < 0x10000) || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff)) return false;
                Utf8(result, cp);
                continue;
            }
            if (pos == input.size()) return false;
            switch (input[pos++]) {
                case '"': result += '"'; break;
                case '\\': result += '\\'; break;
                case '/': result += '/'; break;
                case 'b': result += '\b'; break;
                case 'f': result += '\f'; break;
                case 'n': result += '\n'; break;
                case 'r': result += '\r'; break;
                case 't': result += '\t'; break;
                case 'u': {
                    uint32_t cp; if (!Hex(cp)) return false;
                    if (cp >= 0xd800 && cp <= 0xdbff) {
                        if (pos + 2 > input.size() || input[pos++] != '\\' || input[pos++] != 'u') return false;
                        uint32_t low; if (!Hex(low) || low < 0xdc00 || low > 0xdfff) return false;
                        cp = 0x10000 + ((cp - 0xd800) << 10) + low - 0xdc00;
                    } else if (cp >= 0xdc00 && cp <= 0xdfff) return false;
                    Utf8(result, cp); break;
                }
                default: return false;
            }
        }
        return false;
    }
    bool Value(Json& out, unsigned depth) {
        if (depth > 64 || ++nodes > 2000000) return false;
        Space(); if (pos == input.size()) return false;
        const char c = input[pos];
        if (c == '{') {
            out.kind = Json::Kind::Object; ++pos;
            if (Take('}')) return true;
            do {
                std::string key; Json child;
                if (!String(key) || !Take(':') || !Value(child, depth + 1) || !out.object.emplace(std::move(key), std::move(child)).second) return false;
                if (Take('}')) return true;
            } while (Take(','));
            return false;
        }
        if (c == '[') {
            out.kind = Json::Kind::Array; ++pos;
            if (Take(']')) return true;
            do {
                Json child; if (!Value(child, depth + 1)) return false;
                out.array.push_back(std::move(child));
                if (Take(']')) return true;
            } while (Take(','));
            return false;
        }
        if (c == '"') { out.kind = Json::Kind::String; return String(out.string); }
        for (const auto* word : {"true", "false", "null"}) {
            const std::string_view token(word);
            if (input.substr(pos, token.size()) == token) {
                pos += token.size(); out.kind = token == "null" ? Json::Kind::Null : Json::Kind::Boolean;
                out.boolean = token == "true"; return true;
            }
        }
        const size_t start = pos;
        if (input[pos] == '-') ++pos;
        if (pos == input.size()) return false;
        if (input[pos] == '0') ++pos;
        else {
            if (input[pos] < '1' || input[pos] > '9') return false;
            while (pos < input.size() && input[pos] >= '0' && input[pos] <= '9') ++pos;
        }
        if (pos < input.size() && input[pos] == '.') {
            ++pos; const size_t digits = pos;
            while (pos < input.size() && input[pos] >= '0' && input[pos] <= '9') ++pos;
            if (digits == pos) return false;
        }
        if (pos < input.size() && (input[pos] == 'e' || input[pos] == 'E')) {
            ++pos; if (pos < input.size() && (input[pos] == '+' || input[pos] == '-')) ++pos;
            const size_t digits = pos;
            while (pos < input.size() && input[pos] >= '0' && input[pos] <= '9') ++pos;
            if (digits == pos) return false;
        }
        std::istringstream number(std::string(input.substr(start, pos - start)));
        number.imbue(std::locale::classic()); number >> out.number;
        out.kind = Json::Kind::Number;
        return !number.fail() && std::isfinite(out.number);
    }
};

inline std::string Escape(std::string_view text) {
    std::string out;
    constexpr char hex[] = "0123456789abcdef";
    for (const unsigned char c : text) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 32) { out += "\\u00"; out += hex[c >> 4]; out += hex[c & 15]; }
                else out += static_cast<char>(c);
        }
    }
    return out;
}
inline bool StringField(const Json& object, const char* key, std::string& out, bool required = false) {
    const Json* value = object.Find(key);
    if (!value) return !required;
    if (value->kind != Json::Kind::String) return false;
    out = value->string; return true;
}
inline bool FloatField(const Json& object, const char* key, float& out) {
    const Json* value = object.Find(key); if (!value) return true;
    if (value->kind != Json::Kind::Number || std::abs(value->number) > std::numeric_limits<float>::max()) return false;
    out = static_cast<float>(value->number); return std::isfinite(out);
}
inline bool IntField(const Json& object, const char* key, int& out) {
    const Json* value = object.Find(key); if (!value) return true;
    if (value->kind != Json::Kind::Number || std::floor(value->number) != value->number || value->number < std::numeric_limits<int>::min() || value->number > std::numeric_limits<int>::max()) return false;
    out = static_cast<int>(value->number); return true;
}
inline bool BoolField(const Json& object, const char* key, bool& out) {
    const Json* value = object.Find(key); if (!value) return true;
    if (value->kind != Json::Kind::Boolean) return false;
    out = value->boolean; return true;
}
template<class Vec3> bool Vec3Field(const Json& object, const char* key, Vec3& out) {
    const Json* value = object.Find(key); if (!value) return true;
    if (value->kind != Json::Kind::Array || value->array.size() != 3) return false;
    for (int i = 0; i < 3; ++i) {
        if (value->array[i].kind != Json::Kind::Number || std::abs(value->array[i].number) > std::numeric_limits<float>::max()) return false;
        out[i] = static_cast<float>(value->array[i].number);
    }
    return true;
}
} // namespace Archura::Serialization
