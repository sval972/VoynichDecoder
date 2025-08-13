#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <iostream>

enum class Alphabet {
    EVA,
    HEBREW
};

class Word {
private:
    std::wstring text;
    std::vector<int> binaryMatrix;
    Alphabet alphabet;
    
    static const std::unordered_map<wchar_t, int> evaAlphabet;
    static const std::unordered_map<wchar_t, int> hebrewAlphabet;
    
    void generateBinaryMatrix();
    
public:
    Word(const std::wstring& word, Alphabet alph);
    
    const std::wstring& getText() const;
    const std::vector<int>& getBinaryMatrix() const;
    Alphabet getAlphabet() const;
    void printBinaryMatrix() const;
};