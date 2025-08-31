#!/bin/bash

# Clean script for Wind Turbine Predictive Maintenance System
# Removes all build artifacts

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Get the directory where this script is located
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

echo -e "${YELLOW}Cleaning build artifacts...${NC}"

# Navigate to project root
cd "$PROJECT_ROOT"

# Remove build directories
if [ -d "build" ]; then
    rm -rf build
    echo "Removed build directory"
fi

# Remove any compiled examples
find examples -name "*.o" -type f -delete 2>/dev/null || true
find examples -name "*.out" -type f -delete 2>/dev/null || true
find examples -name "basic_tasks" -type f -delete 2>/dev/null || true

# Remove any temporary files
find . -name "*.swp" -type f -delete 2>/dev/null || true
find . -name "*.swo" -type f -delete 2>/dev/null || true
find . -name ".DS_Store" -type f -delete 2>/dev/null || true

echo -e "${GREEN}Clean complete!${NC}"