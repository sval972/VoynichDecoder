# Resources

This folder contains the data files required for the Voynich Decoder analysis.

## Files

### Script_freq100.txt
- **Purpose**: Voynich Manuscript words in EVA (European Voynich Alphabet) transcription
- **Content**: List of the most frequent words from the Voynich Manuscript
- **Format**: Plain text, one word per line
- **Usage**: Source text for translation analysis

### Tanah2.txt
- **Purpose**: Hebrew lexicon for word validation
- **Content**: Comprehensive Hebrew dictionary containing biblical and modern Hebrew words
- **Format**: Plain text, one word per line in Hebrew characters
- **Usage**: Validation corpus to check if translated text contains real Hebrew words
- **Size**: ~40,000 Hebrew words

## Usage

These files are automatically loaded by the VoynichDecoder when using default configuration:

```cpp
config.voynichWordsPath = "resources/Script_freq100.txt";
config.hebrewLexiconPath = "resources/Tanah2.txt";
```

## Data Sources

- **Script_freq100.txt**: Based on EVA transcription studies of the Voynich Manuscript
- **Tanah2.txt**: Derived from Hebrew biblical and linguistic sources

## File Requirements

Both files must be present for the decoder to function. The application will report an error if either file is missing or cannot be read.