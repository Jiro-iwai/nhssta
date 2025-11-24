SHELL = /bin/sh

# Verbose mode: set V=1 to see full compilation commands
# Usage: make V=1 test
V ?= 0
ifeq ($(V),1)
    Q :=
    ECHO := @echo
else
    Q := @
    ECHO := @echo
endif

CXX ?= g++
CXXFLAGS = -g -Wall -std=c++17 
# Boost dependencies have been removed in Phase 1
# If you need Boost for legacy code, uncomment and set BOOST_DIR:
# BOOST_DIR = /opt/homebrew/opt/boost
# INCLUDE = -I$(BOOST_DIR)/include
INCLUDE = -I../include 
CXXSRCS = covariance.cpp max.cpp sub.cpp normal.cpp \
	random_variable.cpp add.cpp util_numerical.cpp gate.cpp \
	parser.cpp ssta.cpp expression.cpp main.cpp
OBJS = $(CXXSRCS:.cpp=.o) 
DEPS = $(CXXSRCS:.cpp=.d) 
TARGET = nhssta

# Google Test configuration
# GTEST_DIR can be overridden via:
#   1. Environment variable: export GTEST_DIR=/path/to/googletest
#   2. Make command line: make GTEST_DIR=/path/to/googletest test
# Default paths for common installations:
#   macOS (Homebrew): /opt/homebrew/opt/googletest or /usr/local/opt/googletest
#   Ubuntu/Debian: /usr
#   Custom: Set GTEST_DIR environment variable or pass as make argument
# Use ?= to allow override via environment variable or command line
GTEST_DIR ?= /opt/homebrew/opt/googletest
GTEST_INCLUDE := -I$(GTEST_DIR)/include -I../include
GTEST_LIBS := -L$(GTEST_DIR)/lib -lgtest -lgtest_main -pthread

# Test sources (relative to src directory)
TEST_SRCS = ../test/test_randomvariable.cpp ../test/test_normal.cpp \
	../test/test_add.cpp ../test/test_max.cpp ../test/test_covariance.cpp \
	../test/test_integration.cpp ../test/test_smartptr.cpp \
	../test/test_error_handling.cpp ../test/test_ssta_results.cpp \
	../test/test_ssta_logic_integration.cpp ../test/test_parser.cpp \
	../test/test_performance.cpp ../test/test_expression_print.cpp \
	../test/test_main_cli.cpp ../test/test_parser_lexical_cast.cpp \
	../test/test_parser_tokenizer.cpp ../test/test_exception_unification.cpp \
	../test/test_gate.cpp ../test/test_ssta_unit.cpp \
	../test/test_expression_assert_migration.cpp ../test/test_util.cpp \
	../test/test_public_api_headers.cpp ../test/test_cli_exit_codes.cpp \
	../test/test_handle_ownership.cpp ../test/test_rcobject_removal.cpp \
	../test/test_smartptr_removal.cpp ../test/test_util_boundary.cpp \
	../test/test_coverage_gaps.cpp ../test/test_makefile_portability.cpp \
	../test/test_dev_scripts.cpp ../test/test_regression.cpp \
	../test/test_numerical_error_metrics.cpp ../test/test_documentation_accuracy.cpp \
	../test/test_architecture_documentation.cpp ../test/test_profiling.cpp \
	../test/test_map_to_unordered_map.cpp
TEST_OBJS = $(TEST_SRCS:.cpp=.o)
TEST_TARGET = ../test/nhssta_test

# Core library objects (without main.cpp)
LIB_OBJS = covariance.o max.o sub.o normal.o \
	random_variable.o add.o util_numerical.o gate.o \
	parser.o ssta.o expression.o

$(TARGET) : $(OBJS) 
	$(ECHO) "Linking $(TARGET) from $(words $(OBJS)) object files"
	$(Q)$(CXX) $(CXXFLAGS) $(INCLUDE) -o $(TARGET) $(OBJS)

%.o : %.cpp
	$(ECHO) "Compiling $< -> $@"
	$(Q)if echo "$(CXXFLAGS)" | grep -q "coverage\|profile-arcs"; then \
		$(CXX) $(CXXFLAGS) $(COVERAGE_FLAGS) $(INCLUDE) -c $<; \
	else \
		$(CXX) $(CXXFLAGS) $(INCLUDE) -c $<; \
	fi

%.d : %.cpp
	rm -f $@
	@if echo "$(CXXFLAGS)" | grep -q "coverage\|profile-arcs"; then \
		$(CXX) -MM $(CXXFLAGS) $(COVERAGE_FLAGS) $(INCLUDE) $< | sed "s/\($*\)\.o[ :]*/\1.o $@ : /g" > $@; \
	else \
		$(CXX) -MM $(CXXFLAGS) $(INCLUDE) $< | sed "s/\($*\)\.o[ :]*/\1.o $@ : /g" > $@; \
	fi

.PHONY: test test-build tidy coverage coverage-clean

# Test target
test: $(TARGET) $(TEST_TARGET)
	@echo "Running unit tests..."
	@cd .. && ./test/nhssta_test || echo "Note: Some tests may require Boost dependencies to be resolved"

