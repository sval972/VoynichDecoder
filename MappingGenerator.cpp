#include "MappingGenerator.h"
#include <algorithm>
#include <iostream>
#include <fstream>
#include <chrono>
#include <iomanip>

// Include nlohmann/json - assume it's available in project
#ifdef _WIN32
    #pragma warning(push)
    #pragma warning(disable: 4996) // Disable deprecation warnings for JSON
#endif

// For now, let's implement a simple JSON parser manually to avoid dependencies
// This would normally use nlohmann/json library
#include <sstream>

namespace {
    // Simple JSON serialization helpers (would normally use nlohmann/json)
    std::string timePointToString(const std::chrono::system_clock::time_point& tp) {
        auto time_t = std::chrono::system_clock::to_time_t(tp);
        std::stringstream ss;
        ss << time_t;
        return ss.str();
    }
    
    std::chrono::system_clock::time_point stringToTimePoint(const std::string& str) {
        if (str.empty()) return std::chrono::system_clock::time_point{};
        std::stringstream ss(str);
        std::time_t time_t;
        ss >> time_t;
        return std::chrono::system_clock::from_time_t(time_t);
    }
}

MappingGenerator::MappingGenerator(const GeneratorConfig& config) 
    : config(config) {
    
    // Load previous state if enabled
    if (config.enableStateFile) {
        if (!loadStateFromJson()) {
            // Initialize with default state if loading fails
            state = GeneratorState();
            std::wcout << L"Starting with clean generator state" << std::endl;
        } else {
            std::wcout << L"Loaded generator state: Block " << state.nextBlockToGenerate 
                       << L", Total generated: " << state.totalBlocksGenerated 
                       << L", Completed: " << state.totalBlocksCompleted << std::endl;
        }
    }
    
    // Load pending blocks from state file if enabled
    if (config.enableStateFile) {
        loadPendingBlocksFromState();
    }
}

MappingGenerator::~MappingGenerator() {
    // Save state on destruction
    if (config.enableStateFile) {
        saveStateToJson();
    }
}

std::vector<std::unique_ptr<Mapping>> MappingGenerator::getNextBlock(int threadId) {
    std::lock_guard<std::mutex> lock(generatorMutex);
    
    // Check if this thread already has an active block
    auto threadIt = threadToBlock.find(threadId);
    if (threadIt != threadToBlock.end()) {
        // Thread already has a block, mark the previous one as completed first
        completeBlockForThread(threadId);
    }
    
    if (state.isComplete) {
        return {}; // No more blocks available
    }
    
    // First, look for existing pending blocks that aren't assigned
    for (auto& block : blockWindow) {
        if (block.state == BlockState::PENDING && block.assignedThreadId == -1) {
            // Assign this pending block to the thread
            block.assignedThreadId = threadId;
            block.assignedTime = std::chrono::system_clock::now();
            threadToBlock[threadId] = block.blockIndex;
            
            std::wcout << L"[BLOCK] Reassigned pending block " << block.blockIndex << L" to thread " << threadId << std::endl;
            
            // Save state after reassignment
            if (config.enableStateFile) {
                saveStateToJson();
            }
            
            // Generate the block mappings
            return generateBlock(block.blockIndex);
        }
    }
    
    // No pending blocks available, create a new block for this thread
    uint64_t newBlockIndex = createNewBlockForThread(threadId);
    if (newBlockIndex == UINT64_MAX) {
        return {}; // No more blocks can be created
    }
    
    // Generate the block mappings
    return generateBlock(newBlockIndex);
}

void MappingGenerator::completeCurrentBlock(int threadId) {
    std::lock_guard<std::mutex> lock(generatorMutex);
    completeBlockForThread(threadId);
    
    // Clean up completed blocks from the beginning of the sequence
    removeCompletedSequentialBlocks();
    
    // Save state after completion
    if (config.enableStateFile) {
        saveStateToJson();
    }
}


void MappingGenerator::completeBlockForThread(int threadId) {
    auto threadIt = threadToBlock.find(threadId);
    if (threadIt == threadToBlock.end()) {
        return; // Thread has no active block
    }
    
    uint64_t blockIndex = threadIt->second;
    BlockInfo* block = findBlockInWindow(blockIndex);
    if (block && block->state == BlockState::PENDING && block->assignedThreadId == threadId) {
        // Mark block as completed
        block->state = BlockState::COMPLETED;
        block->completedTime = std::chrono::system_clock::now();
        state.totalBlocksCompleted++;
        
        // Log block completion
        std::wcout << L"[BLOCK] Completed block " << blockIndex << L" by thread " << threadId 
                   << L" (Total completed: " << state.totalBlocksCompleted << L")" << std::endl;
        
        // Save state immediately after block completion
        if (config.enableStateFile) {
            saveStateToJson();
        }
    }
    
    // Remove thread mapping
    threadToBlock.erase(threadIt);
}

