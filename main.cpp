#include "owned_by_unique.hpp"


int main()
{
    using namespace pobu;
    owned_by_unique<int> v = make_owned_by_unique<int>();
    
    std::unique_ptr<int> x{new int{1}};
    owned_by_unique<int> a = link(x);

    auto u = v.unique_ptr();
}
