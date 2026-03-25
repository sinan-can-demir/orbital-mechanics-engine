# Makefile for Orbital Mechanics Engine
# ======================================

# Configuration
BUILD_DIR        := build
BUILD_VIEWER     ?= 1
CMAKE_FLAGS      ?=
CMAKE            := cmake
CMAKE_CONFIG     := $(CMAKE) .. $(CMAKE_FLAGS) -DBUILD_VIEWER=$(BUILD_VIEWER)

# Directories
PROJECT_ROOT     := $(shell cd $(BUILD_DIR)/.. && pwd)
SYSTEMS_DIR      := $(PROJECT_ROOT)/systems
RESULTS_DIR      := $(PROJECT_ROOT)/results
SCRIPTS_DIR      := $(PROJECT_ROOT)/plotting_scripts

# Executables
SIM_EXE          := $(BUILD_DIR)/bin/orbit-sim
VIEWER_EXE       := $(BUILD_DIR)/bin/orbit-viewer

# Default simulation parameters
DEFAULT_SYSTEM   := $(SYSTEMS_DIR)/earth_moon.json
DEFAULT_OUTPUT   := $(RESULTS_DIR)/out.csv
DEFAULT_STEPS    := 500
DEFAULT_DT       := 3600

# Python
PYTHON           := python3

