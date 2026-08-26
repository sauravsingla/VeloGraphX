# Multicore execution

The runtime contains a portable thread pool and contiguous partition helper, while frontier and push/pull policies expose adaptation points for irregular workloads. Work stealing, degree-aware partitioning, affinity and NUMA-local queues are experimental targets and must be justified by scaling measurements rather than assumed superior.
