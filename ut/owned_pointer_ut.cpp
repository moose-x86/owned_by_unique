/**
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 Przemyslaw Wos
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
**/
#include <gmock/gmock-generated-matchers.h>
#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "owned_pointer.hpp"
#include "gmock_macros_for_unique_ptr.hpp"

using namespace ::testing;

class owned_pointer_ut : public ::testing::Test
{
protected:
  struct simple_base_class
  {
    virtual ~simple_base_class() = default;
  };

  struct destruction_test_mock : public simple_base_class
  {
    int x;
    destruction_test_mock(int y = 1) : x(y) {}

    MOCK_METHOD0(die, void());
    virtual ~destruction_test_mock(){ x = 0; die(); }
  };

  struct mock_interface
  {
    virtual int test(const std::unique_ptr<simple_base_class>) const = 0;
    virtual ~mock_interface() = default;
  };

  struct mock_class : public mock_interface
  {
    MOCK_UNIQUE_CONST_METHOD1(test, int(const std::unique_ptr<simple_base_class>));
  };

  typedef StrictMock<destruction_test_mock> test_mock;

  template<typename T> void assert_that_operators_throw(csp::owned_pointer<T> &p)
  {
    ASSERT_TRUE(p.expired());
    ASSERT_NO_THROW(p.get(std::nothrow));
    ASSERT_TRUE(p.get(std::nothrow) == nullptr);

    ASSERT_THROW(*p, csp::ptr_is_already_deleted);
    ASSERT_THROW(p.get(), csp::ptr_is_already_deleted);
    ASSERT_THROW(p.operator->(), csp::ptr_is_already_deleted);
  }

  template<typename T> void assert_that_operators_dont_throw(csp::owned_pointer<T> &p)
  {
    ASSERT_FALSE(p.expired());

    ASSERT_NO_THROW(*p);
    ASSERT_NO_THROW(p.get());
    ASSERT_NO_THROW(p.operator->());
    ASSERT_NO_THROW(p.get(std::nothrow));
  }

  template<typename T> void assert_that_get_unique_throws(csp::owned_pointer<T> &p)
  {
    ASSERT_THROW(p.unique_ptr(), csp::unique_ptr_already_acquired);
  }

  template<typename T>
  std::unique_ptr<T> expect_that_get_unique_dont_throw(csp::owned_pointer<T> &p)
  {
    std::unique_ptr<T> u;
    EXPECT_NO_THROW(u = p.unique_ptr());

    return u;
  }

  template<typename T> bool equal(csp::owned_pointer<T>& p1, csp::owned_pointer<T>& p2)
  {
    return (p1 == p2) and (p1.acquired() == p2.acquired()) and (p1.expired() == p2.expired());
  }

  void expect_object_will_be_deleted(csp::owned_pointer<destruction_test_mock> p)
  {
    EXPECT_CALL(*p, die()).Times(1);
  }

  void create_nine_copies_of(const csp::owned_pointer<destruction_test_mock>& p)
  {
    static std::vector<csp::owned_pointer<destruction_test_mock>> copiesOfPointers;
    copiesOfPointers = {p, p, p, p, p, p, p, p, p};
  }

  template<typename T>
  void release_unique_ptr_and_delete_object(std::unique_ptr<T>& u)
  {
    delete u.release();
  }

  void test_link_semantics(csp::owned_pointer<simple_base_class> p)
  {
    ASSERT_TRUE(p.acquired());
  }

  void test_move_semantics(csp::owned_pointer<test_mock> p)
  {
    ASSERT_FALSE(p.acquired());
  }

  struct RefString : std::stringstream
  {
    RefString(const std::string& a) : s(a) {}
    const std::string& s;
  };

  struct OStreamFactory
  {
    virtual std::unique_ptr<std::ostream> create(const std::string&) = 0;
    virtual ~OStreamFactory(){}
  };

  struct OStreamFactoryMock : OStreamFactory
  {
    MOCK_UNIQUE_METHOD1(create, std::unique_ptr<std::ostream>(const std::string&));
  };

  struct Taker
  {
    virtual void giveme(std::ostream&) = 0;
  };

  struct TakerMock : Taker
  {
    MOCK_METHOD1(giveme, void(std::ostream&));
  };
};

TEST_F(owned_pointer_ut, isUniqueAndPtrOwnedPointingSameAddress)
{
  auto p = csp::make_owned<int>();
  auto u = expect_that_get_unique_dont_throw(p);

  ASSERT_EQ(u.get(), p.get());
  ASSERT_FALSE(is_expired_enabled_f(p));
}

