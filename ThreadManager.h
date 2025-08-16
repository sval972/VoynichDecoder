#pragma once

#include "VoynichDecoder.h"
#include "StatsProvider.h"
#include "MappingGenerator.h"
#include <vector>
#include <thread>
#include <atomic>
#include <memory>
#include <csignal>
#ifdef _WIN32
#include <windows.h>
#endif

class ThreadManager {
public:
    // Configuration for the thread manager
    struct ThreadManagerConfig {
        size_t numThreads;                    // Number of worker threads (0 = auto-detect)
        VoynichDecoder::TranslatorType translatorType;        // Type of translator implementation to use
        std::string voynichWordsPath;         // Path to Voynich manuscript words
        std::string hebrewLexiconPath;        // Path to Hebrew lexicon
        std::string resultsFilePath;          // Path to save results
        double scoreThreshold;                // Minimum score to save results
        size_t statusUpdateIntervalMs;        // How often to print status (milliseconds)
        size_t maxMappingsToProcess;          // Maximum mappings to process (0 = unlimited)
        
        // MappingGenerator configuration
        size_t mappingBlockSize;              // Mappings per block in generator
        std::string generatorStateFile;       // Generator state persistence file
        
        ThreadManagerConfig() :
            numThreads(0),  // Auto-detect
            translatorType(VoynichDecoder::TranslatorType::AUTO),  // Auto-detect best implementation
            voynichWordsPath("resources/Script_freq100.txt"),
            hebrewLexiconPath("resources/Tanah2.txt"),
            resultsFilePath("voynich_decoder_results.txt"),
            scoreThreshold(25.0),
            statusUpdateIntervalMs(5000),  // 5 seconds
            maxMappingsToProcess(0),  // Unlimited
            mappingBlockSize(1000000),
            generatorStateFile("mapping_generator_state.json") {}
    };

private:
    ThreadManagerConfig config;
    
    // Core components
    std::unique_ptr<MappingGenerator> mappingGenerator;
    std::unique_ptr<StatsProvider> statsProvider;
    
    // Threading
    std::vector<std::thread> workerThreads;
    std::vector<std::unique_ptr<VoynichDecoder>> decoders;  // One decoder per thread
    std::atomic<bool> shouldStop{false};
    std::atomic<bool> isRunning{false};
    
    // Signal handling
    static std::atomic<bool> signalReceived;
    static ThreadManager* instance;
    static void signalHandler(int signal);
#ifdef _WIN32
    static BOOL WINAPI consoleHandler(DWORD dwCtrlType);
#endif
    
    // Worker thread function
    void workerThreadFunction(int threadId);
    
    // Utilities
    size_t getOptimalThreadCount() const;
    void setupSignalHandling();
    void cleanupSignalHandling();
    
public:
    explicit ThreadManager(const ThreadManagerConfig& config = ThreadManagerConfig());
    ~ThreadManager();
    
    // Main operations
    bool initialize();
    void start();
    void stop();
    void waitForCompletion();
    
    // Status and control
    bool isManagerRunning() const { return isRunning.load(); }
    StatsProvider::StatsSnapshot getCurrentStats() const;
    
    // Configuration access
    const ThreadManagerConfig& getConfig() const { return config; }
    void updateScoreThreshold(double newThreshold);
    
    // Convenience method for complete run
    void runDecoding();
};