# Out-of-core roadmap

The out-of-core design target is partitioned graph storage with a memory budget, bounded resident cache, asynchronous prefetch and Linux io_uring where available. Repeated rescanning of an edge list will not be treated as an out-of-core engine. The in-memory dynamic representation is kept modular so partition-backed adjacency can be introduced behind the same graph API.
