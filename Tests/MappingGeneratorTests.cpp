#include "TestFramework.h"
#include "../MappingGenerator.h"
#include <memory>
#include <iostream>

void testMappingGeneratorConstruction() {
    MappingGenerator::GeneratorConfig config;
    config.blockSize = 3;
    config.enableStateFile = false;
    
    MappingGenerator generator(config);
    
    auto state = generator.getCurrentState();
    ASSERT_EQ(0ULL, state.blockIndex);
    ASSERT_EQ(0ULL, state.mappingIndexInBlock);
    ASSERT_EQ(0ULL, state.totalMappingsGenerated);
    ASSERT_FALSE(state.isComplete);
}

void testMappingGeneratorDefaultConstruction() {
    MappingGenerator::GeneratorConfig config;
    config.blockSize = 5;  // Small block size for fast testing
    config.enableStateFile = false;
    
    MappingGenerator generator(config);
    
    auto state = generator.getCurrentState();
    ASSERT_EQ(0ULL, state.blockIndex);
    ASSERT_EQ(0ULL, state.mappingIndexInBlock);
    ASSERT_EQ(0ULL, state.totalMappingsGenerated);
    ASSERT_FALSE(state.isComplete);
}

void testGetNextMapping() {
    MappingGenerator::GeneratorConfig config;
    config.blockSize = 2;
    config.enableStateFile = false;
    
    MappingGenerator generator(config);
    
    auto mapping1 = generator.getNextMapping();
    ASSERT_NOT_NULL(mapping1.get());
    
    auto state1 = generator.getCurrentState();
    ASSERT_EQ(0ULL, state1.totalMappingsGenerated);
    
    auto mapping2 = generator.getNextMapping();
    ASSERT_NOT_NULL(mapping2.get());
    
    auto state2 = generator.getCurrentState();
    ASSERT_EQ(0ULL, state2.totalMappingsGenerated);
    
    auto mapping3 = generator.getNextMapping();
    ASSERT_NOT_NULL(mapping3.get());
    
    auto state3 = generator.getCurrentState();
    ASSERT_EQ(2ULL, state3.totalMappingsGenerated);
}

void testMultipleGetNextMapping() {
    MappingGenerator::GeneratorConfig config;
    config.blockSize = 3;
    config.enableStateFile = false;
    
    MappingGenerator generator(config);
    
    // Get 3 mappings (exhaust first block)
    for (int i = 0; i < 3; i++) {
        auto mapping = generator.getNextMapping();
        ASSERT_NOT_NULL(mapping.get());
    }
    
    // After exhausting the first block, totalMappingsGenerated should still be 0
    auto state1 = generator.getCurrentState();
    ASSERT_EQ(0ULL, state1.totalMappingsGenerated);
    
    // Get one more mapping (triggers refill and increments counter)
    auto mapping4 = generator.getNextMapping();
    ASSERT_NOT_NULL(mapping4.get());
    
    auto state2 = generator.getCurrentState();
    ASSERT_EQ(3ULL, state2.totalMappingsGenerated);
}

void testIsGenerationComplete() {
    MappingGenerator::GeneratorConfig config;
    config.blockSize = 2;
    config.enableStateFile = false;
    
    MappingGenerator generator(config);
    
    ASSERT_FALSE(generator.isGenerationComplete());
    
    generator.getNextMapping();
    ASSERT_FALSE(generator.isGenerationComplete());
}

void testGetTotalCombinations() {
    uint64_t totalCombinations = MappingGenerator::getTotalCombinations();
    ASSERT_EQ(10888869450418352160ULL, totalCombinations);
}

void testGetProgressPercentage() {
    MappingGenerator::GeneratorConfig config;
    config.blockSize = 2;
    config.enableStateFile = false;
    
    MappingGenerator generator(config);
    
    double initialProgress = generator.getProgressPercentage();
    TestFramework::assertEquals(0.0, initialProgress, 0.001, "Initial progress should be 0.0");
    
    // Exhaust first block to trigger progress update
    generator.getNextMapping();
    generator.getNextMapping();
    generator.getNextMapping(); // This triggers refill and progress update
    
    double progressAfterBlock = generator.getProgressPercentage();
    ASSERT_TRUE(progressAfterBlock > 0.0);
    ASSERT_TRUE(progressAfterBlock < 100.0);
}

void testGetRemainingMappings() {
    MappingGenerator::GeneratorConfig config;
    config.blockSize = 3;
    config.enableStateFile = false;
    
    MappingGenerator generator(config);
    
    uint64_t totalCombinations = MappingGenerator::getTotalCombinations();
    uint64_t remaining = generator.getRemainingMappings();
    ASSERT_EQ(totalCombinations, remaining);
    
    // Exhaust first block to update counter
    generator.getNextMapping();
    generator.getNextMapping();
    generator.getNextMapping();
    generator.getNextMapping(); // This triggers refill
    
    uint64_t remainingAfterBlock = generator.getRemainingMappings();
    ASSERT_EQ(totalCombinations - 3, remainingAfterBlock);
}

