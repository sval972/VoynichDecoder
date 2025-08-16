#include "StatsProvider.h"
#include <iostream>
#include <iomanip>

StatsProvider::StatsProvider(const StatsConfig& config) : config(config) {
    startTime = std::chrono::steady_clock::now();
    lastStatusTime = startTime;
}

StatsProvider::~StatsProvider() {
    stop();
}

void StatsProvider::start() {
    if (statsThread.joinable()) {
        return; // Already started
    }
    
    shouldStop = false;
    startTime = std::chrono::steady_clock::now();
    lastStatusTime = startTime;
    
    // Reset statistics
    totalMappingsProcessed = 0;
    totalWordsValidated = 0;
    highestScore = 0.0;
    highScoreCount = 0;
    activeThreads = 0;
    lastMappingsCount = 0;
    recentMappingsPerSecond = 0.0;
    
    // Start message processing thread
    statsThread = std::thread(&StatsProvider::processMessages, this);
    
    std::wcout << L"StatsProvider started" << std::endl;
}

void StatsProvider::stop() {
    if (!statsThread.joinable()) {
        return; // Not started or already stopped
    }
    
    shouldStop = true;
    
    // Send shutdown message
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        messageQueue.push(std::make_unique<StatsMessage>(MessageType::SHUTDOWN));
    }
    queueCondition.notify_one();
    
    // Wait for processing thread to finish
    if (statsThread.joinable()) {
        statsThread.join();
    }
    
    printFinalResults();
}

void StatsProvider::submitMappingProcessed(int threadId, uint64_t mappingId, uint64_t wordsValidated, double score) {
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        messageQueue.push(std::make_unique<MappingProcessedMessage>(threadId, mappingId, wordsValidated, score));
    }
    queueCondition.notify_one();
}

void StatsProvider::submitBatchStats(int threadId, uint64_t mappingsProcessed, uint64_t wordsValidated, double highestScore, bool hasHighScore) {
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        messageQueue.push(std::make_unique<BatchStatsMessage>(threadId, mappingsProcessed, wordsValidated, highestScore, hasHighScore));
    }
    queueCondition.notify_one();
}

void StatsProvider::submitHighScore(int threadId, uint64_t mappingId, double score, size_t matchedWords, size_t totalWords, double matchPercentage) {
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        messageQueue.push(std::make_unique<HighScoreMessage>(threadId, mappingId, score, matchedWords, totalWords, matchPercentage));
    }
    queueCondition.notify_one();
}

void StatsProvider::submitThreadStarted(int threadId) {
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        messageQueue.push(std::make_unique<ThreadLifecycleMessage>(MessageType::THREAD_STARTED, threadId));
    }
    queueCondition.notify_one();
}

void StatsProvider::submitThreadCompleted(int threadId, uint64_t localMappingsProcessed) {
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        messageQueue.push(std::make_unique<ThreadLifecycleMessage>(MessageType::THREAD_COMPLETED, threadId, localMappingsProcessed));
    }
    queueCondition.notify_one();
}

void StatsProvider::requestStatusUpdate() {
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        messageQueue.push(std::make_unique<StatsMessage>(MessageType::STATUS_UPDATE_REQUEST));
    }
    queueCondition.notify_one();
}

void StatsProvider::processMessages() {
    auto lastStatusUpdate = std::chrono::steady_clock::now();
    
    while (!shouldStop.load()) {
        std::unique_lock<std::mutex> lock(queueMutex);
        
        // Wait for messages or timeout for periodic status updates
        if (queueCondition.wait_for(lock, std::chrono::milliseconds(config.statusUpdateIntervalMs), 
                                   [this] { return !messageQueue.empty() || shouldStop.load(); })) {
            
            // Process all available messages
            while (!messageQueue.empty()) {
                auto message = std::move(messageQueue.front());
                messageQueue.pop();
                lock.unlock(); // Release lock while processing message
                
                switch (message->type) {
                    case MessageType::MAPPING_PROCESSED:
                        handleMappingProcessed(static_cast<const MappingProcessedMessage&>(*message));
                        break;
                        
                    case MessageType::BATCH_STATS:
                        handleBatchStats(static_cast<const BatchStatsMessage&>(*message));
                        break;
                        
                    case MessageType::HIGH_SCORE_FOUND:
                        handleHighScore(static_cast<const HighScoreMessage&>(*message));
                        break;
                        
                    case MessageType::THREAD_STARTED:
                    case MessageType::THREAD_COMPLETED:
                        handleThreadLifecycle(static_cast<const ThreadLifecycleMessage&>(*message));
                        break;
                        
                    case MessageType::STATUS_UPDATE_REQUEST:
                        printStatus();
                        break;
                        
                    case MessageType::SHUTDOWN:
                        return; // Exit processing loop
                }
                
                lock.lock(); // Re-acquire lock for next iteration
            }
        }
        
        // Periodic status update
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastStatusUpdate).count() >= 
            static_cast<long long>(config.statusUpdateIntervalMs)) {
            lock.unlock();
            updateMappingsPerSecond();
            printStatus();
            lastStatusUpdate = now;
            lock.lock();
        }
    }
}

