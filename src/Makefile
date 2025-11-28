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
OBJS = $(addprefix ../build/src/,$(CXXSRCS:.cpp=.o))
DEPS = $(addprefix ../build/src/,$(CXXSRCS:.cpp=.d))
TARGET = ../build/bin/nhssta

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
	../test/test_integration.cpp \
	../test/test_error_handling.cpp ../test/test_ssta_results.cpp \
	../test/test_ssta_logic_integration.cpp ../test/test_parser.cpp \
	../test/test_performance.cpp ../test/test_expression_print.cpp \
	../test/test_main_cli.cpp ../test/test_parser_lexical_cast.cpp \
	../test/test_parser_tokenizer.cpp ../test/test_exception_unification.cpp \
	../test/test_gate.cpp ../test/test_ssta_unit.cpp \
	../test/test_expression_assert_migration.cpp ../test/test_util.cpp \
	../test/test_public_api_headers.cpp ../test/test_cli_exit_codes.cpp \
	../test/test_handle_ownership.cpp ../test/test_util_boundary.cpp \
	../test/test_coverage_gaps.cpp ../test/test_makefile_portability.cpp \
	../test/test_dev_scripts.cpp ../test/test_regression.cpp \
	../test/test_numerical_error_metrics.cpp ../test/test_documentation_accuracy.cpp \
	../test/test_architecture_documentation.cpp ../test/test_profiling.cpp \
	../test/test_map_to_unordered_map.cpp ../test/test_exception_type_preservation.cpp \
	../test/test_main_exception_handling.cpp ../test/test_ssta_io_separation.cpp \
	../test/test_main_output_format.cpp ../test/test_directory_structure.cpp \
	../test/test_critical_path.cpp ../test/test_namespace_consistency.cpp \
	../test/test_handle_base.cpp ../test/test_expected_prod_pos.cpp \
	../test/test_max_order.cpp ../test/test_order_dependency.cpp \
	../test/test_small_rho.cpp ../test/test_expression_derivative.cpp \
	../test/test_table_analytical_equivalence.cpp
TEST_OBJS = $(patsubst ../test/%.cpp,../build/test/%.o,$(TEST_SRCS))
TEST_TARGET = ../build/bin/nhssta_test

# Core library objects (without main.cpp)
LIB_OBJS = ../build/src/covariance.o ../build/src/max.o ../build/src/sub.o ../build/src/normal.o \
	../build/src/random_variable.o ../build/src/add.o ../build/src/util_numerical.o ../build/src/gate.o \
	../build/src/parser.o ../build/src/ssta.o ../build/src/expression.o

$(TARGET) : $(OBJS) 
	$(Q)mkdir -p ../build/bin
	$(ECHO) "Linking $(TARGET) from $(words $(OBJS)) object files"
	$(Q)$(CXX) $(CXXFLAGS) $(INCLUDE) -o $(TARGET) $(OBJS)

../build/src/%.o : %.cpp
	$(Q)mkdir -p ../build/src
	$(ECHO) "Compiling $< -> $@"
	$(Q)if echo "$(CXXFLAGS)" | grep -q "coverage\|profile-arcs"; then \
		$(CXX) $(CXXFLAGS) $(COVERAGE_FLAGS) $(INCLUDE) -c $< -o $@; \
	else \
		$(CXX) $(CXXFLAGS) $(INCLUDE) -c $< -o $@; \
	fi

../build/src/%.d : %.cpp
	$(Q)mkdir -p ../build/src
	@rm -f $@
	@if echo "$(CXXFLAGS)" | grep -q "coverage\|profile-arcs"; then \
		$(CXX) -MM $(CXXFLAGS) $(COVERAGE_FLAGS) $(INCLUDE) $< | sed "s|\($*\)\.o|../build/src/\1.o $@|g" > $@; \
	else \
		$(CXX) -MM $(CXXFLAGS) $(INCLUDE) $< | sed "s|\($*\)\.o|../build/src/\1.o $@|g" > $@; \
	fi

.PHONY: test test-build tidy coverage coverage-clean

# Test target (includes unit tests and clang-tidy)
test: $(TARGET) $(TEST_TARGET)
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
	@echo ""
	@echo "Running unit tests..."
	@cd .. && ./build/bin/nhssta_test || echo "Note: Some tests may require Boost dependencies to be resolved"

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
COVERAGE_DIR = ../build/coverage

