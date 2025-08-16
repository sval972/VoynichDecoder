#pragma once

#include "WordSet.h"
#include "Mapping.h"
#include <unordered_set>
#include <memory>
#include <string>
#include <vector>
#include <mutex>
#include <fstream>
#include <cstdint>

class HebrewValidator {
public:
    // Validation result structure
    struct ValidationResult {
        size_t totalWords;           // Total words checked
        size_t matchedWords;         // Words found in Hebrew lexicon
        double matchPercentage;      // Percentage of matched words
        double score;                // Calculated score (0.0 - 100.0)
        bool isHighScore;            // Whether this exceeds threshold
        
        ValidationResult() : totalWords(0), matchedWords(0), matchPercentage(0.0), score(0.0), isHighScore(false) {}
    };
    
    // Configuration for the validator
    struct ValidatorConfig {
        std::string hebrewLexiconPath;    // Path to Hebrew words file
        std::string resultsFilePath;     // Path to save high scores
        double scoreThreshold;           // Minimum score to save (default: 25.0)
        bool enableResultsSaving;        // Whether to save high scores
        size_t maxResultsToSave;         // Maximum results to keep in file
        
        ValidatorConfig() : 
            hebrewLexiconPath("Tanah2.txt"),
            resultsFilePath("hebrew_validation_results.txt"),
            scoreThreshold(25.0),
            enableResultsSaving(true),
            maxResultsToSave(1000) {}
    };
    
    // Result entry for file storage
    struct ValidationEntry {
        uint64_t mappingId;          // Unique identifier for mapping
        double score;                // Validation score
        size_t matchedWords;         // Number of matched words
        size_t totalWords;           // Total words validated
        std::string timestamp;       // When validation occurred
        std::vector<uint8_t> mappingData; // Serialized mapping for reproduction
    };

private:
    // Instance-specific Hebrew lexicon (each validator has its own copy)
    struct Lexicon {
        std::unordered_set<uint32_t> binaryHashes;     // 32-bit hashes of Hebrew word binary vectors
        std::unordered_set<uint64_t> binarySignatures; // 64-bit signatures for collision detection
        size_t wordCount;                              // Total Hebrew words loaded
        bool isLoaded;                                 // Loading completion flag
        
        Lexicon() : wordCount(0), isLoaded(false) {}
    };
    
    Lexicon lexicon;                          // Instance lexicon (no thread safety needed)
    ValidatorConfig config;                   // Instance configuration
    mutable std::mutex resultsMutex;          // Protects results file operations
    
    // Binary vector conversion methods
    static uint32_t binaryVectorToHash(const std::vector<int>& binaryVector);
    static uint64_t binaryVectorToSignature(const std::vector<int>& binaryVector);
    
    // Lexicon management
    bool loadHebrewLexicon(const std::string& filePath);
    
    // Scoring and persistence
    double calculateScore(const ValidationResult& result) const;
    bool saveValidationResult(const ValidationResult& result, uint64_t mappingId, const std::vector<uint8_t>& mappingData);
    std::string getCurrentTimestamp() const;
    
    // Thread-safe file operations
    bool appendResultToFile(const ValidationEntry& entry);
    
public:
    explicit HebrewValidator(const ValidatorConfig& config = ValidatorConfig());
    
    // Main validation method - validates translated words against Hebrew lexicon
    ValidationResult validateTranslation(const WordSet& translatedWords);
    
    // Validate with mapping context (for result saving)
    ValidationResult validateTranslationWithMapping(
        const WordSet& translatedWords,
        uint64_t mappingId,
        const std::vector<uint8_t>& mappingData
    );
    
    // Check if lexicon is loaded and ready
    bool isLexiconReady() const;
    
    // Initialize lexicon for this instance
    bool initializeLexicon();
    
    // Get lexicon statistics
    struct LexiconStats {
        size_t wordCount;
        size_t uniqueHashes;
        size_t uniqueSignatures;
        bool isLoaded;
    };
    
    LexiconStats getLexiconStats() const;
    
    // Performance testing
    struct PerformanceStats {
        size_t wordsValidated;
        double validationTimeMs;
        double throughputWordsPerSecond;
    };
    
    ValidationResult validateWithPerformanceStats(const WordSet& translatedWords, PerformanceStats& perfStats);
    
    // Utility methods
    static bool isValidHebrewBinaryVector(const std::vector<int>& binaryVector);
    
    // Configuration access
    const ValidatorConfig& getConfig() const { return config; }
    void updateScoreThreshold(double newThreshold) { config.scoreThreshold = newThreshold; }
    
    // Results management
    struct HighScoresSummary {
        size_t totalResults;
        double highestScore;
        double averageScore;
        size_t totalWordsValidated;
    };
    
    HighScoresSummary getHighScoresSummary() const;
    bool clearResults(); // Clear results file
};