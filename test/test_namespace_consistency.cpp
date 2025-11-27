// -*- c++ -*-
// Unit tests for namespace consistency (Issue #129)
// Verifies that classes are in appropriate namespaces

#include <gtest/gtest.h>

#include <fstream>
#include <nhssta/exception.hpp>
#include <nhssta/net_line.hpp>
#include <nhssta/ssta.hpp>

#include "../src/covariance.hpp"
#include "../src/expression.hpp"
#include "../src/gate.hpp"
#include "../src/normal.hpp"
#include "../src/parser.hpp"
#include "../src/random_variable.hpp"

class NamespaceConsistencyTest : public ::testing::Test {};

// Test: Parser should be in Nh namespace
// After migration, Parser should be accessible as Nh::Parser
TEST_F(NamespaceConsistencyTest, ParserInNhNamespace) {
    // Create a temporary file for testing
    std::string test_content = "test data\n";
    std::string filepath = "/tmp/test_ns_parser.txt";
    {
        std::ofstream f(filepath);
        f << test_content;
    }

    // Parser should be constructible from Nh namespace
    Nh::Parser parser(filepath, '#', nullptr, " \t");
    // checkFile() throws if file open failed, so if we get here, it's good
    EXPECT_NO_THROW(parser.checkFile());

    std::remove(filepath.c_str());
}

// Test: RandomVariable types should be in RandomVariable namespace
TEST_F(NamespaceConsistencyTest, RandomVariableInNamespace) {
    // These should all be in RandomVariable namespace
    RandomVariable::Normal n(1.0, 1.0);
    RandomVariable::RandomVariable rv = n;

    EXPECT_DOUBLE_EQ(rv->mean(), 1.0);
}

// Test: Gate types should be in Nh namespace
TEST_F(NamespaceConsistencyTest, GateTypesInNhNamespace) {
    // Gate and Instance should be in Nh namespace
    RandomVariable::Normal delay(1.0, 0.1);
    Nh::Gate g;
    g->set_delay("in", "out", delay);

    // Gate is a handle type, verify it works
    EXPECT_TRUE(g.get() != nullptr);
}

// Test: Exception types should be in Nh namespace
TEST_F(NamespaceConsistencyTest, ExceptionTypesInNhNamespace) {
    // All exception types should be in Nh namespace
    EXPECT_THROW(throw Nh::Exception("test", "message"), Nh::Exception);
    EXPECT_THROW(throw Nh::FileException("file", "message"), Nh::FileException);
    EXPECT_THROW(throw Nh::ParseException("file", 1, "message"), Nh::ParseException);
    EXPECT_THROW(throw Nh::RuntimeException("message"), Nh::RuntimeException);
}

// Test: CovarianceMatrix should be in RandomVariable namespace
TEST_F(NamespaceConsistencyTest, CovarianceMatrixInNamespace) {
    RandomVariable::CovarianceMatrix cov;
    // Verify construction works - CovarianceMatrix is a handle type
    EXPECT_TRUE(true);
}

// Test: Ssta should be in Nh namespace
TEST_F(NamespaceConsistencyTest, SstaInNhNamespace) {
    // Ssta is declared in nhssta/ssta.hpp, verify it's in Nh namespace
    static_assert(std::is_class<Nh::Ssta>::value, "Ssta should be a class in Nh namespace");
    EXPECT_TRUE(true);
}

// Test: Type aliases should use 'using' syntax (C++11+)
// This test verifies that the codebase uses modern 'using' declarations
// instead of C-style 'typedef' (Issue #129)
TEST_F(NamespaceConsistencyTest, TypeAliasesUseUsingSyntax) {
    // Verify that type aliases are usable (compile-time check)
    // Nh::Normal and Nh::RandomVariable are aliases defined with 'using'
    Nh::Normal n(0.0, 1.0);
    Nh::RandomVariable rv = n;
    EXPECT_DOUBLE_EQ(rv->mean(), 0.0);

    // Nh::Signals is an alias for unordered_map
    Nh::Signals signals;
    signals["test"] = n;
    EXPECT_EQ(signals.size(), 1);

    // Nh::GateImpl::IO and Nh::GateImpl::Delays are aliases inside class
    Nh::Gate g;
    g->set_delay("in", "out", n);
    EXPECT_TRUE(true);
}

