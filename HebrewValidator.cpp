#include "HebrewValidator.h"
#include <chrono>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <algorithm>
#include <shared_mutex>
#include <thread>

// Static member definitions
std::shared_ptr<HebrewValidator::SharedLexicon> HebrewValidator::sharedLexicon = nullptr;
std::once_flag HebrewValidator::lexiconInitFlag;

HebrewValidator::HebrewValidator(const ValidatorConfig& config) : config(config) {
    // Initialize shared lexicon (thread-safe, happens only once)
    std::call_once(lexiconInitFlag, [&config]() {
        sharedLexicon = std::make_shared<SharedLexicon>();
        initializeSharedLexicon(config.hebrewLexiconPath);
    });
}

void HebrewValidator::initializeSharedLexicon(const std::string& lexiconPath) {
    if (!sharedLexicon) return;
    
    std::wcout << L"Loading Hebrew lexicon from: " << lexiconPath.c_str() << std::endl;
    
    // Load Hebrew words
    WordSet hebrewWords;
    hebrewWords.readFromFile(lexiconPath, Alphabet::HEBREW);
    
    if (hebrewWords.size() == 0) {
        std::wcerr << L"Warning: No Hebrew words loaded from " << lexiconPath.c_str() << std::endl;
        sharedLexicon->isLoaded = true; // Mark as loaded even if empty to prevent retries
        return;
    }
    
    std::unique_lock<std::shared_mutex> lock(sharedLexicon->lexiconMutex);
    
    // Convert Hebrew words to binary hashes and signatures
    for (const auto& word : hebrewWords) {
        const auto& binaryVector = word.getBinaryMatrix();
        
        if (isValidHebrewBinaryVector(binaryVector)) {
            uint32_t hash = binaryVectorToHash(binaryVector);
            uint64_t signature = binaryVectorToSignature(binaryVector);
            
            sharedLexicon->binaryHashes.insert(hash);
            sharedLexicon->binarySignatures.insert(signature);
        }
    }
    
    sharedLexicon->wordCount = hebrewWords.size();
    sharedLexicon->isLoaded = true;
    
    std::wcout << L"Hebrew lexicon loaded: " << sharedLexicon->wordCount << L" words, "
               << sharedLexicon->binaryHashes.size() << L" unique hashes, "
               << sharedLexicon->binarySignatures.size() << L" unique signatures" << std::endl;
}

uint32_t HebrewValidator::binaryVectorToHash(const std::vector<int>& binaryVector) {
    // Convert 27-bit binary vector to 32-bit hash using polynomial rolling hash
    uint32_t hash = 0;
    uint32_t base = 31; // Prime base for polynomial hash
    
    for (size_t i = 0; i < std::min(binaryVector.size(), size_t(27)); ++i) {
        if (binaryVector[i]) {
            hash = hash * base + (i + 1); // +1 to avoid zero multiplication
        }
    }
    
    return hash;
}

uint64_t HebrewValidator::binaryVectorToSignature(const std::vector<int>& binaryVector) {
    // Convert 27-bit binary vector to 64-bit signature for collision detection
    uint64_t signature = 0;
    
    // Direct bit packing for first 27 bits
    for (size_t i = 0; i < std::min(binaryVector.size(), size_t(27)); ++i) {
        if (binaryVector[i]) {
            signature |= (1ULL << i);
        }
    }
    
    // Add position-weighted hash for additional entropy
    uint64_t weightedHash = 0;
    for (size_t i = 0; i < std::min(binaryVector.size(), size_t(27)); ++i) {
        if (binaryVector[i]) {
            weightedHash += (i + 1) * (i + 1); // Quadratic weighting
        }
    }
    
    // Combine bit pattern with weighted hash in upper bits
    signature |= (weightedHash << 32);
    
    return signature;
}

HebrewValidator::ValidationResult HebrewValidator::validateTranslation(const WordSet& translatedWords) {
    ValidationResult result;
    result.totalWords = translatedWords.size();
    
    if (!isLexiconReady() || result.totalWords == 0) {
        return result;
    }
    
    // Get shared read access to lexicon
    std::shared_lock<std::shared_mutex> lock(sharedLexicon->lexiconMutex);
    
    // Validate each translated word
    for (const auto& word : translatedWords) {
        const auto& binaryVector = word.getBinaryMatrix();
        
        if (isValidHebrewBinaryVector(binaryVector)) {
            uint32_t hash = binaryVectorToHash(binaryVector);
            uint64_t signature = binaryVectorToSignature(binaryVector);
            
            // O(1) hash lookup with signature verification for collision detection
            if (sharedLexicon->binaryHashes.find(hash) != sharedLexicon->binaryHashes.end() &&
                sharedLexicon->binarySignatures.find(signature) != sharedLexicon->binarySignatures.end()) {
                result.matchedWords++;
            }
        }
    }
    
    // Calculate metrics
    result.matchPercentage = (result.totalWords > 0) ? 
        (static_cast<double>(result.matchedWords) / static_cast<double>(result.totalWords)) * 100.0 : 0.0;
    
    result.score = calculateScore(result);
    result.isHighScore = result.score >= config.scoreThreshold;
    
    return result;
}

HebrewValidator::ValidationResult HebrewValidator::validateTranslationWithMapping(
    const WordSet& translatedWords,
    uint64_t mappingId,
    const std::vector<uint8_t>& mappingData) {
    
    ValidationResult result = validateTranslation(translatedWords);
    
    // Save high scores if enabled
    if (result.isHighScore && config.enableResultsSaving) {
        saveValidationResult(result, mappingId, mappingData);
    }
    
    return result;
}

