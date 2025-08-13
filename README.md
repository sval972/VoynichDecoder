# Voynich Manuscript Decoder

A high-performance C++ application for systematic analysis of potential EVA-to-Hebrew alphabet translations of the Voynich Manuscript. This tool tests all possible permutations of the EVA alphabet mapped to Hebrew characters to discover meaningful linguistic patterns.

## Project Purpose

The Voynich Manuscript is a mysterious 15th-century codex written in an unknown script. This project implements a brute-force computational approach to test the hypothesis that the manuscript might be written in Hebrew using a substitution cipher with the EVA (European Voynich Alphabet) transcription system.

### Key Features

- **Systematic Analysis**: Tests all 27! (10,888,869,450,418,352,160) possible EVA-to-Hebrew alphabet mappings
- **High Performance**: Multi-threaded processing with optimized binary matrix operations
- **Real-time Statistics**: Live progress reporting with mappings/second and highest scores
- **State Persistence**: Automatic saving and resuming of progress for long-running analysis
- **Hebrew Validation**: Validates translated text against a comprehensive Hebrew lexicon (40,000+ words)
- **Smart Result Filtering**: Saves only high-scoring translations above configurable thresholds
- **Graceful Shutdown**: Ctrl+C handling with state preservation

## How It Works

1. **Input Processing**: Loads Voynich words from EVA transcription and Hebrew lexicon
2. **Mapping Generation**: Systematically generates all possible EVA→Hebrew character mappings
3. **Translation**: Converts Voynich words to Hebrew using current mapping via binary matrix multiplication
4. **Validation**: Checks translated text against Hebrew dictionary for meaningful words
5. **Scoring**: Calculates percentage of valid Hebrew words in translation
6. **Results**: Saves high-scoring mappings with detailed analysis to output file

## Requirements

- **Compiler**: Visual Studio 2022 (MSVC) with C++17 support
- **Platform**: Windows (x64)
- **RAM**: 8GB+ recommended for optimal performance
- **Storage**: Several GB for state files during long-running analysis

## Required Files

The required data files are included in the `resources/` folder:

- `resources/Script_freq100.txt` - Voynich manuscript words in EVA transcription
- `resources/Tanah2.txt` - Hebrew lexicon for word validation (~40,000 Hebrew words)

These files are automatically loaded using the default configuration paths.

## Project Structure

```
VoynichDecoder/
├── resources/           # Data files
│   ├── Script_freq100.txt    # Voynich words (EVA transcription)
│   ├── Tanah2.txt           # Hebrew lexicon
│   └── README.md            # Resource documentation
├── *.cpp, *.h          # Source code files
├── VoynichDecoder.sln  # Visual Studio solution
├── VoynichDecoder.vcxproj   # Project file
└── README.md           # This file
```

## Building the Project

### Using Visual Studio 2022

1. Open `VoynichDecoder.sln` in Visual Studio 2022
2. Select configuration:
   - **Debug**: For development and testing
   - **Release**: For production runs (recommended for performance)
3. Select platform: **x64**
4. Build → Build Solution (Ctrl+Shift+B)

### Using MSBuild Command Line

```cmd
# Debug build
"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" VoynichDecoder.sln -p:Configuration=Debug -p:Platform=x64

# Release build  
"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" VoynichDecoder.sln -p:Configuration=Release -p:Platform=x64
```

## Running the Decoder

### Basic Usage

```cmd
# Navigate to output directory
cd x64\Debug
# or for Release build:
cd x64\Release

# Run the decoder
VoynichDecoder.exe
```

### Configuration

Edit `main.cpp` to modify analysis parameters:

```cpp
VoynichDecoder::DecoderConfig config;
config.numThreads = 0;                    // 0 = auto-detect, or specify count
config.scoreThreshold = 45.0;             // Minimum % for saving results
config.statusUpdateIntervalMs = 5000;     // Progress update frequency
config.maxMappingsToProcess = 0;          // 0 = unlimited
config.mappingBlockSize = 100000;         // Mappings per processing block
```

### Runtime Behavior

```
Voynich Manuscript Decoder
=========================
Systematic analysis of EVA-to-Hebrew translation mappings

Initializing Voynich Decoder...
Loaded 100 Voynich words
Generated block 0 with 100,000 mappings
Configured for 8 worker threads
Score threshold: 45.0
Max mappings to process: unlimited

[2.5min] Mappings: 245,892 (294.2/sec), Highest Score: 43.84
*** HIGH SCORE *** Thread 3: Score=47.12, Matches=47/100 (47.1%), Mapping=387291
Saved state: Block 3, Total generated: 300,000
```

### Output Files

- **`voynich_analysis_results.txt`**: High-scoring translations with detailed mappings
- **`voynich_decoder_state.dat`**: Binary state file for resuming interrupted analysis

### Sample Result Format

```
Date/Time: 2025-01-15 14:32:17
Score: 47.12% (47/100 matches)
EVA: a b c d e f g h i j k l m n o p q r s t u v w x y z  
HEB: א ב ג ד ה ו ז ח ט י כ ל מ נ ס ע פ צ ק ר ש ת ם ן ף ץ ך

Translated Hebrew words:
שלום, בית, איש, אשה, ילד...
```

## Performance Expectations

- **Processing Rate**: ~25k mappings/second (varies by hardware)
- **Memory Usage**: ~2-4GB RAM during operation
- **Total Runtime**: Theoretical completion would take millions of years
- **Practical Usage**: Run until satisfactory results found or specific time limit

## State Management

The decoder automatically saves progress every block completion:

- **Resume**: Restart the application to continue from last saved state
- **Reset**: Delete `voynich_decoder_state.dat` to start fresh
- **Interrupt**: Use Ctrl+C for graceful shutdown with state preservation

## Architecture Overview

### Core Components

- **`VoynichDecoder`**: Main orchestration and multi-threading
- **`MappingGenerator`**: Systematic permutation generation with state persistence
- **`Translator`**: EVA→Hebrew conversion using binary matrix operations
- **`HebrewValidator`**: Dictionary-based validation and scoring
- **`WordSet`**: Optimized word storage and manipulation

### Key Algorithms

- **Factorial Number System**: Efficient permutation enumeration
- **Binary Matrix Translation**: O(1) character mapping operations
- **Hash-based Validation**: Fast Hebrew word lookup
- **Thread-safe Block Processing**: Concurrent analysis with progress tracking

## Scientific Applications

This tool enables researchers to:

- Test computational linguistics hypotheses about the Voynich Manuscript
- Explore statistical patterns in potential Hebrew translations
- Identify promising character mapping candidates for further analysis
- Validate or refute Hebrew-language theories about the manuscript

## Limitations

- **Computational Scope**: Complete analysis requires astronomical time
- **Language Assumption**: Only tests Hebrew translation hypothesis
- **Dictionary Dependency**: Limited to words in provided Hebrew lexicon
- **Character Mapping**: Assumes 1:1 EVA-to-Hebrew character correspondence

## Contributing

This is a research tool for cryptographic and linguistic analysis. Contributions welcome for:

- Performance optimizations
- Additional language support
- Enhanced statistical analysis
- Improved result visualization

## License

Research and educational use. Please cite appropriately in academic work.