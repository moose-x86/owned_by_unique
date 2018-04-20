# Class ptr_owned_by_unique
This is test utility for dependency injection with ```std::unique_ptr``` smart pointer, written in C++ 11.
Class ```pobu::ptr_owned_by_unique``` acts as facade to ```std::unique_ptr``` and has semantics and syntax of smart pointer.
Class ```pobu::ptr_owned_by_unique``` is not intended to be used in production code. It is best to use this class with google mock, to inject mock to cut(class under test)  by ```std::unique_ptr``` and have reference to this mock in test suite.

## Semantics and syntax of ```pobu::ptr_owned_by_unique```
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

Using ```pobu::ptr_owned_by_unique```:

```c++
using namespace pobu;

ptr_owned_by_unique<T> p = make_owned_by_unique<T>(1, 2, 3);
X x{p.unique_ptr()}; //after this point owned_by_unique is not owner of memory

x.u->x = 10;

assert(x.u->x == p->x);
```
Class ```pobu::ptr_owned_by_unique``` is not owner of memory after ```unique_ptr()``` invokation.

```c++
using namespace pobu;

std::unique_ptr<T> u;
{
  auto p = make_owned_by_unique<T>();
  u = p.unique_ptr();
}
auto naked_ptr = u.get(); //u is still valid after destruction of p
```
If class which is pointed by ```pobu::ptr_owned_by_unique``` has virtual dtor and was created by ```pobu::make_owned_by_unique```, ```pobu::ptr_owned_by_unique``` is able to detect that ```std::unique_ptr``` deleted resource. If class has no virtual dtor invoking ```get()```, ```operator->()``` or ```operator*()``` after ```std::unique_ptr``` deleted resource is undefined.

```c++
struct D
{
  int x = 0;
  virtual ~D() = default;
};

using namespace pobu;

try
{
  auto p = make_owned_by_unique<D>();
  { auto u = p.unique_ptr(); } //nested scope, u deleted
  p->x = 10; // this throws, for T type this would be undefined
}
catch(const ptr_was_alredy_deleted&)
{}

```
Smart pointer ```pobu::ptr_owned_by_unique``` behaves like ```std::shared_ptr``` if member function ```unique_ptr()``` was not invoked. This means that it will destroy allocated memory, if ```std::unique_ptr``` was not acquired.

You can invoke ```unique_ptr()``` only once if ```pobu::ptr_owned_by_unique``` was in charge of valid memory or infinite number of times if ```pobu::ptr_owned_by_unique``` was pointing to nullptr.

```c++
using namespace pobu;

try
{
  auto p = make_owned_by_unique<T>();
  auto u = p.unique_ptr();
  auto v = p.unique_ptr(); //this throws
}
catch(const unique_ptr_already_acquired&)
{}
```
You can also create ```pobu::ptr_owned_by_unique``` from valid ```std::unique_ptr```. Please note that detecting of ```std::unique_ptr``` deletion will not work.

```c++
using namespace pobu;

auto u = std::make_unique<T>();
ptr_owned_by_unique<T> p{link(u)};

assert(p.get() == u.get());
auto v = p.unique_ptr(); //this will throw
```
Also you can move ```std::unique_ptr``` to ```pobu::ptr_owned_by_unique```. Please note that detecting of ```std::unique_ptr``` deletion will not work.

```c++
using namespace pobu;

auto u = std::make_unique<T>();
ptr_owned_by_unique<T> p{std::move(u)};

assert(u.get() == nullptr);
assert(p.get() != nullptr);
u = p.unique_ptr(); // this will not throw
```
Smart pointer ```pobu::ptr_owned_by_unique``` can be copied after acquirng ```std::unique_ptr```.

```c++
using namespace pobu;

auto p = make_owned_by_unique<D>();
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
  virtual ~Mock() = default;
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

  ptr_owned_by_unique<predicate_mock> p = make_owned_by_unique<StrictMock<predicate_mock>>();
  cut s{p.unique_ptr()};

  EXPECT_CALL(*p, is_true()).WillOnce(Return(true));
  ASSERT_TRUE(cut.do_something());
}
```

## Example of usage owned_by_unique with google mock, mocking factory

Because of ```pobu::ptr_owned_by_unique``` is copyable, it can be used with ```std::unique_ptr``` when mocking Factories methods, which return ```std::unique_ptr``` and also member functions which take ```std::unique_ptr``` as paramter. In order to do that, in ```gmock_macro_for_unique_ptr.hpp``` header there are special macros which create dummy member functions inside mock class. They are used like this:

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
  auto p = pobu::make_owned_by_unique<item>();
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
  auto p = pobu::make_owned_by_unique<app>();

  EXPECT_CALL(f, _install(Eq(p)));
  auto u = static_cast<system&>(f).install(p.unique_ptr());

  ASSERT_NO_THROW(*p);
}
```
When ```pobu::ptr_owned_by_unique``` is created and ```unique_ptr()``` is not invoked, it can indicate a problem in test. This is why it would be good to invoke assert to indicate to the developer, that ```owned_by_unique``` is owner of memory and its name don't indicate real ownership(owned_by_unique). This assert is disabled by default, but it can be enabled by compiling with define ```OWNED_BY_UNIQUE_ASSERT_DTOR```.

```c++
#ifdef OWNED_BY_UNIQUE_ASSERT_DTOR
   assert(is_acquired() and
           "owned_by_unique: you created owned_by_unique, but unique_ptr was never acquired");
#endif
```
