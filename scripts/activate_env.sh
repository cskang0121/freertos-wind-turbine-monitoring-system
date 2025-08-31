#!/bin/bash

# Activation script for Wind Turbine Predictor development environment
# This script activates the Python virtual environment and sets up the development environment

# Colors for output
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Get the directory where this script is located
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Navigate to project root
cd "$PROJECT_ROOT"

# Activate Python virtual environment
if [ -f "venv/bin/activate" ]; then
    source venv/bin/activate
    echo -e "${GREEN}✓ Python virtual environment activated${NC}"
else
    echo -e "${YELLOW}Warning: Python venv not found. Creating it now...${NC}"
    python3 -m venv venv
    source venv/bin/activate
    pip install -r requirements.txt
    echo -e "${GREEN}✓ Python virtual environment created and activated${NC}"
fi

# Display environment info
echo ""
echo -e "${BLUE}=== Development Environment ===${NC}"
echo -e "Project: Wind Turbine Predictive Maintenance"
echo -e "Python: $(which python3)"
echo -e "CMake: $(cmake --version | head -n1)"
echo -e "ARM GCC: $(arm-none-eabi-gcc --version | head -n1)"
echo ""
echo -e "${GREEN}Ready for development!${NC}"
echo ""
echo "Quick commands:"
echo "  Build:     ./scripts/build.sh simulation"
echo "  Clean:     ./scripts/clean.sh"
echo "  Run:       ./build/simulation/wind_turbine_predictor"
echo "  Deactivate: deactivate"
echo ""