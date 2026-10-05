#pragma once
// Minimal JSON value/parser/writer (dependency-free) for the project format.
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace mc::io {

class Json {
public:
    using Array = std::vector<Json>;
    using Object = std::map<std::string, Json>;

    Json() : v_(nullptr) {}
    Json(std::nullptr_t) : v_(nullptr) {}
    Json(bool b) : v_(b) {}
    Json(int i) : v_((double)i) {}
    Json(long long i) : v_((double)i) {}
    Json(long i) : v_((double)i) {}
    Json(unsigned long long i) : v_((double)i) {}
    Json(unsigned long i) : v_((double)i) {}
    Json(double d) : v_(d) {}
    Json(float d) : v_((double)d) {}
    Json(const char* s) : v_(std::string(s)) {}
    Json(std::string s) : v_(std::move(s)) {}
    Json(Array a) : v_(std::move(a)) {}
    Json(Object o) : v_(std::move(o)) {}

    bool isNull() const { return std::holds_alternative<std::nullptr_t>(v_); }
    bool isObject() const { return std::holds_alternative<Object>(v_); }
    bool isArray() const { return std::holds_alternative<Array>(v_); }

    // Lenient accessors with defaults: missing/mistyped fields never throw
    // (forward/backward compatible loading).
    double num(double def = 0) const { auto* p = std::get_if<double>(&v_); return p ? *p : def; }
    bool boolean(bool def = false) const { auto* p = std::get_if<bool>(&v_); return p ? *p : def; }
    std::string str(const std::string& def = {}) const { auto* p = std::get_if<std::string>(&v_); return p ? *p : def; }
    const Array& arr() const;
    const Object& obj() const;
    const Json& operator[](const std::string& key) const; // null if missing
    Json& set(const std::string& key, Json v);            // turns null into object

    std::string dump(int indent = 2) const;
    static Json parse(const std::string& text); // throws std::runtime_error

private:
    void dumpImpl(std::string& out, int indent, int level) const;
    std::variant<std::nullptr_t, bool, double, std::string, Array, Object> v_;
};

} // namespace mc::io
