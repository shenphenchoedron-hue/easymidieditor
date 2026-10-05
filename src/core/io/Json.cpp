#include "io/Json.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace mc::io {

namespace {
const Json kNull;
const Json::Array kEmptyArray;
const Json::Object kEmptyObject;

void escape(std::string& out, const std::string& s)
{
    out += '"';
    for (unsigned char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) { char b[8]; std::snprintf(b, sizeof b, "\\u%04x", c); out += b; }
                else out += (char)c;
        }
    }
    out += '"';
}

struct Parser {
    const std::string& s;
    size_t i = 0;
    [[noreturn]] void fail(const char* m) { throw std::runtime_error(std::string("JSON: ") + m + " at " + std::to_string(i)); }
    void ws() { while (i < s.size() && (s[i] == ' ' || s[i] == '\n' || s[i] == '\r' || s[i] == '\t')) ++i; }
    bool lit(const char* l) { size_t n = std::char_traits<char>::length(l); if (s.compare(i, n, l) == 0) { i += n; return true; } return false; }
    void appendUtf8(std::string& o, unsigned cp)
    {
        if (cp < 0x80) o += (char)cp;
        else if (cp < 0x800) { o += (char)(0xC0 | (cp >> 6)); o += (char)(0x80 | (cp & 0x3F)); }
        else if (cp < 0x10000) { o += (char)(0xE0 | (cp >> 12)); o += (char)(0x80 | ((cp >> 6) & 0x3F)); o += (char)(0x80 | (cp & 0x3F)); }
        else { o += (char)(0xF0 | (cp >> 18)); o += (char)(0x80 | ((cp >> 12) & 0x3F)); o += (char)(0x80 | ((cp >> 6) & 0x3F)); o += (char)(0x80 | (cp & 0x3F)); }
    }
    unsigned hex4()
    {
        if (i + 4 > s.size()) fail("bad \\u");
        unsigned v = (unsigned)std::strtoul(s.substr(i, 4).c_str(), nullptr, 16);
        i += 4;
        return v;
    }
    std::string string()
    {
        if (s[i] != '"') fail("expected string");
        ++i;
        std::string o;
        while (i < s.size() && s[i] != '"') {
            char c = s[i++];
            if (c != '\\') { o += c; continue; }
            if (i >= s.size()) fail("bad escape");
            char e = s[i++];
            switch (e) {
                case 'n': o += '\n'; break; case 't': o += '\t'; break; case 'r': o += '\r'; break;
                case 'b': o += '\b'; break; case 'f': o += '\f'; break;
                case 'u': {
                    unsigned cp = hex4();
                    if (cp >= 0xD800 && cp < 0xDC00 && lit("\\u")) cp = 0x10000 + ((cp - 0xD800) << 10) + (hex4() - 0xDC00);
                    appendUtf8(o, cp);
                    break;
                }
                default: o += e;
            }
        }
        if (i >= s.size()) fail("unterminated string");
        ++i;
        return o;
    }
    Json value()
    {
        ws();
        if (i >= s.size()) fail("unexpected end");
        char c = s[i];
        if (c == '{') {
            ++i; Json::Object o; ws();
            if (s[i] == '}') { ++i; return o; }
            for (;;) {
                ws(); std::string k = string(); ws();
                if (s[i++] != ':') fail("expected :");
                o[k] = value(); ws();
                if (s[i] == ',') { ++i; continue; }
                if (s[i] == '}') { ++i; return o; }
                fail("expected , or }");
            }
        }
        if (c == '[') {
            ++i; Json::Array a; ws();
            if (s[i] == ']') { ++i; return a; }
            for (;;) {
                a.push_back(value()); ws();
                if (s[i] == ',') { ++i; continue; }
                if (s[i] == ']') { ++i; return a; }
                fail("expected , or ]");
            }
        }
        if (c == '"') return string();
        if (lit("true")) return true;
        if (lit("false")) return false;
        if (lit("null")) return nullptr;
        char* end = nullptr;
        double d = std::strtod(s.c_str() + i, &end);
        if (end == s.c_str() + i) fail("bad value");
        i = (size_t)(end - s.c_str());
        return d;
    }
};
} // namespace

const Json::Array& Json::arr() const { auto* p = std::get_if<Array>(&v_); return p ? *p : kEmptyArray; }
const Json::Object& Json::obj() const { auto* p = std::get_if<Object>(&v_); return p ? *p : kEmptyObject; }

const Json& Json::operator[](const std::string& key) const
{
    auto& o = obj();
    auto it = o.find(key);
    return it == o.end() ? kNull : it->second;
}

Json& Json::set(const std::string& key, Json v)
{
    if (!isObject()) v_ = Object{};
    return std::get<Object>(v_)[key] = std::move(v);
}

std::string Json::dump(int indent) const
{
    std::string out;
    dumpImpl(out, indent, 0);
    return out;
}

void Json::dumpImpl(std::string& out, int indent, int level) const
{
    auto nl = [&](int l) { if (indent > 0) { out += '\n'; out.append((size_t)(l * indent), ' '); } };
    if (isNull()) out += "null";
    else if (auto* b = std::get_if<bool>(&v_)) out += *b ? "true" : "false";
    else if (auto* d = std::get_if<double>(&v_)) {
        if (std::isfinite(*d) && *d == std::floor(*d) && std::fabs(*d) < 9e15) out += std::to_string((long long)*d);
        else { char b[32]; std::snprintf(b, sizeof b, "%.17g", std::isfinite(*d) ? *d : 0.0); out += b; }
    }
    else if (auto* s = std::get_if<std::string>(&v_)) escape(out, *s);
    else if (auto* a = std::get_if<Array>(&v_)) {
        out += '[';
        for (size_t k = 0; k < a->size(); ++k) { if (k) out += ','; nl(level + 1); (*a)[k].dumpImpl(out, indent, level + 1); }
        if (!a->empty()) nl(level);
        out += ']';
    } else if (auto* o = std::get_if<Object>(&v_)) {
        out += '{';
        bool first = true;
        for (auto& [k, v] : *o) {
            if (!first) out += ',';
            first = false;
            nl(level + 1); escape(out, k); out += indent > 0 ? ": " : ":";
            v.dumpImpl(out, indent, level + 1);
        }
        if (!o->empty()) nl(level);
        out += '}';
    }
}

Json Json::parse(const std::string& text)
{
    Parser p{text};
    Json v = p.value();
    p.ws();
    if (p.i != text.size()) p.fail("trailing characters");
    return v;
}

} // namespace mc::io
