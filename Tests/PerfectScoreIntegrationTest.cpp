#include "TestFramework.h"
#include "../VoynichDecoder.h"
#include "../MappingGenerator.h"
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <random>
#include <iostream>
#include <thread>
#include <chrono>
#include <unordered_map>

class PerfectScoreIntegrationTest {
private:
    std::vector<std::string> evaWords;
    std::vector<std::string> hebrewWords;
    std::string testVoynichFile;
    std::string testHebrewFile;
    std::string testStateFile;
    std::string testResultsFile;
    
    // Helper function to create EVA words that map to Hebrew words under identity mapping
    std::vector<std::string> createEvaWordsFromHebrew(const std::vector<std::string>& hebrewWordList) {
        // Hebrew to index mapping (from Word.cpp)
        std::unordered_map<wchar_t, int> hebrewToIndex = {
            {0x05D0, 0}, {0x05D1, 1}, {0x05D2, 2}, {0x05D3, 3}, {0x05D4, 4}, {0x05D5, 5}, {0x05D6, 6},
            {0x05D7, 7}, {0x05D8, 8}, {0x05D9, 9}, {0x05DB, 10}, {0x05DC, 11}, {0x05DE, 12}, {0x05E0, 13},
            {0x05E1, 14}, {0x05E2, 15}, {0x05E4, 16}, {0x05E6, 17}, {0x05E7, 18}, {0x05E8, 19}, {0x05E9, 20},
            {0x05EA, 21}, {0x05DA, 22}, {0x05DD, 23}, {0x05DF, 24}, {0x05E3, 25}, {0x05E5, 26}
        };
        
        // Index to EVA character mapping
        std::unordered_map<int, char> indexToEva = {
            {0, 'a'}, {1, 'b'}, {2, 'c'}, {3, 'd'}, {4, 'e'}, {5, 'f'}, {6, 'g'}, {7, 'h'}, {8, 'i'}, {9, 'j'},
            {10, 'k'}, {11, 'l'}, {12, 'm'}, {13, 'n'}, {14, 'o'}, {15, 'p'}, {16, 'q'}, {17, 'r'}, {18, 's'},
            {19, 't'}, {20, 'u'}, {21, 'v'}, {22, 'w'}, {23, 'x'}, {24, 'y'}, {25, 'z'}, {26, ' '}
        };
        
        std::vector<std::string> result;
        
        for (size_t wordIndex = 0; wordIndex < hebrewWordList.size(); ++wordIndex) {
            const auto& hebrewWord = hebrewWordList[wordIndex];
            std::string evaWord;
            
            // Parse UTF-8 Hebrew string byte by byte
            for (size_t i = 0; i < hebrewWord.length(); ) {
                uint32_t codepoint = 0;
                int bytesToRead = 0;
                
                // Determine UTF-8 sequence length and extract codepoint
                unsigned char byte = hebrewWord[i];
                if (byte < 0x80) {
                    // 1-byte ASCII
                    codepoint = byte;
                    bytesToRead = 1;
                } else if ((byte & 0xE0) == 0xC0) {
                    // 2-byte sequence
                    codepoint = byte & 0x1F;
                    bytesToRead = 2;
                } else if ((byte & 0xF0) == 0xE0) {
                    // 3-byte sequence (Hebrew is here)
                    codepoint = byte & 0x0F;
                    bytesToRead = 3;
                } else if ((byte & 0xF8) == 0xF0) {
                    // 4-byte sequence
                    codepoint = byte & 0x07;
                    bytesToRead = 4;
                } else {
                    // Invalid UTF-8
                    i++;
                    continue;
                }
                
                // Read the remaining bytes
                for (int j = 1; j < bytesToRead && (i + j) < hebrewWord.length(); j++) {
                    unsigned char nextByte = hebrewWord[i + j];
                    if ((nextByte & 0xC0) == 0x80) {
                        codepoint = (codepoint << 6) | (nextByte & 0x3F);
                    } else {
                        // Invalid continuation byte
                        break;
                    }
                }
                
                // Look up Hebrew character in mapping
                wchar_t hebrewChar = static_cast<wchar_t>(codepoint);
                auto it = hebrewToIndex.find(hebrewChar);
                if (it != hebrewToIndex.end()) {
                    int index = it->second;
                    auto evaIt = indexToEva.find(index);
                    if (evaIt != indexToEva.end()) {
                        evaWord += evaIt->second;
                    }
                }
                
                i += bytesToRead;
            }
            
            // For RTL->LTR conversion, reverse the EVA word
            std::reverse(evaWord.begin(), evaWord.end());
            
            // Ensure we have at least some content
            if (evaWord.empty()) {
                evaWord = "abc"; // Fallback
            }
            
            if (wordIndex < 5) {
                std::cout << "DEBUG: Hebrew word " << wordIndex << ": '" << hebrewWord << "' -> EVA: '" << evaWord << "'" << std::endl;
            }
            
            result.push_back(evaWord);
        }
        
        return result;
    }

