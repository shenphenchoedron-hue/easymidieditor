#include "Test.h"

int main()
{
    for (auto& c : t::registry()) {
        const int before = t::failures();
        c.fn();
        std::cout << (t::failures() == before ? "[ OK ] " : "[FAIL] ") << c.name << "\n";
    }
    std::cout << t::registry().size() << " tests, " << t::failures() << " failed checks\n";
    return t::failures() == 0 ? 0 : 1;
}