MappingGenerator::BlockInfo* MappingGenerator::findBlockInWindow(uint64_t blockIndex) {
    for (auto& block : blockWindow) {
        if (block.blockIndex == blockIndex) {
            return &block;
        }
    }
    return nullptr;
}

uint64_t MappingGenerator::createNewBlockForThread(int threadId) {
    // Check if we've reached the total combinations limit
    uint64_t totalMappingsGenerated = state.nextBlockToGenerate * config.blockSize;
    if (totalMappingsGenerated >= TOTAL_COMBINATIONS) {
        state.isComplete = true;
        return UINT64_MAX;
    }
    
    
    uint64_t newBlockIndex = state.nextBlockToGenerate;
    
    // Create new block info and assign to thread immediately as PENDING
    addBlockToWindow(newBlockIndex, threadId);
    
    state.nextBlockToGenerate++;
    state.totalBlocksGenerated++;
    
    // Save state immediately after block allocation
    if (config.enableStateFile) {
        saveStateToJson();
    }
    
    return newBlockIndex;
}

void MappingGenerator::addBlockToWindow(uint64_t blockIndex, int threadId) {
    // Check if block is already being tracked
    if (findBlockInWindow(blockIndex)) {
        return;
    }
    
    // Create new block info assigned as PENDING to thread
    BlockInfo newBlock(blockIndex, threadId);
    
    // Insert in sorted order (should be at the end for sequential generation)
    blockWindow.push_back(newBlock);
    
    // Update thread mapping
    threadToBlock[threadId] = blockIndex;
    
    // Log block allocation
    std::wcout << L"[BLOCK] Allocated block " << blockIndex << L" to thread " << threadId << std::endl;
}

void MappingGenerator::removeCompletedSequentialBlocks() {
    // Remove completed blocks from the beginning of the sequence only
    size_t removedCount = 0;
    while (!blockWindow.empty() && 
           blockWindow.front().state == BlockState::COMPLETED &&
           blockWindow.front().blockIndex == state.oldestTrackedBlock) {
        
        uint64_t removedBlockIndex = blockWindow.front().blockIndex;
        blockWindow.pop_front();
        state.oldestTrackedBlock++;
        removedCount++;
    }
    
    if (removedCount > 0) {
        std::wcout << L"[BLOCK] Cleaned up " << removedCount << L" completed sequential blocks, "
                   << L"oldest tracked now: " << state.oldestTrackedBlock << std::endl;
        
        // Save state immediately after cleanup
        if (config.enableStateFile) {
            saveStateToJson();
        }
    }
}

std::vector<std::unique_ptr<Mapping>> MappingGenerator::generateBlock(uint64_t blockIndex) {
    std::vector<std::unique_ptr<Mapping>> block;
    block.reserve(config.blockSize);
    
    uint64_t startIndex = blockIndex * config.blockSize;
    uint64_t endIndex = startIndex + config.blockSize;
    
    // Make sure we don't exceed total combinations
    if (endIndex > TOTAL_COMBINATIONS) {
        endIndex = TOTAL_COMBINATIONS;
    }
    
    for (uint64_t i = startIndex; i < endIndex; ++i) {
        std::unique_ptr<Mapping> mapping;
        if (generateSingleMapping(i, mapping)) {
            block.push_back(std::move(mapping));
        }
    }
    
    return block;
}

