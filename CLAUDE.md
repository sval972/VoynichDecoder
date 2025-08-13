# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a Visual Studio C++ project named VoynichDecoder, designed to work with what appears to be Voynich Manuscript text analysis. The project contains:

- **VoynichDecoder.cpp**: Main C++ source file (currently contains a basic "Hello World" program)
- **Script_freq100.txt**: Data file containing what appears to be Voynich script text/tokens
- Standard Visual Studio project files (.sln, .vcxproj, .vcxproj.filters, .vcxproj.user)

## Build and Development Commands

This is a Visual Studio C++ project using MSBuild. Build commands should be executed in a Visual Studio Developer Command Prompt or PowerShell with VS build tools.

### Building the Project
```powershell
# Build Debug configuration for x64
msbuild VoynichDecoder.sln /p:Configuration=Debug /p:Platform=x64

# Build Release configuration for x64  
msbuild VoynichDecoder.sln /p:Configuration=Release /p:Platform=x64

# Build all configurations
msbuild VoynichDecoder.sln
```

### Running the Application
```powershell
# Run Debug build
.\x64\Debug\VoynichDecoder.exe

# Run Release build (when built)
.\x64\Release\VoynichDecoder.exe
```

### Cleaning the Project
```powershell
msbuild VoynichDecoder.sln /t:Clean
```

## Architecture and Structure

- **Single-file console application**: The main logic is contained in VoynichDecoder.cpp
- **Data-driven approach**: Uses Script_freq100.txt as input data containing Voynich script tokens
- **Standard C++ console application**: Uses iostream for basic I/O
- **Visual Studio project structure**: Standard MSVC project with x64 Debug/Release configurations
- **Platform toolset**: v143 (Visual Studio 2022)
- **Character set**: Unicode
- **Target platform**: Windows 10

## Development Notes

- The project is currently in early development stage (main function only contains Hello World)
- The Script_freq100.txt file contains what appears to be Voynich manuscript tokens/words, suggesting this will be a text analysis or decoding tool
- No external dependencies beyond standard C++ library
- No testing framework currently configured