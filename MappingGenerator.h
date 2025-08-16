#pragma once

#include "Mapping.h"
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <string>
#include <fstream>
#include <cstdint>
#include <unordered_map>
#include <deque>
#include <chrono>

// Forward declaration for JSON support
namespace nlohmann { class json; }

// Forward declaration for friend class
class VoynichDecoder;

class MappingGenerator {
    friend class VoynichDecoder;
public:
    // Block states for tracking
    enum class BlockState {
        PENDING,    // Block assigned to thread, work in progress
        COMPLETED   // Block completed by thread
    };
    
    // Block information with enhanced tracking
    struct BlockInfo {
        uint64_t blockIndex;
        BlockState state;
        int assignedThreadId;  // -1 if not assigned
        std::chrono::system_clock::time_point assignedTime;
        std::chrono::system_clock::time_point completedTime;
        
        BlockInfo() : blockIndex(0), state(BlockState::PENDING), assignedThreadId(-1) {}
        BlockInfo(uint64_t idx, int threadId) : blockIndex(idx), state(BlockState::PENDING), assignedThreadId(threadId) {
            assignedTime = std::chrono::system_clock::now();
        }
    };
    
    // Overall generator state
    struct GeneratorState {
        uint64_t nextBlockToGenerate;     // Next block index to create
        uint64_t oldestTrackedBlock;      // Lowest block index still in sliding window
        uint64_t totalBlocksGenerated;    // Total blocks created so far
        uint64_t totalBlocksCompleted;    // Total blocks completed
        bool isComplete;                  // Whether all combinations have been generated
        
        GeneratorState() : nextBlockToGenerate(0), oldestTrackedBlock(0), 
                          totalBlocksGenerated(0), totalBlocksCompleted(0), isComplete(false) {}
    };
    
    // Configuration for the generator
    struct GeneratorConfig {
        size_t blockSize;              // Number of mappings per block (default: 1,000,000)
        std::string stateFilePath;     // Path to JSON state file
        bool enableStateFile;          // Whether to use persistent state
        
        GeneratorConfig() : blockSize(1000000), stateFilePath("mapping_generator_state.json"), 
                           enableStateFile(true) {}
    };

private:
    mutable std::mutex generatorMutex; // Protects all generator operations
    
    GeneratorState state;              // Current generator state
    GeneratorConfig config;            // Generator configuration
    
    // Dynamic window of block tracking (ordered by block index)
    std::deque<BlockInfo> blockWindow;    // Blocks in memory for tracking
    std::unordered_map<int, uint64_t> threadToBlock;  // threadId -> current block index
    
    // Total possible combinations (27! permutations)
    static constexpr uint64_t TOTAL_COMBINATIONS = 10888869450418352160ULL; // 27!
    
    // Internal mapping generation
    std::vector<std::unique_ptr<Mapping>> generateBlock(uint64_t blockIndex);
    bool generateSingleMapping(uint64_t globalIndex, std::unique_ptr<Mapping>& mapping);
    
    // Combinatorial mathematics
    std::vector<int> indexToPermutation(uint64_t index) const;
    uint64_t factorial(int n) const;
    
    // JSON state management
    bool loadStateFromJson();
    bool saveStateToJson() const;
    std::string blockStateToString(BlockState state) const;
    BlockState stringToBlockState(const std::string& str) const;
    
    // Enhanced block management
    BlockInfo* findBlockInWindow(uint64_t blockIndex);
    uint64_t createNewBlockForThread(int threadId);
    void cleanupCompletedBlocks();
    void completeBlockForThread(int threadId);
    void completeCurrentBlock(int threadId);  // Friend access only
    
    // Dynamic window management
    void addBlockToWindow(uint64_t blockIndex, int threadId);
    void removeCompletedSequentialBlocks();
    void loadPendingBlocksFromState();
    void loadBlockWindowFromJson();

public:
    explicit MappingGenerator(const GeneratorConfig& config = GeneratorConfig());
    ~MappingGenerator();
    
    // Get next block for a thread (thread-safe) - returns full block
    std::vector<std::unique_ptr<Mapping>> getNextBlock(int threadId);
    
    
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
    
    // Enhanced status information
    struct BlockStatus {
        size_t blockSize;
        uint64_t nextBlockToGenerate;
        uint64_t oldestTrackedBlock;
        size_t activeBlocks;      // Blocks in PENDING state
        size_t completedBlocks;   // Total completed blocks
        size_t windowSize;        // Current window size
    };
    
    BlockStatus getBlockStatus() const;
    
    // Get current block assignment for a thread
    struct ThreadBlockInfo {
        uint64_t blockIndex;
        bool hasActiveBlock;
        BlockState blockState;
        std::chrono::system_clock::time_point assignedTime;
    };
    
    ThreadBlockInfo getThreadBlockInfo(int threadId) const;
    
    // Get all blocks in window (for debugging/monitoring)
    std::vector<BlockInfo> getWindowSnapshot() const;
    
    // Complete a block for testing purposes
    void completeBlockForTesting(int threadId);
};

// JSON serialization helpers
void to_json(nlohmann::json& j, const MappingGenerator::BlockInfo& block);
void from_json(const nlohmann::json& j, MappingGenerator::BlockInfo& block);
void to_json(nlohmann::json& j, const MappingGenerator::GeneratorState& state);
void from_json(const nlohmann::json& j, MappingGenerator::GeneratorState& state);