    void createMatchingWordPairs() {
        // Use actual Hebrew words from Tanah2.txt (100 real words with minimum 3 characters)
        hebrewWords = {
            "תנך", "מנוקד", "מפתח", "הבית", "חיפוש", "באתר", "משנה", "תורה", "להרמבם", "בראשית",
            "שמות", "ויקרא", "במדבר", "דברים", "נביאים", "יהושוע", "שופטים", "שמואל", "מלכים", "ישעיהו",
            "ירמיהו", "יחזקאל", "הושע", "יואל", "עמוס", "עובדיה", "יונה", "מיכה", "נחום", "חבקוק",
            "צפניה", "חגיי", "זכריה", "מלאכי", "כתובים", "דברי", "הימים", "תהילים", "איוב", "משלי",
            "שיר", "השירים", "קוהלת", "איכה", "אסתר", "דנייאל", "עזרא", "נחמיה", "סדר", "הקריאות",
            "בבתי", "הכנסת", "הערות", "בהכנה", "התנך", "התורה", "הנביאים", "הכתובים", "וכתובים", "בכתיב",
            "המסורה", "הכתר", "וכתבי", "הקרובים", "מהדורת", "חשוון", "התשעח", "הזכויות", "שמורות", "למכון",
            "ממרא", "רחוב", "חיים", "ויטל", "ירושלים", "הפיסוק", "טעמי", "המקרא", "בקיצור", "כבדות",
            "ההפסק", "הכבד", "והמקף", "מעין", "אנטי", "פיסוק", "שעושה", "מילים", "יותר", "לתיבה",
            "שאלה", "הערה", "אייר", "התשעב", "אלהים", "השמים", "הארץ", "והארץ", "היתה", "ובהו"
        };
        
        // Create EVA words using the createEvaWordsFromHebrew function (accounting for RTL->LTR)
        evaWords = createEvaWordsFromHebrew(hebrewWords);
        
        // Debug: Show first few mappings
        std::cout << "Sample Hebrew -> EVA mappings from real Tanah2.txt words:" << std::endl;
        for (size_t i = 0; i < std::min(size_t(5), hebrewWords.size()); ++i) {
            std::cout << "  " << hebrewWords[i] << " -> " << evaWords[i] << std::endl;
        }
        
        std::cout << "Using 100 real Hebrew words from Tanah2.txt with EVA mappings" << std::endl;
    }
    
    void createTestVoynichFile() {
        testVoynichFile = "test_voynich_words.txt";
        std::ofstream outFile(testVoynichFile);
        
        if (!outFile.is_open()) {
            std::cout << "ERROR: Could not create " << testVoynichFile << std::endl;
            return;
        }
        
        std::cout << "Writing " << evaWords.size() << " EVA words to " << testVoynichFile << std::endl;
        for (size_t i = 0; i < evaWords.size(); ++i) {
            outFile << evaWords[i] << std::endl;
            if (i < 5) {
                std::cout << "  EVA word " << i << ": '" << evaWords[i] << "'" << std::endl;
            }
        }
        outFile.close();
        
        // Verify file was created
        std::ifstream verifyFile(testVoynichFile);
        if (verifyFile.good()) {
            std::cout << "Successfully created " << testVoynichFile << std::endl;
        } else {
            std::cout << "ERROR: Failed to create " << testVoynichFile << std::endl;
        }
        verifyFile.close();
    }
    
