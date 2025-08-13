#include "Mapping.h"
#include <iostream>
#include <iomanip>

Mapping::Mapping() {
    // Initialize 27x27 matrix with zeros
    mappingMatrix.resize(27, std::vector<int>(27, 0));
}

void Mapping::createEvaToHebrewMapping() {
    // Clear existing mappings
    for (int i = 0; i < 27; i++) {
        for (int j = 0; j < 27; j++) {
            mappingMatrix[i][j] = 0;
        }
    }
    
    // Create a sample mapping from EVA to Hebrew
    // This is a hypothetical mapping - in real research this would be based on linguistic analysis
    
    // EVA 'a' (index 0) -> Hebrew aleph (index 0)
    setMapping(0, 0);
    
    // EVA 'b' (index 1) -> Hebrew bet (index 1)  
    setMapping(1, 1);
    
    // EVA 'c' (index 2) -> Hebrew gimel (index 2)
    setMapping(2, 2);
    
    // EVA 'd' (index 3) -> Hebrew dalet (index 3)
    setMapping(3, 3);
    
    // EVA 'e' (index 4) -> Hebrew he (index 4)
    setMapping(4, 4);
    
    // EVA 'f' (index 5) -> Hebrew vav (index 5)
    setMapping(5, 5);
    
    // EVA 'g' (index 6) -> Hebrew zayin (index 6)
    setMapping(6, 6);
    
    // EVA 'h' (index 7) -> Hebrew chet (index 7)
    setMapping(7, 7);
    
    // EVA 'i' (index 8) -> Hebrew yod (index 9)
    setMapping(8, 9);
    
    // EVA 'j' (index 9) -> Hebrew kaf (index 10)
    setMapping(9, 10);
    
    // EVA 'k' (index 10) -> Hebrew lamed (index 11)
    setMapping(10, 11);
    
    // EVA 'l' (index 11) -> Hebrew mem (index 12)
    setMapping(11, 12);
    
    // EVA 'm' (index 12) -> Hebrew nun (index 13)
    setMapping(12, 13);
    
    // EVA 'n' (index 13) -> Hebrew samech (index 14)
    setMapping(13, 14);
    
    // EVA 'o' (index 14) -> Hebrew ayin (index 15)
    setMapping(14, 15);
    
    // EVA 'p' (index 15) -> Hebrew pe (index 16)
    setMapping(15, 16);
    
    // EVA 'q' (index 16) -> Hebrew tsadi (index 17)
    setMapping(16, 17);
    
    // EVA 'r' (index 17) -> Hebrew qof (index 18)
    setMapping(17, 18);
    
    // EVA 's' (index 18) -> Hebrew resh (index 19)
    setMapping(18, 19);
    
    // EVA 't' (index 19) -> Hebrew shin (index 20)
    setMapping(19, 20);
    
    // EVA 'u' (index 20) -> Hebrew tav (index 21)
    setMapping(20, 21);
    
    // EVA 'v' (index 21) -> Hebrew final kaf (index 22)
    setMapping(21, 22);
    
    // EVA 'w' (index 22) -> Hebrew final mem (index 23)
    setMapping(22, 23);
    
    // EVA 'x' (index 23) -> Hebrew final nun (index 24)
    setMapping(23, 24);
    
    // EVA 'y' (index 24) -> Hebrew final pe (index 25)
    setMapping(24, 25);
    
    // EVA 'z' (index 25) -> Hebrew final tsadi (index 26)
    setMapping(25, 26);
    
    // EVA space (index 26) -> Hebrew tet (index 8) - arbitrary mapping for space
    setMapping(26, 8);
}