TEST_F(owned_pointer_ut, isExpiredEnabledTest)
{
  ASSERT_FALSE(csp::is_expired_enabled<csp::owned_pointer<int>>::value);
  ASSERT_FALSE(csp::is_expired_enabled<csp::owned_pointer<int>&>::value);
  ASSERT_FALSE(csp::is_expired_enabled<csp::owned_pointer<int>&&>::value);
  ASSERT_FALSE(csp::is_expired_enabled<const csp::owned_pointer<int>>::value);
  ASSERT_FALSE(csp::is_expired_enabled<const csp::owned_pointer<int>&>::value);
  ASSERT_FALSE(csp::is_expired_enabled<const csp::owned_pointer<int>&&>::value);

  ASSERT_TRUE(csp::is_expired_enabled<csp::owned_pointer<test_mock>>::value);
  ASSERT_TRUE(csp::is_expired_enabled<csp::owned_pointer<test_mock>&>::value);
  ASSERT_TRUE(csp::is_expired_enabled<csp::owned_pointer<test_mock>&&>::value);
  ASSERT_TRUE(csp::is_expired_enabled<const csp::owned_pointer<test_mock>>::value);
  ASSERT_TRUE(csp::is_expired_enabled<const csp::owned_pointer<test_mock>&>::value);
  ASSERT_TRUE(csp::is_expired_enabled<const csp::owned_pointer<test_mock>&&>::value);

  ASSERT_TRUE(csp::is_expired_enabled<csp::owned_pointer<const test_mock>>::value);
  ASSERT_TRUE(csp::is_expired_enabled<csp::owned_pointer<const test_mock>&>::value);
  ASSERT_TRUE(csp::is_expired_enabled<csp::owned_pointer<const test_mock>&&>::value);
  ASSERT_TRUE(csp::is_expired_enabled<const csp::owned_pointer<const test_mock>>::value);
  ASSERT_TRUE(csp::is_expired_enabled<const csp::owned_pointer<const test_mock>&>::value);
  ASSERT_TRUE(csp::is_expired_enabled<const csp::owned_pointer<const test_mock>&&>::value);
}

TEST_F(owned_pointer_ut, getWithNothrowPolicy)
{
  auto p = csp::make_owned<test_mock>();
  auto u = expect_that_get_unique_dont_throw(p);
  expect_object_will_be_deleted(p);

  ASSERT_TRUE(p.get(std::nothrow) != nullptr);
  u.reset();

  ASSERT_NO_THROW(p.get(std::nothrow));
  ASSERT_TRUE(p.get(std::nothrow) == nullptr);

  ASSERT_TRUE(is_expired_enabled_f(p));
}

TEST_F(owned_pointer_ut, throwIsDeletedWhenUniquePtr)
{
  const auto p = csp::make_owned<test_mock>();
  expect_object_will_be_deleted(p);

  {
    auto u = p.unique_ptr();
  }

  ASSERT_THROW(p.unique_ptr(), csp::ptr_is_already_deleted);
}

TEST_F(owned_pointer_ut, testCreatingPtrOwnedByDefaultCtor)
{
  csp::owned_pointer<int> p;
  auto u = expect_that_get_unique_dont_throw(p);

  ASSERT_THAT(p, IsNull());
  ASSERT_EQ(u.get(), p.get());
  ASSERT_TRUE(not u);
  ASSERT_TRUE(not p);
  ASSERT_FALSE(p.expired());
  ASSERT_FALSE(p.acquired());
}

TEST_F(owned_pointer_ut, testCreatingPtrOwnedByUniqueFromNullptr)
{
  csp::owned_pointer<int> p = nullptr;
  auto u = expect_that_get_unique_dont_throw(p);

  ASSERT_THAT(p, IsNull());
  ASSERT_EQ(u.get(), p.get());
  ASSERT_TRUE(not u);
  ASSERT_TRUE(not p);
  ASSERT_FALSE(p.expired());
  ASSERT_FALSE(p.acquired());
}

TEST_F(owned_pointer_ut, copyConstructorTest)
{
  auto p1 = csp::make_owned<int>();
  auto p2 = p1;

  ASSERT_TRUE(equal(p1, p2));

  auto u = expect_that_get_unique_dont_throw(p2);

  ASSERT_TRUE(equal(p1, p2));

  assert_that_get_unique_throws(p1);
  assert_that_get_unique_throws(p2);
}

TEST_F(owned_pointer_ut, testMoveAndLinkSemantics)
{
  auto p = csp::make_owned<test_mock>();
  expect_object_will_be_deleted(p);

  auto u = p.unique_ptr();

  test_link_semantics(csp::link<simple_base_class>(u));
  test_link_semantics(csp::link(u));

  csp::owned_pointer<destruction_test_mock> r = csp::link(u);
  ASSERT_TRUE(r.acquired());

  assert_that_operators_dont_throw(p);
  assert_that_operators_dont_throw(r);

  test_move_semantics(std::move(u));

  assert_that_operators_dont_throw(p);
  assert_that_operators_dont_throw(r);
}