# clang-tidy target
tidy:
	@echo "Running clang-tidy..."
	@CLANG_TIDY=$$(which clang-tidy 2>/dev/null || echo "/opt/homebrew/opt/llvm/bin/clang-tidy"); \
	if [ ! -f "$$CLANG_TIDY" ]; then \
		echo "clang-tidy not found. Install with: brew install llvm"; \
		echo "If installed, ensure /opt/homebrew/opt/llvm/bin is in your PATH"; \
		echo "Skipping clang-tidy check."; \
	else \
		echo "Using clang-tidy: $$CLANG_TIDY"; \
		$$CLANG_TIDY $(CXXSRCS) -- $(CXXFLAGS) $(INCLUDE) 2>&1 | head -100 || true; \
	fi

# Build test target only (without running)
test-build: $(TEST_TARGET)
	@echo "Test binary built successfully: $(TEST_TARGET)"

# Coverage configuration
COVERAGE_FLAGS = -fprofile-arcs -ftest-coverage
COVERAGE_LIBS = -lgcov
COVERAGE_DIR = ../coverage

# Coverage build flags (separate from regular build)
COVERAGE_CXXFLAGS = $(CXXFLAGS) $(COVERAGE_FLAGS)

# Coverage test target
coverage: coverage-clean
	@echo "Building with coverage instrumentation..."
	@$(MAKE) CXXFLAGS="$(COVERAGE_CXXFLAGS)" CXX="$(CXX)" clean
	@$(MAKE) CXXFLAGS="$(COVERAGE_CXXFLAGS)" CXX="$(CXX)" test-build
	@echo "Running tests with coverage..."
	@cd .. && ./test/nhssta_test > /dev/null 2>&1 || true
	@echo "Generating coverage report..."
	@mkdir -p $(COVERAGE_DIR)
	@if command -v gcov >/dev/null 2>&1; then \
		for src in $(CXXSRCS); do \
			if [ -f "$$src.gcda" ]; then \
				gcov -b -c -l -p $$src 2>/dev/null || true; \
			fi; \
		done; \
	else \
		echo "gcov not found. Install gcc to use gcov."; \
	fi
	@if command -v lcov >/dev/null 2>&1; then \
		echo "Generating HTML coverage report with lcov..."; \
		lcov --capture --directory . --directory ../test --output-file $(COVERAGE_DIR)/coverage.info --no-external --ignore-errors inconsistent,unsupported,format,source 2>&1 | grep -E "(Found|Scanning|Writing)" | head -3 || true; \
		if [ -f "$(COVERAGE_DIR)/coverage.info" ] && [ -s "$(COVERAGE_DIR)/coverage.info" ]; then \
			genhtml $(COVERAGE_DIR)/coverage.info --output-directory $(COVERAGE_DIR)/html --ignore-errors inconsistent,unsupported,format,source,category --synthesize-missing 2>&1 | tail -2 || true; \
			echo "Coverage report generated in $(COVERAGE_DIR)/html/index.html"; \
		else \
			echo "No coverage data found. Run tests first."; \
			echo "Debug: Checking for .gcda files..."; \
			find . ../test -name "*.gcda" 2>/dev/null | head -3 || echo "No .gcda files found"; \
		fi; \
	else \
		echo "lcov not found. Install with: brew install lcov (macOS) or apt-get install lcov (Ubuntu)"; \
		echo "Coverage .gcov files generated in src/ directory"; \
	fi

# Clean coverage files
coverage-clean:
	@echo "Cleaning coverage files..."
	@rm -rf $(COVERAGE_DIR)
	@find . -name "*.gcda" -delete
	@find . -name "*.gcno" -delete
	@find . -name "*.gcov" -delete

$(TEST_TARGET): $(TEST_OBJS) $(LIB_OBJS)
	$(Q)mkdir -p ../test
	$(ECHO) "Linking $(TEST_TARGET) from $(words $(TEST_OBJS)) test objects and $(words $(LIB_OBJS)) library objects"
	$(Q)if echo "$(CXXFLAGS)" | grep -qE "(coverage|profile-arcs)"; then \
		if [ "$(CXX)" = "g++" ] || echo "$(CXX)" | grep -q "/g++"; then \
			$(CXX) $(CXXFLAGS) $(INCLUDE) $(GTEST_INCLUDE) -o $(TEST_TARGET) $(TEST_OBJS) $(LIB_OBJS) $(GTEST_LIBS) $(COVERAGE_LIBS); \
		else \
			$(CXX) $(CXXFLAGS) $(INCLUDE) $(GTEST_INCLUDE) -o $(TEST_TARGET) $(TEST_OBJS) $(LIB_OBJS) $(GTEST_LIBS); \
		fi; \
	else \
		$(CXX) $(CXXFLAGS) $(INCLUDE) $(GTEST_INCLUDE) -o $(TEST_TARGET) $(TEST_OBJS) $(LIB_OBJS) $(GTEST_LIBS); \
	fi

$(TEST_OBJS): ../test/%.o: ../test/%.cpp
	$(Q)mkdir -p ../test
	$(ECHO) "Compiling $< -> $@"
	$(Q)if echo "$(CXXFLAGS)" | grep -q "coverage\|profile-arcs"; then \
		$(CXX) $(CXXFLAGS) $(COVERAGE_FLAGS) $(INCLUDE) $(GTEST_INCLUDE) -I. -I../test -c $< -o $@; \
	else \
		$(CXX) $(CXXFLAGS) $(INCLUDE) $(GTEST_INCLUDE) -I. -I../test -c $< -o $@; \
	fi

clean :
	rm -f $(OBJS) $(DEPS) $(TARGET)
	rm -f $(TEST_OBJS) ../test/nhssta_test

tags :
	etags *.hpp *.cpp

-include $(DEPS)
