CXX = g++
CXXFLAGS = -std=c++17

Z3_INCLUDE = -I/opt/homebrew/opt/z3/include
Z3_LIB = -L/opt/homebrew/opt/z3/lib -lz3

CUSTOM_Z3_INCLUDE = -I/Users/rama/Desktop/FYP/z3/src/api/c++ -I/Users/rama/Desktop/FYP/z3/src/api
CUSTOM_Z3_LIB = -L/Users/rama/Desktop/FYP/z3/build -lz3

GTEST_INCLUDE = -I/opt/homebrew/opt/googletest/include
GTEST_LIB = -L/opt/homebrew/opt/googletest/lib -lgtest -lgtest_main -pthread

TARGET = predictor
TEST_TARGET = run_tests

# Directories
SRC_DIR = src
OBJ_DIR = obj
TEST_DIR = tests
TEST_OBJ_DIR = obj/tests

# Source and object files
SRCS = $(wildcard $(SRC_DIR)/*.cpp)
OBJS = $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(SRCS))

TEST_SRCS = $(wildcard $(TEST_DIR)/*.cpp)
TEST_OBJS = $(patsubst $(TEST_DIR)/%.cpp, $(TEST_OBJ_DIR)/%.o, $(TEST_SRCS))

# Header files
HEADERS = $(wildcard $(SRC_DIR)/*.h)

# Default rule to build the target
all: $(TARGET)

# Rule to link the object files into the final executable
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(Z3_LIB) -o $@ $(OBJS)

# Rule to compile source files into object files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp $(HEADERS)
	mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(Z3_INCLUDE) -c $< -o $@

# Rule to compile test files into object files
$(TEST_OBJ_DIR)/%.o: $(TEST_DIR)/%.cpp $(HEADERS)
	mkdir -p $(TEST_OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(Z3_INCLUDE) $(GTEST_INCLUDE) -c $< -o $@

# Rule to build the test executable
$(TEST_TARGET): $(TEST_OBJS)
	$(CXX) $(CXXFLAGS) $(Z3_LIB) $(GTEST_LIB) -o $@ $(TEST_OBJS)

# Rule to run tests
test: $(TEST_TARGET)
	./$(TEST_TARGET)
	rm -rf $(TEST_OBJ_DIR) $(TEST_TARGET)

# Clean rule to remove generated files
clean:
	rm -rf $(OBJ_DIR) $(TARGET)