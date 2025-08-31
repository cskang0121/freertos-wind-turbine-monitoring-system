#!/bin/bash

# Installation script for Wind Turbine Predictor dependencies
# For macOS and Linux

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}============================================${NC}"
echo -e "${BLUE}Wind Turbine Predictor - Dependency Installer${NC}"
echo -e "${BLUE}============================================${NC}"
echo ""

# Detect OS
OS="unknown"
if [[ "$OSTYPE" == "darwin"* ]]; then
    OS="macos"
    echo -e "${GREEN}Detected macOS${NC}"
elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
    OS="linux"
    echo -e "${GREEN}Detected Linux${NC}"
else
    echo -e "${RED}Unsupported OS: $OSTYPE${NC}"
    exit 1
fi

# Check for package manager
if [[ "$OS" == "macos" ]]; then
    if ! command -v brew &> /dev/null; then
        echo -e "${YELLOW}Homebrew not found. Installing Homebrew...${NC}"
        /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
    else
        echo -e "${GREEN}Homebrew found${NC}"
    fi
fi

echo ""
echo -e "${YELLOW}Checking and installing dependencies...${NC}"
echo ""

# Function to check and install a command
install_if_missing() {
    local cmd=$1
    local package=$2
    local brew_package=${3:-$package}
    local apt_package=${4:-$package}
    
    if command -v $cmd &> /dev/null; then
        echo -e "${GREEN}✓ $cmd already installed${NC}"
    else
        echo -e "${YELLOW}Installing $package...${NC}"
        if [[ "$OS" == "macos" ]]; then
            brew install $brew_package
        else
            sudo apt-get update && sudo apt-get install -y $apt_package
        fi
        echo -e "${GREEN}✓ $package installed${NC}"
    fi
}

# Core build tools
echo -e "${BLUE}Core Build Tools:${NC}"
install_if_missing "gcc" "gcc" "gcc" "build-essential"
install_if_missing "make" "make" "make" "build-essential"
install_if_missing "cmake" "cmake" "cmake" "cmake"
install_if_missing "ninja" "ninja" "ninja" "ninja-build"

# ARM toolchain for embedded development
echo ""
echo -e "${BLUE}ARM Embedded Toolchain:${NC}"
if command -v arm-none-eabi-gcc &> /dev/null; then
    echo -e "${GREEN}✓ ARM GCC toolchain already installed${NC}"
else
    echo -e "${YELLOW}Installing ARM GCC toolchain...${NC}"
    if [[ "$OS" == "macos" ]]; then
        # First try the tap method
        brew tap ArmMbed/homebrew-formulae 2>/dev/null || true
        brew install arm-none-eabi-gcc 2>/dev/null || {
            # If that fails, try the cask version
            echo -e "${YELLOW}Trying alternative installation method...${NC}"
            brew install --cask gcc-arm-embedded
        }
    else
        sudo apt-get update && sudo apt-get install -y gcc-arm-none-eabi
    fi
    echo -e "${GREEN}✓ ARM GCC toolchain installed${NC}"
fi

# Python and required packages
echo ""
echo -e "${BLUE}Python Dependencies:${NC}"
if command -v python3 &> /dev/null; then
    echo -e "${GREEN}✓ Python3 already installed${NC}"
    
    # Check for pip
    if ! command -v pip3 &> /dev/null; then
        echo -e "${YELLOW}Installing pip3...${NC}"
        if [[ "$OS" == "macos" ]]; then
            python3 -m ensurepip
        else
            sudo apt-get install -y python3-pip
        fi
    fi
    
    # Install Python packages
    echo -e "${YELLOW}Installing Python packages...${NC}"
    pip3 install --user numpy pandas matplotlib pyserial
    echo -e "${GREEN}✓ Python packages installed${NC}"
else
    echo -e "${YELLOW}Installing Python3...${NC}"
    install_if_missing "python3" "python3" "python3" "python3"
    install_if_missing "pip3" "pip" "python3" "python3-pip"
    pip3 install --user numpy pandas matplotlib pyserial
fi

# Additional useful tools
echo ""
echo -e "${BLUE}Additional Tools:${NC}"
install_if_missing "git" "git" "git" "git"
install_if_missing "wget" "wget" "wget" "wget"
install_if_missing "screen" "screen" "screen" "screen"

# For simulation mode on Mac, we need standard gcc
if [[ "$OS" == "macos" ]]; then
    echo ""
    echo -e "${BLUE}macOS Simulation Support:${NC}"
    # Check if we have gcc (not just clang alias)
    if gcc --version 2>&1 | grep -q "clang"; then
        echo -e "${YELLOW}Note: 'gcc' is aliased to clang on macOS${NC}"
        echo -e "${YELLOW}This is fine for simulation mode${NC}"
    fi
    echo -e "${GREEN}✓ Compiler ready for simulation${NC}"
fi

# Verify installations
echo ""
echo -e "${BLUE}============================================${NC}"
echo -e "${BLUE}Verification:${NC}"
echo -e "${BLUE}============================================${NC}"

# Function to check version
check_version() {
    local cmd=$1
    local name=$2
    
    if command -v $cmd &> /dev/null; then
        version=$($cmd --version 2>&1 | head -n1)
        echo -e "${GREEN}✓ $name: $version${NC}"
    else
        echo -e "${RED}✗ $name: NOT FOUND${NC}"
    fi
}

check_version "gcc" "GCC"
check_version "cmake" "CMake"
check_version "ninja" "Ninja"
check_version "arm-none-eabi-gcc" "ARM GCC"
check_version "python3" "Python"

# Check Python packages
echo ""
echo -e "${BLUE}Python packages:${NC}"
python3 -c "import numpy; print(f'✓ NumPy: {numpy.__version__}')" 2>/dev/null || echo -e "${RED}✗ NumPy not found${NC}"
python3 -c "import pandas; print(f'✓ Pandas: {pandas.__version__}')" 2>/dev/null || echo -e "${RED}✗ Pandas not found${NC}"
python3 -c "import matplotlib; print(f'✓ Matplotlib: {matplotlib.__version__}')" 2>/dev/null || echo -e "${RED}✗ Matplotlib not found${NC}"
python3 -c "import serial; print(f'✓ PySerial: {serial.__version__}')" 2>/dev/null || echo -e "${RED}✗ PySerial not found${NC}"

echo ""
echo -e "${GREEN}============================================${NC}"
echo -e "${GREEN}Installation Complete!${NC}"
echo -e "${GREEN}============================================${NC}"
echo ""
echo "Next steps:"
echo "1. Restart your terminal or run: source ~/.bashrc (or ~/.zshrc)"
echo "2. Build the project: ./scripts/build.sh simulation"
echo "3. Run the example: ./build/simulation/wind_turbine_predictor"
echo ""

# Check if FreeRTOS kernel needs to be downloaded
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
FREERTOS_PATH="$PROJECT_ROOT/external/FreeRTOS-Kernel"

if [ ! -d "$FREERTOS_PATH" ]; then
    echo -e "${YELLOW}Note: FreeRTOS kernel will be downloaded during first build${NC}"
fi