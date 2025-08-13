#include "WordSet.h"
#include <fstream>
#include <iostream>
#include <locale>
#include <codecvt>

void WordSet::addWord(const Word& word) {
    words.push_back(word);
}

void WordSet::readFromFile(const std::string& filename, Alphabet alphabet) {
    std::wifstream file(filename);
    file.imbue(std::locale(std::locale(), new std::codecvt_utf8<wchar_t>));
    
    if (!file.is_open()) {
        std::wcerr << L"Error: Could not open file " << filename.c_str() << std::endl;
        return;
    }
    
    std::wstring line;
    while (std::getline(file, line)) {
        if (!line.empty()) {
            words.emplace_back(line, alphabet);
        }
    }
    
    file.close();
}

size_t WordSet::size() const {
    return words.size();
}

// Iterator implementation
WordSet::iterator::iterator(std::vector<Word>::iterator iter) : it(iter) {}

Word& WordSet::iterator::operator*() {
    return *it;
}

Word* WordSet::iterator::operator->() {
    return &(*it);
}

WordSet::iterator& WordSet::iterator::operator++() {
    ++it;
    return *this;
}

WordSet::iterator WordSet::iterator::operator++(int) {
    iterator temp = *this;
    ++it;
    return temp;
}

bool WordSet::iterator::operator==(const iterator& other) const {
    return it == other.it;
}

bool WordSet::iterator::operator!=(const iterator& other) const {
    return it != other.it;
}

// Const iterator implementation
WordSet::const_iterator::const_iterator(std::vector<Word>::const_iterator iter) : it(iter) {}

const Word& WordSet::const_iterator::operator*() const {
    return *it;
}

const Word* WordSet::const_iterator::operator->() const {
    return &(*it);
}

WordSet::const_iterator& WordSet::const_iterator::operator++() {
    ++it;
    return *this;
}

WordSet::const_iterator WordSet::const_iterator::operator++(int) {
    const_iterator temp = *this;
    ++it;
    return temp;
}

bool WordSet::const_iterator::operator==(const const_iterator& other) const {
    return it == other.it;
}

bool WordSet::const_iterator::operator!=(const const_iterator& other) const {
    return it != other.it;
}

// Iterator access methods
WordSet::iterator WordSet::begin() {
    return iterator(words.begin());
}

WordSet::iterator WordSet::end() {
    return iterator(words.end());
}

WordSet::const_iterator WordSet::begin() const {
    return const_iterator(words.begin());
}

WordSet::const_iterator WordSet::end() const {
    return const_iterator(words.end());
}

WordSet::const_iterator WordSet::cbegin() const {
    return const_iterator(words.cbegin());
}

WordSet::const_iterator WordSet::cend() const {
    return const_iterator(words.cend());
}