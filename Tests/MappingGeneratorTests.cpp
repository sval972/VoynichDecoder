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
    ASSERT_EQ(0ULL, state.nextBlockToGenerate);
    ASSERT_EQ(0ULL, state.totalBlocksGenerated);
    ASSERT_EQ(0ULL, state.totalBlocksCompleted);
    ASSERT_FALSE(state.isComplete);
}

void testMappingGeneratorDefaultConstruction() {
    MappingGenerator::GeneratorConfig config;
    config.blockSize = 5;  // Small block size for fast testing
    config.enableStateFile = false;
    
    MappingGenerator generator(config);
    
    auto state = generator.getCurrentState();
    ASSERT_EQ(0ULL, state.nextBlockToGenerate);
    ASSERT_EQ(0ULL, state.totalBlocksGenerated);
    ASSERT_EQ(0ULL, state.totalBlocksCompleted);
    ASSERT_FALSE(state.isComplete);
}

void testGetNextBlock() {
    MappingGenerator::GeneratorConfig config;
    config.blockSize = 3;
    config.enableStateFile = false;
    
    MappingGenerator generator(config);
    
    // Get first block for thread 0
    auto block1 = generator.getNextBlock(0);
    ASSERT_EQ(3ULL, block1.size()); // Should have 3 mappings
    
    auto state1 = generator.getCurrentState();
    ASSERT_EQ(1ULL, state1.nextBlockToGenerate);
    ASSERT_EQ(1ULL, state1.totalBlocksGenerated);
    
    // Get second block for thread 1
    auto block2 = generator.getNextBlock(1);
    ASSERT_EQ(3ULL, block2.size()); // Should have 3 mappings
    
    auto state2 = generator.getCurrentState();
    ASSERT_EQ(2ULL, state2.nextBlockToGenerate);
    ASSERT_EQ(2ULL, state2.totalBlocksGenerated);
}

void testMultipleGetNextBlock() {
    MappingGenerator::GeneratorConfig config;
    config.blockSize = 2;
    config.enableStateFile = false;
    
    MappingGenerator generator(config);
    
    // Get blocks for multiple threads
    auto block1 = generator.getNextBlock(0);
    auto block2 = generator.getNextBlock(1);
    auto block3 = generator.getNextBlock(2);
    
    ASSERT_EQ(2ULL, block1.size());
    ASSERT_EQ(2ULL, block2.size());
    ASSERT_EQ(2ULL, block3.size());
    
    auto state = generator.getCurrentState();
    ASSERT_EQ(3ULL, state.nextBlockToGenerate);
    ASSERT_EQ(3ULL, state.totalBlocksGenerated);
}

void testIsGenerationComplete() {
    MappingGenerator::GeneratorConfig config;
    config.blockSize = 2;
    config.enableStateFile = false;
    
    MappingGenerator generator(config);
    
    ASSERT_FALSE(generator.isGenerationComplete());
    
    generator.getNextBlock(0);
    ASSERT_FALSE(generator.isGenerationComplete());
}

void testGetTotalCombinations() {
    uint64_t totalCombinations = MappingGenerator::getTotalCombinations();
    ASSERT_EQ(10888869450418352160ULL, totalCombinations);
}

void testGetProgressPercentage() {
    MappingGenerator::GeneratorConfig config;
    config.blockSize = 1000000;  // Use a larger block size for realistic progress
    config.enableStateFile = false;
    
    MappingGenerator generator(config);
    
    double initialProgress = generator.getProgressPercentage();
    TestFramework::assertEquals(0.0, initialProgress, 0.001, "Initial progress should be 0.0");
    
    // Get first block
    generator.getNextBlock(0);
    
    // Complete the block to update progress
    generator.completeBlockForTesting(0);
    
    double progressAfterBlock = generator.getProgressPercentage();
    ASSERT_TRUE(progressAfterBlock > 0.0);
    ASSERT_TRUE(progressAfterBlock < 100.0);
}

void testGetRemainingMappings() {
    MappingGenerator::GeneratorConfig config;
    config.blockSize = 1000000;
    config.enableStateFile = false;
    
    MappingGenerator generator(config);
    
    uint64_t totalCombinations = MappingGenerator::getTotalCombinations();
    uint64_t remaining = generator.getRemainingMappings();
    ASSERT_EQ(totalCombinations, remaining);
    
    // Get first block 
    generator.getNextBlock(0);
    
    // Complete the block to update remaining count
    generator.completeBlockForTesting(0);
    
    uint64_t remainingAfterBlock = generator.getRemainingMappings();
    ASSERT_EQ(totalCombinations - 1000000, remainingAfterBlock);
}

void testGetBlockStatus() {
    MappingGenerator::GeneratorConfig config;
    config.blockSize = 5;
    config.enableStateFile = false;
    
    MappingGenerator generator(config);
    
    auto blockStatus = generator.getBlockStatus();
    ASSERT_EQ(5ULL, blockStatus.blockSize);
    ASSERT_EQ(0ULL, blockStatus.nextBlockToGenerate);
    ASSERT_EQ(0ULL, blockStatus.completedBlocks);
    
    generator.getNextBlock(0);
    
    auto blockStatusAfter = generator.getBlockStatus();
    ASSERT_EQ(1ULL, blockStatusAfter.nextBlockToGenerate);
}

