#include "VoynichDecoder.h"
#include "WordSet.h"
#include <iostream>
#include <iomanip>
#include <thread>
#include <algorithm>

VoynichDecoder::VoynichDecoder(const DecoderConfig& config) : config(config), nextMappingId(0), useCudaTranslation(false) {
}

bool VoynichDecoder::initialize() {
    // Load Voynich manuscript words
    if (!loadVoynichWords()) {
        std::wcerr << L"Failed to load Voynich words from " << config.voynichWordsPath.c_str() << std::endl;
        return false;
    }
    
    std::wcout << L"Loaded " << voynichWords.size() << L" Voynich words" << std::endl;
    
    // Determine translator implementation
    useCudaTranslation = determineTranslatorImplementation(config.translatorType);
    
    std::wcout << L"Translator implementation: " << getTranslatorTypeName(config.translatorType).c_str() 
               << L" (" << (useCudaTranslation ? "CUDA (Static)" : "CPU (Static)") << L")" << std::endl;
    
    // Initialize Hebrew validator
    HebrewValidator::ValidatorConfig validatorConfig;
    validatorConfig.hebrewLexiconPath = config.hebrewLexiconPath;
    validatorConfig.scoreThreshold = config.scoreThreshold;
    validatorConfig.resultsFilePath = config.resultsFilePath;
    validatorConfig.enableResultsSaving = true;
    
    validator = std::make_unique<HebrewValidator>(validatorConfig);
    
    // Wait for Hebrew lexicon to load
    while (!validator->isLexiconReady()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    return validator->isLexiconReady();
}

VoynichDecoder::ProcessingResult VoynichDecoder::processMapping(const WordSet& voynichWords, const Mapping& mapping, bool useCuda) {
    ProcessingResult result;
    result.mappingId = nextMappingId++;
    
    // Translate Voynich words to Hebrew using static method
    WordSet translatedWords = StaticTranslator::translateWordSet(voynichWords, mapping, useCuda);
    
    // Serialize the mapping for saving
    std::string mappingVisualization = mapping.serializeMappingVisualization();
    std::vector<uint8_t> mappingData(mappingVisualization.begin(), mappingVisualization.end());
    
    // Validate translation against Hebrew lexicon
    auto validationResult = validator->validateTranslationWithMapping(translatedWords, result.mappingId, mappingData);
    
    // Fill result structure
    result.totalWords = validationResult.totalWords;
    result.matchedWords = validationResult.matchedWords;
    result.score = validationResult.score;
    result.matchPercentage = validationResult.matchPercentage;
    result.isHighScore = validationResult.isHighScore;
    
    return result;
}

VoynichDecoder::ProcessingResult VoynichDecoder::processMapping(const Mapping& mapping) {
    return processMapping(voynichWords, mapping, useCudaTranslation);
}

void VoynichDecoder::processMappings(const std::vector<std::unique_ptr<Mapping>>& mappings, 
                                   std::function<void(const ProcessingResult&)> resultCallback) {
    for (const auto& mapping : mappings) {
        auto result = processMapping(*mapping);
        resultCallback(result);
    }
}

void VoynichDecoder::processMappingBlock(MappingGenerator& generator, int threadId,
                                       std::function<void(const ProcessingResult&)> resultCallback,
                                       std::function<void(int, uint64_t, uint64_t, double, bool)> batchStatsCallback,
                                       std::function<bool()> shouldStopCallback) {
    // Get next block of mappings
    auto mappings = generator.getNextBlock(threadId);
    if (mappings.empty()) {
        return;
    }
    
    // Check for early termination before starting block processing
    if (shouldStopCallback && shouldStopCallback()) {
        // Return without completing the block - it should remain PENDING for reassignment
        return;
    }
    
    // Use batch processing for CUDA, single processing for CPU
    if (useCudaTranslation && mappings.size() > 1) {
        // Process in chunks to avoid GPU memory overflow
        const size_t CHUNK_SIZE = 10000; // Process 10K mappings at a time
        for (size_t i = 0; i < mappings.size(); i += CHUNK_SIZE) {
            if (shouldStopCallback && shouldStopCallback()) return;
            
            size_t endIdx = std::min(i + CHUNK_SIZE, mappings.size());
            std::vector<std::unique_ptr<Mapping>> chunk;
            chunk.reserve(endIdx - i);
            
            for (size_t j = i; j < endIdx; ++j) {
                chunk.push_back(std::move(mappings[j]));
            }
            
            processMappingsBatch(chunk, resultCallback, batchStatsCallback, threadId, shouldStopCallback);
            
            // Move the mappings back
            for (size_t j = 0; j < chunk.size(); ++j) {
                mappings[i + j] = std::move(chunk[j]);
            }
        }
    } else {
        // Process all mappings in the block one by one (CPU or single mapping)
        for (auto& mapping : mappings) {
            // Check if we should stop processing on every mapping for immediate response
            if (shouldStopCallback && shouldStopCallback()) {
                // Return without completing the block - it should remain PENDING for reassignment
                return;
            }
            
            auto result = processMapping(*mapping);
            
            // Update thread-local stats
            threadStats.localMappingsProcessed++;
            threadStats.localWordsValidated += result.totalWords;
            
            if (result.score > threadStats.localHighestScore) {
                threadStats.localHighestScore = result.score;
                threadStats.hasHighScore = true;
            }
            
            // Report batch stats if needed (every 1 second)
            reportBatchStatsIfNeeded(batchStatsCallback, threadId);
            
            resultCallback(result);
        }
    }
    
    // Mark block as completed
    generator.completeCurrentBlock(threadId);
}

void VoynichDecoder::processMappingsBatch(const std::vector<std::unique_ptr<Mapping>>& mappings,
                                         std::function<void(const ProcessingResult&)> resultCallback,
                                         std::function<void(int, uint64_t, uint64_t, double, bool)> batchStatsCallback,
                                         int threadId,
                                         std::function<bool()> shouldStopCallback) {
    if (mappings.empty()) return;
    
    // Check for early termination
    if (shouldStopCallback && shouldStopCallback()) {
        return;
    }
    
    // Prepare batch data for CUDA processing
    size_t numWords = voynichWords.size();
    size_t numMappings = mappings.size();
    
    // Convert word set to input matrix (same for all mappings)
    auto inputMatrix = StaticTranslator::wordSetToMatrix(voynichWords);
    
    // Convert all mappings to transform matrices
    std::vector<std::vector<std::vector<int>>> transformMatrices;
    transformMatrices.reserve(numMappings);
    
    for (const auto& mapping : mappings) {
        auto transformMatrix = mapping->getMappingMatrix();
        transformMatrices.push_back(transformMatrix);
    }
    
    // Perform batch CUDA matrix multiplication
    std::vector<std::vector<std::vector<int>>> resultMatrices;
    StaticTranslator::performBatchMatrixMultiplicationCuda(inputMatrix, transformMatrices, resultMatrices);
    
    // Process results for each mapping
    for (size_t i = 0; i < mappings.size(); ++i) {
        if (shouldStopCallback && shouldStopCallback()) return;
        
        // Convert result matrix back to WordSet
        WordSet translatedWords = StaticTranslator::matrixToWordSet(
            resultMatrices[i], voynichWords, Alphabet::HEBREW
        );
        
        // Create ProcessingResult
        ProcessingResult result;
        result.mappingId = nextMappingId++;
        
        // Serialize the mapping for saving
        std::string mappingVisualization = mappings[i]->serializeMappingVisualization();
        std::vector<uint8_t> mappingData(mappingVisualization.begin(), mappingVisualization.end());
        
        // Validate translation against Hebrew lexicon
        auto validationResult = validator->validateTranslationWithMapping(translatedWords, result.mappingId, mappingData);
        
        // Fill result structure
        result.totalWords = validationResult.totalWords;
        result.matchedWords = validationResult.matchedWords;
        result.score = validationResult.score;
        result.matchPercentage = validationResult.matchPercentage;
        result.isHighScore = validationResult.isHighScore;
        
        // Update thread-local stats
        threadStats.localMappingsProcessed++;
        threadStats.localWordsValidated += result.totalWords;
        
        if (result.score > threadStats.localHighestScore) {
            threadStats.localHighestScore = result.score;
            threadStats.hasHighScore = true;
        }
        
        // Report batch stats if needed (every 1 second)
        reportBatchStatsIfNeeded(batchStatsCallback, threadId);
        
        resultCallback(result);
    }
}

void VoynichDecoder::reportBatchStatsIfNeeded(std::function<void(int, uint64_t, uint64_t, double, bool)> batchStatsCallback, int threadId, bool force) {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - threadStats.lastReportTime).count();
    
    // Report every 1000ms (1 second) or when forced
    if (force || elapsed >= 1000) {
        if (threadStats.localMappingsProcessed > 0) {
            batchStatsCallback(threadId, threadStats.localMappingsProcessed, threadStats.localWordsValidated, 
                             threadStats.localHighestScore, threadStats.hasHighScore);
            
            // Reset thread-local counters
            threadStats.localMappingsProcessed = 0;
            threadStats.localWordsValidated = 0;
            threadStats.localHighestScore = 0.0;
            threadStats.hasHighScore = false;
            threadStats.lastReportTime = now;
        }
    }
}