TEST_F(owned_pointer_ut, deleteAfterCopyDontInvalidateCopy)
{
  csp::owned_pointer<destruction_test_mock> copy;
  {
    auto p = csp::make_owned<test_mock>();
    copy = p;
  }

  Mock::VerifyAndClearExpectations(copy.get());

  assert_that_operators_dont_throw(copy);
  expect_object_will_be_deleted(copy);
}

TEST_F(owned_pointer_ut, isAcquireByUniquePtr)
{
  auto p = csp::make_owned<int>();
  auto u = p.unique_ptr();

  ASSERT_TRUE(p.acquired());

  assert_that_get_unique_throws(p);
  assert_that_operators_dont_throw(p);
}

TEST_F(owned_pointer_ut, objectWillBeDeleted)
{
  auto p = csp::make_owned<test_mock>(199);
  expect_object_will_be_deleted(p);
}

TEST_F(owned_pointer_ut, objectWillBeDeletedOnceWhenUniqueIsAcquired)
{
  auto p = csp::make_owned<test_mock>();
  auto u = p.unique_ptr();

  expect_object_will_be_deleted(p);
}

TEST_F(owned_pointer_ut, objectWillBeDeletedOnceWhenUniqueIsAcquiredAndReleased)
{
  auto p = csp::make_owned<test_mock>();
  auto u = p.unique_ptr();

  expect_object_will_be_deleted(p);
  release_unique_ptr_and_delete_object(u);

  assert_that_operators_throw(p);
}

TEST_F(owned_pointer_ut, objectWillBeDeletedWhenMultipleSharedObjects)
{
  auto p = csp::make_owned<test_mock>();

  create_nine_copies_of(p);
  expect_object_will_be_deleted(p);

  ASSERT_FALSE(p.acquired());
  EXPECT_EQ(p.use_count(), 10u);
}

TEST_F(owned_pointer_ut, forNullPointerInvokeUniquePtrHowManyYouWant)
{
  csp::owned_pointer<destruction_test_mock> p;

  for(int i = 0; i < 100; i++)
  {
    ASSERT_FALSE(p.unique_ptr());
    expect_that_get_unique_dont_throw(p);
    assert_that_operators_dont_throw(p);
  }
}

TEST_F(owned_pointer_ut, runtimeErrorIsThrownWhenResourceDeleted)
{
  auto p = csp::make_owned<test_mock>();
  auto r = p;

  create_nine_copies_of(p);

  expect_object_will_be_deleted(p);
  {
    auto u = expect_that_get_unique_dont_throw(p);
  }

  assert_that_operators_throw(p);

  auto w = p;

  assert_that_operators_throw(w);
  assert_that_operators_throw(r);
}

TEST_F(owned_pointer_ut, noRuntimeErrorWhenResourceIsAquiredInUnique)
{
  auto p = csp::make_owned<test_mock>(12324);
  auto u = p.unique_ptr();

  expect_object_will_be_deleted(p);

  for(int i = 0; i < 100; i++)
    assert_that_operators_dont_throw(p);
}

TEST_F(owned_pointer_ut, boolOperator)
{
  csp::owned_pointer<int> r;
  auto p = csp::make_owned<int>(12);

  ASSERT_FALSE(!p);
  ASSERT_FALSE(r);
}

TEST_F(owned_pointer_ut, isUniquePtrValidAfterOwnedPtrDeletion)
{
  std::unique_ptr<test_mock> u;
  {
    auto p = csp::make_owned<test_mock>();

    p->x = 0x123;
    u = expect_that_get_unique_dont_throw(p);
  }

  Mock::VerifyAndClearExpectations(u.get());

  EXPECT_CALL(*u, die()).Times(1);
  ASSERT_EQ(u->x, 0x123);
}

TEST_F(owned_pointer_ut, uniquePtrConstructor)
{
  std::unique_ptr<destruction_test_mock> u(new test_mock());
  csp::owned_pointer<destruction_test_mock> p(std::move(u));

  ASSERT_FALSE(u.get());
  expect_object_will_be_deleted(p);
}

TEST_F(owned_pointer_ut, explicitOperatorTest)
{
  auto p = csp::make_owned<int>();
  const std::unique_ptr<int> u(p);

  ASSERT_TRUE(u.get());
  assert_that_get_unique_throws(p);
}

TEST_F(owned_pointer_ut, testConversionInGoogleMockParams)
{
  mock_class m;
  mock_interface& base = m;
  auto p = csp::make_owned<test_mock>();

  expect_object_will_be_deleted(p);
  EXPECT_CALL(m, _test(Eq(p))).WillOnce(Return(0));

  base.test(p.unique_ptr());
  assert_that_operators_dont_throw(p);
}

