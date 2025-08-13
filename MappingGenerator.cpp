#include "MappingGenerator.h"
#include <algorithm>
#include <iostream>
#include <fstream>
#include <chrono>

MappingGenerator::MappingGenerator(const GeneratorConfig& config) 
    : config(config), currentBlockPosition(0) {
    
    // Load previous state if enabled
    if (config.enableStateFile) {
        if (!loadStateFromFile()) {
            // Initialize with default state if loading fails
            state = GeneratorState();
        }
    }
    
    // Generate initial block
    generateNextBlock();
}

MappingGenerator::~MappingGenerator() {
    // Save state on destruction
    if (config.enableStateFile) {
        saveStateToFile();
    }
}

std::unique_ptr<Mapping> MappingGenerator::getNextMapping() {
    std::lock_guard<std::mutex> lock(generatorMutex);
    
    // Check if we need to refill the block
    if (isCurrentBlockExhausted()) {
        if (state.isComplete) {
            return nullptr; // No more mappings available
        }
        
        refillBlock();
        
        // After refill, check if generation is now complete
        if (state.isComplete) {
            return nullptr;
        }
    }
    
    // Get current position and increment
    size_t position = currentBlockPosition.fetch_add(1);
    
    // Check bounds after increment
    if (position >= currentBlock.size()) {
        // Block became exhausted, return nullptr and let the caller try again
        return nullptr;
    }
    
    // Return the mapping (transfer ownership)
    return std::move(currentBlock[position]);
}

void MappingGenerator::generateNextBlock() {
    // Note: This is called from within refillBlock(), so mutex is already held
    
    currentBlock.clear();
    currentBlock.reserve(config.blockSize);
    currentBlockPosition = 0;
    
    // Calculate starting global index for this block
    uint64_t startIndex = state.blockIndex * config.blockSize;
    
    // Generate mappings for this block
    for (size_t i = 0; i < config.blockSize && startIndex + i < TOTAL_COMBINATIONS; ++i) {
        auto mapping = std::make_unique<Mapping>();
        
        if (generateSingleMapping(startIndex + i, mapping)) {
            currentBlock.push_back(std::move(mapping));
        } else {
            // Generation failed or complete
            state.isComplete = true;
            break;
        }
    }
    
    std::wcout << L"Generated block " << state.blockIndex 
               << L" with " << currentBlock.size() << L" mappings" << std::endl;
}

bool MappingGenerator::generateSingleMapping(uint64_t globalIndex, std::unique_ptr<Mapping>& mapping) {
    if (globalIndex >= TOTAL_COMBINATIONS) {
        return false;
    }
    
    // Convert global index to permutation
    std::vector<int> permutation = indexToPermutation(globalIndex);
    
    // Clear existing mapping matrix
    mapping = std::make_unique<Mapping>();
    
    // Apply permutation as EVA -> Hebrew mapping
    for (int evaIndex = 0; evaIndex < 27; ++evaIndex) {
        int hebrewIndex = permutation[evaIndex];
        mapping->setMapping(evaIndex, hebrewIndex);
    }
    
    return true;
}

std::vector<int> MappingGenerator::indexToPermutation(uint64_t index) const {
    std::vector<int> result;
    std::vector<int> available;
    
    // Initialize available Hebrew character indices (0-26)
    for (int i = 0; i < 27; ++i) {
        available.push_back(i);
    }
    
    // Convert index to permutation using factorial number system
    uint64_t remainingIndex = index;
    
    for (int position = 0; position < 27; ++position) {
        uint64_t factorial_value = factorial(26 - position);
        int choice = static_cast<int>(remainingIndex / factorial_value);
        
        // Ensure choice is within bounds
        if (choice >= static_cast<int>(available.size())) {
            choice = static_cast<int>(available.size()) - 1;
        }
        
        result.push_back(available[choice]);
        available.erase(available.begin() + choice);
        
        remainingIndex %= factorial_value;
    }
    
    return result;
}

uint64_t MappingGenerator::factorial(int n) const {
    if (n <= 1) return 1;
    
    uint64_t result = 1;
    for (int i = 2; i <= n; ++i) {
        result *= static_cast<uint64_t>(i);
    }
    return result;
}

