#include <cassert>
#include <vector>
#include "velographx/kernels/intersection.hpp"
#include "velographx/storage/compressed_adjacency.hpp"
#include "velographx/runtime/push_pull.hpp"
int main(){std::vector<std::uint32_t>a{1,3,5,7},b{0,3,4,7};assert(velographx::kernels::adaptive_intersection(a,b)==2);auto d=velographx::storage::delta_encode(a);assert(velographx::storage::delta_decode(d)==a);assert(velographx::choose_direction(1,100,2,1000)==velographx::TraversalDirection::push);}
