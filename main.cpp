#include "ptr_owned_by_unique.hpp"
#include <iostream>
#include <cassert>

struct Foo{ virtual ~Foo() = default; };

int main()
{
  auto p = pobu::make_owned<Foo>();
  auto r = pobu::make_owned<int>();
  {
    std::unique_ptr<Foo> u{p};
    assert(p.get() == u.get());
  }

  assert(p == p);
  assert(p != r);

  try
  {
    auto value = *p;
  }
  catch(pobu::ptr_is_already_deleted& e)
  {
    std::cout << "error: " << e.what() << std::endl;
  }
}