TEST_F(owned_pointer_ut, testIsNullAndNotNullMatchers)
{
  mock_class m;
  mock_interface& base = m;
  auto p = csp::make_owned<test_mock>(0x123);

  EXPECT_CALL(m, _test(NotNull())).WillOnce(Return(0));
  expect_object_will_be_deleted(p);

  base.test(p.unique_ptr());
  assert_that_operators_dont_throw(p);

  EXPECT_EQ(p->x, 0x123);

  EXPECT_CALL(m, _test(IsNull())).WillOnce(Return(0));
  base.test(nullptr);
}

TEST_F(owned_pointer_ut, assertThatCompareOperatorsDontThrow)
{
  auto p = csp::make_owned<test_mock>();
  auto r = csp::make_owned<test_mock>();
  const void* p_ptr = p.get();
  const void* r_ptr = r.get();
  {
    expect_object_will_be_deleted(p);
    std::unique_ptr<test_mock> u{p};

    ASSERT_TRUE(p == u);
    ASSERT_TRUE(u == p);
    ASSERT_FALSE(p != u);
    ASSERT_FALSE(u != p);
  }

  assert_that_operators_throw(p);

  ASSERT_EQ(p, p);
  ASSERT_NE(p, r);
  ASSERT_TRUE(p != nullptr);
  ASSERT_TRUE(nullptr != p);
  ASSERT_FALSE(p == nullptr);
  ASSERT_FALSE(nullptr == p);
  ASSERT_TRUE(p == p_ptr);
  ASSERT_TRUE(p_ptr == p);
  ASSERT_TRUE(p != r_ptr);
  ASSERT_TRUE(r_ptr != p);

  if(p_ptr < r_ptr)
    ASSERT_TRUE(p < r);
  else
    ASSERT_FALSE(p < r);

  if(p_ptr <= r_ptr)
    ASSERT_TRUE(p <= r);
  else
    ASSERT_FALSE(p <= r);

  if(p_ptr > r_ptr)
    ASSERT_TRUE(p > r);
  else
    ASSERT_FALSE(p > r);

  if(p_ptr >= r_ptr)
    ASSERT_TRUE(p >= r);
  else
    ASSERT_FALSE(p >= r);

  expect_object_will_be_deleted(r);
}

TEST_F(owned_pointer_ut, assertThatSharedStateWillBeUpdateAfterPtrOwnedDeletion)
{
  std::unique_ptr<test_mock> u;
  {
    auto p = csp::make_owned<test_mock>();
    u = p.unique_ptr();
  }

  csp::owned_pointer<test_mock> p{std::move(u)};
  expect_object_will_be_deleted(p);

  p.unique_ptr();
  assert_that_operators_throw(p);
}

TEST_F(owned_pointer_ut, assertThatMoveSemanticsIsWorking)
{
  auto p = csp::make_owned<test_mock>();
  csp::owned_pointer<test_mock> r{std::move(p)};
  expect_object_will_be_deleted(r);

  ASSERT_THAT(p, IsNull());
  ASSERT_THAT(r, NotNull());
  ASSERT_EQ(r.acquired(), false);
  ASSERT_EQ(r.expired(), false);

  r.unique_ptr();

  ASSERT_EQ(r.acquired(), true);
  ASSERT_EQ(r.expired(), true);

  ASSERT_THAT(p, IsNull());
  ASSERT_EQ(p.acquired(), false);
  ASSERT_EQ(p.expired(), false);
}

TEST_F(owned_pointer_ut, testCastingAddressMovement)
{
  OStreamFactoryMock ostream_factory;
  TakerMock taker;
  std::string ss;
  csp::owned_pointer<std::ostream> p = csp::make_owned<RefString>(ss);

  EXPECT_CALL(ostream_factory, _create(_)).WillOnce(Return(p));
  EXPECT_CALL(taker, giveme(Ref(*p)));
  {
    OStreamFactory& osf = ostream_factory;
    Taker& take = taker;

    auto u = osf.create("test-test");
    take.giveme(*u);
  }
}

TEST_F(owned_pointer_ut, testBeginEndAndSizePropagation)
{
  std::vector<int> v{};
  const auto o = csp::make_owned<std::vector<int>>(1, 2, 3, 4, 5);

  std::copy(o.begin(), o.end(), std::back_inserter(v));

  ASSERT_THAT(o.size(), ::testing::Eq(5));
  ASSERT_THAT(v, ::testing::ElementsAre(1,2,3,4,5));
}

TEST_F(owned_pointer_ut, testCBeginEndAndSizePropagation)
{
  std::vector<int> v{};
  const auto o = csp::make_owned<std::vector<int>>(1, 2, 3, 4, 5);

  std::copy(o.cbegin(), o.cend(), std::back_inserter(v));

  ASSERT_THAT(o.size(), ::testing::Eq(5));
  ASSERT_THAT(v, ::testing::ElementsAre(1,2,3,4,5));
}
