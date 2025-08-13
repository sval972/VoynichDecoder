#pragma once

#include "Word.h"
#include <vector>
#include <string>

class Mapping {
private:
    std::vector<std::vector<int>> mappingMatrix; // 27x27 binary matrix
    
public:
    Mapping();
    
    // Create a mapping from EVA to Hebrew alphabet
    void createEvaToHebrewMapping();
    
    // Apply mapping to a Word's binary matrix (matrix multiplication)
    std::vector<int> applyMapping(const std::vector<int>& inputVector) const;
    
    // Translate an EVA Word to Hebrew representation
    Word translateToHebrew(const Word& evaWord) const;
    
    // Print the mapping matrix for debugging
    void printMappingMatrix() const;
    
    // Set a specific mapping between EVA character index and Hebrew character index
    void setMapping(int evaIndex, int hebrewIndex);
    
    // Get the mapping matrix
    const std::vector<std::vector<int>>& getMappingMatrix() const;
    
    // Serialize mapping to show EVA->Hebrew correspondence
    std::string serializeMappingVisualization() const;
};