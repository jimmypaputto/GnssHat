# PrintNavigation Example

Navigation data display examples demonstrating basic and advanced GNSS data presentation.

## Overview

This example provides two different approaches to displaying GNSS navigation data:

- **PrintNavigationBasic**: Simple text output for basic navigation monitoring
- **PrintNavigationAdvanced**: Rich terminal UI with real-time formatted display

Both examples demonstrate how to read and display comprehensive navigation information from the GNSS HAT.

## Features

### PrintNavigationBasic
- Simple, straightforward navigation data output
- Essential positioning information
- Basic fix status and time display
- Lightweight implementation

### PrintNavigationAdvanced  
- Rich terminal user interface with colors and formatting
- Comprehensive data presentation in organized tables
- Real-time updates with screen refresh
- Professional dashboard-style display
- Signal quality and RF monitoring
- Dilution of Precision (DOP) values

## Build Instructions

```bash
# From the PrintNavigation example directory
mkdir -p build
cd build
cmake ..
make
```

This creates two executables:
- `PrintNavigationBasic`
- `PrintNavigationAdvanced`

## Usage

### Basic Navigation Display
```bash
./PrintNavigationBasic
```

### Advanced Navigation Display
```bash
./PrintNavigationAdvanced
```

Press `Ctrl+C` to exit either program.
