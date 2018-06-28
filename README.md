[![Build Status](https://travis-ci.org/moose-x86/owned_by_unique.svg?branch=master)](https://travis-ci.org/moose-x86/owned_by_unique) [![Build status](https://ci.appveyor.com/api/projects/status/drdof9ajfooqk0um?svg=true)](https://ci.appveyor.com/project/moose-x86/owned-by-unique) [![codecov](https://codecov.io/gh/moose-x86/owned_by_unique/branch/master/graph/badge.svg)](https://codecov.io/gh/moose-x86/owned_by_unique) [![godbolt][badge.godbolt]][godbolt]

[badge.godbolt]: https://img.shields.io/badge/try%20it-on%20godbolt-222266.svg
[godbolt]: https://godbolt.org/g/8oS3nV


# owned_pointer
This is test utility for dependency injection with ```std::unique_ptr``` smart pointer, written in C++ 11.
Class ```csp::owned_pointer``` acts as facade to ```std::unique_ptr``` and has semantics and syntax of smart pointer.
Class ```csp::owned_pointer``` is not intended to be used in production code. It is best to use this class with google mock, to inject mock to cut(class under test)  by ```std::unique_ptr``` and have reference to this mock in test suite.

## Semantics and syntax of ```csp::owned_pointer```
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

Using ```csp::owned_pointer```:

```c++
csp::owned_pointer<T> p = csp::make_owned<T>(1, 2, 3);
X x{p.unique_ptr()}; //after this point owned_pointer is not owner of memory

x.u->x = 10;

assert(x.u->x == p->x);
```
Class ```csp::owned_pointer``` is not owner of memory after ```unique_ptr()``` invokation.

```c++

std::unique_ptr<T> u;
{
  auto p = csp::make_owned<T>();
  u = p.unique_ptr();
}
auto naked_ptr = u.get(); //u is still valid after destruction of p
```
If class which is pointed by ```csp::owned_pointer``` has virtual dtor and was created by ```csp::make_owned```, ```csp::owned_pointer``` is able to detect that ```std::unique_ptr``` deleted resource. If class has no virtual dtor invoking ```get()```, ```operator->()``` or ```operator*()``` after ```std::unique_ptr``` deleted resource is undefined.

```c++
struct D
{
  int x = 0;
  virtual ~D() = default;
};

try
{
  auto p = csp::make_owned<D>();
  { auto u = p.unique_ptr(); } //nested scope, u deleted
  p->x = 10; // this throws, for T type this would be undefined
}
catch(const csp::ptr_was_alredy_deleted&)
{}

```
Smart pointer ```csp::owned_pointer``` behaves like ```std::shared_ptr``` if member function ```unique_ptr()``` was not invoked. This means that it will destroy allocated memory, if ```std::unique_ptr``` was not acquired.

You can invoke ```unique_ptr()``` only once if ```csp::owned_pointer``` was in charge of valid memory or infinite number of times if ```csp::owned_pointer``` was pointing to nullptr.

```c++

try
{
  auto p = csp::make_owned<T>();
  auto u = p.unique_ptr();

  if(not p.expired()) //check if u deleted pointer
    auto v = p.unique_ptr(); //this throws
}
catch(const csp::unique_ptr_already_acquired&)
{}
```
You can also create ```csp::owned_pointer``` from valid ```std::unique_ptr```. Please note that detecting of ```std::unique_ptr``` deletion will not work.

```c++

auto u = std::make_unique<T>(1, 2, 3);
csp::owned_pointer<T> p{csp::link(u)};

assert(p.get() == u.get());
auto v = p.unique_ptr(); //this will throw
```
Also you can move ```std::unique_ptr``` to ```csp::owned_pointer```. Please note that detecting of ```std::unique_ptr``` deletion will not work.

```c++

auto u = std::make_unique<T>();
csp::owned_pointer<T> p{std::move(u)};

assert(u.get() == nullptr);
assert(p.get() != nullptr);
u = p.unique_ptr(); // this will not throw
```
Smart pointer ```csp::owned_pointer``` can be copied after acquirng ```std::unique_ptr```.

```c++

auto p = csp::make_owned<D>();
auto u = p.unique_ptr();
auto r = p;

try
{
  auto v = r.unique_ptr(); //this throws
}catch(...){}

assert(p == r);
u.reset();
assert(p == r); //this will not throw - compare operators are noexcept

try
{
  auto x = p.get(); //this throws
}catch(...){}

try
{
  auto y = r.get(); //this throws
}catch(...){}
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
  using namespace testing;

  csp::owned_pointer<predicate_mock> p = csp::make_owned<StrictMock<predicate_mock>>();
  cut s{p.unique_ptr()};

  EXPECT_CALL(*p, is_true()).WillOnce(Return(true));
  ASSERT_TRUE(cut.do_something());
}
```

## Example of usage owned_pointer with google mock, mocking factory

Because of ```csp::owned_pointer``` is copyable, it can be used with ```std::unique_ptr``` when mocking Factories methods, which return ```std::unique_ptr``` and also member functions which take ```std::unique_ptr``` as paramter. In order to do that, in ```gmock_macro_for_unique_ptr.hpp``` header there are special macros which create dummy member functions inside mock class. They are used like this:

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
  auto p = csp::make_owned<item>();
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
  auto p = csp::make_owned<app>();

  EXPECT_CALL(f, _install(Eq(p)));
  auto u = static_cast<system&>(f).install(p.unique_ptr());

  ASSERT_NO_THROW(*p);
}
```
When ```csp::owned_pointer``` is created and ```unique_ptr()``` is not invoked, it can indicate a problem in test. This is why it would be good to invoke assert to indicate to the developer, that ```owned_pointer``` is owner of memory and its name don't indicate real ownership(owned_pointer). This assert is disabled by default, but it can be enabled by compiling with define ```OWNED_POINTER_ASSERT_DTOR```.

```c++
#ifdef OWNED_POINTER_ASSERT_DTOR
assert(acquired() and
       "owned_pointer: you created owned_pointer, but unique_ptr was never acquired");
#endif
```