bool MappingGenerator::generateSingleMapping(uint64_t globalIndex, std::unique_ptr<Mapping>& mapping) {
    if (globalIndex >= TOTAL_COMBINATIONS) {
        return false;
    }
    
    // Convert global index to permutation
    std::vector<int> permutation = indexToPermutation(globalIndex);
    
    // Create mapping from permutation
    mapping = std::make_unique<Mapping>();
    
    // Map EVA alphabet to Hebrew letters based on permutation
    const std::vector<wchar_t> evaAlphabet = {
        L'a', L'b', L'c', L'd', L'e', L'f', L'g', L'h', L'i', L'j', L'k', L'l', L'm',
        L'n', L'o', L'p', L'q', L'r', L's', L't', L'u', L'v', L'w', L'x', L'y', L'z', L' '
    };
    
    const std::vector<wchar_t> hebrewAlphabet = {
        0x05D0, 0x05D1, 0x05D2, 0x05D3, 0x05D4, 0x05D5, 0x05D6, 0x05D7, 0x05D8,
        0x05D9, 0x05DB, 0x05DC, 0x05DE, 0x05E0, 0x05E1, 0x05E2, 0x05E4, 0x05E6,
        0x05E7, 0x05E8, 0x05E9, 0x05EA, 0x05DA, 0x05DD, 0x05DF, 0x05E3, 0x05E5
    };
    
    // Build the mapping based on permutation
    for (size_t i = 0; i < evaAlphabet.size() && i < permutation.size(); ++i) {
        if (permutation[i] < static_cast<int>(hebrewAlphabet.size())) {
            mapping->setMapping(static_cast<int>(i), permutation[i]);
        }
    }
    
    return true;
}

std::vector<int> MappingGenerator::indexToPermutation(uint64_t index) const {
    std::vector<int> result;
    std::vector<int> available;
    
    // Initialize available numbers (0 to 26 for 27 characters)
    for (int i = 0; i < 27; ++i) {
        available.push_back(i);
    }
    
    // Convert index to permutation using factorial number system
    uint64_t remaining = index;
    for (int position = 27; position > 0; --position) {
        uint64_t factorial_val = factorial(position - 1);
        int chosen_index = static_cast<int>(remaining / factorial_val);
        
        if (chosen_index >= static_cast<int>(available.size())) {
            chosen_index = available.size() - 1;
        }
        
        result.push_back(available[chosen_index]);
        available.erase(available.begin() + chosen_index);
        remaining %= factorial_val;
    }
    
    return result;
}

uint64_t MappingGenerator::factorial(int n) const {
    if (n <= 1) return 1;
    uint64_t result = 1;
    for (int i = 2; i <= n; ++i) {
        result *= i;
    }
    return result;
}

bool MappingGenerator::isGenerationComplete() const {
    std::lock_guard<std::mutex> lock(generatorMutex);
    return state.isComplete && blockWindow.empty();
}

MappingGenerator::GeneratorState MappingGenerator::getCurrentState() const {
    std::lock_guard<std::mutex> lock(generatorMutex);
    return state;
}

double MappingGenerator::getProgressPercentage() const {
    std::lock_guard<std::mutex> lock(generatorMutex);
    if (TOTAL_COMBINATIONS == 0) return 100.0;
    
    uint64_t processedMappings = state.totalBlocksCompleted * config.blockSize;
    return (static_cast<double>(processedMappings) / TOTAL_COMBINATIONS) * 100.0;
}

void MappingGenerator::reset() {
    std::lock_guard<std::mutex> lock(generatorMutex);
    
    // Clear all state
    state = GeneratorState();
    blockWindow.clear();
    threadToBlock.clear();
    
    // Remove state file if it exists
    if (config.enableStateFile) {
        std::remove(config.stateFilePath.c_str());
    }
}

bool MappingGenerator::saveCurrentState() const {
    std::lock_guard<std::mutex> lock(generatorMutex);
    return saveStateToJson();
}

uint64_t MappingGenerator::getRemainingMappings() const {
    std::lock_guard<std::mutex> lock(generatorMutex);
    uint64_t processedMappings = state.totalBlocksCompleted * config.blockSize;
    return (processedMappings < TOTAL_COMBINATIONS) ? (TOTAL_COMBINATIONS - processedMappings) : 0;
}

MappingGenerator::BlockStatus MappingGenerator::getBlockStatus() const {
    std::lock_guard<std::mutex> lock(generatorMutex);
    
    BlockStatus status;
    status.blockSize = config.blockSize;
    status.nextBlockToGenerate = state.nextBlockToGenerate;
    status.oldestTrackedBlock = state.oldestTrackedBlock;
    status.completedBlocks = state.totalBlocksCompleted;
    status.windowSize = blockWindow.size();
    
    // Count active (pending) blocks
    status.activeBlocks = 0;
    for (const auto& block : blockWindow) {
        if (block.state == BlockState::PENDING) {
            status.activeBlocks++;
        }
    }
    
    return status;
}