double HebrewValidator::calculateScore(const ValidationResult& result) const {
    if (result.totalWords == 0) return 0.0;
    
    // Score based on match percentage with bonus for higher absolute match counts
    double baseScore = result.matchPercentage;
    
    // Bonus for absolute number of matches (logarithmic scaling)
    double matchBonus = std::log10(static_cast<double>(result.matchedWords) + 1.0) * 5.0;
    
    // Penalty for very short translations (less reliable)
    double lengthPenalty = (result.totalWords < 10) ? (10.0 - result.totalWords) * 2.0 : 0.0;
    
    double finalScore = baseScore + matchBonus - lengthPenalty;
    
    // Clamp to 0-100 range
    return std::max(0.0, std::min(100.0, finalScore));
}

bool HebrewValidator::saveValidationResult(const ValidationResult& result, uint64_t mappingId, const std::vector<uint8_t>& mappingData) {
    std::lock_guard<std::mutex> lock(resultsMutex);
    
    ValidationEntry entry;
    entry.mappingId = mappingId;
    entry.score = result.score;
    entry.matchedWords = result.matchedWords;
    entry.totalWords = result.totalWords;
    entry.timestamp = getCurrentTimestamp();
    entry.mappingData = mappingData;
    
    return appendResultToFile(entry);
}

std::string HebrewValidator::getCurrentTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::tm tm_buf;
    localtime_s(&tm_buf, &time_t);
    
    std::stringstream ss;
    ss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

bool HebrewValidator::appendResultToFile(const ValidationEntry& entry) {
    std::ofstream file(config.resultsFilePath, std::ios::app);
    if (!file.is_open()) {
        return false;
    }
    
    // Write header information
    file << "================================================================================\n";
    file << "Date/Time: " << entry.timestamp << "\n";
    file << "Mapping ID: " << entry.mappingId << "\n";
    file << "Score: " << std::fixed << std::setprecision(2) << entry.score 
         << "% (" << entry.matchedWords << "/" << entry.totalWords << " matches)\n";
    file << "================================================================================\n";
    
    // Write the mapping visualization
    if (!entry.mappingData.empty()) {
        std::string mappingViz(entry.mappingData.begin(), entry.mappingData.end());
        file << mappingViz << "\n";
    }
    
    file << "\n" << std::endl;
    
    file.close();
    return true;
}

bool HebrewValidator::isLexiconReady() const {
    return sharedLexicon && sharedLexicon->isLoaded.load();
}

HebrewValidator::LexiconStats HebrewValidator::getLexiconStats() const {
    LexiconStats stats;
    
    if (!sharedLexicon) {
        return stats;
    }
    
    std::shared_lock<std::shared_mutex> lock(sharedLexicon->lexiconMutex);
    
    stats.wordCount = sharedLexicon->wordCount;
    stats.uniqueHashes = sharedLexicon->binaryHashes.size();
    stats.uniqueSignatures = sharedLexicon->binarySignatures.size();
    stats.isLoaded = sharedLexicon->isLoaded.load();
    
    return stats;
}

HebrewValidator::ValidationResult HebrewValidator::validateWithPerformanceStats(const WordSet& translatedWords, PerformanceStats& perfStats) {
    auto startTime = std::chrono::high_resolution_clock::now();
    
    ValidationResult result = validateTranslation(translatedWords);
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    
    perfStats.wordsValidated = translatedWords.size();
    perfStats.validationTimeMs = duration.count() / 1000.0;
    perfStats.throughputWordsPerSecond = (perfStats.validationTimeMs > 0) ?
        (perfStats.wordsValidated * 1000.0 / perfStats.validationTimeMs) : 0.0;
    
    return result;
}

bool HebrewValidator::isValidHebrewBinaryVector(const std::vector<int>& binaryVector) {
    if (binaryVector.size() != 27) {
        return false;
    }
    
    // Check if vector has at least one bit set (not empty word)
    bool hasSetBit = false;
    for (int bit : binaryVector) {
        if (bit != 0 && bit != 1) {
            return false; // Invalid bit value
        }
        if (bit == 1) {
            hasSetBit = true;
        }
    }
    
    return hasSetBit;
}

HebrewValidator::HighScoresSummary HebrewValidator::getHighScoresSummary() const {
    HighScoresSummary summary;
    
    std::ifstream file(config.resultsFilePath);
    if (!file.is_open()) {
        return summary;
    }
    
    std::string line;
    double totalScore = 0.0;
    size_t totalWordsValidated = 0;
    
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string token;
        
        // Parse tab-separated values
        if (std::getline(iss, token, '\t')) { // mappingId
            if (std::getline(iss, token, '\t')) { // score
                double score = std::stod(token);
                summary.totalResults++;
                totalScore += score;
                summary.highestScore = std::max(summary.highestScore, score);
                
                if (std::getline(iss, token, '\t')) { // matchedWords
                    if (std::getline(iss, token, '\t')) { // totalWords
                        totalWordsValidated += std::stoull(token);
                    }
                }
            }
        }
    }
    
    summary.averageScore = (summary.totalResults > 0) ? (totalScore / summary.totalResults) : 0.0;
    summary.totalWordsValidated = totalWordsValidated;
    
    return summary;
}

bool HebrewValidator::clearResults() {
    std::lock_guard<std::mutex> lock(resultsMutex);
    
    std::ofstream file(config.resultsFilePath, std::ios::trunc);
    return file.is_open();
}