#pragma once

#include <atomic>
#include <chrono>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <string>
#include <memory>
#include <functional>

class StatsProvider {
public:
    // Message types for the stats queue
    enum class MessageType {
        MAPPING_PROCESSED,
        BATCH_STATS,
        HIGH_SCORE_FOUND,
        THREAD_STARTED,
        THREAD_COMPLETED,
        STATUS_UPDATE_REQUEST,
        SHUTDOWN
    };
    
    // Base message structure
    struct StatsMessage {
        MessageType type;
        int threadId;
        std::chrono::steady_clock::time_point timestamp;
        
        StatsMessage(MessageType t, int tid = -1) 
            : type(t), threadId(tid), timestamp(std::chrono::steady_clock::now()) {}
        virtual ~StatsMessage() = default;
    };
    
    // Specific message types
    struct MappingProcessedMessage : public StatsMessage {
        uint64_t mappingId;
        uint64_t wordsValidated;
        double score;
        
        MappingProcessedMessage(int threadId, uint64_t mappingId, uint64_t wordsValidated, double score)
            : StatsMessage(MessageType::MAPPING_PROCESSED, threadId), mappingId(mappingId), 
              wordsValidated(wordsValidated), score(score) {}
    };
    
    struct HighScoreMessage : public StatsMessage {
        uint64_t mappingId;
        double score;
        size_t matchedWords;
        size_t totalWords;
        double matchPercentage;
        
        HighScoreMessage(int threadId, uint64_t mappingId, double score, size_t matched, size_t total, double percentage)
            : StatsMessage(MessageType::HIGH_SCORE_FOUND, threadId), mappingId(mappingId), 
              score(score), matchedWords(matched), totalWords(total), matchPercentage(percentage) {}
    };
    
    struct ThreadLifecycleMessage : public StatsMessage {
        uint64_t localMappingsProcessed;
        
        ThreadLifecycleMessage(MessageType type, int threadId, uint64_t localMappings = 0)
            : StatsMessage(type, threadId), localMappingsProcessed(localMappings) {}
    };
    
    struct BatchStatsMessage : public StatsMessage {
        uint64_t mappingsProcessed;
        uint64_t wordsValidated;
        double highestScore;
        bool hasHighScore;
        
        BatchStatsMessage(int threadId, uint64_t mappings, uint64_t words, double score, bool hasHigh)
            : StatsMessage(MessageType::BATCH_STATS, threadId), mappingsProcessed(mappings),
              wordsValidated(words), highestScore(score), hasHighScore(hasHigh) {}
    };
    
    // Configuration for stats provider
    struct StatsConfig {
        size_t statusUpdateIntervalMs;
        std::string resultsFilePath;
        double scoreThreshold;
        size_t maxMappingsToProcess;
        
        StatsConfig() : statusUpdateIntervalMs(5000), resultsFilePath("voynich_decoder_results.txt"),
                       scoreThreshold(25.0), maxMappingsToProcess(0) {}
    };
    
    // Statistics snapshot
    struct StatsSnapshot {
        uint64_t totalMappingsProcessed;
        uint64_t totalWordsValidated;
        double highestScore;
        uint64_t highScoreCount;
        size_t activeThreads;
        std::chrono::steady_clock::time_point startTime;
        std::chrono::steady_clock::time_point lastUpdateTime;
        uint64_t lastMappingsProcessed;
        double recentMappingsPerSecond;
        
        double getMappingsPerSecond() const {
            return recentMappingsPerSecond;
        }
        
        double getElapsedMinutes() const {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::minutes>(now - startTime).count();
            return static_cast<double>(elapsed);
        }
    };

private:
    StatsConfig config;
    
    // Message queue and processing
    std::queue<std::unique_ptr<StatsMessage>> messageQueue;
    mutable std::mutex queueMutex;
    std::condition_variable queueCondition;
    
    // Stats processing thread
    std::thread statsThread;
    std::atomic<bool> shouldStop{false};
    
    // Statistics data
    std::atomic<uint64_t> totalMappingsProcessed{0};
    std::atomic<uint64_t> totalWordsValidated{0};
    std::atomic<double> highestScore{0.0};
    std::atomic<uint64_t> highScoreCount{0};
    std::atomic<size_t> activeThreads{0};
    std::chrono::steady_clock::time_point startTime;
    std::chrono::steady_clock::time_point lastStatusTime;
    uint64_t lastMappingsCount{0};
    double recentMappingsPerSecond{0.0};
    
    // Internal message processing
    void processMessages();
    void handleMappingProcessed(const MappingProcessedMessage& msg);
    void handleBatchStats(const BatchStatsMessage& msg);
    void handleHighScore(const HighScoreMessage& msg);
    void handleThreadLifecycle(const ThreadLifecycleMessage& msg);
    void printStatus();
    void printFinalResults();
    void updateMappingsPerSecond();
    
public:
    explicit StatsProvider(const StatsConfig& config = StatsConfig());
    ~StatsProvider();
    
    // Lifecycle management
    void start();
    void stop();
    
    // Message submission (thread-safe)
    void submitMappingProcessed(int threadId, uint64_t mappingId, uint64_t wordsValidated, double score);
    void submitBatchStats(int threadId, uint64_t mappingsProcessed, uint64_t wordsValidated, double highestScore, bool hasHighScore);
    void submitHighScore(int threadId, uint64_t mappingId, double score, size_t matchedWords, size_t totalWords, double matchPercentage);
    void submitThreadStarted(int threadId);
    void submitThreadCompleted(int threadId, uint64_t localMappingsProcessed);
    void requestStatusUpdate();
    
    // Statistics access
    StatsSnapshot getCurrentSnapshot() const;
    bool isRunning() const { return !shouldStop.load(); }
    
    // Configuration access
    const StatsConfig& getConfig() const { return config; }
    void updateScoreThreshold(double newThreshold) { config.scoreThreshold = newThreshold; }
};