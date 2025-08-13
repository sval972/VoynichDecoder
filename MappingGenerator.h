#pragma once

#include "Mapping.h"
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <string>
#include <fstream>
#include <cstdint>

class MappingGenerator {
public:
    // Represents the current state of mapping generation
    struct GeneratorState {
        uint64_t blockIndex;           // Current block number
        uint64_t mappingIndexInBlock;  // Current position within block
        uint64_t totalMappingsGenerated; // Total mappings generated so far
        bool isComplete;               // Whether all combinations have been generated
        
        GeneratorState() : blockIndex(0), mappingIndexInBlock(0), totalMappingsGenerated(0), isComplete(false) {}
    };
    
    // Configuration for the generator
    struct GeneratorConfig {
        size_t blockSize;              // Number of mappings per block (default: 1,000,000)
        std::string stateFilePath;     // Path to state file
        bool enableStateFile;          // Whether to use persistent state
        
        GeneratorConfig() : blockSize(1000000), stateFilePath("mapping_generator_state.dat"), enableStateFile(true) {}
    };

private:
    mutable std::mutex generatorMutex; // Protects all generator operations
    
    std::vector<std::unique_ptr<Mapping>> currentBlock; // Current block of mappings
    std::atomic<size_t> currentBlockPosition;           // Current position in block
    
    GeneratorState state;              // Current generator state
    GeneratorConfig config;            // Generator configuration
    
    // Total possible combinations (27! permutations)
    static constexpr uint64_t TOTAL_COMBINATIONS = 10888869450418352160ULL; // 27!
    
    // Internal mapping generation
    void generateNextBlock();
    bool generateSingleMapping(uint64_t globalIndex, std::unique_ptr<Mapping>& mapping);
    
    // Combinatorial mathematics
    std::vector<int> indexToPermutation(uint64_t index) const;
    uint64_t factorial(int n) const;
    
    // State management
    bool loadStateFromFile();
    bool saveStateToFile() const;
    
    // Block management
    bool isCurrentBlockExhausted() const;
    void refillBlock();
    
public:
    explicit MappingGenerator(const GeneratorConfig& config = GeneratorConfig());
    ~MappingGenerator();
    
    // Get next available mapping (thread-safe)
    std::unique_ptr<Mapping> getNextMapping();
    
    // Check if generation is complete
    bool isGenerationComplete() const;
    
    // Get current statistics
    GeneratorState getCurrentState() const;
    
    // Get total possible combinations
    static uint64_t getTotalCombinations() { return TOTAL_COMBINATIONS; }
    
    // Get progress percentage
    double getProgressPercentage() const;
    
    // Reset generator to beginning (clears state file if enabled)
    void reset();
    
    // Force save current state to file
    bool saveCurrentState() const;
    
    // Get estimated remaining mappings
    uint64_t getRemainingMappings() const;
    
    // Thread-safe block status
    struct BlockStatus {
        size_t blockSize;
        size_t currentPosition;
        size_t remainingInBlock;
        uint64_t blockIndex;
    };
    
    BlockStatus getBlockStatus() const;
};