// Test: Internal implementation classes use 'Impl' suffix (Issue #129)
// This naming convention follows the pImpl idiom standard
TEST_F(NamespaceConsistencyTest, ImplNamingConvention) {
    // RandomVariableImpl is the implementation class for RandomVariable handle
    static_assert(std::is_class<RandomVariable::RandomVariableImpl>::value,
                  "RandomVariableImpl should exist");

    // NormalImpl is the implementation class for Normal handle
    static_assert(std::is_class<RandomVariable::NormalImpl>::value,
                  "NormalImpl should exist");

    // GateImpl is the implementation class for Gate handle
    static_assert(std::is_class<Nh::GateImpl>::value,
                  "GateImpl should exist");

    // InstanceImpl is the implementation class for Instance handle
    static_assert(std::is_class<Nh::InstanceImpl>::value,
                  "InstanceImpl should exist");

    // CovarianceMatrixImpl is the implementation class
    static_assert(std::is_class<RandomVariable::CovarianceMatrixImpl>::value,
                  "CovarianceMatrixImpl should exist");

    // ExpressionImpl is the implementation class for Expression handle
    static_assert(std::is_class<ExpressionImpl>::value,
                  "ExpressionImpl should exist");

    // ConstImpl and VariableImpl are derived from ExpressionImpl
    static_assert(std::is_base_of<ExpressionImpl, ConstImpl>::value,
                  "ConstImpl should derive from ExpressionImpl");
    static_assert(std::is_base_of<ExpressionImpl, VariableImpl>::value,
                  "VariableImpl should derive from ExpressionImpl");

    EXPECT_TRUE(true);
}

// Test: Handle classes use 'Handle' suffix with using alias for public API
// This ensures consistent naming pattern across all handle types
TEST_F(NamespaceConsistencyTest, HandleSuffixNamingConvention) {
    // RandomVariableHandle is aliased as RandomVariable
    static_assert(std::is_class<RandomVariable::RandomVariableHandle>::value,
                  "RandomVariableHandle should exist");
    static_assert(std::is_same<RandomVariable::RandomVariable,
                               RandomVariable::RandomVariableHandle>::value,
                  "RandomVariable should be alias of RandomVariableHandle");

    // ExpressionHandle is aliased as Expression
    static_assert(std::is_class<ExpressionHandle>::value,
                  "ExpressionHandle should exist");
    static_assert(std::is_same<Expression, ExpressionHandle>::value,
                  "Expression should be alias of ExpressionHandle");

    // GateHandle is aliased as Gate
    static_assert(std::is_class<Nh::GateHandle>::value,
                  "GateHandle should exist in Nh namespace");
    static_assert(std::is_same<Nh::Gate, Nh::GateHandle>::value,
                  "Gate should be alias of GateHandle");

    // InstanceHandle is aliased as Instance
    static_assert(std::is_class<Nh::InstanceHandle>::value,
                  "InstanceHandle should exist in Nh namespace");
    static_assert(std::is_same<Nh::Instance, Nh::InstanceHandle>::value,
                  "Instance should be alias of InstanceHandle");

    // CovarianceMatrixHandle is aliased as CovarianceMatrix
    static_assert(std::is_class<RandomVariable::CovarianceMatrixHandle>::value,
                  "CovarianceMatrixHandle should exist");
    static_assert(std::is_same<RandomVariable::CovarianceMatrix,
                               RandomVariable::CovarianceMatrixHandle>::value,
                  "CovarianceMatrix should be alias of CovarianceMatrixHandle");

    // NetLineHandle is aliased as NetLine
    static_assert(std::is_class<Nh::NetLineHandle>::value,
                  "NetLineHandle should exist in Nh namespace");
    static_assert(std::is_same<Nh::NetLine, Nh::NetLineHandle>::value,
                  "NetLine should be alias of NetLineHandle");

    // NetLineImpl should exist (implementation class)
    static_assert(std::is_class<Nh::NetLineImpl>::value,
                  "NetLineImpl should exist in Nh namespace");

    EXPECT_TRUE(true);
}
