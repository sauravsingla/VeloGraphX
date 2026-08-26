#include <cassert>
#include "velographx/storage/dynamic_graph.hpp"
#include "velographx/incremental/bfs.hpp"
#include "velographx/incremental/sssp.hpp"
#include "velographx/incremental/kcore.hpp"
#include "velographx/incremental/pagerank.hpp"
int main(){velographx::DynamicGraph g(5,false);velographx::UpdateBatch b;b.add(0,1);b.add(1,2);g.apply(b);velographx::IncrementalBFS bfs(g,0);assert(bfs.distances()[2]==2);velographx::UpdateBatch c;c.add(2,3);bfs.apply(c);assert(bfs.distances()[3]==3);velographx::IncrementalSSSP s(g,0);assert(s.distances()[3]==3);velographx::IncrementalKCore kc(g);assert(kc.core().size()==5);velographx::IncrementalPageRank pr(g);assert(pr.values().size()==5);}