# Colors for help output
BLUE             := \033[0;34m
GREEN            := \033[0;32m
YELLOW           := \033[0;33m
NC               := \033[0m

.PHONY: all build build-sim build-viewer clean reconfigure help
.PHONY: run run-earth-moon run-solar-system view fetch validate validate-earth-moon test
.PHONY: plot plot-energy plot-momentum plot-3d plot-3d-exaggerated plot-3d-earth-moon

# Default target
all: build

# ========================================
# BUILD TARGETS
# ========================================

build: $(BUILD_DIR)/Makefile
	@echo "$(GREEN)Building project...$(NC)"
	@$(MAKE) -C $(BUILD_DIR) -j$$(nproc)
	@echo "$(GREEN)Build complete. Executables in $(BUILD_DIR)/bin/$(NC)"

$(BUILD_DIR)/Makefile:
	@echo "$(BLUE)Configuring project with CMake...$(NC)"
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && $(CMAKE_CONFIG)
	@echo "$(GREEN)Configuration complete. Run 'make -C $(BUILD_DIR)' to build.$(NC)"

build-sim: $(BUILD_DIR)/Makefile
	@$(MAKE) -C $(BUILD_DIR) orbit-sim -j$$(nproc)

build-viewer: $(BUILD_DIR)/Makefile
	@$(MAKE) -C $(BUILD_DIR) orbit-viewer -j$$(nproc)

clean:
	@echo "$(YELLOW)Cleaning build directory...$(NC)"
	@rm -rf $(BUILD_DIR)
	@echo "$(GREEN)Clean complete.$(NC)"

reconfigure:
	@echo "$(BLUE)Reconfiguring project...$(NC)"
	@rm -rf $(BUILD_DIR)
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && $(CMAKE_CONFIG)
	@echo "$(GREEN)Reconfiguration complete.$(NC)"

# ========================================
# RUN TARGETS
# ========================================

run: $(SIM_EXE)
	@$(SIM_EXE) run \
		--system $(DEFAULT_SYSTEM) \
		--steps $(DEFAULT_STEPS) \
		--dt $(DEFAULT_DT) \
		--output $(DEFAULT_OUTPUT)

run-earth-moon: $(SIM_EXE)
	@echo "$(BLUE)Running Earth-Moon simulation...$(NC)"
	@mkdir -p $(RESULTS_DIR)
	@$(SIM_EXE) run \
		--system $(SYSTEMS_DIR)/earth_moon.json \
		--steps $(DEFAULT_STEPS) \
		--dt $(DEFAULT_DT) \
		--output $(RESULTS_DIR)/earth_moon_out.csv
	@echo "$(GREEN)Output: $(RESULTS_DIR)/earth_moon_out.csv$(NC)"

run-solar-system: $(SIM_EXE)
	@echo "$(BLUE)Running Solar System simulation (100k steps)...$(NC)"
	@mkdir -p $(RESULTS_DIR)
	@$(SIM_EXE) run \
		--system $(SYSTEMS_DIR)/solar_system.json \
		--steps 100000 \
		--dt 3600 \
		--output $(RESULTS_DIR)/solar_system_out.csv
	@echo "$(GREEN)Output: $(RESULTS_DIR)/solar_system_out.csv$(NC)"

view: $(VIEWER_EXE)
	@$(VIEWER_EXE) $(DEFAULT_OUTPUT)

view-last: $(VIEWER_EXE)
	@$(VIEWER_EXE) $(RESULTS_DIR)/earth_moon_out.csv

fetch:
	@$(SIM_EXE) fetch \
		--body 399 \
		--center 0 \
		--start 2025-01-01 \
		--stop 2025-01-02 \
		--step "6 h" \
		--output $(RESULTS_DIR)/earth_ephem.txt

validate: $(SIM_EXE)
	@$(SIM_EXE) validate --system $(DEFAULT_SYSTEM)

validate-earth-moon: $(SIM_EXE)
	@$(SIM_EXE) validate --system $(SYSTEMS_DIR)/earth_moon.json

test: validate-earth-moon
	@echo "$(BLUE)Running quick simulation test...$(NC)"
	@$(SIM_EXE) run \
		--system $(SYSTEMS_DIR)/earth_moon.json \
		--steps 100 \
		--dt 3600 \
		--output $(RESULTS_DIR)/test_out.csv
	@echo "$(GREEN)Test complete. Output: $(RESULTS_DIR)/test_out.csv$(NC)"

# ========================================
# PYTHON PLOTTING TARGETS
# ========================================

plot: plot-energy plot-momentum plot-3d plot-3d-exaggerated plot-3d-earth-moon
	@echo "$(GREEN)All plots generated.$(NC)"

plot-energy:
	@echo "$(BLUE)Generating energy conservation plot...$(NC)"
	@cd $(SCRIPTS_DIR) && $(PYTHON) energy_conservation.py

plot-momentum:
	@echo "$(BLUE)Generating angular momentum plot...$(NC)"
	@cd $(SCRIPTS_DIR) && $(PYTHON) angular_momentum_plot.py

plot-3d:
	@echo "$(BLUE)Generating 3D orbit plot...$(NC)"
	@cd $(SCRIPTS_DIR) && $(PYTHON) 3Dplot.py

plot-3d-exaggerated:
	@echo "$(BLUE)Generating 3D exaggerated plot...$(NC)"
	@cd $(SCRIPTS_DIR) && $(PYTHON) 3Dexaggerated_plot.py

plot-3d-earth-moon:
	@echo "$(BLUE)Generating Earth-Moon 3D plot...$(NC)"
	@cd $(SCRIPTS_DIR) && $(PYTHON) 3Dplot_earth_moon.py

# ========================================
# HELP
# ========================================

help:
	@echo ""
	@echo "Orbital Mechanics Engine - Available Targets"
	@echo "============================================"
	@echo ""
	@echo "  Build Targets:"
	@echo "    make build              - Build all executables (default)"
	@echo "    make build-sim          - Build orbit-sim only"
	@echo "    make build-viewer       - Build orbit-viewer only"
	@echo "    make clean              - Remove build directory"
	@echo "    make reconfigure        - Clean and reconfigure CMake"
	@echo ""
	@echo "  Simulation Targets:"
	@echo "    make run                - Run default simulation"
	@echo "    make run-earth-moon     - Run Earth-Moon simulation"
	@echo "    make run-solar-system   - Run full Solar System simulation"
	@echo ""
	@echo "  Viewer Targets:"
	@echo "    make view               - Launch viewer with default output"
	@echo "    make view-last          - Launch viewer with Earth-Moon output"
	@echo ""
	@echo "  Utility Targets:"
	@echo "    make fetch              - Fetch HORIZONS ephemeris data"
	@echo "    make validate           - Validate default system file"
	@echo "    make validate-earth-moon - Validate Earth-Moon system file"
	@echo "    make test               - Quick validation test"
	@echo ""
	@echo "  Python Plotting Targets:"
	@echo "    make plot               - Generate all plots"
	@echo "    make plot-energy        - Energy conservation plot"
	@echo "    make plot-momentum      - Angular momentum plot"
	@echo "    make plot-3d            - 3D orbit visualization"
	@echo "    make plot-3d-exaggerated - 3D with exaggerated scale"
	@echo "    make plot-3d-earth-moon - Earth-Moon 3D plot"
	@echo ""
	@echo "  Configuration Options:"
	@echo "    BUILD_VIEWER=0          - Build without OpenGL viewer (CI/headless)"
	@echo "    BUILD_VIEWER=1          - Build with OpenGL viewer (default)"
	@echo "    CMAKE_FLAGS='...'       - Pass additional flags to CMake"
	@echo ""
	@echo "  Examples:"
	@echo "    make build              # Normal build"
	@echo "    make BUILD_VIEWER=0 build  # Headless build"
	@echo "    make run-earth-moon    # Quick simulation"
	@echo "    make view-last          # View results"
	@echo ""
	@echo "  Executables location: $(BUILD_DIR)/bin/"
	@echo ""

# ========================================
# DEPENDENCY CHECKS
# ========================================

$(SIM_EXE): | $(BUILD_DIR)/Makefile
	@$(MAKE) -C $(BUILD_DIR) orbit-sim

$(VIEWER_EXE): | $(BUILD_DIR)/Makefile
	@$(MAKE) -C $(BUILD_DIR) orbit-viewer
