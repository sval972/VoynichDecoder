#include "VoynichDecoder.h"
#include "StaticTranslator.h"
#include <iostream>
#include <iomanip>
#include <algorithm>

// Static member definitions
std::atomic<bool> VoynichDecoder::signalReceived{false};
VoynichDecoder* VoynichDecoder::instance = nullptr;

VoynichDecoder::VoynichDecoder(const DecoderConfig& config) : config(config) {
    instance = this;
}

VoynichDecoder::~VoynichDecoder() {
    stop();
    cleanupSignalHandling();
    instance = nullptr;
}

void VoynichDecoder::signalHandler(int signal) {
    if (signal == SIGINT) {
        signalReceived = true;
        std::wcout << L"\n\nReceived Ctrl+C signal. Initiating graceful shutdown..." << std::endl;
        
        if (instance) {
            instance->stop();
        }
    }
}

bool VoynichDecoder::initialize() {
    std::wcout << L"Initializing Voynich Decoder..." << std::endl;
    
    // Load Voynich manuscript words
    if (!loadVoynichWords()) {
        std::wcerr << L"Failed to load Voynich words from " << config.voynichWordsPath.c_str() << std::endl;
        return false;
    }
    
    std::wcout << L"Loaded " << voynichWords.size() << L" Voynich words" << std::endl;
    
    // Initialize mapping generator
    MappingGenerator::GeneratorConfig genConfig;
    genConfig.blockSize = config.mappingBlockSize;
    genConfig.stateFilePath = config.generatorStateFile;
    genConfig.enableStateFile = true;
    
    mappingGenerator = std::make_unique<MappingGenerator>(genConfig);
    
    // Determine number of threads
    if (config.numThreads == 0) {
        config.numThreads = getOptimalThreadCount();
    }
    
    std::wcout << L"Configured for " << config.numThreads << L" worker threads" << std::endl;
    
    // Determine translator implementation (static methods - no instance needed)
    useCudaTranslation = determineTranslatorImplementation(config.translatorType);
    
    std::wcout << L"Translator implementation: " << getTranslatorTypeName(config.translatorType).c_str() 
               << L" (" << (useCudaTranslation ? "CUDA (Static)" : "CPU (Static)") << L")" << std::endl;
    std::wcout << L"Score threshold: " << std::fixed << std::setprecision(1) << config.scoreThreshold << std::endl;
    std::wcout << L"Max mappings to process: " << (config.maxMappingsToProcess > 0 ? 
        std::to_wstring(config.maxMappingsToProcess) : L"unlimited") << std::endl;
    
    // Setup signal handling
    setupSignalHandling();
    
    std::wcout << L"Initialization complete. Press Ctrl+C to stop gracefully." << std::endl;
    return true;
}

bool VoynichDecoder::loadVoynichWords() {
    voynichWords.readFromFile(config.voynichWordsPath, Alphabet::EVA);
    return voynichWords.size() > 0;
}

size_t VoynichDecoder::getOptimalThreadCount() const {
    size_t hardwareThreads = std::thread::hardware_concurrency();
    return hardwareThreads > 0 ? hardwareThreads : 4;
}

void VoynichDecoder::setupSignalHandling() {
    std::signal(SIGINT, signalHandler);
}

void VoynichDecoder::cleanupSignalHandling() {
    std::signal(SIGINT, SIG_DFL);
}

