CXX = g++
CXXFLAGS = -std=c++17

DEBUGFLAGS = -DDEBUG

Z3_LIB = -lz3

BIN_DIR=bin
SRC_DIR=src

TRACE_DIR=traces
STD_TRACE_DIR=$(TRACE_DIR)/STD-Format
HUMANREADABLE_TRACE_DIR=$(TRACE_DIR)/human_readable_traces
FORMATTED_TRACE_DIR=$(TRACE_DIR)/formatted_traces

TARGET = $(BIN_DIR)/rvpredict
TRACE_GENERATOR = $(BIN_DIR)/trace_generator

DEPS = $(SRC_DIR)/event.cpp $(SRC_DIR)/trace.cpp $(SRC_DIR)/maximal_casual_model.cpp $(SRC_DIR)/expr.cpp
SRC = $(SRC_DIR)/rvpredict.cpp
TRACE_GENERATOR_SRC = $(SRC_DIR)/trace_generator.cpp

STD_CONVERTER=scripts/convert.py

readable_traces = $(shell ls $(HUMANREADABLE_TRACE_DIR)/*.txt)

all: $(TARGET)

$(TARGET): $(SRC) $(DEPS)
	mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC) $(Z3_LIB)

$(TRACE_GENERATOR): $(TRACE_GENERATOR_SRC)
	mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $(TRACE_GENERATOR) $(TRACE_GENERATOR_SRC)

debug: $(SRC) $(DEPS)
	mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(DEBUGFLAGS) -o $(TARGET) $(SRC) $(Z3_LIB)

gen_from_std_trace:
	mkdir -p $(HUMANREADABLE_TRACE_DIR)
	python $(STD_CONVERTER) $(STD_TRACE_DIR) $(HUMANREADABLE_TRACE_DIR)

gen_single_trace: $(TRACE_GENERATOR)
	mkdir -p $(FORMATTED_TRACE_DIR)
	$(TRACE_GENERATOR) $(args) $(FORMATTED_TRACE_DIR)

gen_traces: $(TRACE_GENERATOR)
	mkdir -p $(FORMATTED_TRACE_DIR)
	@for file in $(readable_traces); do \
		echo "Generating trace for $$file..."; \
		$(TRACE_GENERATOR) $$file $(FORMATTED_TRACE_DIR);\
	done

run: $(TARGET)
	@$(TARGET) $(FORMATTED_TRACE_DIR)/$(file)

# Clean target to remove the compiled files
clean:
	rm -f $(TARGET) $(TRACE_GENERATOR) $(TARGET_STRICT)
