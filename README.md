[![Build Status](https://travis-ci.org/moose-x86/owned_by_unique.svg?branch=master)](https://travis-ci.org/moose-x86/owned_by_unique) [![Build status](https://ci.appveyor.com/api/projects/status/drdof9ajfooqk0um?svg=true)](https://ci.appveyor.com/project/moose-x86/owned-by-unique) [![codecov](https://codecov.io/gh/moose-x86/owned_by_unique/branch/master/graph/badge.svg)](https://codecov.io/gh/moose-x86/owned_by_unique)

# Class owned_pointer
This is test utility for dependency injection with ```std::unique_ptr``` smart pointer, written in C++ 11.
Class ```pobu::owned_pointer``` acts as facade to ```std::unique_ptr``` and has semantics and syntax of smart pointer.
Class ```pobu::owned_pointer``` is not intended to be used in production code. It is best to use this class with google mock, to inject mock to cut(class under test)  by ```std::unique_ptr``` and have reference to this mock in test suite.

## Semantics and syntax of ```pobu::owned_pointer```
Example of test classes:

```c++
struct T
{
  T(int px = 0, int py = 0, int pz = 0) : x(px), y(py), z(pz){}
  int x, y, z;
};

struct X
{
  X(std::unique_ptr<T> m) : u(std::move(m)){}
  std::unique_ptr<T> u;
};
```

Using ```pobu::owned_pointer```:

```c++
using namespace pobu;

owned_pointer<T> p = make_owned<T>(1, 2, 3);
X x{p.unique_ptr()}; //after this point owned_pointer is not owner of memory

x.u->x = 10;

assert(x.u->x == p->x);
```
Class ```pobu::owned_pointer``` is not owner of memory after ```unique_ptr()``` invokation.

```c++
using namespace pobu;

std::unique_ptr<T> u;
{
  auto p = make_owned<T>();
  u = p.unique_ptr();
}
auto naked_ptr = u.get(); //u is still valid after destruction of p
```
If class which is pointed by ```pobu::owned_pointer``` has virtual dtor and was created by ```pobu::make_owned```, ```pobu::owned_pointer``` is able to detect that ```std::unique_ptr``` deleted resource. If class has no virtual dtor invoking ```get()```, ```operator->()``` or ```operator*()``` after ```std::unique_ptr``` deleted resource is undefined.

```c++
struct D
{
  int x = 0;
  virtual ~D() = default;
};

using namespace pobu;

try
{
  auto p = make_owned<D>();
  { auto u = p.unique_ptr(); } //nested scope, u deleted
  p->x = 10; // this throws, for T type this would be undefined
}
catch(const ptr_was_alredy_deleted&)
{}

```
Smart pointer ```pobu::owned_pointer``` behaves like ```std::shared_ptr``` if member function ```unique_ptr()``` was not invoked. This means that it will destroy allocated memory, if ```std::unique_ptr``` was not acquired.

You can invoke ```unique_ptr()``` only once if ```pobu::owned_pointer``` was in charge of valid memory or infinite number of times if ```pobu::owned_pointer``` was pointing to nullptr.

```c++
using namespace pobu;

try
{
  auto p = make_owned<T>();
  auto u = p.unique_ptr();
  if(!p.expired())
  {
    auto v = p.unique_ptr(); //this throws
  }
}
catch(const unique_ptr_already_acquired&)
{}
```
You can also create ```pobu::owned_pointer``` from valid ```std::unique_ptr```. Please note that detecting of ```std::unique_ptr``` deletion will not work.

```c++
using namespace pobu;

auto u = std::make_unique<T>();
owned_pointer<T> p{link(u)};

assert(p.get() == u.get());
auto v = p.unique_ptr(); //this will throw
```
Also you can move ```std::unique_ptr``` to ```pobu::owned_pointer```. Please note that detecting of ```std::unique_ptr``` deletion will not work.

```c++
using namespace pobu;

auto u = std::make_unique<T>();
owned_pointer<T> p{std::move(u)};

assert(u.get() == nullptr);
assert(p.get() != nullptr);
u = p.unique_ptr(); // this will not throw
```
Smart pointer ```pobu::owned_pointer``` can be copied after acquirng ```std::unique_ptr```.

```c++
using namespace pobu;

auto p = make_owned<D>();
auto u = p.unique_ptr();
auto r = p;
auto v = r.unique_ptr(); //this throws

assert(p.get() == r.get());
u.reset();

auto x = p.get(); //this throws
auto y = r.get(); //this throws
```

This code was tested with g++ and clang++ compilers.

## Example with google mock

```c++
struct predicate
{
  virtual bool is_true() const = 0;
  virtual ~predicate() = default;
};

struct predicate_mock : public predicate
{
  MOCK_CONST_METHOD0(is_true, bool());
};

struct cut
{
  cut(std::unique_ptr<predicate> m) : mock(std::move(m)) {}
  bool do_something(){ return mock->is_true(); }
private:
  std::unique_ptr<predicate> mock;
};

TEST(test_cut, test)
{
  using namespace pobu;
  using namespace testing;

  owned_pointer<predicate_mock> p = make_owned<StrictMock<predicate_mock>>();
  cut s{p.unique_ptr()};

  EXPECT_CALL(*p, is_true()).WillOnce(Return(true));
  ASSERT_TRUE(cut.do_something());
}
```

## Example of usage owned_pointer with google mock, mocking factory

Because of ```pobu::owned_pointer``` is copyable, it can be used with ```std::unique_ptr``` when mocking Factories methods, which return ```std::unique_ptr``` and also member functions which take ```std::unique_ptr``` as paramter. In order to do that, in ```gmock_macro_for_unique_ptr.hpp``` header there are special macros which create dummy member functions inside mock class. They are used like this:

```c++
struct item { virtual ~item() = default; };

struct factory
{
  virtual std::unique_ptr<item> create() const = 0;
  virtual ~factory() = default;
};

struct factory_mock : public factory
{
  MOCK_UNIQUE_CONST_METHOD0(create, std::unique_ptr<item>());
};

TEST(test_cut, test)
{
  using namespace testing;

  factory_mock f;
  auto p = pobu::make_owned<item>();
  EXPECT_CALL(f, _create()).WillOnce(Return(p));

  auto u = static_cast<factory&>(f).create();
  ASSERT_EQ(u.get(), p.get());
}
```
All macros from gmock have thier equivalents in ```gmock_macro_for_unique_ptr.hpp``` to enable mocking member functions with ```std::unique_ptr```. Those macros also support ```override``` of mocked functions, so mocking of member functions should be less error prone.

```c++

struct app { virtual ~app() = default; };

struct system
{
  virtual void install(std::unique_ptr<app>) const = 0;
  virtual ~factory() = default;
};

struct system_mock : public system
{
  MOCK_UNIQUE_CONST_METHOD1(install, void(std::unique_ptr<app>));
};

TEST(test_cut, test)
{
  using namespace testing;

  system_mock f;
  auto p = pobu::make_owned<app>();

  EXPECT_CALL(f, _install(Eq(p)));
  auto u = static_cast<system&>(f).install(p.unique_ptr());

  ASSERT_NO_THROW(*p);
}
```
When ```pobu::owned_pointer``` is created and ```unique_ptr()``` is not invoked, it can indicate a problem in test. This is why it would be good to invoke assert to indicate to the developer, that ```owned_pointer``` is owner of memory and its name don't indicate real ownership(owned_pointer). This assert is disabled by default, but it can be enabled by compiling with define ```OWNED_POINTER_ASSERT_DTOR```.

```c++
#ifdef OWNED_POINTER_ASSERT_DTOR
   assert(is_acquired() and
           "owned_pointer: you created owned_pointer, but unique_ptr was never acquired");
#endif
```
