#pragma once
// Tiny dependency-free test harness.
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace t {
struct Case { const char* name; std::function<void()> fn; };
inline std::vector<Case>& registry() { static std::vector<Case> r; return r; }
inline int& failures() { static int f = 0; return f; }
struct Reg { Reg(const char* n, std::function<void()> f) { registry().push_back({n, std::move(f)}); } };

template <typename T> std::string str(const T& v) { std::ostringstream o; o << v; return o.str(); }
template <typename T> std::string str(const std::vector<T>& v)
{
    std::ostringstream o; o << "{";
    for (size_t i = 0; i < v.size(); ++i) o << (i ? "," : "") << v[i];
    o << "}"; return o.str();
}
} // namespace t

#define TC_CAT2(a, b) a##b
#define TC_CAT(a, b) TC_CAT2(a, b)
#define TEST(name) static void name(); static t::Reg TC_CAT(reg_, name)(#name, name); static void name()
#define CHECK(c) do { if (!(c)) { ++t::failures(); std::cerr << __FILE__ << ":" << __LINE__ << " CHECK failed: " #c "\n"; } } while (0)
#define CHECK_EQ(a, b) do { auto _a = (a); auto _b = (b); if (!(_a == _b)) { ++t::failures(); \
    std::cerr << __FILE__ << ":" << __LINE__ << " CHECK_EQ failed: " #a " = " << t::str(_a) << ", expected " << t::str(_b) << "\n"; } } while (0)
