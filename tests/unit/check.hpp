// Minimal test harness: no external dependencies, failures report file and line.
#pragma once

#include <functional>
#include <iostream>
#include <source_location>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace test {

struct Case {
    std::string name;
    std::function<void()> body;
};

class Suite {
public:
    void add(std::string name, std::function<void()> body) { cases_.push_back({std::move(name), std::move(body)}); }
    const std::vector<Case>& cases() const { return cases_; }

private:
    std::vector<Case> cases_;
};

inline int& failureCount() {
    static int failures = 0;
    return failures;
}

inline void fail(std::string_view message, const std::source_location& where) {
    failureCount()++;
    std::cerr << where.file_name() << ':' << where.line() << ": " << message << '\n';
}

inline void expect(bool condition, std::string_view what = "expectation failed",
                   std::source_location where = std::source_location::current()) {
    if (!condition)
        fail(what, where);
}

template <class A, class B>
void expectEq(const A& actual, const B& expected, std::source_location where = std::source_location::current()) {
    if (!(actual == expected)) {
        std::cerr << "  actual:   " << actual << "\n  expected: " << expected << '\n';
        fail("values differ", where);
    }
}

void addIsaTests(Suite& suite);
void addAssemblerTests(Suite& suite);
void addAluTests(Suite& suite);
void addMemoryTests(Suite& suite);
void addCpuTests(Suite& suite);

}  // namespace test
