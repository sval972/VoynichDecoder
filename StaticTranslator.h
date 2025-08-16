#pragma once

#include "WordSet.h"
#include "Mapping.h"
#include <vector>
#include <string>

// Static translator interface - no instances needed, all methods are static
class StaticTranslator {
public:
    // Translation statistics
    struct TranslationStats {
        size_t wordsTranslated;
        double translationTimeMs;
        double throughputWordsPerSecond;
        size_t threadsUsed;
        std::string implementationType;
    };

    // Main translation methods
    static WordSet translateWordSet(const WordSet& evaWords, const Mapping& mapping, bool useCuda = false);
    static WordSet translateWordSetWithStats(const WordSet& evaWords, const Mapping& mapping, TranslationStats& stats, bool useCuda = false);
    
    // Utility methods
    static bool validateInputAlphabet(const WordSet& words);
    static std::string getCudaDeviceInfo();
    static bool isCudaAvailable();
    
    // Matrix conversion utilities (public for batch processing)
    static std::vector<std::vector<int>> wordSetToMatrix(const WordSet& words);
    static WordSet matrixToWordSet(
        const std::vector<std::vector<int>>& matrix,
        const WordSet& originalWords,
        Alphabet targetAlphabet
    );
    
    // High-performance batch processing for multiple mappings
    static void performBatchMatrixMultiplicationCuda(
        const std::vector<std::vector<int>>& inputMatrix,
        const std::vector<std::vector<std::vector<int>>>& transformMatrices,
        std::vector<std::vector<std::vector<int>>>& resultMatrices
    );

private:
    // Internal utility methods
    static std::wstring binaryToHebrewText(const std::vector<int>& binaryVector);
    static size_t getOptimalThreadCount();
    
    // Core matrix multiplication implementations
    static void performMatrixMultiplicationCpu(
        const std::vector<std::vector<int>>& inputMatrix,
        const std::vector<std::vector<int>>& transformMatrix,
        std::vector<std::vector<int>>& resultMatrix
    );
    
    static void performMatrixMultiplicationCuda(
        const std::vector<std::vector<int>>& inputMatrix,
        const std::vector<std::vector<int>>& transformMatrix,
        std::vector<std::vector<int>>& resultMatrix
    );
    
    // CPU-specific optimized batch processing
    static void matrixMultiplyBatch(
        const std::vector<std::vector<int>>& inputMatrix,
        const std::vector<std::vector<int>>& transformMatrix,
        std::vector<std::vector<int>>& resultMatrix,
        size_t startRow,
        size_t endRow
    );
};