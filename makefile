# Define the compiler and flags
CXX = g++
CXXFLAGS = -std=c++17

# Path to Z3 library (optional if Z3 is installed in a standard path)
Z3_LIB = -lz3

# Name of the executable
TARGET = rvpredict
TRACE_GENERATOR = trace_generator

# Source file
SRC = rvpredict.cpp
TRACE_GENERATOR_SRC = trace_generator.cpp

DEPS = event.cpp trace.cpp maximal_casual_model.cpp expr.cpp

# Build target
all: $(TARGET)

# Rule to build the executable
$(TARGET): $(SRC) $(DEPS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC) $(Z3_LIB)

$(TRACE_GENERATOR): $(TRACE_GENERATOR_SRC)
	$(CXX) $(CXXFLAGS) -o $(TRACE_GENERATOR) $(TRACE_GENERATOR_SRC)

trace_gen: $(TRACE_GENERATOR)
	./$(TRACE_GENERATOR) $(args)

# Clean target to remove the compiled files
clean:
	rm -f $(TARGET)
