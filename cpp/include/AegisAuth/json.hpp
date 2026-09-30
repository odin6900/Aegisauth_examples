// Minimal JSON value type + parser/serializer for the Aegis C++ SDK.
// Header-only, C++17, no third-party dependencies.
#pragma once

#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace aegisauth {

class Json;
using JsonObject = std::map<std::string, Json>;
using JsonArray = std::vector<Json>;

/** A dynamically typed JSON value. */
class Json {
public:
    using Value = std::variant<std::monostate, bool, double, std::string, JsonArray, JsonObject>;

    Json() = default;
    Json(bool v) : value_(v) {}
    Json(double v) : value_(v) {}
    Json(int v) : value_(static_cast<double>(v)) {}
    Json(const char* v) : value_(std::string(v)) {}
    Json(std::string v) : value_(std::move(v)) {}
    Json(JsonArray v) : value_(std::move(v)) {}
    Json(JsonObject v) : value_(std::move(v)) {}

    bool isNull() const { return std::holds_alternative<std::monostate>(value_); }
    bool isObject() const { return std::holds_alternative<JsonObject>(value_); }
    bool isArray() const { return std::holds_alternative<JsonArray>(value_); }

    /** Object member access; returns a null Json when absent. */
    const Json& operator[](const std::string& key) const {
        static const Json kNull;
        if (auto* obj = std::get_if<JsonObject>(&value_)) {
            auto it = obj->find(key);
            if (it != obj->end()) return it->second;
        }
        return kNull;
    }

    const JsonArray& items() const {
        static const JsonArray kEmpty;
        auto* arr = std::get_if<JsonArray>(&value_);
        return arr ? *arr : kEmpty;
    }

    std::string asString(const std::string& fallback = "") const {
        if (auto* s = std::get_if<std::string>(&value_)) return *s;
        return fallback;
    }
    double asNumber(double fallback = 0) const {
        if (auto* n = std::get_if<double>(&value_)) return *n;
        return fallback;
    }
    bool asBool(bool fallback = false) const {
        if (auto* b = std::get_if<bool>(&value_)) return *b;
        return fallback;
    }

    void set(const std::string& key, Json v) {
        if (!isObject()) value_ = JsonObject{};
        std::get<JsonObject>(value_).emplace(key, std::move(v));
    }

    std::string dump() const {
        std::ostringstream out;
        write(out);
        return out.str();
    }

    static Json parse(const std::string& text) {
        size_t i = 0;
        Json value = parseValue(text, i);
        return value;
    }

private:
    Value value_{};

    void write(std::ostringstream& out) const {
        struct Writer {
            std::ostringstream& out;
            void operator()(std::monostate) const { out << "null"; }
            void operator()(bool v) const { out << (v ? "true" : "false"); }
            void operator()(double v) const {
                if (v == static_cast<long long>(v)) out << static_cast<long long>(v);
                else out << v;
            }
            void operator()(const std::string& v) const { writeString(out, v); }
            void operator()(const JsonArray& v) const {
                out << '[';
                for (size_t i = 0; i < v.size(); ++i) {
                    if (i) out << ',';
                    v[i].write(out);
                }
                out << ']';
            }
            void operator()(const JsonObject& v) const {
                out << '{';
                bool first = true;
                for (const auto& [key, item] : v) {
                    if (!first) out << ',';
                    first = false;
                    writeString(out, key);
                    out << ':';
                    item.write(out);
                }
                out << '}';
            }
        };
        std::visit(Writer{out}, value_);
    }

    static void writeString(std::ostringstream& out, const std::string& text) {
        out << '"';
        for (char c : text) {
            switch (c) {
                case '"': out << "\\\""; break;
                case '\\': out << "\\\\"; break;
                case '\n': out << "\\n"; break;
                case '\r': out << "\\r"; break;
                case '\t': out << "\\t"; break;
                default:
                    if (static_cast<unsigned char>(c) < 0x20) {
                        char buf[7];
                        std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                        out << buf;
                    } else {
                        out << c;
                    }
            }
        }
        out << '"';
    }

    static void skipWhitespace(const std::string& s, size_t& i) {
        while (i < s.size() && (s[i] == ' ' || s[i] == '\n' || s[i] == '\r' || s[i] == '\t')) ++i;
    }

    static std::string parseString(const std::string& s, size_t& i) {
        if (s[i] != '"') throw std::runtime_error("Expected string in JSON payload.");
        ++i;
        std::string out;
        while (i < s.size() && s[i] != '"') {
            if (s[i] == '\\' && i + 1 < s.size()) {
                ++i;
                switch (s[i]) {
                    case 'n': out += '\n'; break;
                    case 't': out += '\t'; break;
                    case 'r': out += '\r'; break;
                    case 'b': out += '\b'; break;
                    case 'f': out += '\f'; break;
                    case 'u': {
                        unsigned code = std::stoul(s.substr(i + 1, 4), nullptr, 16);
                        i += 4;
                        if (code < 0x80) {
                            out += static_cast<char>(code);
                        } else if (code < 0x800) {
                            out += static_cast<char>(0xC0 | (code >> 6));
                            out += static_cast<char>(0x80 | (code & 0x3F));
                        } else {
                            out += static_cast<char>(0xE0 | (code >> 12));
                            out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
                            out += static_cast<char>(0x80 | (code & 0x3F));
                        }
                        break;
                    }
                    default: out += s[i];
                }
            } else {
                out += s[i];
            }
            ++i;
        }
        ++i;
        return out;
    }

    static Json parseValue(const std::string& s, size_t& i) {
        skipWhitespace(s, i);
        if (i >= s.size()) return Json{};
        char c = s[i];
        if (c == '{') {
            ++i;
            JsonObject obj;
            skipWhitespace(s, i);
            if (i < s.size() && s[i] == '}') { ++i; return Json(obj); }
            while (i < s.size()) {
                skipWhitespace(s, i);
                std::string key = parseString(s, i);
                skipWhitespace(s, i);
                if (i < s.size() && s[i] == ':') ++i;
                obj.emplace(key, parseValue(s, i));
                skipWhitespace(s, i);
                if (i < s.size() && s[i] == ',') { ++i; continue; }
                if (i < s.size() && s[i] == '}') { ++i; break; }
                break;
            }
            return Json(obj);
        }
        if (c == '[') {
            ++i;
            JsonArray arr;
            skipWhitespace(s, i);
            if (i < s.size() && s[i] == ']') { ++i; return Json(arr); }
            while (i < s.size()) {
                arr.push_back(parseValue(s, i));
                skipWhitespace(s, i);
                if (i < s.size() && s[i] == ',') { ++i; continue; }
                if (i < s.size() && s[i] == ']') { ++i; break; }
                break;
            }
            return Json(arr);
        }
        if (c == '"') return Json(parseString(s, i));
        if (s.compare(i, 4, "true") == 0) { i += 4; return Json(true); }
        if (s.compare(i, 5, "false") == 0) { i += 5; return Json(false); }
        if (s.compare(i, 4, "null") == 0) { i += 4; return Json{}; }
        size_t start = i;
        while (i < s.size() && (std::isdigit(static_cast<unsigned char>(s[i])) || s[i] == '-' || s[i] == '+' || s[i] == '.' || s[i] == 'e' || s[i] == 'E')) ++i;
        return Json(std::stod(s.substr(start, i - start)));
    }
};

}  // namespace aegisauth