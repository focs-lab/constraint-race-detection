CXX = g++
CXXFLAGS = -std=c++17

DEBUGFLAGS = -DDEBUG

Z3_INCLUDE = -I/opt/homebrew/opt/z3/include
Z3_LIB = -L/opt/homebrew/opt/z3/lib -lz3

CUSTOM_Z3_INCLUDE = -I/Users/rama/Desktop/FYP/z3/src/api/c++ -I/Users/rama/Desktop/FYP/z3/src/api
CUSTOM_Z3_LIB = -L/Users/rama/Desktop/FYP/z3/build -lz3

# export DYLD_LIBRARY_PATH=/Users/rama/Desktop/FYP/z3/build:$DYLD_LIBRARY_PATH

BIN_DIR=bin
SRC_DIR=src

TRACE_DIR=traces
STD_TRACE_DIR=$(TRACE_DIR)/STD-Format
HUMANREADABLE_TRACE_DIR=$(TRACE_DIR)/human_readable_traces
FORMATTED_TRACE_DIR=$(TRACE_DIR)/formatted_traces

TARGET = $(BIN_DIR)/rvpredict
TRACE_GENERATOR = $(BIN_DIR)/trace_generator

DEPS = $(SRC_DIR)/event.cpp $(SRC_DIR)/trace.cpp $(SRC_DIR)/model.cpp $(SRC_DIR)/custom_maximal_casual_model.cpp $(SRC_DIR)/expr.cpp $(SRC_DIR)/z3_maximal_casual_model.cpp $(SRC_DIR)/partial_maximal_casual_model.cpp
DEPS += $(SRC_DIR)/model_logger.cpp
SRC = $(SRC_DIR)/rvpredict.cpp
TRACE_GENERATOR_SRC = $(SRC_DIR)/trace_generator.cpp

STD_CONVERTER=scripts/convert.py

readable_traces = $(shell ls $(HUMANREADABLE_TRACE_DIR)/*.txt)

all: $(TARGET) $(TRACE_GENERATOR)

$(TARGET): $(SRC) $(DEPS)
	mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(Z3_INCLUDE) -o $(TARGET) $(SRC) $(Z3_LIB)

$(TRACE_GENERATOR): $(TRACE_GENERATOR_SRC)
	mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $(TRACE_GENERATOR) $(TRACE_GENERATOR_SRC)

debug: $(SRC) $(DEPS)
	mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(Z3_INCLUDE) $(DEBUGFLAGS) -o $(TARGET) $(SRC) $(Z3_LIB)

custom_debug: $(SRC) $(DEPS)
	mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(CUSTOM_Z3_INCLUDE) $(DEBUGFLAGS) -o $(TARGET) $(SRC) $(CUSTOM_Z3_LIB)

gen_from_std_trace:
	mkdir -p $(HUMANREADABLE_TRACE_DIR)
	@for file in $(shell ls $(STD_TRACE_DIR)/*.std); do \
		echo "Generating human readable trace for $$file..."; \
		python $(STD_CONVERTER) $$file $(HUMANREADABLE_TRACE_DIR);\
	done

gen_single_trace: $(TRACE_GENERATOR)
	mkdir -p $(FORMATTED_TRACE_DIR)
	$(TRACE_GENERATOR) $(HUMANREADABLE_TRACE_DIR)/$(file).txt $(FORMATTED_TRACE_DIR)

gen_traces: $(TRACE_GENERATOR)
	mkdir -p $(FORMATTED_TRACE_DIR)
	@for file in $(readable_traces); do \
		echo "Generating trace for $$file..."; \
		$(TRACE_GENERATOR) $$file $(FORMATTED_TRACE_DIR);\
	done

run: $(TARGET)
	@$(TARGET) -f $(FORMATTED_TRACE_DIR)/$(file) --log_witness

gen_and_run: gen_single_trace run

# Clean target to remove the compiled files
clean:
	rm -f $(TARGET) $(TRACE_GENERATOR) $(TARGET_STRICT)
