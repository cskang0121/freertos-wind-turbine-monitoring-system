#!/bin/bash

# Build script for Wind Turbine Predictive Maintenance System
# Usage: ./build.sh [simulation|target] [clean]

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Get the directory where this script is located
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Default build type
BUILD_TYPE="simulation"
CLEAN_BUILD=false

# Parse arguments
if [ "$1" == "target" ]; then
    BUILD_TYPE="target"
elif [ "$1" == "simulation" ]; then
    BUILD_TYPE="simulation"
elif [ "$1" == "clean" ]; then
    CLEAN_BUILD=true
fi

if [ "$2" == "clean" ]; then
    CLEAN_BUILD=true
fi

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}Wind Turbine Predictor - Build System${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""

# Navigate to project root
cd "$PROJECT_ROOT"

# Set build directory based on build type
if [ "$BUILD_TYPE" == "simulation" ]; then
    BUILD_DIR="build/simulation"
    CMAKE_ARGS="-DSIMULATION_MODE=ON"
    echo -e "${YELLOW}Building for SIMULATION (Mac/Linux)${NC}"
else
    BUILD_DIR="build/target"
    CMAKE_ARGS="-DSIMULATION_MODE=OFF"
    echo -e "${YELLOW}Building for TARGET (AmebaPro2)${NC}"
fi

# Clean if requested
if [ "$CLEAN_BUILD" == true ]; then
    echo -e "${YELLOW}Cleaning previous build...${NC}"
    rm -rf "$BUILD_DIR"
fi

# Create build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure with CMake
echo ""
echo -e "${YELLOW}Configuring with CMake...${NC}"
cmake ../.. $CMAKE_ARGS -DCMAKE_BUILD_TYPE=Debug -DBUILD_EXAMPLES=ON

# Build
echo ""
echo -e "${YELLOW}Building project...${NC}"
make -j$(sysctl -n hw.ncpu 2>/dev/null || nproc)

# Check if build succeeded
if [ $? -eq 0 ]; then
    echo ""
    echo -e "${GREEN}========================================${NC}"
    echo -e "${GREEN}Build completed successfully!${NC}"
    echo -e "${GREEN}========================================${NC}"
    echo ""
    echo "Executable location: $BUILD_DIR/wind_turbine_predictor"
    echo ""
    echo "To run the program:"
    echo "  cd $BUILD_DIR"
    echo "  ./wind_turbine_predictor"
    echo ""
else
    echo ""
    echo -e "${RED}========================================${NC}"
    echo -e "${RED}Build failed!${NC}"
    echo -e "${RED}========================================${NC}"
    exit 1
fi