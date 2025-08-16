#pragma once

#include "StaticTranslator.h"
#include "HebrewValidator.h"
#include "WordSet.h"
#include "Mapping.h"
#include "MappingGenerator.h"
#include <vector>
#include <chrono>
#include <memory>
#include <functional>

class VoynichDecoder {
public:
    // Translator implementation types
    enum class TranslatorType {
        CPU,    // Use CPU-based implementation
        CUDA,   // Use CUDA GPU implementation (falls back to CPU if unavailable)
        AUTO    // Automatically choose best available (CUDA if available, otherwise CPU)
    };
    
    // Configuration for the decoder
    struct DecoderConfig {
        std::string hebrewLexiconPath;        // Path to Hebrew lexicon
        std::string voynichWordsPath;         // Path to Voynich manuscript words
        std::string resultsFilePath;          // Path to save results
        double scoreThreshold;                // Minimum score to save results
        TranslatorType translatorType;        // Type of translator implementation to use
        
        DecoderConfig() :
            hebrewLexiconPath("resources/Tanah2.txt"),
            voynichWordsPath("resources/Script_freq100.txt"),
            resultsFilePath("voynich_decoder_results.txt"),
            scoreThreshold(25.0),
            translatorType(TranslatorType::AUTO) {}
    };
    
    // Processing result structure
    struct ProcessingResult {
        uint64_t mappingId;
        size_t totalWords;
        size_t matchedWords;
        double score;
        double matchPercentage;
        bool isHighScore;
        
        ProcessingResult() : mappingId(0), totalWords(0), matchedWords(0), 
                           score(0.0), matchPercentage(0.0), isHighScore(false) {}
    };

private:
    DecoderConfig config;
    
    // Core components
    std::unique_ptr<HebrewValidator> validator;
    WordSet voynichWords;
    uint64_t nextMappingId;
    bool useCudaTranslation;
    
    // Thread-local performance tracking (to minimize StatsProvider contention)
    struct ThreadStats {
        uint64_t localMappingsProcessed = 0;
        uint64_t localWordsValidated = 0;
        double localHighestScore = 0.0;
        bool hasHighScore = false;
        std::chrono::steady_clock::time_point lastReportTime;
        
        ThreadStats() : lastReportTime(std::chrono::steady_clock::now()) {}
    };
    ThreadStats threadStats;
    
    // Private helper methods
    bool loadVoynichWords();
    bool determineTranslatorImplementation(TranslatorType type);
    std::string getTranslatorTypeName(TranslatorType type) const;
    
    // Batch processing for CUDA optimization
    void processMappingsBatch(const std::vector<std::unique_ptr<Mapping>>& mappings,
                             std::function<void(const ProcessingResult&)> resultCallback,
                             std::function<void(int, uint64_t, uint64_t, double, bool)> batchStatsCallback,
                             int threadId,
                             std::function<bool()> shouldStopCallback);
    
public:
    explicit VoynichDecoder(const DecoderConfig& config = DecoderConfig());
    ~VoynichDecoder() = default;
    
    // Main decoder operations
    bool initialize();
    
    // Process a single mapping (original method for backwards compatibility)
    ProcessingResult processMapping(const WordSet& voynichWords, const Mapping& mapping, bool useCuda = false);
    
    // Process a single mapping using internal Voynich words
    ProcessingResult processMapping(const Mapping& mapping);
    
    // Process multiple mappings using a callback for results
    void processMappings(const std::vector<std::unique_ptr<Mapping>>& mappings, 
                        std::function<void(const ProcessingResult&)> resultCallback);
    
    // Process a block of mappings from generator
    void processMappingBlock(MappingGenerator& generator, int threadId,
                           std::function<void(const ProcessingResult&)> resultCallback,
                           std::function<void(int, uint64_t, uint64_t, double, bool)> batchStatsCallback,
                           std::function<bool()> shouldStopCallback = nullptr);
    
    // Configuration access
    const DecoderConfig& getConfig() const { return config; }
    void updateScoreThreshold(double newThreshold);
    
    // Access to internal state
    const WordSet& getVoynichWords() const { return voynichWords; }
    bool isUsingCudaTranslation() const { return useCudaTranslation; }
    
    // Performance tracking
    void reportBatchStatsIfNeeded(std::function<void(int, uint64_t, uint64_t, double, bool)> batchStatsCallback, int threadId, bool force = false);
};