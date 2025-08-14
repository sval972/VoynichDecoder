#include "TestFramework.h"
#include <iomanip>
#include <stdexcept>

TestFramework::TestFramework() : totalTests(0), passedTests(0), failedTests(0) {
}

void TestFramework::addTest(const std::string& name, std::function<void()> testFunction) {
    tests.push_back({name, testFunction});
}

void TestFramework::runAllTests() {
    totalTests = static_cast<int>(tests.size());
    passedTests = 0;
    failedTests = 0;
    results.clear();

    std::cout << "Running " << totalTests << " tests...\n\n";

    for (const auto& test : tests) {
        TestResult result;
        result.testName = test.name;
        
        auto startTime = std::chrono::high_resolution_clock::now();
        
        try {
            test.testFunction();
            result.passed = true;
            result.errorMessage = "";
            passedTests++;
            std::cout << "[PASS] " << test.name << std::endl;
        }
        catch (const std::exception& e) {
            result.passed = false;
            result.errorMessage = e.what();
            failedTests++;
            std::cout << "[FAIL] " << test.name << " - " << e.what() << std::endl;
        }
        catch (...) {
            result.passed = false;
            result.errorMessage = "Unknown exception";
            failedTests++;
            std::cout << "[FAIL] " << test.name << " - Unknown exception" << std::endl;
        }
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
        result.executionTimeMs = duration.count() / 1000.0;
        
        results.push_back(result);
    }
}

void TestFramework::printResults() const {
    std::cout << "\n" << std::string(50, '=') << std::endl;
    std::cout << "TEST RESULTS" << std::endl;
    std::cout << std::string(50, '=') << std::endl;
    
    std::cout << "Total tests: " << totalTests << std::endl;
    std::cout << "Passed: " << passedTests << std::endl;
    std::cout << "Failed: " << failedTests << std::endl;
    std::cout << "Success rate: " << std::fixed << std::setprecision(1) 
              << (totalTests > 0 ? (static_cast<double>(passedTests) / totalTests * 100.0) : 0.0) << "%" << std::endl;
    
    if (failedTests > 0) {
        std::cout << "\nFailed tests:" << std::endl;
        for (const auto& result : results) {
            if (!result.passed) {
                std::cout << "  - " << result.testName << ": " << result.errorMessage << std::endl;
            }
        }
    }
    
    std::cout << std::string(50, '=') << std::endl;
}

bool TestFramework::allTestsPassed() const {
    return failedTests == 0;
}

void TestFramework::assertTrue(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error("Assertion failed: " + message);
    }
}

void TestFramework::assertFalse(bool condition, const std::string& message) {
    if (condition) {
        throw std::runtime_error("Assertion failed: " + message);
    }
}

void TestFramework::assertEquals(int expected, int actual, const std::string& message) {
    if (expected != actual) {
        throw std::runtime_error("Assertion failed: " + message + " (expected: " + 
                                std::to_string(expected) + ", actual: " + std::to_string(actual) + ")");
    }
}

void TestFramework::assertEquals(uint64_t expected, uint64_t actual, const std::string& message) {
    if (expected != actual) {
        throw std::runtime_error("Assertion failed: " + message + " (expected: " + 
                                std::to_string(expected) + ", actual: " + std::to_string(actual) + ")");
    }
}

void TestFramework::assertEquals(double expected, double actual, double tolerance, const std::string& message) {
    if (std::abs(expected - actual) > tolerance) {
        throw std::runtime_error("Assertion failed: " + message + " (expected: " + 
                                std::to_string(expected) + ", actual: " + std::to_string(actual) + 
                                ", tolerance: " + std::to_string(tolerance) + ")");
    }
}

void TestFramework::assertNotNull(void* ptr, const std::string& message) {
    if (ptr == nullptr) {
        throw std::runtime_error("Assertion failed: " + message);
    }
}