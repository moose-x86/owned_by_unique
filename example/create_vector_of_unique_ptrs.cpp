#include "../inc/owned_pointer.hpp"
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
  using namespace ::std;
  using namespace ::csp;

  std::vector<unique_ptr<Foo>> u;
  std::vector<owned_pointer<Foo>> v(15, nullptr);

  std::generate(v.begin(), v.end(), [](){ return make_owned<Foo>(); });
  std::cout << "---------------------------\n";

  for(auto& e : v)
    u.push_back(e.unique_ptr());

  u.clear();

  v.erase(
      std::remove_if(v.begin(), v.end(), [](const owned_pointer<Foo>& p) { return p.expired(); }),
      v.end()
  );
  std::cout << "---------------------------\n";
}