bool VoynichDecoder::determineTranslatorImplementation(TranslatorType type) const {
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

void VoynichDecoder::start() {
    if (isRunning.load()) {
        std::wcout << L"Decoder is already running!" << std::endl;
        return;
    }
    
    shouldStop = false;
    signalReceived = false;
    isRunning = true;
    
    // Reset statistics
    stats.totalMappingsProcessed = 0;
    stats.totalWordsValidated = 0;
    stats.highestScore = 0.0;
    stats.highScoreCount = 0;
    auto now = std::chrono::steady_clock::now();
    stats.startTime = now;
    stats.lastUpdateTime = now;
    
    std::wcout << L"Starting Voynich Decoder with " << config.numThreads << L" threads..." << std::endl;
    
    // Start worker threads
    for (size_t i = 0; i < config.numThreads; ++i) {
        workerThreads.emplace_back(&VoynichDecoder::workerThreadFunction, this, static_cast<int>(i));
    }
    
    // Start status reporting thread
    std::thread statusThread(&VoynichDecoder::statusReportingThread, this);
    statusThread.detach();
    
    std::wcout << L"All threads started. Decoding in progress..." << std::endl;
}

void VoynichDecoder::stop() {
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
    isRunning = false;
    
    printFinalResults();
}

void VoynichDecoder::waitForCompletion() {
    while (isRunning.load() && !shouldStop.load() && !signalReceived.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Check if we've reached the maximum mappings limit
        if (config.maxMappingsToProcess > 0 && 
            stats.totalMappingsProcessed.load() >= config.maxMappingsToProcess) {
            std::wcout << L"\nReached maximum mappings limit. Stopping..." << std::endl;
            shouldStop = true;
            break;
        }
    }
    
    stop();
}

void VoynichDecoder::workerThreadFunction(int threadId) {
    try {
        // No translator instance needed - using static methods
        
        HebrewValidator::ValidatorConfig validatorConfig;
        validatorConfig.hebrewLexiconPath = config.hebrewLexiconPath;
        validatorConfig.scoreThreshold = config.scoreThreshold;
        validatorConfig.resultsFilePath = config.resultsFilePath;
        validatorConfig.enableResultsSaving = true;
        
        HebrewValidator validator(validatorConfig);
        
        // Wait for Hebrew lexicon to load
        while (!validator.isLexiconReady() && !shouldStop.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        if (shouldStop.load()) return;
        
        {
            std::lock_guard<std::mutex> lock(printMutex);
            std::wcout << L"Thread " << threadId << L" started" << std::endl;
        }
        
        uint64_t localMappingsProcessed = 0;
        
        while (!shouldStop.load() && !signalReceived.load()) {
            // Get next mapping
            auto mapping = mappingGenerator->getNextMapping();
            if (!mapping) {
                // Check if generation is truly complete or just temporarily exhausted
                if (mappingGenerator->isGenerationComplete()) {
                    {
                        std::lock_guard<std::mutex> lock(printMutex);
                        std::wcout << L"Thread " << threadId << L" finished - no more mappings" << std::endl;
                    }
                    break;
                } else {
                    // Block is being refilled, wait a bit and try again
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    continue;
                }
            }
            
            // Create mapping ID for tracking
            uint64_t mappingId = stats.totalMappingsProcessed.fetch_add(1);
            
            // Translate Voynich words to Hebrew using static method
            WordSet translatedWords = StaticTranslator::translateWordSet(voynichWords, *mapping, useCudaTranslation);
            
            // Serialize the mapping for saving
            std::string mappingVisualization = mapping->serializeMappingVisualization();
            std::vector<uint8_t> mappingData(mappingVisualization.begin(), mappingVisualization.end());
            
            // Validate translation against Hebrew lexicon
            auto result = validator.validateTranslationWithMapping(translatedWords, mappingId, mappingData);
            
            // Update statistics
            localMappingsProcessed++;
            stats.totalWordsValidated.fetch_add(result.totalWords);
            
            // Update highest score (thread-safe) - regardless of threshold
            double currentHighest = stats.highestScore.load();
            while (result.score > currentHighest && 
                   !stats.highestScore.compare_exchange_weak(currentHighest, result.score)) {
                // Loop until we successfully update or find a higher score
            }
            
            // Only count as high score if above threshold (for file saving)
            if (result.isHighScore) {
                stats.highScoreCount.fetch_add(1);
                
                std::lock_guard<std::mutex> lock(printMutex);
                std::wcout << L"*** HIGH SCORE *** Thread " << threadId 
                           << L": Score=" << std::fixed << std::setprecision(2) << result.score
                           << L", Matches=" << result.matchedWords << L"/" << result.totalWords
                           << L" (" << std::fixed << std::setprecision(1) << result.matchPercentage << L"%)"
                           << L", Mapping=" << mappingId << std::endl;
            }
            
            // Check termination conditions
            if (config.maxMappingsToProcess > 0 && 
                stats.totalMappingsProcessed.load() >= config.maxMappingsToProcess) {
                break;
            }
        }
        
        {
            std::lock_guard<std::mutex> lock(printMutex);
            std::wcout << L"Thread " << threadId << L" completed. Processed " 
                       << localMappingsProcessed << L" mappings" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> lock(printMutex);
        std::wcerr << L"Thread " << threadId << L" error: " << e.what() << std::endl;
    }
}

void VoynichDecoder::statusReportingThread() {
    while (isRunning.load() && !shouldStop.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(config.statusUpdateIntervalMs));
        
        if (!shouldStop.load()) {
            printStatus();
        }
    }
}

void VoynichDecoder::printStatus() const {
    std::lock_guard<std::mutex> lock(printMutex);
    
    auto currentStats = getCurrentStats();
    double mappingsPerSec = currentStats.getMappingsPerSecond();
    double elapsedMin = currentStats.getElapsedMinutes();
    
    std::wcout << L"[" << std::fixed << std::setprecision(1) << elapsedMin << L"min] "
               << L"Mappings: " << currentStats.totalMappingsProcessed 
               << L" (" << std::fixed << std::setprecision(1) << mappingsPerSec << L"/sec), "
               << L"Highest Score: " << std::fixed << std::setprecision(2) << currentStats.highestScore << std::endl;
}

void VoynichDecoder::printFinalResults() const {
    std::lock_guard<std::mutex> lock(printMutex);
    
    auto finalStats = getCurrentStats();
    double totalMinutes = finalStats.getElapsedMinutes();
    double avgMappingsPerSec = finalStats.getMappingsPerSecond();
    
    std::wcout << L"\n" << L"=" << std::wstring(60, L'=') << std::endl;
    std::wcout << L"VOYNICH DECODER - FINAL RESULTS" << std::endl;
    std::wcout << L"=" << std::wstring(60, L'=') << std::endl;
    std::wcout << L"Total runtime: " << std::fixed << std::setprecision(1) << totalMinutes << L" minutes" << std::endl;
    std::wcout << L"Mappings processed: " << finalStats.totalMappingsProcessed << std::endl;
    std::wcout << L"Words validated: " << finalStats.totalWordsValidated << std::endl;
    std::wcout << L"Average rate: " << std::fixed << std::setprecision(1) << avgMappingsPerSec << L" mappings/sec" << std::endl;
    std::wcout << L"Highest score achieved: " << std::fixed << std::setprecision(2) << finalStats.highestScore << std::endl;
    std::wcout << L"High-scoring results: " << finalStats.highScoreCount << std::endl;
    
    if (finalStats.highScoreCount > 0) {
        std::wcout << L"Results saved to: " << config.resultsFilePath.c_str() << std::endl;
    }
    
    std::wcout << L"=" << std::wstring(60, L'=') << std::endl;
}

VoynichDecoder::StatsSummary VoynichDecoder::getCurrentStats() const {
    StatsSummary current;
    current.totalMappingsProcessed = stats.totalMappingsProcessed.load();
    current.totalWordsValidated = stats.totalWordsValidated.load();
    current.highestScore = stats.highestScore.load();
    current.highScoreCount = stats.highScoreCount.load();
    current.startTime = stats.startTime;
    current.lastUpdateTime = stats.lastUpdateTime;
    return current;
}

void VoynichDecoder::runDecoding() {
    if (!initialize()) {
        std::wcerr << L"Failed to initialize decoder" << std::endl;
        return;
    }
    
    start();
    waitForCompletion();
}