    void createTestHebrewFile() {
        testHebrewFile = "test_hebrew_words.txt";
        std::ofstream outFile(testHebrewFile, std::ios::out | std::ios::binary);
        
        // Write UTF-8 BOM
        outFile.write("\xEF\xBB\xBF", 3);
        
        for (const auto& word : hebrewWords) {
            outFile << word << std::endl;
        }
        outFile.close();
    }
    
    void createIdentityMappingStateFile() {
        testStateFile = "test_mapping_state.dat";
        
        // The identity mapping corresponds to global index 0 in the factorial number system
        // Global index 0 means: permutation [0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26]
        // This is in block 0 (since startIndex = blockIndex * blockSize, and 0 / blockSize = 0)
        // Position within block 0 = 0 % blockSize = 0
        
        MappingGenerator::GeneratorState state;
        state.blockIndex = 0;           // Identity mapping is in block 0
        state.mappingIndexInBlock = 0;  // Identity mapping is at position 0 in block 0  
        state.totalMappingsGenerated = 0; // No mappings have been generated yet
        state.isComplete = false;       // Generation is not complete
        
        std::ofstream stateFile(testStateFile, std::ios::binary);
        stateFile.write(reinterpret_cast<const char*>(&state.blockIndex), sizeof(state.blockIndex));
        stateFile.write(reinterpret_cast<const char*>(&state.mappingIndexInBlock), sizeof(state.mappingIndexInBlock));
        stateFile.write(reinterpret_cast<const char*>(&state.totalMappingsGenerated), sizeof(state.totalMappingsGenerated));
        stateFile.write(reinterpret_cast<const char*>(&state.isComplete), sizeof(state.isComplete));
        stateFile.close();
        
        std::cout << "Created state file to start at block 0, position 0 (identity mapping)" << std::endl;
        
        // Verify the state file was created correctly
        std::ifstream verifyFile(testStateFile, std::ios::binary);
        if (verifyFile.good()) {
            MappingGenerator::GeneratorState readState;
            verifyFile.read(reinterpret_cast<char*>(&readState.blockIndex), sizeof(readState.blockIndex));
            verifyFile.read(reinterpret_cast<char*>(&readState.mappingIndexInBlock), sizeof(readState.mappingIndexInBlock));
            verifyFile.read(reinterpret_cast<char*>(&readState.totalMappingsGenerated), sizeof(readState.totalMappingsGenerated));
            verifyFile.read(reinterpret_cast<char*>(&readState.isComplete), sizeof(readState.isComplete));
            verifyFile.close();
            
            std::cout << "Verified state file: blockIndex=" << readState.blockIndex 
                      << ", mappingIndexInBlock=" << readState.mappingIndexInBlock 
                      << ", totalGenerated=" << readState.totalMappingsGenerated 
                      << ", complete=" << readState.isComplete << std::endl;
        }
    }
    
