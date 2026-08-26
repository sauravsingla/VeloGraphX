#include <cassert>
#include <cstdio>
#include "velographx/io/binary_graph.hpp"
int main(){ velographx::DynamicGraph g(3,false); g.add_edge(0,1); g.add_edge(1,2); g.compact(); velographx::io::save_binary(g,"velographx-test.bin"); auto h=velographx::io::load_binary("velographx-test.bin",false); assert(h.has_edge(0,1)); assert(h.has_edge(1,2)); std::remove("velographx-test.bin"); }
