// Test for Issue #44: Public API headers organization
// This test verifies that public API headers can be included from include/nhssta/

#include <gtest/gtest.h>
#include <nhssta/ssta.hpp>
#include <nhssta/exception.hpp>
#include <nhssta/ssta_results.hpp>

// Test that public API headers can be included with the new path
TEST(PublicApiHeadersTest, IncludeSstaHeader) {
    // Verify that ssta.hpp can be included from include/nhssta/
    // This test compiles successfully if the header is accessible
    EXPECT_TRUE(true);
}

TEST(PublicApiHeadersTest, IncludeExceptionHeader) {
    // Test that exception.hpp can be included from include/nhssta/
    Nh::Exception e("test");
    EXPECT_STREQ(e.what(), "test");
}

TEST(PublicApiHeadersTest, IncludeSstaResultsHeader) {
    // Test that ssta_results.hpp can be included from include/nhssta/
    Nh::LatResult result("test_node", 10.0, 2.0);
    EXPECT_EQ(result.node_name, "test_node");
    EXPECT_DOUBLE_EQ(result.mean, 10.0);
    EXPECT_DOUBLE_EQ(result.std_dev, 2.0);
    
    Nh::CorrelationMatrix matrix;
    EXPECT_EQ(matrix.node_names.size(), 0);
    EXPECT_EQ(matrix.correlations.size(), 0);
}

// Test that Ssta class can be instantiated using public API
TEST(PublicApiHeadersTest, SstaClassInstantiation) {
    Nh::Ssta ssta;
    // If this compiles and runs, the public API is accessible
    EXPECT_TRUE(true);
}