    void cleanup() {
        std::remove(testVoynichFile.c_str());
        std::remove(testHebrewFile.c_str());
        std::remove(testStateFile.c_str());
        if (!testResultsFile.empty()) {
            std::remove(testResultsFile.c_str());
        }
    }

private:
    void runPerfectScoreTestWithTranslator(VoynichDecoder::TranslatorType translatorType, const std::string& testName) {
        std::cout << "=== " << testName << " (with Hebrew from Tanah2.txt) ===" << std::endl;
        
        // Setup test data with matching EVA/Hebrew word pairs
        createMatchingWordPairs();
        createTestVoynichFile();
        createTestHebrewFile();
        createIdentityMappingStateFile();
        
        std::cout << "Created " << evaWords.size() << " EVA words and " << hebrewWords.size() << " Hebrew words for test" << std::endl;
        std::cout << "Using real long Hebrew biblical words from Tanah2.txt with EVA patterns" << std::endl;
        std::cout << "MappingGenerator positioned to produce identity mapping (global index 0)" << std::endl;
        
        // Configure decoder to find the perfect score with identity mapping
        VoynichDecoder::DecoderConfig config;
        config.voynichWordsPath = testVoynichFile;
        config.hebrewLexiconPath = testHebrewFile;  // Use our test Hebrew lexicon
        config.scoreThreshold = 95.0;    // Low threshold to see all scores
        config.maxMappingsToProcess = 10;  // Process more mappings with 100 words
        config.mappingBlockSize = 15;   // Slightly larger block size
        config.generatorStateFile = testStateFile;
        config.numThreads = 1;  // Single thread for predictable behavior
        config.statusUpdateIntervalMs = 1000;  // Frequent updates
        config.translatorType = translatorType;
        
        testResultsFile = std::string("test_perfect_score_results_") + 
            (translatorType == VoynichDecoder::TranslatorType::CPU ? "cpu" : "cuda") + ".txt";
        config.resultsFilePath = testResultsFile;
        
        try {
            VoynichDecoder decoder(config);
            
            std::cout << "Initializing decoder..." << std::endl;
            ASSERT_TRUE(decoder.initialize());
            
            std::cout << "Starting decoding process..." << std::endl;
            
            // Start decoder in a controlled way
            decoder.start();
            
            // Wait for processing (should be quick with identity mapping)
            std::this_thread::sleep_for(std::chrono::seconds(5));
            
            // Stop decoder
            decoder.stop();
            decoder.waitForCompletion();
            
            auto stats = decoder.getCurrentStats();
            
            std::cout << "Decoding completed!" << std::endl;
            std::cout << "Mappings processed: " << stats.totalMappingsProcessed << std::endl;
            std::cout << "Highest score achieved: " << stats.highestScore << "%" << std::endl;
            std::cout << "High scores found: " << stats.highScoreCount << std::endl;
            
            // Verify that we processed some mappings
            ASSERT_TRUE(stats.totalMappingsProcessed > 0);
            
            // With EVA words and matching Hebrew words, the identity mapping should score very highly
            // The first mapping (index 0) creates an identity transformation
            std::cout << "System successfully processed mappings and completed without errors." << std::endl;
            std::cout << "Expected high score due to identity mapping between EVA and Hebrew words." << std::endl;
            
            // Debug: Print actual score achieved
            std::cout << "DEBUG: Actual highest score achieved: " << stats.highestScore << "%" << std::endl;
            std::cout << "DEBUG: Total mappings processed: " << stats.totalMappingsProcessed << std::endl;
            std::cout << "DEBUG: High score count: " << stats.highScoreCount << std::endl;
            
            // With correctly mapped EVA words and identity mapping, should achieve very high scores
            // Each EVA word maps character-by-character to its corresponding Hebrew word
            std::cout << "Identity mapping with character-mapped EVA words achieved: " << stats.highestScore << "%" << std::endl;
            
            // Verify system completed successfully and achieved high scores with proper mapping
            ASSERT_TRUE(stats.totalMappingsProcessed >= 10);
            ASSERT_TRUE(stats.highestScore >= 95.0); // Should achieve high scores with correct RTL->LTR mapping
            
            // Check that results file was created (may not exist if no high scores found)
            std::ifstream resultsFile(testResultsFile);
            if (resultsFile.good()) {
                std::cout << "Results file was created successfully." << std::endl;
            } else {
                std::cout << "No results file created (no scores exceeded threshold)." << std::endl;
            }
            
            std::cout << "✓ " << testName << " passed!" << std::endl;
            
        } catch (const std::exception& e) {
            std::cout << "Test failed with exception: " << e.what() << std::endl;
            cleanup();
            throw;
        }
         
        cleanup();
    }

public:
    void runPerfectScoreTestCPU() {
        runPerfectScoreTestWithTranslator(VoynichDecoder::TranslatorType::CPU, "Perfect Score Integration Test (CPU)");
    }
    
    void runPerfectScoreTestCUDA() {
        runPerfectScoreTestWithTranslator(VoynichDecoder::TranslatorType::CUDA, "Perfect Score Integration Test (CUDA)");
    }
};

void testPerfectScoreIntegrationCPU() {
    PerfectScoreIntegrationTest test;
    test.runPerfectScoreTestCPU();
}

void testPerfectScoreIntegrationCUDA() {
    PerfectScoreIntegrationTest test;
    test.runPerfectScoreTestCUDA();
}

void registerPerfectScoreTest(TestFramework& framework) {
    framework.addTest("Perfect Score Integration Test (CPU)", testPerfectScoreIntegrationCPU);
    framework.addTest("Perfect Score Integration Test (CUDA)", testPerfectScoreIntegrationCUDA);
}