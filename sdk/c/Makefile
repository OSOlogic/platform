# Define the compiler and flags
CXX = g++
CXXFLAGS = -fPIC -std=c++17 -Wall

# Define the target name
TARGET = libplc_osologic.so

# Define the source files and header files
SRCS = plc_osologic_api.cpp
HEADERS = plc_osologic_api.h

# Define the output object file
OBJS = $(SRCS:.cpp=.o)

# Define the library flags
LDFLAGS = -shared

# Default target
all: $(TARGET)

# Rule to create the dynamic library
$(TARGET): $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^

# Rule to compile source files into object files
%.o: %.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean target to remove generated files
clean:
	rm -f $(OBJS) $(TARGET)