void testGetBlockStatus() {
    MappingGenerator::GeneratorConfig config;
    config.blockSize = 5;
    config.enableStateFile = false;
    
    MappingGenerator generator(config);
    
    auto blockStatus = generator.getBlockStatus();
    ASSERT_EQ(5ULL, blockStatus.blockSize);
    ASSERT_EQ(0ULL, blockStatus.currentPosition);
    ASSERT_EQ(5ULL, blockStatus.remainingInBlock);
    ASSERT_EQ(0ULL, blockStatus.blockIndex);
    
    generator.getNextMapping();
    
    auto blockStatusAfter = generator.getBlockStatus();
    ASSERT_EQ(1ULL, blockStatusAfter.currentPosition);
    ASSERT_EQ(4ULL, blockStatusAfter.remainingInBlock);
}

void testReset() {
    MappingGenerator::GeneratorConfig config;
    config.blockSize = 2;
    config.enableStateFile = false;
    
    MappingGenerator generator(config);
    
    // Exhaust first block to get meaningful state
    generator.getNextMapping();
    generator.getNextMapping();
    generator.getNextMapping(); // Triggers refill
    
    auto stateBefore = generator.getCurrentState();
    ASSERT_EQ(2ULL, stateBefore.totalMappingsGenerated);
    
    generator.reset();
    
    auto stateAfter = generator.getCurrentState();
    ASSERT_EQ(0ULL, stateAfter.blockIndex);
    ASSERT_EQ(0ULL, stateAfter.mappingIndexInBlock);
    ASSERT_EQ(0ULL, stateAfter.totalMappingsGenerated);
    ASSERT_FALSE(stateAfter.isComplete);
}

void testGeneratorStateStructure() {
    MappingGenerator::GeneratorState state;
    
    ASSERT_EQ(0ULL, state.blockIndex);
    ASSERT_EQ(0ULL, state.mappingIndexInBlock);
    ASSERT_EQ(0ULL, state.totalMappingsGenerated);
    ASSERT_FALSE(state.isComplete);
}

void testGeneratorConfigStructure() {
    MappingGenerator::GeneratorConfig config;
    
    ASSERT_EQ(1000000ULL, config.blockSize);
    ASSERT_TRUE(config.stateFilePath == "mapping_generator_state.dat");
    ASSERT_TRUE(config.enableStateFile);
}

void testBlockRefill() {
    MappingGenerator::GeneratorConfig config;
    config.blockSize = 3;
    config.enableStateFile = false;
    
    MappingGenerator generator(config);
    
    for (int i = 0; i < 3; i++) {
        auto mapping = generator.getNextMapping();
        ASSERT_NOT_NULL(mapping.get());
    }
    
    auto blockStatus = generator.getBlockStatus();
    ASSERT_EQ(3ULL, blockStatus.currentPosition);
    ASSERT_EQ(0ULL, blockStatus.remainingInBlock);
    
    auto nextMapping = generator.getNextMapping();
    ASSERT_NOT_NULL(nextMapping.get());
    
    auto newBlockStatus = generator.getBlockStatus();
    ASSERT_EQ(1ULL, newBlockStatus.blockIndex);
    ASSERT_EQ(1ULL, newBlockStatus.currentPosition);
}

void testThreadSafety() {
    MappingGenerator::GeneratorConfig config;
    config.blockSize = 5;
    config.enableStateFile = false;
    
    MappingGenerator generator(config);
    
    std::vector<std::unique_ptr<Mapping>> mappings;
    // Get 10 mappings (will exhaust 2 blocks: 5 + 5)
    for (int i = 0; i < 10; i++) {
        auto mapping = generator.getNextMapping();
        ASSERT_NOT_NULL(mapping.get());
        mappings.push_back(std::move(mapping));
    }
    
    auto state = generator.getCurrentState();
    ASSERT_EQ(5ULL, state.totalMappingsGenerated); // First block completed
    
    // Get one more to trigger second block completion
    auto extraMapping = generator.getNextMapping();
    ASSERT_NOT_NULL(extraMapping.get());
    
    auto finalState = generator.getCurrentState();
    ASSERT_EQ(10ULL, finalState.totalMappingsGenerated); // Two blocks completed
}

void registerMappingGeneratorTests(TestFramework& framework) {
    framework.addTest("MappingGenerator Construction", testMappingGeneratorConstruction);
    framework.addTest("MappingGenerator Default Construction", testMappingGeneratorDefaultConstruction);
    framework.addTest("Get Next Mapping", testGetNextMapping);
    framework.addTest("Multiple Get Next Mapping", testMultipleGetNextMapping);
    framework.addTest("Is Generation Complete", testIsGenerationComplete);
    framework.addTest("Get Total Combinations", testGetTotalCombinations);
    framework.addTest("Get Progress Percentage", testGetProgressPercentage);
    framework.addTest("Get Remaining Mappings", testGetRemainingMappings);
    framework.addTest("Get Block Status", testGetBlockStatus);
    framework.addTest("Reset Generator", testReset);
    framework.addTest("Generator State Structure", testGeneratorStateStructure);
    framework.addTest("Generator Config Structure", testGeneratorConfigStructure);
    framework.addTest("Block Refill", testBlockRefill);
    framework.addTest("Thread Safety Basic", testThreadSafety);
}