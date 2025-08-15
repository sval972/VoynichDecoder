#pragma once

#include "MappingGenerator.h"
#include "StaticTranslator.h"
#include "HebrewValidator.h"
#include "WordSet.h"
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>
#include <memory>
#include <csignal>

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
        size_t numThreads;                    // Number of worker threads (0 = auto-detect)
        TranslatorType translatorType;        // Type of translator implementation to use
        std::string voynichWordsPath;         // Path to Voynich manuscript words
        std::string hebrewLexiconPath;        // Path to Hebrew lexicon
        std::string resultsFilePath;          // Path to save results
        double scoreThreshold;                // Minimum score to save results
        size_t statusUpdateIntervalMs;        // How often to print status (milliseconds)
        size_t maxMappingsToProcess;          // Maximum mappings to process (0 = unlimited)
        
        // MappingGenerator configuration
        size_t mappingBlockSize;              // Mappings per block in generator
        std::string generatorStateFile;       // Generator state persistence file
        
        DecoderConfig() :
            numThreads(0),  // Auto-detect
            translatorType(TranslatorType::AUTO),  // Auto-detect best implementation
            voynichWordsPath("resources/Script_freq100.txt"),
            hebrewLexiconPath("resources/Tanah2.txt"),
            resultsFilePath("voynich_decoder_results.txt"),
            scoreThreshold(25.0),
            statusUpdateIntervalMs(5000),  // 5 seconds
            maxMappingsToProcess(0),  // Unlimited
            mappingBlockSize(1000000),
            generatorStateFile("voynich_decoder_state.dat") {}
    };
    
    // Real-time statistics
    struct DecoderStats {
        std::atomic<uint64_t> totalMappingsProcessed{0};
        std::atomic<uint64_t> totalWordsValidated{0};
        std::atomic<double> highestScore{0.0};
        std::atomic<uint64_t> highScoreCount{0};
        std::chrono::steady_clock::time_point startTime;
        std::chrono::steady_clock::time_point lastUpdateTime;
        
        DecoderStats() {
            auto now = std::chrono::steady_clock::now();
            startTime = now;
            lastUpdateTime = now;
        }
        
        double getMappingsPerSecond() const {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count();
            return (elapsed > 0) ? (totalMappingsProcessed.load() * 1000.0 / elapsed) : 0.0;
        }
        
        double getElapsedMinutes() const {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::minutes>(now - startTime).count();
            return static_cast<double>(elapsed);
        }
    };

private:
    DecoderConfig config;
    DecoderStats stats;
    
    // Core components
    std::unique_ptr<MappingGenerator> mappingGenerator;
    WordSet voynichWords;
    bool useCudaTranslation;  // Flag to determine if CUDA should be used for translation
    
    // Threading
    std::vector<std::thread> workerThreads;
    std::atomic<bool> shouldStop{false};
    std::atomic<bool> isRunning{false};
    
    // Thread synchronization
    mutable std::mutex statsMutex;
    mutable std::mutex printMutex;
    
    // Signal handling
    static std::atomic<bool> signalReceived;
    static VoynichDecoder* instance;
    static void signalHandler(int signal);
    
    // Worker thread function
    void workerThreadFunction(int threadId);
    
    // Status reporting
    void statusReportingThread();
    void printStatus() const;
    void printFinalResults() const;
    
    // Utilities
    size_t getOptimalThreadCount() const;
    bool loadVoynichWords();
    void setupSignalHandling();
    void cleanupSignalHandling();
    bool determineTranslatorImplementation(TranslatorType type) const;
    std::string getTranslatorTypeName(TranslatorType type) const;
    
public:
    explicit VoynichDecoder(const DecoderConfig& config = DecoderConfig());
    ~VoynichDecoder();
    
    // Main decoder operations
    bool initialize();
    void start();
    void stop();
    void waitForCompletion();
    
    // Non-atomic stats for returning values
    struct StatsSummary {
        uint64_t totalMappingsProcessed;
        uint64_t totalWordsValidated;
        double highestScore;
        uint64_t highScoreCount;
        std::chrono::steady_clock::time_point startTime;
        std::chrono::steady_clock::time_point lastUpdateTime;
        
        double getMappingsPerSecond() const {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count();
            return (elapsed > 0) ? (totalMappingsProcessed * 1000.0 / elapsed) : 0.0;
        }
        
        double getElapsedMinutes() const {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::minutes>(now - startTime).count();
            return static_cast<double>(elapsed);
        }
    };

    // Status and control
    bool isDecoderRunning() const { return isRunning.load(); }
    StatsSummary getCurrentStats() const;
    
    // Configuration access
    const DecoderConfig& getConfig() const { return config; }
    void updateScoreThreshold(double newThreshold) { config.scoreThreshold = newThreshold; }
    
    // Convenience method for complete run
    void runDecoding();
};