// -*- c++ -*-
// Unit tests for HandleBase template

#include <gtest/gtest.h>

#include "../src/handle.hpp"

// Test implementation class
class TestImpl {
   public:
    TestImpl() : value_(0) {}
    explicit TestImpl(int value) : value_(value) {}
    virtual ~TestImpl() = default;

    [[nodiscard]] int value() const { return value_; }
    void set_value(int v) { value_ = v; }

   private:
    int value_;
};

// Derived test implementation class
class DerivedTestImpl : public TestImpl {
   public:
    explicit DerivedTestImpl(int value) : TestImpl(value) {}
    [[nodiscard]] int doubled() const { return value() * 2; }
};

// Test handle class using HandleBase
class TestHandle : public HandleBase<TestImpl> {
   public:
    using HandleBase<TestImpl>::HandleBase;  // Inherit constructors
};

class HandleBaseTest : public ::testing::Test {};

// Test: Default constructor creates null handle
TEST_F(HandleBaseTest, DefaultConstructorCreatesNull) {
    TestHandle h;
    EXPECT_FALSE(static_cast<bool>(h));
    EXPECT_EQ(h.get(), nullptr);
}

// Test: nullptr constructor creates null handle
TEST_F(HandleBaseTest, NullptrConstructorCreatesNull) {
    TestHandle h(nullptr);
    EXPECT_FALSE(static_cast<bool>(h));
    EXPECT_EQ(h.get(), nullptr);
}

// Test: Raw pointer constructor takes ownership
TEST_F(HandleBaseTest, RawPointerConstructor) {
    auto* raw = new TestImpl(42);
    TestHandle h(raw);
    EXPECT_TRUE(static_cast<bool>(h));
    EXPECT_EQ(h->value(), 42);
}

// Test: shared_ptr constructor takes ownership
TEST_F(HandleBaseTest, SharedPtrConstructor) {
    auto sp = std::make_shared<TestImpl>(42);
    TestHandle h(sp);
    EXPECT_TRUE(static_cast<bool>(h));
    EXPECT_EQ(h->value(), 42);
}

// Test: operator-> provides access
TEST_F(HandleBaseTest, ArrowOperator) {
    auto sp = std::make_shared<TestImpl>(42);
    TestHandle h(sp);
    EXPECT_EQ(h->value(), 42);
    h->set_value(100);
    EXPECT_EQ(h->value(), 100);
}

// Test: operator* provides reference access
TEST_F(HandleBaseTest, DereferenceOperator) {
    auto sp = std::make_shared<TestImpl>(42);
    TestHandle h(sp);
    TestImpl& ref = *h;
    EXPECT_EQ(ref.value(), 42);
}

// Test: get() returns raw pointer
TEST_F(HandleBaseTest, GetReturnsRawPointer) {
    auto sp = std::make_shared<TestImpl>(42);
    TestHandle h(sp);
    EXPECT_EQ(h.get(), sp.get());
}

// Test: shared() returns shared_ptr
TEST_F(HandleBaseTest, SharedReturnsSharedPtr) {
    auto sp = std::make_shared<TestImpl>(42);
    TestHandle h(sp);
    EXPECT_EQ(h.shared(), sp);
    // use_count is 3: sp, h.body_, and the temporary returned by shared()
    EXPECT_EQ(h.shared().use_count(), 3);
}

// Test: operator bool for null check
TEST_F(HandleBaseTest, BoolOperator) {
    TestHandle null_h;
    EXPECT_FALSE(static_cast<bool>(null_h));

    auto sp = std::make_shared<TestImpl>(42);
    TestHandle valid_h(sp);
    EXPECT_TRUE(static_cast<bool>(valid_h));
}

// Test: Equality comparison
TEST_F(HandleBaseTest, EqualityComparison) {
    auto sp1 = std::make_shared<TestImpl>(42);
    auto sp2 = std::make_shared<TestImpl>(42);
    TestHandle h1(sp1);
    TestHandle h2(sp1);  // Same underlying object
    TestHandle h3(sp2);  // Different underlying object

    EXPECT_EQ(h1, h2);
    EXPECT_NE(h1, h3);
}

// Test: Copy shares ownership
TEST_F(HandleBaseTest, CopySharesOwnership) {
    auto sp = std::make_shared<TestImpl>(42);
    TestHandle h1(sp);
    TestHandle h2 = h1;

    EXPECT_EQ(h1, h2);
    EXPECT_EQ(sp.use_count(), 3);  // sp, h1, h2 all own it

    h1->set_value(100);
    EXPECT_EQ(h2->value(), 100);  // Same underlying object
}

// Test: Dynamic pointer cast success
TEST_F(HandleBaseTest, DynamicPointerCastSuccess) {
    auto sp = std::make_shared<DerivedTestImpl>(21);
    TestHandle h(std::static_pointer_cast<TestImpl>(sp));

    auto derived = h.dynamic_pointer_cast<DerivedTestImpl>();
    EXPECT_NE(derived, nullptr);
    EXPECT_EQ(derived->doubled(), 42);
}

// Test: Dynamic pointer cast failure throws
TEST_F(HandleBaseTest, DynamicPointerCastFailureThrows) {
    auto sp = std::make_shared<TestImpl>(42);  // Base type, not derived
    TestHandle h(sp);

    EXPECT_THROW((void)h.dynamic_pointer_cast<DerivedTestImpl>(), Nh::RuntimeException);
}

// Test: Constructor from derived shared_ptr
TEST_F(HandleBaseTest, ConstructFromDerivedSharedPtr) {
    auto derived_sp = std::make_shared<DerivedTestImpl>(21);
    TestHandle h(derived_sp);  // Should accept derived type

    EXPECT_TRUE(static_cast<bool>(h));
    EXPECT_EQ(h->value(), 21);
}

// Test: element_type typedef exists
TEST_F(HandleBaseTest, ElementTypeExists) {
    static_assert(std::is_same_v<TestHandle::element_type, TestImpl>,
                  "element_type should be TestImpl");
    EXPECT_TRUE(true);
}