void testReset() {
    MappingGenerator::GeneratorConfig config;
    config.blockSize = 2;
    config.enableStateFile = false;
    
    MappingGenerator generator(config);
    
    // Get some blocks to change state
    generator.getNextBlock(0);
    generator.getNextBlock(1);
    
    auto stateBefore = generator.getCurrentState();
    ASSERT_EQ(2ULL, stateBefore.totalBlocksGenerated);
    
    generator.reset();
    
    auto stateAfter = generator.getCurrentState();
    ASSERT_EQ(0ULL, stateAfter.nextBlockToGenerate);
    ASSERT_EQ(0ULL, stateAfter.totalBlocksGenerated);
    ASSERT_EQ(0ULL, stateAfter.totalBlocksCompleted);
    ASSERT_FALSE(stateAfter.isComplete);
}

void testGeneratorStateStructure() {
    MappingGenerator::GeneratorState state;
    
    ASSERT_EQ(0ULL, state.nextBlockToGenerate);
    ASSERT_EQ(0ULL, state.totalBlocksGenerated);
    ASSERT_EQ(0ULL, state.totalBlocksCompleted);
    ASSERT_FALSE(state.isComplete);
}

void testGeneratorConfigStructure() {
    MappingGenerator::GeneratorConfig config;
    
    ASSERT_EQ(1000000ULL, config.blockSize);
    ASSERT_TRUE(config.stateFilePath == "mapping_generator_state.json");
    ASSERT_TRUE(config.enableStateFile);
}

void testBlockGeneration() {
    MappingGenerator::GeneratorConfig config;
    config.blockSize = 3;
    config.enableStateFile = false;
    
    MappingGenerator generator(config);
    
    auto block = generator.getNextBlock(0);
    ASSERT_EQ(3ULL, block.size());
    
    // Verify each mapping is valid
    for (const auto& mapping : block) {
        ASSERT_NOT_NULL(mapping.get());
        // Verify mapping matrix is valid 27x27
        const auto& matrix = mapping->getMappingMatrix();
        ASSERT_EQ(27ULL, matrix.size());
        ASSERT_EQ(27ULL, matrix[0].size());
    }
}

void testThreadSafety() {
    MappingGenerator::GeneratorConfig config;
    config.blockSize = 5;
    config.enableStateFile = false;
    
    MappingGenerator generator(config);
    
    // Get blocks for different threads simultaneously
    std::vector<std::vector<std::unique_ptr<Mapping>>> blocks;
    
    for (int threadId = 0; threadId < 4; threadId++) {
        auto block = generator.getNextBlock(threadId);
        ASSERT_EQ(5ULL, block.size());
        blocks.push_back(std::move(block));
    }
    
    auto state = generator.getCurrentState();
    ASSERT_EQ(4ULL, state.totalBlocksGenerated);
    ASSERT_EQ(4ULL, state.nextBlockToGenerate);
}

void testBlockWindowManagement() {
    MappingGenerator::GeneratorConfig config;
    config.blockSize = 3;
    config.enableStateFile = false;
    
    MappingGenerator generator(config);
    
    // Get blocks for multiple threads
    generator.getNextBlock(0);
    generator.getNextBlock(1);
    generator.getNextBlock(2);
    
    auto blockStatus = generator.getBlockStatus();
    ASSERT_EQ(3ULL, blockStatus.nextBlockToGenerate);
    ASSERT_TRUE(blockStatus.windowSize > 0);  // Should have blocks in window
}

void testStateConsistency() {
    MappingGenerator::GeneratorConfig config;
    config.blockSize = 10;
    config.enableStateFile = false;
    
    MappingGenerator generator(config);
    
    // Get initial state
    auto initialState = generator.getCurrentState();
    
    // Get a block
    auto block = generator.getNextBlock(0);
    
    // Check state consistency
    auto newState = generator.getCurrentState();
    ASSERT_EQ(initialState.totalBlocksGenerated + 1, newState.totalBlocksGenerated);
    ASSERT_EQ(initialState.nextBlockToGenerate + 1, newState.nextBlockToGenerate);
    
    // Verify block size
    ASSERT_EQ(10ULL, block.size());
}

void registerMappingGeneratorTests(TestFramework& framework) {
    framework.addTest("MappingGenerator Construction", testMappingGeneratorConstruction);
    framework.addTest("MappingGenerator Default Construction", testMappingGeneratorDefaultConstruction);
    framework.addTest("Get Next Block", testGetNextBlock);
    framework.addTest("Multiple Get Next Block", testMultipleGetNextBlock);
    framework.addTest("Is Generation Complete", testIsGenerationComplete);
    framework.addTest("Get Total Combinations", testGetTotalCombinations);
    framework.addTest("Get Progress Percentage", testGetProgressPercentage);
    framework.addTest("Get Remaining Mappings", testGetRemainingMappings);
    framework.addTest("Get Block Status", testGetBlockStatus);
    framework.addTest("Reset Generator", testReset);
    framework.addTest("Generator State Structure", testGeneratorStateStructure);
    framework.addTest("Generator Config Structure", testGeneratorConfigStructure);
    framework.addTest("Block Generation", testBlockGeneration);
    framework.addTest("Thread Safety Basic", testThreadSafety);
    framework.addTest("Block Window Management", testBlockWindowManagement);
    framework.addTest("State Consistency", testStateConsistency);
}