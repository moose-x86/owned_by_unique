#include "owned_pointer.hpp"
#include <iostream>
#include <cassert>
#include <algorithm>
#include <vector>

struct Foo
{
  Foo() { puts("Foo"); }
  virtual ~Foo() { puts("~Foo"); }
};

int main()
{
  using namespace ::pobu;

  std::vector<std::unique_ptr<Foo>> u;
  std::vector<owned_pointer<Foo>> v(10);
  std::generate(v.begin(), v.end(), [](){ return pobu::make_owned<Foo>(); });

  std::cout << "---------------------------\n";

  for(int i = 0; i < v.size(); i += 2)
    u.push_back(v[i].unique_ptr());

  u.clear();

  v.erase(std::remove_if(v.begin(), v.end(), [](auto& p) { return p.expired(); }), v.end());
  std::cout << "---------------------------\n";
}
