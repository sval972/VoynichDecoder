#include "VoynichDecoder.h"
#include <iostream>
#include <locale>

int main()
{
    // Set console to handle Unicode output
    std::wcout.imbue(std::locale(""));
    
    std::wcout << L"Voynich Manuscript Decoder" << std::endl;
    std::wcout << L"=========================" << std::endl;
    std::wcout << L"Systematic analysis of EVA-to-Hebrew translation mappings" << std::endl;
    std::wcout << std::endl;
    
    // Configure the decoder
    VoynichDecoder::DecoderConfig config;
    config.numThreads = 6;  // 0 - Auto-detect optimal thread count
    config.voynichWordsPath = "resources/Script_freq100.txt";
    config.hebrewLexiconPath = "resources/Tanah2.txt";
    config.resultsFilePath = "voynich_analysis_results.txt";
    config.scoreThreshold = 45.0;  // Save results with 45%+ Hebrew word matches
    config.statusUpdateIntervalMs = 5000;  // Status update every 5 seconds
    config.maxMappingsToProcess = 0;  // Unlimited for long-running analysis
    config.mappingBlockSize = 1000000;  // 1M mappings per generator block
    
    // Create and run the decoder
    VoynichDecoder decoder(config);
    decoder.runDecoding();
    
    std::wcout << L"\nVoynich Decoder completed. Check " << config.resultsFilePath.c_str() 
               << L" for any high-scoring translation results." << std::endl;
    
    return 0;
}