bool VoynichDecoder::loadVoynichWords() {
    voynichWords.readFromFile(config.voynichWordsPath, Alphabet::EVA);
    return voynichWords.size() > 0;
}

bool VoynichDecoder::determineTranslatorImplementation(TranslatorType type) {
    switch (type) {
        case TranslatorType::CPU:
            return false;  // Use CPU
            
        case TranslatorType::CUDA:
            // Force CUDA - throw exception if not available
            if (!StaticTranslator::isCudaAvailable()) {
                throw std::runtime_error("CUDA is not available on this system");
            }
            return true;  // Use CUDA
            
        case TranslatorType::AUTO:
            // Auto-select: prefer CUDA if available, otherwise CPU
            return StaticTranslator::isCudaAvailable();
            
        default:
            return false;  // Default to CPU
    }
}

std::string VoynichDecoder::getTranslatorTypeName(TranslatorType type) const {
    switch (type) {
        case TranslatorType::CPU:
            return "CPU";
        case TranslatorType::CUDA:
            return "CUDA";
        case TranslatorType::AUTO:
            return StaticTranslator::isCudaAvailable() ? "AUTO (CUDA)" : "AUTO (CPU)";
        default:
            return "Unknown";
    }
}

void VoynichDecoder::updateScoreThreshold(double newThreshold) {
    config.scoreThreshold = newThreshold;
    if (validator) {
        validator->updateScoreThreshold(newThreshold);
    }
}