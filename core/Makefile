# =============================================================================
# === 1. CONFIGURATION ========================================================
# =============================================================================
CXX = g++
RM = rm -f

# Compilation and linking flags
CXXFLAGS = -g -Wall -std=c++17 # -g for debug, -Wall for warnings, -std=c++17
LDFLAGS = -g
LDLIBS = -lwiringPi -lpthread -lmariadbclient -lrt -lstdc++fs # Or -lmariadb if correct on your system

# Directories and executable name
BUILD_DIR = build
TARGET := $(BUILD_DIR)/core_plc_osologic

# =============================================================================
# === 2. AUTOMATIC LOGIC ======================================================
# =============================================================================

# Automatic recursive detection of all .cpp files
SRCS := $(shell find . -name '*.cpp')

# Map .cpp files to .o files in the build directory
OBJS := $(patsubst ./%.cpp,$(BUILD_DIR)/%.o,$(SRCS))

# Map .o files to dependency files (.d)
DEPS := $(OBJS:.o=.d)

# Default rule
all: $(TARGET)

# Debug target (adds -DDEBUG flag and compiles)
debug: CXXFLAGS += -DDEBUG
debug: all

# Trace target (adds -DTRACE flag for data flow traceability)
trace: CXXFLAGS += -DTRACE
trace: all

# Rule to link the final executable
# Depends on the objects and the build directory (order-only prerequisite)
$(TARGET): $(OBJS) | $(BUILD_DIR)
	@echo "Linking final executable..."
	$(CXX) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)
	@echo "==> Executable created at: $(TARGET)"

# Rule to compile each .cpp to .o
$(BUILD_DIR)/%.o: ./%.cpp | $(BUILD_DIR)
	@mkdir -p $(@D)
	@echo "Compiling $<..."
	$(CXX) $(CXXFLAGS) -c $< -o $@ -MMD -MP

# Rule to create the build directory
$(BUILD_DIR):
	@mkdir -p $@

# Cleaning
clean:
	@echo "Cleaning the build directory..."
	$(RM) -r $(BUILD_DIR)

distclean: clean
	@echo "Cleaning everything..."
	$(RM) -r $(BUILD_DIR) *~

# Include all automatically generated dependency files
# The leading '-' prevents errors if the file doesn't exist on the first run
-include $(DEPS)

# Declare targets that are not files
.PHONY: all debug trace clean distclean