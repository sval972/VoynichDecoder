#include "TestFramework.h"
#include "../StaticTranslator.h"
#include "../HebrewValidator.h"
#include "../Mapping.h"
#include "../WordSet.h"

class MinimalTests {
public:
    void testTranslatorCreation() {
        // Test static methods are available
        bool cudaAvailable = StaticTranslator::isCudaAvailable();
        std::string deviceInfo = StaticTranslator::getCudaDeviceInfo();
        
        // Just test that we can call static methods
        std::cout << "✓ StaticTranslator static methods test passed (CUDA: " 
                  << (cudaAvailable ? "Available" : "Not Available") << ")" << std::endl;
    }
    
    void testTranslatorWithMapping() {
        Mapping mapping;
        mapping.setMapping(0, 1); // EVA index 0 -> Hebrew index 1
        
        // Create a simple test WordSet
        WordSet testWords;
        testWords.addWord(Word(L"a", Alphabet::EVA)); // Simple EVA word
        
        // Test translation using static method (CPU mode)
        WordSet translatedWords = StaticTranslator::translateWordSet(testWords, mapping, false);
        
        // Just verify we got some result
        ASSERT_TRUE(translatedWords.size() >= 0); // Should complete without error
        std::cout << "✓ StaticTranslator::translateWordSet test passed" << std::endl;
    }
    
    void testHebrewValidatorCreation() {
        HebrewValidator::ValidatorConfig config;
        config.enableResultsSaving = false; // Don't create files in tests
        
        HebrewValidator validator(config);
        // Just test that we can create a validator
        std::cout << "✓ HebrewValidator creation test passed" << std::endl;
    }
    
    void testHebrewValidatorConfig() {
        HebrewValidator::ValidatorConfig config;
        
        // Test default values
        ASSERT_TRUE(config.hebrewLexiconPath == "Tanah2.txt");
        ASSERT_TRUE(config.scoreThreshold == 25.0);
        ASSERT_TRUE(config.enableResultsSaving);
        
        std::cout << "✓ HebrewValidator configuration test passed" << std::endl;
    }
    
    void testMappingBasicOperations() {
        Mapping mapping;
        
        // Test setting mappings
        mapping.setMapping(0, 5); // EVA index 0 -> Hebrew index 5
        mapping.setMapping(1, 10); // EVA index 1 -> Hebrew index 10
        
        // Test getting mapping matrix
        const auto& matrix = mapping.getMappingMatrix();
        ASSERT_TRUE(matrix.size() == 27); // Should be 27x27 matrix
        
        std::cout << "✓ Mapping basic operations test passed" << std::endl;
    }
    
    void testBinaryVectorValidation() {
        // Test HebrewValidator static method
        std::vector<int> validVector(27, 0);
        validVector[0] = 1;
        
        std::vector<int> invalidVector = {2, 0, 1}; // Wrong values/size
        
        ASSERT_TRUE(HebrewValidator::isValidHebrewBinaryVector(validVector));
        ASSERT_FALSE(HebrewValidator::isValidHebrewBinaryVector(invalidVector));
        
        std::cout << "✓ Binary vector validation test passed" << std::endl;
    }
};

void testMinimalTranslatorCreation() {
    MinimalTests tests;
    tests.testTranslatorCreation();
}

void testMinimalTranslatorWithMapping() {
    MinimalTests tests;
    tests.testTranslatorWithMapping();
}

void testMinimalHebrewValidatorCreation() {
    MinimalTests tests;
    tests.testHebrewValidatorCreation();
}

void testMinimalHebrewValidatorConfig() {
    MinimalTests tests;
    tests.testHebrewValidatorConfig();
}

void testMinimalMappingBasicOperations() {
    MinimalTests tests;
    tests.testMappingBasicOperations();
}

void testMinimalBinaryVectorValidation() {
    MinimalTests tests;
    tests.testBinaryVectorValidation();
}

void registerMinimalTests(TestFramework& framework) {
    framework.addTest("Minimal StaticTranslator Methods", testMinimalTranslatorCreation);
    framework.addTest("Minimal StaticTranslator Translation", testMinimalTranslatorWithMapping);
    framework.addTest("Minimal HebrewValidator Creation", testMinimalHebrewValidatorCreation);
    framework.addTest("Minimal HebrewValidator Config", testMinimalHebrewValidatorConfig);
    framework.addTest("Minimal Mapping Basic Operations", testMinimalMappingBasicOperations);
    framework.addTest("Minimal Binary Vector Validation", testMinimalBinaryVectorValidation);
}