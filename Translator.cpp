#include "Translator.h"
#include <chrono>
#include <algorithm>
#include <iostream>

Translator::Translator() : mapping(std::make_unique<Mapping>()) {
    mapping->createEvaToHebrewMapping();
}

Translator::Translator(std::unique_ptr<Mapping> mapping) : mapping(std::move(mapping)) {
}

void Translator::setMapping(std::unique_ptr<Mapping> newMapping) {
    mapping = std::move(newMapping);
}

const Mapping& Translator::getMapping() const {
    return *mapping;
}

void Translator::matrixMultiplyBatch(
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
            size_t k = 0;
            for (; k + 3 < 27; k += 4) {
                sum += (inputMatrix[i][k] & transformMatrix[k][j]) +
                       (inputMatrix[i][k + 1] & transformMatrix[k + 1][j]) +
                       (inputMatrix[i][k + 2] & transformMatrix[k + 2][j]) +
                       (inputMatrix[i][k + 3] & transformMatrix[k + 3][j]);
            }
            
            // Handle remaining elements
            for (; k < 27; ++k) {
                sum += inputMatrix[i][k] & transformMatrix[k][j];
            }
            
            // For binary matrices, any non-zero result becomes 1
            resultMatrix[i][j] = (sum > 0) ? 1 : 0;
        }
    }
}

std::vector<std::vector<int>> Translator::wordSetToMatrix(const WordSet& words) const {
    std::vector<std::vector<int>> matrix;
    matrix.reserve(words.size());
    
    for (const auto& word : words) {
        matrix.push_back(word.getBinaryMatrix());
    }
    
    return matrix;
}

WordSet Translator::matrixToWordSet(
    const std::vector<std::vector<int>>& matrix,
    const WordSet& originalWords,
    Alphabet targetAlphabet
) const {
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

std::wstring Translator::binaryToHebrewText(const std::vector<int>& binaryVector) const {
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

size_t Translator::getOptimalThreadCount() const {
    size_t hardwareThreads = std::thread::hardware_concurrency();
    return hardwareThreads > 0 ? hardwareThreads : 4; // Default to 4 if detection fails
}

WordSet Translator::translateWordSet(const WordSet& evaWords) const {
    if (evaWords.size() == 0) {
        return WordSet();
    }
    
    // Validate input
    if (!validateInputAlphabet(evaWords)) {
        std::wcerr << L"Warning: Some words are not in EVA alphabet" << std::endl;
    }
    
    // Get mapping (no locking needed - each thread has its own instance)
    const auto& mappingMatrix = mapping->getMappingMatrix();
    
    // Convert WordSet to matrix
    auto inputMatrix = wordSetToMatrix(evaWords);
    
    // Prepare result matrix
    std::vector<std::vector<int>> resultMatrix(inputMatrix.size(), std::vector<int>(27, 0));
    
    // Perform batch matrix multiplication
    matrixMultiplyBatch(inputMatrix, mappingMatrix, resultMatrix, 0, inputMatrix.size());
    
    // Convert back to WordSet
    return matrixToWordSet(resultMatrix, evaWords, Alphabet::HEBREW);
}

WordSet Translator::translateWordSetParallel(const WordSet& evaWords, size_t numThreads) const {
    if (evaWords.size() == 0) {
        return WordSet();
    }
    
    if (numThreads == 0) {
        numThreads = getOptimalThreadCount();
    }
    
    // For small word sets, use single-threaded approach
    if (evaWords.size() < numThreads * 10) {
        return translateWordSet(evaWords);
    }
    
    // Validate input
    if (!validateInputAlphabet(evaWords)) {
        std::wcerr << L"Warning: Some words are not in EVA alphabet" << std::endl;
    }
    
    // Get mapping (no locking needed - each thread has its own instance)
    const auto& mappingMatrix = mapping->getMappingMatrix();
    
    // Convert WordSet to matrix
    auto inputMatrix = wordSetToMatrix(evaWords);
    
    // Prepare result matrix
    std::vector<std::vector<int>> resultMatrix(inputMatrix.size(), std::vector<int>(27, 0));
    
    // Calculate work distribution
    size_t wordsPerThread = inputMatrix.size() / numThreads;
    size_t remainder = inputMatrix.size() % numThreads;
    
    // Launch parallel matrix multiplication
    std::vector<std::future<void>> futures;
    futures.reserve(numThreads);
    
    size_t startRow = 0;
    for (size_t t = 0; t < numThreads; ++t) {
        size_t endRow = startRow + wordsPerThread + (t < remainder ? 1 : 0);
        
        futures.emplace_back(std::async(std::launch::async, 
            [&inputMatrix, &mappingMatrix, &resultMatrix, startRow, endRow]() {
                matrixMultiplyBatch(inputMatrix, mappingMatrix, resultMatrix, startRow, endRow);
            }));
        
        startRow = endRow;
    }
    
    // Wait for all threads to complete
    for (auto& future : futures) {
        future.wait();
    }
    
    // Convert back to WordSet
    return matrixToWordSet(resultMatrix, evaWords, Alphabet::HEBREW);
}

WordSet Translator::translateWithStats(const WordSet& evaWords, TranslationStats& stats) const {
    auto startTime = std::chrono::high_resolution_clock::now();
    
    size_t numThreads = getOptimalThreadCount();
    WordSet result = translateWordSetParallel(evaWords, numThreads);
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    
    stats.wordsTranslated = evaWords.size();
    stats.translationTimeMs = duration.count() / 1000.0;
    stats.throughputWordsPerSecond = (stats.translationTimeMs > 0) ? 
        (stats.wordsTranslated * 1000.0 / stats.translationTimeMs) : 0.0;
    stats.threadsUsed = numThreads;
    
    return result;
}

bool Translator::validateInputAlphabet(const WordSet& words) const {
    for (const auto& word : words) {
        if (word.getAlphabet() != Alphabet::EVA) {
            return false;
        }
    }
    return true;
}