std::vector<int> Mapping::applyMapping(const std::vector<int>& inputVector) const {
    if (inputVector.size() != 27) {
        std::wcerr << L"Error: Input vector must have 27 elements" << std::endl;
        return std::vector<int>(27, 0);
    }
    
    std::vector<int> result(27, 0);
    
    // Matrix multiplication: result = inputVector * mappingMatrix
    // inputVector is 1x27, mappingMatrix is 27x27, result is 1x27
    for (int j = 0; j < 27; j++) {
        for (int i = 0; i < 27; i++) {
            if (inputVector[i] && mappingMatrix[i][j]) {
                result[j] = 1; // Binary OR operation for multiple mappings
            }
        }
    }
    
    return result;
}

Word Mapping::translateToHebrew(const Word& evaWord) const {
    if (evaWord.getAlphabet() != Alphabet::EVA) {
        std::wcerr << L"Error: Input word must be in EVA alphabet" << std::endl;
        return Word(L"", Alphabet::HEBREW);
    }
    
    // Get the binary matrix of the EVA word
    std::vector<int> evaBinary = evaWord.getBinaryMatrix();
    
    // Apply the mapping
    std::vector<int> hebrewBinary = applyMapping(evaBinary);
    
    // Create a Hebrew word representation using ASCII for better console compatibility
    std::wstring approximateHebrew = L"H:";
    
    // Convert binary result back to Hebrew letter names for visibility
    const wchar_t* hebrewNames[] = {
        L"aleph", L"bet", L"gimel", L"dalet", L"he", L"vav", L"zayin",
        L"chet", L"tet", L"yod", L"kaf", L"lamed", L"mem", L"nun",
        L"samech", L"ayin", L"pe", L"tsadi", L"qof", L"resh", L"shin",
        L"tav", L"kaf_final", L"mem_final", L"nun_final", L"pe_final", L"tsadi_final"
    };
    
    bool hasMapping = false;
    for (int i = 0; i < 27; i++) {
        if (hebrewBinary[i]) {
            if (hasMapping) approximateHebrew += L"-";
            approximateHebrew += hebrewNames[i];
            hasMapping = true;
        }
    }
    
    if (!hasMapping) {
        approximateHebrew = L"[no_mapping]";
    }
    
    return Word(approximateHebrew, Alphabet::HEBREW);
}

void Mapping::printMappingMatrix() const {
    std::wcout << L"EVA to Hebrew Mapping Matrix (27x27):" << std::endl;
    std::wcout << L"    ";
    
    // Print column headers (Hebrew alphabet indices)
    for (int j = 0; j < 27; j++) {
        std::wcout << std::setw(2) << j << L" ";
    }
    std::wcout << std::endl;
    
    // Print matrix with row headers (EVA alphabet indices)
    for (int i = 0; i < 27; i++) {
        std::wcout << std::setw(2) << i << L": ";
        for (int j = 0; j < 27; j++) {
            std::wcout << std::setw(2) << mappingMatrix[i][j] << L" ";
        }
        std::wcout << std::endl;
    }
}

void Mapping::setMapping(int evaIndex, int hebrewIndex) {
    if (evaIndex >= 0 && evaIndex < 27 && hebrewIndex >= 0 && hebrewIndex < 27) {
        mappingMatrix[evaIndex][hebrewIndex] = 1;
    }
}

const std::vector<std::vector<int>>& Mapping::getMappingMatrix() const {
    return mappingMatrix;
}