bool MappingGenerator::loadStateFromFile() {
    std::ifstream file(config.stateFilePath, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    try {
        file.read(reinterpret_cast<char*>(&state.blockIndex), sizeof(state.blockIndex));
        file.read(reinterpret_cast<char*>(&state.mappingIndexInBlock), sizeof(state.mappingIndexInBlock));
        file.read(reinterpret_cast<char*>(&state.totalMappingsGenerated), sizeof(state.totalMappingsGenerated));
        file.read(reinterpret_cast<char*>(&state.isComplete), sizeof(state.isComplete));
        
        if (file.good()) {
            std::wcout << L"Loaded state: Block " << state.blockIndex 
                       << L", Total generated: " << state.totalMappingsGenerated << std::endl;
            return true;
        }
    } catch (const std::exception& e) {
        std::wcerr << L"Error loading state file: " << e.what() << std::endl;
    }
    
    return false;
}

bool MappingGenerator::saveStateToFile() const {
    std::ofstream file(config.stateFilePath, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    try {
        file.write(reinterpret_cast<const char*>(&state.blockIndex), sizeof(state.blockIndex));
        file.write(reinterpret_cast<const char*>(&state.mappingIndexInBlock), sizeof(state.mappingIndexInBlock));
        file.write(reinterpret_cast<const char*>(&state.totalMappingsGenerated), sizeof(state.totalMappingsGenerated));
        file.write(reinterpret_cast<const char*>(&state.isComplete), sizeof(state.isComplete));
        
        file.flush();
        return file.good();
    } catch (const std::exception& e) {
        std::wcerr << L"Error saving state file: " << e.what() << std::endl;
        return false;
    }
}


bool MappingGenerator::isCurrentBlockExhausted() const {
    return currentBlockPosition.load() >= currentBlock.size();
}

void MappingGenerator::refillBlock() {
    // Note: mutex already held by caller, update state directly
    state.totalMappingsGenerated += currentBlock.size();
    state.blockIndex++;
    state.mappingIndexInBlock = 0;
    
    // Check if we've reached the end
    if (state.blockIndex * config.blockSize >= TOTAL_COMBINATIONS) {
        state.isComplete = true;
    }
    
    // Save state after completing a block
    if (config.enableStateFile) {
        saveStateToFile();
        std::wcout << L"Saved state: Block " << state.blockIndex 
                   << L", Total generated: " << state.totalMappingsGenerated << std::endl;
    }
    
    if (!state.isComplete) {
        generateNextBlock();
    }
}

bool MappingGenerator::isGenerationComplete() const {
    std::lock_guard<std::mutex> lock(generatorMutex);
    return state.isComplete;
}

MappingGenerator::GeneratorState MappingGenerator::getCurrentState() const {
    std::lock_guard<std::mutex> lock(generatorMutex);
    return state;
}

double MappingGenerator::getProgressPercentage() const {
    std::lock_guard<std::mutex> lock(generatorMutex);
    return (static_cast<double>(state.totalMappingsGenerated) / static_cast<double>(TOTAL_COMBINATIONS)) * 100.0;
}

void MappingGenerator::reset() {
    std::lock_guard<std::mutex> lock(generatorMutex);
    
    // Reset state
    state = GeneratorState();
    currentBlockPosition = 0;
    currentBlock.clear();
    
    // Delete state file if it exists
    if (config.enableStateFile) {
        std::remove(config.stateFilePath.c_str());
    }
    
    // Generate initial block
    generateNextBlock();
}

bool MappingGenerator::saveCurrentState() const {
    std::lock_guard<std::mutex> lock(generatorMutex);
    return saveStateToFile();
}

uint64_t MappingGenerator::getRemainingMappings() const {
    std::lock_guard<std::mutex> lock(generatorMutex);
    return TOTAL_COMBINATIONS - state.totalMappingsGenerated;
}

MappingGenerator::BlockStatus MappingGenerator::getBlockStatus() const {
    std::lock_guard<std::mutex> lock(generatorMutex);
    
    BlockStatus status;
    status.blockSize = config.blockSize;
    status.currentPosition = currentBlockPosition.load();
    status.remainingInBlock = (status.currentPosition < currentBlock.size()) ? 
        (currentBlock.size() - status.currentPosition) : 0;
    status.blockIndex = state.blockIndex;
    
    return status;
}