void StatsProvider::handleMappingProcessed(const MappingProcessedMessage& msg) {
    totalMappingsProcessed.fetch_add(1);
    totalWordsValidated.fetch_add(msg.wordsValidated);
    
    // Update highest score (thread-safe)
    double currentHighest = highestScore.load();
    while (msg.score > currentHighest && 
           !highestScore.compare_exchange_weak(currentHighest, msg.score)) {
        // Loop until we successfully update or find a higher score
    }
}

void StatsProvider::handleBatchStats(const BatchStatsMessage& msg) {
    totalMappingsProcessed.fetch_add(msg.mappingsProcessed);
    totalWordsValidated.fetch_add(msg.wordsValidated);
    
    // Update highest score if this batch has a higher score
    if (msg.hasHighScore) {
        double currentHighest = highestScore.load();
        while (msg.highestScore > currentHighest && 
               !highestScore.compare_exchange_weak(currentHighest, msg.highestScore)) {
            // Loop until we successfully update or find a higher score
        }
    }
}

void StatsProvider::handleHighScore(const HighScoreMessage& msg) {
    highScoreCount.fetch_add(1);
    
    std::wcout << L"*** HIGH SCORE *** Thread " << msg.threadId 
               << L": Score=" << std::fixed << std::setprecision(2) << msg.score
               << L", Matches=" << msg.matchedWords << L"/" << msg.totalWords
               << L" (" << std::fixed << std::setprecision(1) << msg.matchPercentage << L"%)"
               << L", Mapping=" << msg.mappingId << std::endl;
}

void StatsProvider::handleThreadLifecycle(const ThreadLifecycleMessage& msg) {
    if (msg.type == MessageType::THREAD_STARTED) {
        activeThreads.fetch_add(1);
        std::wcout << L"Thread " << msg.threadId << L" started" << std::endl;
    } else if (msg.type == MessageType::THREAD_COMPLETED) {
        activeThreads.fetch_sub(1);
        std::wcout << L"Thread " << msg.threadId << L" completed. Processed " 
                   << msg.localMappingsProcessed << L" mappings" << std::endl;
    }
}

void StatsProvider::updateMappingsPerSecond() {
    uint64_t currentMappings = totalMappingsProcessed.load();
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastStatusTime).count();
    
    if (elapsed > 0) {
        uint64_t mappingsDelta = currentMappings - lastMappingsCount;
        recentMappingsPerSecond = (mappingsDelta * 1000.0) / elapsed;
        lastMappingsCount = currentMappings;
        lastStatusTime = now;
    }
}

void StatsProvider::printStatus() {
    auto snapshot = getCurrentSnapshot();
    double mappingsPerSec = snapshot.getMappingsPerSecond();
    double elapsedMin = snapshot.getElapsedMinutes();
    
    std::wcout << L"[" << std::fixed << std::setprecision(1) << elapsedMin << L"min] "
               << L"Mappings: " << snapshot.totalMappingsProcessed 
               << L" (" << std::fixed << std::setprecision(1) << mappingsPerSec << L"/sec), "
               << L"Highest Score: " << std::fixed << std::setprecision(2) << snapshot.highestScore
               << L", Active Threads: " << snapshot.activeThreads << std::endl;
}

void StatsProvider::printFinalResults() {
    auto finalStats = getCurrentSnapshot();
    double totalMinutes = finalStats.getElapsedMinutes();
    // Calculate overall average rate for final results
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime).count();
    double avgMappingsPerSec = (elapsed > 0) ? (finalStats.totalMappingsProcessed * 1000.0 / elapsed) : 0.0;
    
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

StatsProvider::StatsSnapshot StatsProvider::getCurrentSnapshot() const {
    StatsSnapshot snapshot;
    snapshot.totalMappingsProcessed = totalMappingsProcessed.load();
    snapshot.totalWordsValidated = totalWordsValidated.load();
    snapshot.highestScore = highestScore.load();
    snapshot.highScoreCount = highScoreCount.load();
    snapshot.activeThreads = activeThreads.load();
    snapshot.startTime = startTime;
    snapshot.lastUpdateTime = lastStatusTime;
    snapshot.lastMappingsProcessed = lastMappingsCount;
    snapshot.recentMappingsPerSecond = recentMappingsPerSecond;
    return snapshot;
}