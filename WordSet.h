#pragma once

#include "Word.h"
#include <vector>
#include <string>

class WordSet {
private:
    std::vector<Word> words;
    
public:
    WordSet() = default;
    
    void addWord(const Word& word);
    void readFromFile(const std::string& filename, Alphabet alphabet);
    size_t size() const;
    
    class iterator {
    private:
        std::vector<Word>::iterator it;
        
    public:
        iterator(std::vector<Word>::iterator iter);
        
        Word& operator*();
        Word* operator->();
        iterator& operator++();
        iterator operator++(int);
        bool operator==(const iterator& other) const;
        bool operator!=(const iterator& other) const;
    };
    
    class const_iterator {
    private:
        std::vector<Word>::const_iterator it;
        
    public:
        const_iterator(std::vector<Word>::const_iterator iter);
        
        const Word& operator*() const;
        const Word* operator->() const;
        const_iterator& operator++();
        const_iterator operator++(int);
        bool operator==(const const_iterator& other) const;
        bool operator!=(const const_iterator& other) const;
    };
    
    iterator begin();
    iterator end();
    const_iterator begin() const;
    const_iterator end() const;
    const_iterator cbegin() const;
    const_iterator cend() const;
};