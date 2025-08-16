#include "ThreadManager.h"
#include "StaticTranslator.h"
#include <iostream>
#include <locale>
#include <exception>

int main()
{
    // Set console to handle Unicode output
    std::wcout.imbue(std::locale(""));
    
    std::wcout << L"Voynich Manuscript Decoder" << std::endl;
    std::wcout << L"=========================" << std::endl;
    std::wcout << L"Systematic analysis of EVA-to-Hebrew translation mappings" << std::endl;
    std::wcout << std::endl;
    
    // Display available translator implementations
    std::wcout << L"Available Translator Implementations:" << std::endl;
    std::wcout << L"  CPU  - High-performance CPU implementation with multi-threading" << std::endl;
    std::wcout << L"  CUDA - GPU-accelerated implementation (if CUDA is available)" << std::endl;
    std::wcout << L"  AUTO - Automatically choose best available implementation" << std::endl;
    std::wcout << std::endl;
    
    // Check CUDA availability
    bool cudaAvailable = StaticTranslator::isCudaAvailable();
    std::wcout << L"CUDA Status: " << (cudaAvailable ? L"Available" : L"Not Available") << std::endl;
    if (cudaAvailable) {
        std::wcout << L"CUDA Device: " << StaticTranslator::getCudaDeviceInfo().c_str() << std::endl;
    }
    std::wcout << std::endl;
    
    // Configure the thread manager
    ThreadManager::ThreadManagerConfig config;
    config.numThreads = 10;  // 0 - Auto-detect optimal thread count
    
    // Choose translator implementation
    // Options: VoynichDecoder::TranslatorType::CPU, CUDA, or AUTO
    //config.translatorType = VoynichDecoder::TranslatorType::AUTO;  // Let system choose best
    
    // Alternative configurations:
     config.translatorType = VoynichDecoder::TranslatorType::CPU;   // Force CPU implementation
     //config.translatorType = VoynichDecoder::TranslatorType::CUDA;  // Force CUDA (will throw exception if unavailable)
    
    // Note: If you force CUDA on a system without CUDA, the decoder will throw an exception
    // Use AUTO for automatic fallback to CPU when CUDA is not available
    
    config.voynichWordsPath = "resources/Script_freq100.txt";
    config.hebrewLexiconPath = "resources/Tanah2.txt";
    config.resultsFilePath = "voynich_analysis_results.txt";
    config.scoreThreshold = 45.0;  // Save results with 45%+ Hebrew word matches
    config.statusUpdateIntervalMs = 5000;  // Status update every 5 seconds
    config.maxMappingsToProcess = 0;  // Limited for batch CUDA performance testing
    config.mappingBlockSize = 1000000;  // 1M mappings per generator block
    
    // Create and run the thread manager with exception handling
    try {
        ThreadManager threadManager(config);
        threadManager.runDecoding();
        
        std::wcout << L"\nVoynich Decoder completed. Check " << config.resultsFilePath.c_str() 
                   << L" for any high-scoring translation results." << std::endl;
    } catch (const std::exception& e) {
        // Check if it's a CUDA-related error
        std::string errorMsg = e.what();
        if (errorMsg.find("CUDA") != std::string::npos) {
            std::wcerr << L"\nCUDA Error: " << errorMsg.c_str() << std::endl;
            std::wcerr << L"Try using CPU mode instead: config.translatorType = VoynichDecoder::TranslatorType::CPU;" << std::endl;
        } else {
            std::wcerr << L"\nError: " << errorMsg.c_str() << std::endl;
        }
        return 1;
    }
    
    return 0;
}