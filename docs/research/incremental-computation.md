# Incremental computation

The runtime now exposes graph versions, update batches, incremental triangle maintenance, insertion-optimized connected components with deletion fallback, localized PageRank repair, sparse/dense frontier representation, and a transparent incremental-versus-full execution estimator. These mechanisms are engineering baselines. The research target is a data-driven planner that accurately predicts crossover points and can explain its decision in measurable work units.
