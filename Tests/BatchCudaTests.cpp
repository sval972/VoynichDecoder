#include "TestFramework.h"
#include "../StaticTranslator.h"
#include "../Mapping.h"
#include "../WordSet.h"
#include <vector>
#include <chrono>

class BatchCudaTests {
public:
    void testBatchCudaAvailability() {
        bool cudaAvailable = StaticTranslator::isCudaAvailable();
        if (cudaAvailable) {
            std::string deviceInfo = StaticTranslator::getCudaDeviceInfo();
            std::cout << "✓ CUDA is available: " << deviceInfo << std::endl;
        } else {
            std::cout << "⚠ CUDA is not available - batch CUDA tests will be skipped" << std::endl;
        }
    }
    
    void testMatrixConversionUtilities() {
        // Test public matrix conversion utilities
        WordSet testWords;
        testWords.addWord(Word(L"a", Alphabet::EVA));
        testWords.addWord(Word(L"b", Alphabet::EVA));
        testWords.addWord(Word(L"c", Alphabet::EVA));
        
        // Test wordSetToMatrix
        auto inputMatrix = StaticTranslator::wordSetToMatrix(testWords);
        ASSERT_TRUE(inputMatrix.size() == 3); // 3 words
        ASSERT_TRUE(inputMatrix[0].size() == 27); // 27 dimensions each
        
        // Test matrixToWordSet
        WordSet reconstructed = StaticTranslator::matrixToWordSet(inputMatrix, testWords, Alphabet::HEBREW);
        ASSERT_TRUE(reconstructed.size() == 3); // Should have same number of words
        
        std::cout << "✓ Matrix conversion utilities test passed" << std::endl;
    }
    
    void testBatchCudaProcessing() {
        if (!StaticTranslator::isCudaAvailable()) {
            std::cout << "⚠ Skipping batch CUDA test - CUDA not available" << std::endl;
            return;
        }
        
        // Create test data
        WordSet testWords;
        testWords.addWord(Word(L"a", Alphabet::EVA));
        testWords.addWord(Word(L"b", Alphabet::EVA));
        testWords.addWord(Word(L"c", Alphabet::EVA));
        
        // Convert to input matrix
        auto inputMatrix = StaticTranslator::wordSetToMatrix(testWords);
        
        // Create multiple test mappings
        const size_t NUM_MAPPINGS = 100; // Test with 100 mappings
        std::vector<std::vector<std::vector<int>>> transformMatrices;
        transformMatrices.reserve(NUM_MAPPINGS);
        
        for (size_t i = 0; i < NUM_MAPPINGS; ++i) {
            Mapping mapping;
            // Create different mappings by setting different character pairs
            mapping.setMapping(static_cast<int>(i % 27), static_cast<int>((i + 1) % 27));
            transformMatrices.push_back(mapping.getMappingMatrix());
        }
        
        // Test batch processing
        std::vector<std::vector<std::vector<int>>> resultMatrices;
        
        try {
            auto start = std::chrono::high_resolution_clock::now();
            StaticTranslator::performBatchMatrixMultiplicationCuda(inputMatrix, transformMatrices, resultMatrices);
            auto end = std::chrono::high_resolution_clock::now();
            
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            
            // Verify results
            ASSERT_TRUE(resultMatrices.size() == NUM_MAPPINGS);
            ASSERT_TRUE(resultMatrices[0].size() == 3); // 3 words
            ASSERT_TRUE(resultMatrices[0][0].size() == 27); // 27 dimensions
            
            std::cout << "✓ Batch CUDA processing test passed (" << NUM_MAPPINGS 
                      << " mappings in " << duration.count() << "ms)" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "✗ Batch CUDA processing failed: " << e.what() << std::endl;
            ASSERT_TRUE(false); // Fail the test
        }
    }
    