MappingGenerator::ThreadBlockInfo MappingGenerator::getThreadBlockInfo(int threadId) const {
    std::lock_guard<std::mutex> lock(generatorMutex);
    
    ThreadBlockInfo info;
    info.hasActiveBlock = false;
    info.blockIndex = 0;
    info.blockState = BlockState::PENDING;
    
    auto threadIt = threadToBlock.find(threadId);
    if (threadIt != threadToBlock.end()) {
        uint64_t blockIndex = threadIt->second;
        const BlockInfo* block = const_cast<MappingGenerator*>(this)->findBlockInWindow(blockIndex);
        if (block) {
            info.hasActiveBlock = true;
            info.blockIndex = blockIndex;
            info.blockState = block->state;
            info.assignedTime = block->assignedTime;
        }
    }
    
    return info;
}

std::vector<MappingGenerator::BlockInfo> MappingGenerator::getWindowSnapshot() const {
    std::lock_guard<std::mutex> lock(generatorMutex);
    return std::vector<BlockInfo>(blockWindow.begin(), blockWindow.end());
}

void MappingGenerator::completeBlockForTesting(int threadId) {
    completeCurrentBlock(threadId);
}

std::string MappingGenerator::blockStateToString(BlockState state) const {
    switch (state) {
        case BlockState::PENDING: return "PENDING";
        case BlockState::COMPLETED: return "COMPLETED";
        default: return "UNKNOWN";
    }
}

MappingGenerator::BlockState MappingGenerator::stringToBlockState(const std::string& str) const {
    if (str == "PENDING") return BlockState::PENDING;
    if (str == "COMPLETED") return BlockState::COMPLETED;
    return BlockState::PENDING;
}

// Simple JSON implementation for now (would normally use nlohmann/json)
bool MappingGenerator::loadStateFromJson() {
    std::ifstream file(config.stateFilePath);
    if (!file.is_open()) {
        return false;
    }
    
    // Simple JSON parsing for our specific structure
    std::string line;
    bool inBlocksArray = false;
    
    while (std::getline(file, line)) {
        // Remove whitespace
        line.erase(std::remove_if(line.begin(), line.end(), ::isspace), line.end());
        
        if (line.find("\"nextBlockToGenerate\":") != std::string::npos) {
            size_t pos = line.find(':') + 1;
            size_t end = line.find(',', pos);
            if (end == std::string::npos) end = line.find('}', pos);
            state.nextBlockToGenerate = std::stoull(line.substr(pos, end - pos));
        }
        else if (line.find("\"oldestTrackedBlock\":") != std::string::npos) {
            size_t pos = line.find(':') + 1;
            size_t end = line.find(',', pos);
            if (end == std::string::npos) end = line.find('}', pos);
            state.oldestTrackedBlock = std::stoull(line.substr(pos, end - pos));
        }
        else if (line.find("\"totalBlocksGenerated\":") != std::string::npos) {
            size_t pos = line.find(':') + 1;
            size_t end = line.find(',', pos);
            if (end == std::string::npos) end = line.find('}', pos);
            state.totalBlocksGenerated = std::stoull(line.substr(pos, end - pos));
        }
        else if (line.find("\"totalBlocksCompleted\":") != std::string::npos) {
            size_t pos = line.find(':') + 1;
            size_t end = line.find(',', pos);
            if (end == std::string::npos) end = line.find('}', pos);
            state.totalBlocksCompleted = std::stoull(line.substr(pos, end - pos));
        }
        else if (line.find("\"isComplete\":") != std::string::npos) {
            state.isComplete = line.find("true") != std::string::npos;
        }
    }
    
    file.close();
    
    // Parse and restore block window from JSON
    loadBlockWindowFromJson();
    
    return true;
}

