#include "TestFramework.h"
#include <iostream>
#include <cstdlib>

void registerMappingGeneratorTests(TestFramework& framework);
void registerMinimalTests(TestFramework& framework);
void registerPerfectScoreTest(TestFramework& framework);

int main() {
    std::cout << "VoynichDecoder Unit Tests" << std::endl;
    std::cout << "=========================" << std::endl << std::endl;
    
    TestFramework testFramework;
    
    // Register all test suites
    registerPerfectScoreTest(testFramework);
    registerMappingGeneratorTests(testFramework);
    registerMinimalTests(testFramework);    
    
    testFramework.runAllTests();
    testFramework.printResults();
    
    if (testFramework.allTestsPassed()) {
        std::cout << std::endl << "All tests passed! ✓" << std::endl;
        return EXIT_SUCCESS;
    } else {
        std::cout << std::endl << "Some tests failed! ✗" << std::endl;
        return EXIT_FAILURE;
    }
}