    void testBatchCudaPerformanceComparison() {
        if (!StaticTranslator::isCudaAvailable()) {
            std::cout << "⚠ Skipping batch CUDA performance test - CUDA not available" << std::endl;
            return;
        }
        
        // Create test data
        WordSet testWords;
        for (int i = 0; i < 50; ++i) { // More words for better performance testing
            testWords.addWord(Word(L"test" + std::to_wstring(i), Alphabet::EVA));
        }
        
        auto inputMatrix = StaticTranslator::wordSetToMatrix(testWords);
        
        // Create test mappings
        const size_t NUM_MAPPINGS = 1000;
        std::vector<std::vector<std::vector<int>>> transformMatrices;
        transformMatrices.reserve(NUM_MAPPINGS);
        
        for (size_t i = 0; i < NUM_MAPPINGS; ++i) {
            Mapping mapping;
            mapping.setMapping(static_cast<int>(i % 27), static_cast<int>((i + 7) % 27));
            mapping.setMapping(static_cast<int>((i + 1) % 27), static_cast<int>((i + 13) % 27));
            transformMatrices.push_back(mapping.getMappingMatrix());
        }
        
        try {
            // Test batch CUDA processing
            std::vector<std::vector<std::vector<int>>> batchResults;
            auto batchStart = std::chrono::high_resolution_clock::now();
            StaticTranslator::performBatchMatrixMultiplicationCuda(inputMatrix, transformMatrices, batchResults);
            auto batchEnd = std::chrono::high_resolution_clock::now();
            
            auto batchDuration = std::chrono::duration_cast<std::chrono::microseconds>(batchEnd - batchStart);
            
            double batchThroughput = (double)NUM_MAPPINGS / batchDuration.count() * 1000000.0; // mappings per second
            
            std::cout << "✓ Batch CUDA performance: " << NUM_MAPPINGS << " mappings in " 
                      << batchDuration.count() / 1000.0 << "ms (" 
                      << static_cast<int>(batchThroughput) << " mappings/sec)" << std::endl;
            
            ASSERT_TRUE(batchResults.size() == NUM_MAPPINGS);
            
        } catch (const std::exception& e) {
            std::cout << "✗ Batch CUDA performance test failed: " << e.what() << std::endl;
            ASSERT_TRUE(false);
        }
    }
    
    void testBatchCudaMemoryLimits() {
        if (!StaticTranslator::isCudaAvailable()) {
            std::cout << "⚠ Skipping batch CUDA memory test - CUDA not available" << std::endl;
            return;
        }
        
        // Test with larger batch to verify memory handling
        WordSet testWords;
        for (int i = 0; i < 100; ++i) { // 100 words
            testWords.addWord(Word(L"word" + std::to_wstring(i), Alphabet::EVA));
        }
        
        auto inputMatrix = StaticTranslator::wordSetToMatrix(testWords);
        
        // Create a large number of mappings to test memory limits
        const size_t LARGE_NUM_MAPPINGS = 10000; // 10K mappings
        std::vector<std::vector<std::vector<int>>> transformMatrices;
        transformMatrices.reserve(LARGE_NUM_MAPPINGS);
        
        for (size_t i = 0; i < LARGE_NUM_MAPPINGS; ++i) {
            Mapping mapping;
            // Create more complex mappings
            mapping.setMapping(static_cast<int>(i % 27), static_cast<int>((i + 3) % 27));
            mapping.setMapping(static_cast<int>((i + 5) % 27), static_cast<int>((i + 17) % 27));
            mapping.setMapping(static_cast<int>((i + 11) % 27), static_cast<int>((i + 23) % 27));
            transformMatrices.push_back(mapping.getMappingMatrix());
        }
        
        try {
            std::vector<std::vector<std::vector<int>>> results;
            auto start = std::chrono::high_resolution_clock::now();
            StaticTranslator::performBatchMatrixMultiplicationCuda(inputMatrix, transformMatrices, results);
            auto end = std::chrono::high_resolution_clock::now();
            
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            
            ASSERT_TRUE(results.size() == LARGE_NUM_MAPPINGS);
            
            double throughput = (double)LARGE_NUM_MAPPINGS / duration.count() * 1000.0;
            
            std::cout << "✓ Batch CUDA memory test passed: " << LARGE_NUM_MAPPINGS 
                      << " mappings in " << duration.count() << "ms (" 
                      << static_cast<int>(throughput) << " mappings/sec)" << std::endl;
                      
        } catch (const std::exception& e) {
            std::cout << "⚠ Batch CUDA memory test hit limits (expected): " << e.what() << std::endl;
            // This is acceptable - we're testing memory limits
        }
    }
};

void testBatchCudaAvailability() {
    BatchCudaTests tests;
    tests.testBatchCudaAvailability();
}

void testBatchCudaMatrixConversion() {
    BatchCudaTests tests;
    tests.testMatrixConversionUtilities();
}

void testBatchCudaProcessing() {
    BatchCudaTests tests;
    tests.testBatchCudaProcessing();
}

void testBatchCudaPerformance() {
    BatchCudaTests tests;
    tests.testBatchCudaPerformanceComparison();
}

void testBatchCudaMemoryLimits() {
    BatchCudaTests tests;
    tests.testBatchCudaMemoryLimits();
}

void registerBatchCudaTests(TestFramework& framework) {
    framework.addTest("Batch CUDA Availability", testBatchCudaAvailability);
    framework.addTest("Batch CUDA Matrix Conversion", testBatchCudaMatrixConversion);
    framework.addTest("Batch CUDA Processing", testBatchCudaProcessing);
    framework.addTest("Batch CUDA Performance", testBatchCudaPerformance);
    framework.addTest("Batch CUDA Memory Limits", testBatchCudaMemoryLimits);
}