bool MappingGenerator::saveStateToJson() const {
    std::ofstream file(config.stateFilePath);
    if (!file.is_open()) {
        return false;
    }
    
    // Write JSON manually (would normally use nlohmann/json)
    file << "{\n";
    file << "  \"generator_state\": {\n";
    file << "    \"nextBlockToGenerate\": " << state.nextBlockToGenerate << ",\n";
    file << "    \"oldestTrackedBlock\": " << state.oldestTrackedBlock << ",\n";
    file << "    \"totalBlocksGenerated\": " << state.totalBlocksGenerated << ",\n";
    file << "    \"totalBlocksCompleted\": " << state.totalBlocksCompleted << ",\n";
    file << "    \"isComplete\": " << (state.isComplete ? "true" : "false") << "\n";
    file << "  },\n";
    
    file << "  \"block_window\": [\n";
    for (size_t i = 0; i < blockWindow.size(); ++i) {
        const auto& block = blockWindow[i];
        file << "    {\n";
        file << "      \"blockIndex\": " << block.blockIndex << ",\n";
        file << "      \"state\": \"" << blockStateToString(block.state) << "\",\n";
        file << "      \"assignedThreadId\": " << block.assignedThreadId << ",\n";
        file << "      \"assignedTime\": \"" << timePointToString(block.assignedTime) << "\",\n";
        file << "      \"completedTime\": \"" << timePointToString(block.completedTime) << "\"\n";
        file << "    }" << (i == blockWindow.size() - 1 ? "\n" : ",\n");
    }
    file << "  ],\n";
    
    file << "  \"config\": {\n";
    file << "    \"blockSize\": " << config.blockSize << "\n";
    file << "  }\n";
    file << "}\n";
    
    file.close();
    return true;
}

void MappingGenerator::loadPendingBlocksFromState() {
    // Reset pending blocks to be unassigned so they can be served to new threads
    for (auto& block : blockWindow) {
        if (block.state == BlockState::PENDING) {
            // Reset the assignment but keep it as pending
            block.assignedThreadId = -1;
            block.assignedTime = std::chrono::system_clock::time_point{};
            std::wcout << L"[BLOCK] Reset pending block " << block.blockIndex << L" for reassignment" << std::endl;
        }
    }
    
    // Clear thread mappings since we're resetting pending blocks
    threadToBlock.clear();
}

void MappingGenerator::loadBlockWindowFromJson() {
    std::ifstream file(config.stateFilePath);
    if (!file.is_open()) {
        return;
    }
    
    blockWindow.clear();
    threadToBlock.clear();
    
    // Simple JSON parsing for block window
    std::string line;
    bool inBlockWindow = false;
    bool inBlockObject = false;
    BlockInfo currentBlock;
    
    while (std::getline(file, line)) {
        // Remove whitespace
        line.erase(std::remove_if(line.begin(), line.end(), ::isspace), line.end());
        
        if (line.find("\"block_window\":") != std::string::npos) {
            inBlockWindow = true;
            continue;
        }
        
        if (inBlockWindow && line == "{") {
            inBlockObject = true;
            currentBlock = BlockInfo(); // Reset current block
            continue;
        }
        
        if (inBlockObject && line.find("\"blockIndex\":") != std::string::npos) {
            size_t pos = line.find(':') + 1;
            size_t end = line.find(',', pos);
            if (end == std::string::npos) end = line.find('}', pos);
            currentBlock.blockIndex = std::stoull(line.substr(pos, end - pos));
        }
        else if (inBlockObject && line.find("\"state\":") != std::string::npos) {
            if (line.find("\"PENDING\"") != std::string::npos) {
                currentBlock.state = BlockState::PENDING;
            } else if (line.find("\"COMPLETED\"") != std::string::npos) {
                currentBlock.state = BlockState::COMPLETED;
            }
        }
        else if (inBlockObject && line.find("\"assignedThreadId\":") != std::string::npos) {
            size_t pos = line.find(':') + 1;
            size_t end = line.find(',', pos);
            if (end == std::string::npos) end = line.find('}', pos);
            currentBlock.assignedThreadId = std::stoi(line.substr(pos, end - pos));
        }
        else if (inBlockObject && line.find("\"assignedTime\":") != std::string::npos) {
            size_t pos = line.find('"', line.find(':')) + 1;
            size_t end = line.find('"', pos);
            currentBlock.assignedTime = stringToTimePoint(line.substr(pos, end - pos));
        }
        else if (inBlockObject && line.find("\"completedTime\":") != std::string::npos) {
            size_t pos = line.find('"', line.find(':')) + 1;
            size_t end = line.find('"', pos);
            currentBlock.completedTime = stringToTimePoint(line.substr(pos, end - pos));
        }
        
        if (inBlockObject && (line == "}" || line == "},")) {
            // Add the completed block to window
            blockWindow.push_back(currentBlock);
            inBlockObject = false;
        }
        
        if (inBlockWindow && line == "],") {
            inBlockWindow = false;
            break;
        }
    }
    
    file.close();
    
    std::wcout << L"[BLOCK] Loaded " << blockWindow.size() << L" blocks from state file" << std::endl;
}

#ifdef _WIN32
    #pragma warning(pop)
#endif