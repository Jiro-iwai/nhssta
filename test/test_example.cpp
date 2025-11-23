#include <gtest/gtest.h>
#include <string>

// Test fixture for shared setup/teardown
class MathTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Common setup code
    }

    void TearDown() override {
        // Common teardown code
    }
};

// Basic arithmetic tests
TEST_F(MathTest, Addition) {
    EXPECT_EQ(1 + 1, 2);
    EXPECT_EQ(2 + 2, 4);
    ASSERT_EQ(5 + 3, 8);
}

TEST_F(MathTest, Multiplication) {
    EXPECT_EQ(2 * 3, 6);
    EXPECT_EQ(5 * 0, 0);
    EXPECT_EQ(4 * 5, 20);
}

// String operation tests
TEST(StringOperationsTest, Concatenation) {
    std::string hello = "Hello";
    std::string world = "World";

    std::string result = hello + " " + world;
    EXPECT_EQ(result, "Hello World");
    EXPECT_EQ(result.length(), 11);
}

TEST(StringOperationsTest, Length) {
    std::string hello = "Hello";
    std::string world = "World";

    EXPECT_EQ(hello.length(), 5);
    EXPECT_EQ(world.length(), 5);
    EXPECT_LT(hello.length(), world.length() + 1);
}

// Parameterized test example
class MathParameterizedTest : public ::testing::TestWithParam<std::tuple<int, int, int>> {
};

TEST_P(MathParameterizedTest, AdditionParameterized) {
    int a, b, expected;
    std::tie(a, b, expected) = GetParam();
    EXPECT_EQ(a + b, expected);
}

INSTANTIATE_TEST_SUITE_P(
    AdditionTests,
    MathParameterizedTest,
    ::testing::Values(
        std::make_tuple(1, 1, 2),
        std::make_tuple(2, 3, 5),
        std::make_tuple(10, 20, 30)
    )
);

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}