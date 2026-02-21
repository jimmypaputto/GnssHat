#!/bin/bash

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${YELLOW}Building all examples...${NC}"

# Determine examples directory based on script location or current directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
EXAMPLES_DIR=""

# Check if we're in the scripts directory and examples is in parent
if [ -d "$SCRIPT_DIR/../examples" ]; then
    EXAMPLES_DIR="$(cd "$SCRIPT_DIR/../examples" && pwd)"
    echo "Found examples directory relative to script: $EXAMPLES_DIR"
# Check if we're in the root directory with examples folder
elif [ -d "./examples" ]; then
    EXAMPLES_DIR="$(cd "./examples" && pwd)"
    echo "Found examples directory in current location: $EXAMPLES_DIR"
# Check if current directory itself contains CMakeLists.txt files (we might be in examples)
elif [ -d "." ] && find . -maxdepth 2 -name "CMakeLists.txt" | grep -q .; then
    EXAMPLES_DIR="$(pwd)"
    echo "Using current directory as examples: $EXAMPLES_DIR"
else
    echo -e "${RED}Error: Could not find examples directory${NC}"
    echo "Please run this script from:"
    echo "  - Project root directory (where examples/ folder is)"
    echo "  - scripts/ directory"
    echo "  - examples/ directory itself"
    exit 1
fi

if [ ! -d "$EXAMPLES_DIR" ]; then
    echo -e "${RED}Error: Examples directory $EXAMPLES_DIR does not exist${NC}"
    exit 1
fi

EXAMPLES=()
EXAMPLE_PATHS=()
echo "Discovering examples..."

# Discover examples in language subdirectories (CPP/, C/) and top-level
for dir in "$EXAMPLES_DIR"/*; do
    if [ ! -d "$dir" ]; then
        continue
    fi

    dirname=$(basename "$dir")

    # Language grouping directories: scan their subdirectories
    if [[ "$dirname" == "CPP" || "$dirname" == "C" ]]; then
        for subdir in "$dir"/*; do
            if [ -d "$subdir" ] && [ -f "$subdir/CMakeLists.txt" ]; then
                example_name="$dirname/$(basename "$subdir")"
                EXAMPLES+=("$example_name")
                EXAMPLE_PATHS+=("$subdir")
                echo "  Found: $example_name"
            fi
        done
    # Top-level examples (GpsdIntegration, etc.)
    elif [ -f "$dir/CMakeLists.txt" ]; then
        example_name="$dirname"
        EXAMPLES+=("$example_name")
        EXAMPLE_PATHS+=("$dir")
        echo "  Found: $example_name"
    fi
done

if [ ${#EXAMPLES[@]} -eq 0 ]; then
    echo -e "${RED}Error: No examples found in $EXAMPLES_DIR${NC}"
    echo "Looking for directories containing CMakeLists.txt"
    exit 1
fi

echo -e "${GREEN}Found ${#EXAMPLES[@]} examples to build${NC}"
echo ""

build_example() {
    local example_name="$1"
    local example_path="$2"
    
    echo -e "${YELLOW}Building $example_name...${NC}"
    
    if [ ! -d "$example_path" ]; then
        echo -e "${RED}Error: Directory $example_path does not exist${NC}"
        return 1
    fi
    
    cd "$example_path"
    
    mkdir -p build
    cd build
    
    if [ ! -f "../CMakeLists.txt" ]; then
        echo -e "${RED}Error: CMakeLists.txt not found in $example_path${NC}"
        return 1
    fi
    
    echo "  Running cmake..."
    if cmake .. > cmake_output.log 2>&1; then
        echo "  Running make..."
        if make > make_output.log 2>&1; then
            local binaries=()
            while IFS= read -r -d '' binary; do
                if [[ -x "$binary" && ! "$binary" =~ \.(so|a|cmake|log|txt)$ && ! -d "$binary" ]]; then
                    local basename_binary=$(basename "$binary")
                    if [[ ! "$basename_binary" =~ ^(CMake|cmake|CTest|ctest|_|\.|\[) ]]; then
                        binaries+=("$basename_binary")
                    fi
                fi
            done < <(find . -maxdepth 1 -type f -executable -print0 2>/dev/null)
            
            if [ ${#binaries[@]} -gt 0 ]; then
                echo -e "${GREEN}✓ $example_name built successfully${NC}"
                echo "  Found ${#binaries[@]} binary(ies):"
                for binary in "${binaries[@]}"; do
                    echo "    - $example_path/build/$binary"
                done
                return 0
            else
                echo -e "${RED}Error: No executable binaries found after build${NC}"
                echo "Build directory contents:"
                ls -la .
                return 1
            fi
        else
            echo -e "${RED}Error: make failed for $example_name${NC}"
            echo "Make output:"
            cat make_output.log
            return 1
        fi
    else
        echo -e "${RED}Error: cmake failed for $example_name${NC}"
        echo "CMake output:"
        cat cmake_output.log
        return 1
    fi
}

echo "Checking if GnssHat library is installed..."
if ! ldconfig -p | grep -q "libGnssHat.so"; then
    echo -e "${RED}Error: GnssHat library not found in system${NC}"
    echo "Please build and install the main library first:"
    echo "  cd /home/pi/GnssHat"
    echo "  mkdir -p build && cd build"
    echo "  cmake .. && make && sudo make install && sudo ldconfig"
    exit 1
fi

echo -e "${GREEN}✓ GnssHat library found${NC}"
echo ""

ORIGINAL_DIR=$(pwd)

SUCCESS_COUNT=0
FAILED_EXAMPLES=()

for i in "${!EXAMPLES[@]}"; do
    if build_example "${EXAMPLES[$i]}" "${EXAMPLE_PATHS[$i]}"; then
        ((SUCCESS_COUNT++))
    else
        FAILED_EXAMPLES+=("${EXAMPLES[$i]}")
    fi
    echo ""
done

cd "$ORIGINAL_DIR"

echo "============================================"
echo -e "${YELLOW}Build Summary:${NC}"
echo -e "${GREEN}Successfully built: $SUCCESS_COUNT/${#EXAMPLES[@]} examples${NC}"

if [ ${#FAILED_EXAMPLES[@]} -gt 0 ]; then
    echo -e "${RED}Failed examples:${NC}"
    for failed in "${FAILED_EXAMPLES[@]}"; do
        echo -e "${RED}  - $failed${NC}"
    done
    echo ""
    echo -e "${YELLOW}Note: Some examples may have failed but script continued to build others${NC}"
else
    echo -e "${GREEN}All examples built successfully!${NC}"
    echo ""
    echo "Example binaries are located in:"
    for i in "${!EXAMPLES[@]}"; do
        example_path="${EXAMPLE_PATHS[$i]}/build"
        if [ -d "$example_path" ]; then
            found_binaries=()
            while IFS= read -r -d '' binary; do
                if [[ -x "$binary" && ! "$binary" =~ \.(so|a|cmake|log|txt)$ && ! -d "$binary" ]]; then
                    basename_binary=$(basename "$binary")
                    if [[ ! "$basename_binary" =~ ^(CMake|cmake|CTest|ctest|_|\.|\[) ]]; then
                        found_binaries+=("$binary")
                    fi
                fi
            done < <(find "$example_path" -maxdepth 1 -type f -executable -print0 2>/dev/null)
            
            if [ ${#found_binaries[@]} -gt 0 ]; then
                for binary in "${found_binaries[@]}"; do
                    echo "  $binary"
                done
            else
                echo "  $example_path/<no binaries found>"
            fi
        else
            echo "  $example_path/<build dir not found>"
        fi
    done
fi
