#pragma once

#include <iostream>
#include <string>
#include <functional>
#include <vector>
#include <chrono>

class TestFramework {
public:
    struct TestResult {
        std::string testName;
        bool passed;
        std::string errorMessage;
        double executionTimeMs;
    };

    struct TestCase {
        std::string name;
        std::function<void()> testFunction;
    };

private:
    std::vector<TestCase> tests;
    std::vector<TestResult> results;
    int totalTests;
    int passedTests;
    int failedTests;

public:
    TestFramework();
    
    void addTest(const std::string& name, std::function<void()> testFunction);
    
    void runAllTests();
    
    void printResults() const;
    
    bool allTestsPassed() const;
    
    static void assertTrue(bool condition, const std::string& message = "Assertion failed");
    static void assertFalse(bool condition, const std::string& message = "Assertion failed");
    static void assertEquals(int expected, int actual, const std::string& message = "Values not equal");
    static void assertEquals(uint64_t expected, uint64_t actual, const std::string& message = "Values not equal");
    static void assertEquals(double expected, double actual, double tolerance = 0.001, const std::string& message = "Values not equal");
    static void assertNotNull(void* ptr, const std::string& message = "Pointer is null");
};

#define TEST_CASE(framework, name, code) \
    framework.addTest(name, [&]() { code });

#define ASSERT_TRUE(condition) TestFramework::assertTrue(condition, #condition)
#define ASSERT_FALSE(condition) TestFramework::assertFalse(condition, #condition)
#define ASSERT_EQ(expected, actual) TestFramework::assertEquals(expected, actual, #expected " == " #actual)
#define ASSERT_NOT_NULL(ptr) TestFramework::assertNotNull(ptr, #ptr " is not null")