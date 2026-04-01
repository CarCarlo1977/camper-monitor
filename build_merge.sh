#!/bin/bash
set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

BUILD_DIR=".pio/build/esp32doit-devkit-v1"

echo -e "${GREEN}=== CamperMonitor Build ===${NC}\n"

echo -e "${YELLOW}[1/2] Compiling firmware...${NC}"
if ! pio run -e esp32doit-devkit-v1; then
  echo -e "${RED}ERROR: Compilation failed!${NC}"
  exit 1
fi
echo -e "${GREEN}✓ Firmware compiled${NC}\n"

echo -e "${YELLOW}[2/2] Building filesystem...${NC}"
if ! pio run -e esp32doit-devkit-v1 -t buildfs; then
  echo -e "${RED}ERROR: Filesystem build failed!${NC}"
  exit 1
fi
echo -e "${GREEN}✓ Filesystem built${NC}\n"

cp "$BUILD_DIR/firmware.bin" ./firmware.bin
cp "$BUILD_DIR/littlefs.bin" ./littlefs.bin

echo -e "${GREEN}=== Build Complete ===${NC}"
echo -e "Firmware: ${YELLOW}firmware.bin${NC}"
echo -e "Filesystem: ${YELLOW}littlefs.bin${NC}\n"
echo -e "${GREEN}✓ Files copied to project root${NC}\n"