#include "StaticTranslator.h"
#include <chrono>
#include <algorithm>
#include <iostream>
#include <thread>
#include <future>

WordSet StaticTranslator::translateWordSet(const WordSet& evaWords, const Mapping& mapping, bool useCuda) {
    // Validate input
    if (!validateInputAlphabet(evaWords)) {
        std::wcerr << L"Warning: Some words are not in EVA alphabet" << std::endl;
    }
    
    // Get mapping matrix
    const auto& mappingMatrix = mapping.getMappingMatrix();
    
    // Convert WordSet to matrix
    auto inputMatrix = wordSetToMatrix(evaWords);
    
    // Prepare result matrix
    std::vector<std::vector<int>> resultMatrix(inputMatrix.size(), std::vector<int>(27, 0));
    
    // Perform matrix multiplication using CPU or CUDA
    if (useCuda && isCudaAvailable()) {
        performMatrixMultiplicationCuda(inputMatrix, mappingMatrix, resultMatrix);
    } else {
        performMatrixMultiplicationCpu(inputMatrix, mappingMatrix, resultMatrix);
    }
    
    // Convert back to WordSet
    return matrixToWordSet(resultMatrix, evaWords, Alphabet::HEBREW);
}

WordSet StaticTranslator::translateWordSetWithStats(const WordSet& evaWords, const Mapping& mapping, TranslationStats& stats, bool useCuda) {
    auto startTime = std::chrono::high_resolution_clock::now();
    
    WordSet result = translateWordSet(evaWords, mapping, useCuda);
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    
    stats.wordsTranslated = evaWords.size();
    stats.translationTimeMs = duration.count() / 1000.0;
    stats.throughputWordsPerSecond = (stats.translationTimeMs > 0) ? 
        (stats.wordsTranslated * 1000.0 / stats.translationTimeMs) : 0.0;
    stats.threadsUsed = 1;  // Default single-threaded
    stats.implementationType = useCuda ? "CUDA (Static)" : "CPU (Static)";
    
    return result;
}

bool StaticTranslator::validateInputAlphabet(const WordSet& words) {
    for (const auto& word : words) {
        if (word.getAlphabet() != Alphabet::EVA) {
            return false;
        }
    }
    return true;
}

// Forward declarations - implementations are in StaticCudaTranslator.cu
extern bool isCudaAvailable_impl();
extern std::string getCudaDeviceInfo_impl();

std::string StaticTranslator::getCudaDeviceInfo() {
    return getCudaDeviceInfo_impl();
}

bool StaticTranslator::isCudaAvailable() {
    return isCudaAvailable_impl();
}

std::vector<std::vector<int>> StaticTranslator::wordSetToMatrix(const WordSet& words) {
    std::vector<std::vector<int>> matrix;
    matrix.reserve(words.size());
    
    for (const auto& word : words) {
        matrix.push_back(word.getBinaryMatrix());
    }
    
    return matrix;
}

WordSet StaticTranslator::matrixToWordSet(
    const std::vector<std::vector<int>>& matrix,
    const WordSet& originalWords,
    Alphabet targetAlphabet
) {
    WordSet result;
    
    size_t wordIndex = 0;
    for (const auto& originalWord : originalWords) {
        if (wordIndex >= matrix.size()) break;
        
        // Generate Hebrew text representation
        std::wstring hebrewText = binaryToHebrewText(matrix[wordIndex]);
        
        // Create Hebrew word
        // Note: We create a temporary Hebrew word, but the binary matrix won't match
        // because Word class generates its own binary matrix from text
        Word hebrewWord(hebrewText, targetAlphabet);
        result.addWord(hebrewWord);
        
        ++wordIndex;
    }
    
    return result;
}

std::wstring StaticTranslator::binaryToHebrewText(const std::vector<int>& binaryVector) {
    const wchar_t hebrewChars[] = {
        0x05D0, // aleph א
        0x05D1, // bet ב
        0x05D2, // gimel ג
        0x05D3, // dalet ד
        0x05D4, // he ה
        0x05D5, // vav ו
        0x05D6, // zayin ז
        0x05D7, // chet ח
        0x05D8, // tet ט
        0x05D9, // yod י
        0x05DB, // kaf כ
        0x05DC, // lamed ל
        0x05DE, // mem מ
        0x05E0, // nun נ
        0x05E1, // samech ס
        0x05E2, // ayin ע
        0x05E4, // pe פ
        0x05E6, // tsadi צ
        0x05E7, // qof ק
        0x05E8, // resh ר
        0x05E9, // shin ש
        0x05EA, // tav ת
        0x05DA, // kaf final ך
        0x05DD, // mem final ם
        0x05DF, // nun final ן
        0x05E3, // pe final ף
        0x05E5  // tsadi final ץ
    };
    
    std::wstring result;
    
    for (size_t i = 0; i < std::min(binaryVector.size(), size_t(27)); ++i) {
        if (binaryVector[i]) {
            result += hebrewChars[i];
        }
    }
    
    return result.empty() ? L"" : result;
}

size_t StaticTranslator::getOptimalThreadCount() {
    size_t hardwareThreads = std::thread::hardware_concurrency();
    return hardwareThreads > 0 ? hardwareThreads : 4;
}

void StaticTranslator::performMatrixMultiplicationCpu(
    const std::vector<std::vector<int>>& inputMatrix,
    const std::vector<std::vector<int>>& transformMatrix,
    std::vector<std::vector<int>>& resultMatrix
) {
    matrixMultiplyBatch(inputMatrix, transformMatrix, resultMatrix, 0, inputMatrix.size());
}

// CUDA implementation is in StaticCudaTranslator.cu
// This declaration is needed for linking

void StaticTranslator::matrixMultiplyBatch(
    const std::vector<std::vector<int>>& inputMatrix,
    const std::vector<std::vector<int>>& transformMatrix,
    std::vector<std::vector<int>>& resultMatrix,
    size_t startRow,
    size_t endRow
) {
    // Optimized matrix multiplication: inputMatrix * transformMatrix = resultMatrix
    // inputMatrix: N x 27, transformMatrix: 27 x 27, result: N x 27
    
    for (size_t i = startRow; i < endRow; ++i) {
        for (size_t j = 0; j < 27; ++j) {
            int sum = 0;
            
            // Vectorized inner product with loop unrolling
            const auto& inputRow = inputMatrix[i];
            size_t k = 0;
            
            // Unroll by 4 for better performance
            for (; k + 3 < 27; k += 4) {
                sum += (inputRow[k] & transformMatrix[k][j]) +
                       (inputRow[k+1] & transformMatrix[k+1][j]) +
                       (inputRow[k+2] & transformMatrix[k+2][j]) +
                       (inputRow[k+3] & transformMatrix[k+3][j]);
            }
            
            // Handle remaining elements
            for (; k < 27; ++k) {
                sum += inputRow[k] & transformMatrix[k][j];
            }
            
            // For binary matrices, any non-zero result becomes 1
            resultMatrix[i][j] = (sum > 0) ? 1 : 0;
        }
    }
}