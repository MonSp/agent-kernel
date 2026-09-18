#pragma once
// JsonUtils — shared JSON helpers for agent-kernel.
// Single source of truth for escaping and parsing to avoid duplication.

#include <string>
#include <cstdio>

namespace JsonUtils {

// Escape a string for embedding in a JSON string literal.
// Handles quotes, backslashes, control chars (\b \f \n \r \t), and \uXXXX for < 0x20.
inline void appendEscaped(std::string& out, const std::string& s) {
    out.reserve(out.size() + s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b";  break;
            case '\f': out += "\\f";  break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x",
                                  static_cast<unsigned int>(static_cast<unsigned char>(c)));
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
}

inline std::string escape(const std::string& s) {
    std::string out;
    appendEscaped(out, s);
    return out;
}

// Extract a string value for a top-level JSON key: "key":"value"
// Returns empty string if not found. Handles escaped quotes inside value.
inline std::string findString(const std::string& json, const std::string& key) {
    std::string needle = "\"" + key + "\"";
    auto pos = json.find(needle);
    if (pos == std::string::npos) return "";

    pos = json.find(':', pos + needle.size());
    if (pos == std::string::npos) return "";
    ++pos;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' ||
           json[pos] == '\n' || json[pos] == '\r')) ++pos;

    if (pos >= json.size() || json[pos] != '"') return "";
    ++pos;

    std::string val;
    while (pos < json.size()) {
        char c = json[pos++];
        if (c == '\\' && pos < json.size()) {
            char esc = json[pos++];
            switch (esc) {
                case '"':  val += '"';  break;
                case '\\': val += '\\'; break;
                case 'n':  val += '\n'; break;
                case 'r':  val += '\r'; break;
                case 't':  val += '\t'; break;
                case 'b':  val += '\b'; break;
                case 'f':  val += '\f'; break;
                default:   val += esc;  break;
            }
        } else if (c == '"') {
            break;
        } else {
            val += c;
        }
    }
    return val;
}

// Extract an integer value for a top-level JSON key.
inline int findInt(const std::string& json, const std::string& key, int defaultVal = 0) {
    std::string needle = "\"" + key + "\"";
    auto pos = json.find(needle);
    if (pos == std::string::npos) return defaultVal;

    pos = json.find(':', pos + needle.size());
    if (pos == std::string::npos) return defaultVal;
    ++pos;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' ||
           json[pos] == '\n' || json[pos] == '\r')) ++pos;

    bool neg = false;
    if (pos < json.size() && json[pos] == '-') { neg = true; ++pos; }
    int val = 0;
    bool foundDigit = false;
    while (pos < json.size() && json[pos] >= '0' && json[pos] <= '9') {
        val = val * 10 + (json[pos] - '0');
        ++pos;
        foundDigit = true;
    }
    if (!foundDigit) return defaultVal;
    return neg ? -val : val;
}

// Extract a float value for a top-level JSON key.
inline float findFloat(const std::string& json, const std::string& key, float defaultVal) {
    std::string needle = "\"" + key + "\"";
    auto pos = json.find(needle);
    if (pos == std::string::npos) return defaultVal;

    pos = json.find(':', pos + needle.size());
    if (pos == std::string::npos) return defaultVal;
    ++pos;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' ||
           json[pos] == '\n' || json[pos] == '\r')) ++pos;

    if (pos >= json.size()) return defaultVal;

    // Parse number (possibly negative, possibly with decimal)
    size_t start = pos;
    if (pos < json.size() && json[pos] == '-') ++pos;
    while (pos < json.size() && ((json[pos] >= '0' && json[pos] <= '9') || json[pos] == '.')) ++pos;

    if (pos == start) return defaultVal;
    try {
        return std::stof(json.substr(start, pos - start));
    } catch (...) {
        return defaultVal;
    }
}

// Find a JSON object in the text (first '{' ... matching '}').
// Returns the substring, or empty if not found.
inline std::string extractObject(const std::string& text) {
    size_t start = text.find('{');
    if (start == std::string::npos) return "";

    int depth = 0;
    for (size_t i = start; i < text.size(); ++i) {
        if (text[i] == '{') ++depth;
        else if (text[i] == '}') {
            --depth;
            if (depth == 0) return text.substr(start, i - start + 1);
        }
    }
    return ""; // unmatched braces
}

} // namespace JsonUtils
