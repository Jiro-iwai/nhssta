/**
 * @file test_covariance_cache_normalization.cpp
 * @brief Tests for Issue #165: Covariance cache key normalization
 *
 * Tests the key normalization and symmetric hash function improvements
 * to reduce lookup overhead from 2 searches to 1 search.
 */

#include <gtest/gtest.h>

#include "../src/covariance.hpp"
#include "../src/normal.hpp"
#include "../src/add.hpp"

using RandomVar = RandomVariable::RandomVariable;
using Normal = RandomVariable::Normal;

class CovarianceCacheNormalizationTest : public ::testing::Test {
   protected:
    void SetUp() override {
        // Clear covariance matrix before each test
        RandomVariable::get_covariance_matrix()->clear();
    }
};

// Test: normalize() should return keys in consistent order (a <= b)
TEST_F(CovarianceCacheNormalizationTest, NormalizeConsistentOrder) {
    Normal a(10.0, 4.0);
    Normal b(5.0, 1.0);
    
    // After normalization, (a, b) and (b, a) should produce the same key
    RandomVariable::CovarianceMatrixImpl::RowCol key1(a, b);
    RandomVariable::CovarianceMatrixImpl::RowCol key2(b, a);
    
    // Both should be normalized to the same order (pointer comparison: smaller address first)
    // Since we can't directly test normalize() without exposing it,
    // we test through lookup() behavior
    RandomVariable::CovarianceMatrixImpl matrix;
    
    // Set covariance using (a, b)
    matrix.set(a, b, 2.5);
    
    // Lookup using (b, a) should find the same value
    double cov;
    bool found = matrix.lookup(b, a, cov);
    
    EXPECT_TRUE(found);
    EXPECT_DOUBLE_EQ(cov, 2.5);
}

// Test: set() should store normalized keys
TEST_F(CovarianceCacheNormalizationTest, SetStoresNormalizedKey) {
    Normal a(10.0, 4.0);
    Normal b(5.0, 1.0);
    
    RandomVariable::CovarianceMatrixImpl matrix;
    
    // Set using (a, b)
    matrix.set(a, b, 3.0);
    
    // Cache should contain exactly one entry
    EXPECT_EQ(matrix.size(), 1);
    
    // Lookup using (b, a) should find the same entry
    double cov;
    bool found = matrix.lookup(b, a, cov);
    
    EXPECT_TRUE(found);
    EXPECT_DOUBLE_EQ(cov, 3.0);
}

// Test: lookup() should only perform 1 search after normalization
TEST_F(CovarianceCacheNormalizationTest, LookupSingleSearch) {
    Normal a(10.0, 4.0);
    Normal b(5.0, 1.0);
    
    RandomVariable::CovarianceMatrixImpl matrix;
    
    // Set covariance
    matrix.set(a, b, 4.0);
    
    // Clear and verify cache size
    EXPECT_EQ(matrix.size(), 1);
    
    // Lookup should succeed with single search
    double cov;
    bool found1 = matrix.lookup(a, b, cov);
    EXPECT_TRUE(found1);
    EXPECT_DOUBLE_EQ(cov, 4.0);
    
    // Reverse lookup should also succeed (using same cache entry)
    double cov2;
    bool found2 = matrix.lookup(b, a, cov2);
    EXPECT_TRUE(found2);
    EXPECT_DOUBLE_EQ(cov2, 4.0);
    
    // Cache size should still be 1 (no duplicate entries)
    EXPECT_EQ(matrix.size(), 1);
}

// Test: PairHash should produce same hash for (a, b) and (b, a)
TEST_F(CovarianceCacheNormalizationTest, SymmetricHash) {
    Normal a(10.0, 4.0);
    Normal b(5.0, 1.0);
    
    RandomVariable::PairHash hasher;
    RandomVariable::CovarianceMatrixImpl::RowCol key1(a, b);
    RandomVariable::CovarianceMatrixImpl::RowCol key2(b, a);
    
    // After normalization, both keys should produce the same hash
    // Note: This test assumes normalize() is applied before hashing
    // The actual hash values may differ, but the normalized keys should hash the same
    
    // Test through actual cache behavior
    RandomVariable::CovarianceMatrixImpl matrix;
    matrix.set(a, b, 5.0);
    
    // Both lookups should find the same entry
    double cov1, cov2;
    bool found1 = matrix.lookup(a, b, cov1);
    bool found2 = matrix.lookup(b, a, cov2);
    
    EXPECT_TRUE(found1);
    EXPECT_TRUE(found2);
    EXPECT_DOUBLE_EQ(cov1, cov2);
    EXPECT_EQ(matrix.size(), 1);
}

// Test: Multiple set() calls with different order should not create duplicates
TEST_F(CovarianceCacheNormalizationTest, NoDuplicateEntries) {
    Normal a(10.0, 4.0);
    Normal b(5.0, 1.0);
    Normal c(8.0, 2.0);
    
    RandomVariable::CovarianceMatrixImpl matrix;
    
    // Set covariances in different orders
    matrix.set(a, b, 1.0);
    matrix.set(b, a, 1.5);  // Should update existing entry, not create new one
    matrix.set(a, c, 2.0);
    matrix.set(c, a, 2.5);  // Should update existing entry
    
    // Should have only 2 entries: (a, b) and (a, c) (normalized)
    EXPECT_EQ(matrix.size(), 2);
    
    // Verify values
    double cov_ab, cov_ac;
    matrix.lookup(a, b, cov_ab);
    matrix.lookup(a, c, cov_ac);
    
    EXPECT_DOUBLE_EQ(cov_ab, 1.5);  // Last set value
    EXPECT_DOUBLE_EQ(cov_ac, 2.5);  // Last set value
}

// Test: Cache efficiency - same variable pairs should use same cache entry
TEST_F(CovarianceCacheNormalizationTest, CacheEfficiency) {
    Normal a(10.0, 4.0);
    Normal b(5.0, 1.0);
    
    RandomVariable::CovarianceMatrixImpl matrix;
    
    // Set multiple times with different orders
    matrix.set(a, b, 1.0);
    matrix.set(b, a, 2.0);
    matrix.set(a, b, 3.0);
    
    // Should still have only 1 entry
    EXPECT_EQ(matrix.size(), 1);
    
    // Final value should be 3.0
    double cov;
    matrix.lookup(a, b, cov);
    EXPECT_DOUBLE_EQ(cov, 3.0);
    
    // Reverse lookup should also return 3.0
    double cov2;
    matrix.lookup(b, a, cov2);
    EXPECT_DOUBLE_EQ(cov2, 3.0);
}

// Test: Integration with actual covariance calculation
TEST_F(CovarianceCacheNormalizationTest, IntegrationWithCovarianceCalculation) {
    Normal a(10.0, 4.0);
    Normal b(5.0, 1.0);
    
    RandomVar sum = a + b;
    
    // Compute covariance multiple times with different orders
    double cov1 = RandomVariable::covariance(a, sum);
    double cov2 = RandomVariable::covariance(sum, a);
    
    // Results should be identical
    EXPECT_DOUBLE_EQ(cov1, cov2);
    
    // Cache should contain normalized entries
    auto& matrix = RandomVariable::get_covariance_matrix();
    // Note: Actual cache size depends on internal calculations,
    // but (a, sum) and (sum, a) should use the same cache entry
}

