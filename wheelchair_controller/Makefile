# Simple Makefile wrapper for idf.py
#
# Use this Makefile from within the 'wheelchair_controller' directory.
#
# Usage:
#   make build          - Build the project
#   make flash          - Flash the project (using default PORT or overriding: make flash PORT=/dev/tty.USBXXX)
#   make monitor        - Start the serial monitor (using default PORT or overriding)
#   make all            - Build, flash, and monitor (using default PORT or overriding)
#   make build-flash    - Build and flash
#   make clean          - Clean the build output
#   make menuconfig     - Run project configuration menu
#

# Default serial port - override if necessary:
# Example: make flash PORT=/dev/tty.usbserial-XXXXX
ESPPORT ?= /dev/tty.usbserial-0001

PYTHON := python3
# Prepend sourcing the export script to ensure environment is set for idf.py calls
# Note: This might be slightly redundant if you've already sourced it in your main shell.
IDF_PY := . ~/esp/esp-idf/export.sh && $(PYTHON) ~/esp/esp-idf/tools/idf.py

# Phony targets (targets that aren't actual files)
.PHONY: build flash monitor all build-flash clean menuconfig

build:
	@echo "Building project..."
	@$(IDF_PY) build

flash:
	@echo "Flashing project to $(ESPPORT)..."
	@$(IDF_PY) -p $(ESPPORT) flash

monitor:
	@echo "Starting serial monitor on $(ESPPORT)..."
	@$(IDF_PY) -p $(ESPPORT) monitor

# Build, Flash, and Monitor
all: build flash monitor

build-flash:
	@echo "Building and flashing project to $(ESPPORT)..."
	@$(IDF_PY) -p $(ESPPORT) build flash

clean:
	@echo "Cleaning project..."
	@$(IDF_PY) fullclean

menuconfig:
	@$(IDF_PY) menuconfig

# Default target
default: all 