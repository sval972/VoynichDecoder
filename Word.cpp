#include "Word.h"

// EVA alphabet mapping (a-z plus space for 27 total)
const std::unordered_map<wchar_t, int> Word::evaAlphabet = {
    {L'a', 0}, {L'b', 1}, {L'c', 2}, {L'd', 3}, {L'e', 4}, {L'f', 5}, {L'g', 6}, 
    {L'h', 7}, {L'i', 8}, {L'j', 9}, {L'k', 10}, {L'l', 11}, {L'm', 12}, {L'n', 13}, 
    {L'o', 14}, {L'p', 15}, {L'q', 16}, {L'r', 17}, {L's', 18}, {L't', 19}, {L'u', 20}, 
    {L'v', 21}, {L'w', 22}, {L'x', 23}, {L'y', 24}, {L'z', 25}, {L' ', 26}
};

// Hebrew alphabet mapping (aleph to tav, 22 letters + 5 final forms = 27 total)
const std::unordered_map<wchar_t, int> Word::hebrewAlphabet = {
    {0x05D0, 0}, {0x05D1, 1}, {0x05D2, 2}, {0x05D3, 3}, {0x05D4, 4}, {0x05D5, 5}, {0x05D6, 6},
    {0x05D7, 7}, {0x05D8, 8}, {0x05D9, 9}, {0x05DB, 10}, {0x05DC, 11}, {0x05DE, 12}, {0x05E0, 13},
    {0x05E1, 14}, {0x05E2, 15}, {0x05E4, 16}, {0x05E6, 17}, {0x05E7, 18}, {0x05E8, 19}, {0x05E9, 20},
    {0x05EA, 21}, {0x05DA, 22}, {0x05DD, 23}, {0x05DF, 24}, {0x05E3, 25}, {0x05E5, 26}
};

void Word::generateBinaryMatrix() {
    binaryMatrix.assign(27, 0);
    
    const auto& currentAlphabet = (alphabet == Alphabet::EVA) ? evaAlphabet : hebrewAlphabet;
    
    for (wchar_t ch : text) {
        auto it = currentAlphabet.find(ch);
        if (it != currentAlphabet.end()) {
            binaryMatrix[it->second] = 1;
        }
    }
}

Word::Word(const std::wstring& word, Alphabet alph) : text(word), alphabet(alph) {
    generateBinaryMatrix();
}

const std::wstring& Word::getText() const {
    return text;
}

const std::vector<int>& Word::getBinaryMatrix() const {
    return binaryMatrix;
}

Alphabet Word::getAlphabet() const {
    return alphabet;
}

void Word::printBinaryMatrix() const {
    for (int bit : binaryMatrix) {
        std::wcout << bit << L" ";
    }
    std::wcout << std::endl;
}