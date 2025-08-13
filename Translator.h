#pragma once

#include "WordSet.h"
#include "Mapping.h"
#include <vector>
#include <memory>
#include <thread>
#include <future>

class Translator {
private:
    std::unique_ptr<Mapping> mapping;
    
    // High-performance matrix multiplication implementation
    static void matrixMultiplyBatch(
        const std::vector<std::vector<int>>& inputMatrix,    // N x 27 matrix (N words)
        const std::vector<std::vector<int>>& transformMatrix, // 27 x 27 mapping matrix
        std::vector<std::vector<int>>& resultMatrix,          // N x 27 result matrix
        size_t startRow,
        size_t endRow
    );
    
    // Convert WordSet to matrix representation
    std::vector<std::vector<int>> wordSetToMatrix(const WordSet& words) const;
    
    // Convert matrix back to WordSet
    WordSet matrixToWordSet(
        const std::vector<std::vector<int>>& matrix,
        const WordSet& originalWords,
        Alphabet targetAlphabet
    ) const;
    
    // Generate Hebrew text representation from binary vector
    std::wstring binaryToHebrewText(const std::vector<int>& binaryVector) const;
    
    // Number of threads to use for parallel processing
    size_t getOptimalThreadCount() const;
    
public:
    Translator();
    explicit Translator(std::unique_ptr<Mapping> mapping);
    
    // Mapping management (no locking needed - each thread has its own instance)
    void setMapping(std::unique_ptr<Mapping> newMapping);
    const Mapping& getMapping() const;
    
    // High-performance batch translation using single matrix multiplication
    WordSet translateWordSet(const WordSet& evaWords) const;
    
    // Parallel batch translation for very large word sets
    WordSet translateWordSetParallel(const WordSet& evaWords, size_t numThreads = 0) const;
    
    // Performance statistics
    struct TranslationStats {
        size_t wordsTranslated;
        double translationTimeMs;
        double throughputWordsPerSecond;
        size_t threadsUsed;
    };
    
    // Translate with performance metrics
    WordSet translateWithStats(const WordSet& evaWords, TranslationStats& stats) const;
    
    // Validate that all words are EVA alphabet
    bool validateInputAlphabet(const WordSet& words) const;
};