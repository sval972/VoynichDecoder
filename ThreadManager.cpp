#include "ThreadManager.h"
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <thread>

// Static member definitions
std::atomic<bool> ThreadManager::signalReceived{false};
ThreadManager* ThreadManager::instance = nullptr;

ThreadManager::ThreadManager(const ThreadManagerConfig& config) : config(config) {
    instance = this;
}

ThreadManager::~ThreadManager() {
    stop();
    cleanupSignalHandling();
    instance = nullptr;
}

void ThreadManager::signalHandler(int signal) {
    if (signal == SIGINT) {
        signalReceived = true;
        std::wcout << L"\n\nReceived Ctrl+C signal. Initiating graceful shutdown..." << std::endl;
        
        if (instance) {
            instance->stop();
        }
    }
}

#ifdef _WIN32
BOOL WINAPI ThreadManager::consoleHandler(DWORD dwCtrlType) {
    switch (dwCtrlType) {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_CLOSE_EVENT:
            signalReceived = true;
            std::wcout << L"\n\nReceived Windows console signal. Initiating graceful shutdown..." << std::endl;
            std::wcout.flush(); // Ensure message is displayed immediately
            
            if (instance) {
                instance->shouldStop = true;
                // Give threads a moment to see the signal, then force stop
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                instance->stop();
            }
            return TRUE; // Signal handled
        default:
            return FALSE; // Signal not handled
    }
}
#endif

bool ThreadManager::initialize() {
    std::wcout << L"Initializing Thread Manager..." << std::endl;
    
    // Initialize mapping generator
    MappingGenerator::GeneratorConfig genConfig;
    genConfig.blockSize = config.mappingBlockSize;
    genConfig.stateFilePath = config.generatorStateFile;
    genConfig.enableStateFile = true;
    
    mappingGenerator = std::make_unique<MappingGenerator>(genConfig);
    
    // Display block status information
    auto blockStatus = mappingGenerator->getBlockStatus();
    std::wcout << L"Block Status: Window " << blockStatus.windowSize 
               << L", Next block: " << blockStatus.nextBlockToGenerate 
               << L", Completed: " << blockStatus.completedBlocks << std::endl;
    
    // Initialize stats provider
    StatsProvider::StatsConfig statsConfig;
    statsConfig.statusUpdateIntervalMs = config.statusUpdateIntervalMs;
    statsConfig.resultsFilePath = config.resultsFilePath;
    statsConfig.scoreThreshold = config.scoreThreshold;
    statsConfig.maxMappingsToProcess = config.maxMappingsToProcess;
    
    statsProvider = std::make_unique<StatsProvider>(statsConfig);
    
    // Determine number of threads
    if (config.numThreads == 0) {
        config.numThreads = getOptimalThreadCount();
    }
    
    std::wcout << L"Configured for " << config.numThreads << L" worker threads" << std::endl;
    
    std::wcout << L"Score threshold: " << std::fixed << std::setprecision(1) << config.scoreThreshold << std::endl;
    std::wcout << L"Max mappings to process: " << (config.maxMappingsToProcess > 0 ? 
        std::to_wstring(config.maxMappingsToProcess) : L"unlimited") << std::endl;
    
    // Create decoder instances for each thread
    decoders.reserve(config.numThreads);
    for (size_t i = 0; i < config.numThreads; ++i) {
        VoynichDecoder::DecoderConfig decoderConfig;
        decoderConfig.hebrewLexiconPath = config.hebrewLexiconPath;
        decoderConfig.voynichWordsPath = config.voynichWordsPath;
        decoderConfig.scoreThreshold = config.scoreThreshold;
        decoderConfig.resultsFilePath = config.resultsFilePath;
        decoderConfig.translatorType = config.translatorType;
        
        decoders.push_back(std::make_unique<VoynichDecoder>(decoderConfig));
    }
    
    // Setup signal handling
    setupSignalHandling();
    
    std::wcout << L"Initialization complete. Press Ctrl+C to stop gracefully." << std::endl;
    return true;
}


size_t ThreadManager::getOptimalThreadCount() const {
    size_t hardwareThreads = std::thread::hardware_concurrency();
    return hardwareThreads > 0 ? hardwareThreads : 4;
}

void ThreadManager::setupSignalHandling() {
    std::signal(SIGINT, signalHandler);
#ifdef _WIN32
    SetConsoleCtrlHandler(consoleHandler, TRUE);
#endif
}

void ThreadManager::cleanupSignalHandling() {
    std::signal(SIGINT, SIG_DFL);
#ifdef _WIN32
    SetConsoleCtrlHandler(consoleHandler, FALSE);
#endif
}