std::string Mapping::serializeMappingVisualization() const {
    // EVA characters in alphabetical order
    const char evaChars[] = "abcdefghijklmnopqrstuvwxyz ";
    
    // Hebrew characters corresponding to indices 0-26
    const char hebrewChars[] = {
        (char)0xD0, (char)0x05, // aleph א (0x05D0)
        (char)0xD1, (char)0x05, // bet ב (0x05D1) 
        (char)0xD2, (char)0x05, // gimel ג
        (char)0xD3, (char)0x05, // dalet ד
        (char)0xD4, (char)0x05, // he ה
        (char)0xD5, (char)0x05, // vav ו
        (char)0xD6, (char)0x05, // zayin ז
        (char)0xD7, (char)0x05, // chet ח
        (char)0xD8, (char)0x05, // tet ט
        (char)0xD9, (char)0x05, // yod י
        (char)0xDB, (char)0x05, // kaf כ
        (char)0xDC, (char)0x05, // lamed ל
        (char)0xDE, (char)0x05, // mem מ
        (char)0xE0, (char)0x05, // nun נ
        (char)0xE1, (char)0x05, // samech ס
        (char)0xE2, (char)0x05, // ayin ע
        (char)0xE4, (char)0x05, // pe פ
        (char)0xE6, (char)0x05, // tsadi צ
        (char)0xE7, (char)0x05, // qof ק
        (char)0xE8, (char)0x05, // resh ר
        (char)0xE9, (char)0x05, // shin ש
        (char)0xEA, (char)0x05, // tav ת
        (char)0xDA, (char)0x05, // kaf final ך
        (char)0xDD, (char)0x05, // mem final ם
        (char)0xDF, (char)0x05, // nun final ן
        (char)0xE3, (char)0x05, // pe final ף
        (char)0xE5, (char)0x05  // tsadi final ץ
    };
    
    std::string result;
    
    // First line: EVA characters
    result += "EVA: ";
    for (int i = 0; i < 27; ++i) {
        if (i > 0) result += " ";
        result += evaChars[i];
    }
    result += "\n";
    
    // Second line: Hebrew characters (show which Hebrew char each EVA maps to)
    result += "HEB: ";
    for (int i = 0; i < 27; ++i) {
        if (i > 0) result += " ";
        
        // Find which Hebrew character this EVA character maps to
        int hebrewIndex = -1;
        for (int j = 0; j < 27; ++j) {
            if (mappingMatrix[i][j] == 1) {
                hebrewIndex = j;
                break;
            }
        }
        
        if (hebrewIndex != -1) {
            // Convert Hebrew index to Unicode characters
            const char* hebrewUnicode = nullptr;
            switch (hebrewIndex) {
                case 0: hebrewUnicode = "א"; break;   // aleph
                case 1: hebrewUnicode = "ב"; break;   // bet
                case 2: hebrewUnicode = "ג"; break;   // gimel
                case 3: hebrewUnicode = "ד"; break;   // dalet
                case 4: hebrewUnicode = "ה"; break;   // he
                case 5: hebrewUnicode = "ו"; break;   // vav
                case 6: hebrewUnicode = "ז"; break;   // zayin
                case 7: hebrewUnicode = "ח"; break;   // chet
                case 8: hebrewUnicode = "ט"; break;   // tet
                case 9: hebrewUnicode = "י"; break;   // yod
                case 10: hebrewUnicode = "כ"; break;  // kaf
                case 11: hebrewUnicode = "ל"; break;  // lamed
                case 12: hebrewUnicode = "מ"; break;  // mem
                case 13: hebrewUnicode = "נ"; break;  // nun
                case 14: hebrewUnicode = "ס"; break;  // samech
                case 15: hebrewUnicode = "ע"; break;  // ayin
                case 16: hebrewUnicode = "פ"; break;  // pe
                case 17: hebrewUnicode = "צ"; break;  // tsadi
                case 18: hebrewUnicode = "ק"; break;  // qof
                case 19: hebrewUnicode = "ר"; break;  // resh
                case 20: hebrewUnicode = "ש"; break;  // shin
                case 21: hebrewUnicode = "ת"; break;  // tav
                case 22: hebrewUnicode = "ך"; break;  // kaf final
                case 23: hebrewUnicode = "ם"; break;  // mem final
                case 24: hebrewUnicode = "ן"; break;  // nun final
                case 25: hebrewUnicode = "ף"; break;  // pe final
                case 26: hebrewUnicode = "ץ"; break;  // tsadi final
                default: hebrewUnicode = "?"; break;
            }
            result += hebrewUnicode;
        } else {
            result += "?";
        }
    }
    
    return result;
}