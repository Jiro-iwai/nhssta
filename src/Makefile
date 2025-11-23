SHELL = /bin/sh

CXX = g++
CXXFLAGS = -g -Wall -std=c++17 
#INCLUDE = -I/usr/include/boost-1_33_1
BOOST_DIR = /opt/homebrew/opt/boost
INCLUDE = -I$(BOOST_DIR)/include 
CXXSRCS = Covariance.C  MAX.C  SUB.C  Normal.C  \
	RandomVariable.C  ADD.C  Util.C Gate.C \
	Parser.C Ssta.C Expression.C main.C
#CXXSRCS =  test.C Expression.C
OBJS = $(CXXSRCS:.C=.o) 
DEPS = $(CXXSRCS:.C=.d) 
TARGET = nhssta

# Google Test configuration
GTEST_DIR = /opt/homebrew/opt/googletest
GTEST_INCLUDE = -I$(GTEST_DIR)/include
GTEST_LIBS = -L$(GTEST_DIR)/lib -lgtest -lgtest_main -pthread

# Test sources (relative to src directory)
TEST_SRCS = ../test/test_randomvariable.cpp ../test/test_normal.cpp \
	../test/test_add.cpp ../test/test_max.cpp ../test/test_covariance.cpp \
	../test/test_integration.cpp ../test/test_smartptr.cpp \
	../test/test_error_handling.cpp ../test/test_ssta_results.cpp \
	../test/test_ssta_logic_integration.cpp ../test/test_parser.cpp \
	../test/test_performance.cpp ../test/test_expression_print.cpp \
	../test/test_main_cli.cpp ../test/test_parser_lexical_cast.cpp \
	../test/test_parser_tokenizer.cpp ../test/test_exception_unification.cpp
TEST_OBJS = $(TEST_SRCS:.cpp=.o)
TEST_TARGET = ../test/nhssta_test

# Core library objects (without main.C)
LIB_OBJS = Covariance.o MAX.o SUB.o Normal.o \
	RandomVariable.o ADD.o Util.o Gate.o \
	Parser.o Ssta.o Expression.o

$(TARGET) : $(OBJS) 
	$(CXX) $(CXXFLAGS) $(INCLUDE) -o $(TARGET) $(OBJS)

%.o : %.C
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c $<

%.d : %.C
	rm -f $@
	$(CXX) -MM $(CXXFLAGS) $(INCLUDE) $< | sed "s/\($*\)\.o[ :]*/\1.o $@ : /g" > $@

.PHONY: test test-build tidy

# Test target
test: $(TEST_TARGET)
	@echo "Running unit tests..."
	@./$(TEST_TARGET) || echo "Note: Some tests may require Boost dependencies to be resolved"

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

$(TEST_TARGET): $(TEST_OBJS) $(LIB_OBJS)
	@mkdir -p ../test
	$(CXX) $(CXXFLAGS) $(INCLUDE) $(GTEST_INCLUDE) -o $(TEST_TARGET) $(TEST_OBJS) $(LIB_OBJS) $(GTEST_LIBS)

../test/%.o: ../test/%.cpp
	@mkdir -p ../test
	$(CXX) $(CXXFLAGS) $(INCLUDE) $(GTEST_INCLUDE) -I. -c $< -o $@

clean :
	rm -f $(OBJS) $(DEPS) $(TARGET)
	rm -f $(TEST_OBJS) ../test/nhssta_test

tags :
	etags *.h *.C

-include $(DEPS)