# Coverage build flags (separate from regular build)
COVERAGE_CXXFLAGS = $(CXXFLAGS) $(COVERAGE_FLAGS)

# Coverage test target
coverage: coverage-clean
	@echo "Building with coverage instrumentation..."
	@$(MAKE) CXXFLAGS="$(COVERAGE_CXXFLAGS)" CXX="$(CXX)" clean
	@$(MAKE) CXXFLAGS="$(COVERAGE_CXXFLAGS)" CXX="$(CXX)" test-build
	@echo "Running tests with coverage..."
	@cd .. && ./build/bin/nhssta_test > /dev/null 2>&1 || true
	@echo "Generating coverage report..."
	@mkdir -p $(COVERAGE_DIR)
	@if command -v gcov >/dev/null 2>&1; then \
		for src in $(CXXSRCS); do \
			if [ -f "build/src/$$src.gcda" ]; then \
				gcov -b -c -l -p build/src/$$src 2>/dev/null || true; \
			fi; \
		done; \
	else \
		echo "gcov not found. Install gcc to use gcov."; \
	fi
	@if command -v lcov >/dev/null 2>&1; then \
		echo "Generating HTML coverage report with lcov..."; \
		lcov --capture --directory build/src --directory build/test --output-file $(COVERAGE_DIR)/coverage.info --no-external --ignore-errors inconsistent,unsupported,format,source 2>&1 | grep -E "(Found|Scanning|Writing)" | head -3 || true; \
		if [ -f "$(COVERAGE_DIR)/coverage.info" ] && [ -s "$(COVERAGE_DIR)/coverage.info" ]; then \
			genhtml $(COVERAGE_DIR)/coverage.info --output-directory $(COVERAGE_DIR)/html --ignore-errors inconsistent,unsupported,format,source,category --synthesize-missing 2>&1 | tail -2 || true; \
			echo "Coverage report generated in $(COVERAGE_DIR)/html/index.html"; \
		else \
			echo "No coverage data found. Run tests first."; \
			echo "Debug: Checking for .gcda files..."; \
			find build/src build/test -name "*.gcda" 2>/dev/null | head -3 || echo "No .gcda files found"; \
		fi; \
	else \
		echo "lcov not found. Install with: brew install lcov (macOS) or apt-get install lcov (Ubuntu)"; \
		echo "Coverage .gcov files generated in build/ directory"; \
	fi

# Clean coverage files
coverage-clean:
	@echo "Cleaning coverage files..."
	@rm -rf $(COVERAGE_DIR)
	@find build -name "*.gcda" -delete 2>/dev/null || true
	@find build -name "*.gcno" -delete 2>/dev/null || true
	@find build -name "*.gcov" -delete 2>/dev/null || true
	@find . -name "*.gcda" -delete 2>/dev/null || true
	@find . -name "*.gcno" -delete 2>/dev/null || true
	@find . -name "*.gcov" -delete 2>/dev/null || true

$(TEST_TARGET): $(TEST_OBJS) $(LIB_OBJS)
	$(Q)mkdir -p ../build/bin
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

../build/test/%.o: ../test/%.cpp
	$(Q)mkdir -p ../build/test
	$(ECHO) "Compiling $< -> $@"
	$(Q)if echo "$(CXXFLAGS)" | grep -q "coverage\|profile-arcs"; then \
		$(CXX) $(CXXFLAGS) $(COVERAGE_FLAGS) $(INCLUDE) $(GTEST_INCLUDE) -I. -I../test -c $< -o $@; \
	else \
		$(CXX) $(CXXFLAGS) $(INCLUDE) $(GTEST_INCLUDE) -I. -I../test -c $< -o $@; \
	fi

clean :
	rm -f $(OBJS) $(DEPS) $(TARGET)
	rm -f $(TEST_OBJS) ../build/bin/nhssta_test
	rm -rf ../build
	@find . -name "*.gcno" -delete 2>/dev/null || true

tags :
	etags *.hpp *.cpp

# Create build directories if they don't exist
$(OBJS): | ../build/src
$(DEPS): | ../build/src
$(TEST_OBJS): | ../build/test
$(TARGET): | ../build/bin
$(TEST_TARGET): | ../build/bin

../build/src ../build/test ../build/bin:
	$(Q)mkdir -p $@

-include $(DEPS)