void ThreadManager::start() {
    if (isRunning.load()) {
        std::wcout << L"Thread Manager is already running!" << std::endl;
        return;
    }
    
    shouldStop = false;
    signalReceived = false;
    isRunning = true;
    
    std::wcout << L"Starting Thread Manager with " << config.numThreads << L" threads..." << std::endl;
    
    // Start stats provider
    statsProvider->start();
    
    // Start worker threads
    for (size_t i = 0; i < config.numThreads; ++i) {
        workerThreads.emplace_back(&ThreadManager::workerThreadFunction, this, static_cast<int>(i));
    }
    
    std::wcout << L"All threads started. Decoding in progress..." << std::endl;
}

void ThreadManager::stop() {
    if (!isRunning.load()) {
        return;
    }
    
    shouldStop = true;
    
    // Wait for all worker threads to complete
    for (auto& thread : workerThreads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    
    workerThreads.clear();
    
    // Stop stats provider
    if (statsProvider) {
        statsProvider->stop();
    }
    
    isRunning = false;
}

void ThreadManager::waitForCompletion() {
    while (isRunning.load() && !shouldStop.load() && !signalReceived.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50)); // Check more frequently
        
        // Check if we've reached the maximum mappings limit
        if (config.maxMappingsToProcess > 0) {
            auto stats = getCurrentStats();
            if (stats.totalMappingsProcessed >= config.maxMappingsToProcess) {
                std::wcout << L"\nReached maximum mappings limit. Stopping..." << std::endl;
                shouldStop = true;
                break;
            }
        }
        
        // Check for Ctrl+C signal more frequently
        if (signalReceived.load()) {
            std::wcout << L"\nSignal received in waitForCompletion. Shutting down gracefully..." << std::endl;
            shouldStop = true;
            break;
        }
    }
    
    stop();
}

void ThreadManager::workerThreadFunction(int threadId) {
    try {
        auto& decoder = *decoders[threadId];
        
        // Initialize decoder
        if (!decoder.initialize()) {
            statsProvider->submitThreadCompleted(threadId, 0);
            return;
        }
        
        statsProvider->submitThreadStarted(threadId);
        
        uint64_t localMappingsProcessed = 0;
        
        while (!shouldStop.load() && !signalReceived.load()) {
            // Check if generation is complete
            if (mappingGenerator->isGenerationComplete()) {
                break;
            }
            
            // Process a block of mappings using decoder with batch stats callback
            decoder.processMappingBlock(*mappingGenerator, threadId, 
                [this, threadId, &localMappingsProcessed](const VoynichDecoder::ProcessingResult& result) {
                    localMappingsProcessed++;
                    
                    // Only submit high scores individually for immediate reporting
                    if (result.isHighScore) {
                        statsProvider->submitHighScore(threadId, result.mappingId, result.score, 
                                                     result.matchedWords, result.totalWords, result.matchPercentage);
                    }
                },
                [this](int tId, uint64_t mappings, uint64_t words, double highScore, bool hasHigh) {
                    // Batch stats callback - called every 1 second by decoder
                    statsProvider->submitBatchStats(tId, mappings, words, highScore, hasHigh);
                },
                [this]() -> bool {
                    // Signal check callback - returns true if thread should stop
                    return shouldStop.load() || signalReceived.load();
                });
            
            // Check termination conditions
            if (config.maxMappingsToProcess > 0) {
                auto stats = getCurrentStats();
                if (stats.totalMappingsProcessed >= config.maxMappingsToProcess) {
                    shouldStop = true;
                    break;
                }
            }
            
            // Small delay if no mappings were processed
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        // Final report of any remaining stats
        decoder.reportBatchStatsIfNeeded(
            [this](int tId, uint64_t mappings, uint64_t words, double highScore, bool hasHigh) {
                statsProvider->submitBatchStats(tId, mappings, words, highScore, hasHigh);
            }, threadId, true); // force = true
        
        statsProvider->submitThreadCompleted(threadId, localMappingsProcessed);
        
    } catch (const std::exception& e) {
        std::wcerr << L"Thread " << threadId << L" error: " << e.what() << std::endl;
        statsProvider->submitThreadCompleted(threadId, 0);
    }
}

StatsProvider::StatsSnapshot ThreadManager::getCurrentStats() const {
    if (statsProvider) {
        return statsProvider->getCurrentSnapshot();
    }
    return {};
}

void ThreadManager::updateScoreThreshold(double newThreshold) {
    config.scoreThreshold = newThreshold;
    if (statsProvider) {
        statsProvider->updateScoreThreshold(newThreshold);
    }
    for (auto& decoder : decoders) {
        decoder->updateScoreThreshold(newThreshold);
    }
}

void ThreadManager::runDecoding() {
    if (!initialize()) {
        std::wcerr << L"Failed to initialize thread manager" << std::endl;
        return;
    }
    
    start();
    waitForCompletion();
}