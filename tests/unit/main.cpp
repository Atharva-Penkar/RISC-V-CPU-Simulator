#include "check.hpp"

int main() {
    test::Suite suite;
    test::addIsaTests(suite);
    test::addAssemblerTests(suite);
    test::addAluTests(suite);
    test::addMemoryTests(suite);
    test::addCpuTests(suite);
    test::addTimelineTests(suite);

    int failedCases = 0;
    for (const test::Case& c : suite.cases()) {
        const int before = test::failureCount();
        c.body();
        if (test::failureCount() != before) {
            failedCases++;
            std::cerr << "FAILED: " << c.name << "\n\n";
        }
    }
    std::cout << suite.cases().size() - static_cast<std::size_t>(failedCases) << '/' << suite.cases().size()
              << " test cases passed\n";
    return failedCases == 0 ? 0 : 1;
}
