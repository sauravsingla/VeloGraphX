# NUMA roadmap

NUMA support is planned around topology detection, first-touch allocation, thread affinity, partition-local queues, and explicit measurement of remote traffic. The design will retain a portable fallback when libnuma is unavailable. Claims will require multi-socket experiments comparing local, interleaved, and NUMA-unaware modes.
