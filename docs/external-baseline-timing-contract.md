# External Baseline Timing Contract

Direct performance comparisons must use an explicit timing ledger. A result is not accepted merely because two systems ran the same algorithm: the measured envelope must also be comparable.

## Required ledger

Every external comparison records the following fields for **both** VeloGraphX and the baseline:

| Stage | VeloGraphX | External baseline |
| --- | --- | --- |
| Standard input representation | required | required |
| Graph ingestion included? | yes/no + time | yes/no + time |
| Representation conversion included? | yes/no + time | yes/no + time |
| Preprocessing/index construction included? | yes/no + time | yes/no + time |
| Initial algorithm-state construction included? | yes/no + time | yes/no + time |
| Update application included? | yes/no + time | yes/no + time |
| Incremental repair/recomputation included? | yes/no + time | yes/no + time |
| Required synchronization included? | yes/no + time | yes/no + time |
| Validation/checksum included? | normally no | normally no |
| Steady-state/kernel timing | separately labelled | separately labelled |
| End-to-end timing | required for publication comparisons | required for publication comparisons |

Blank cells are not allowed in an accepted result. If a stage is not applicable, record `N/A` and explain why.

## Standard boundary

The default end-to-end clock starts when a neutral, non-system-specific representation such as edge list, COO or CSR is handed to the system. System-specific conversion, compression, indexing, snapshot construction or auxiliary-state construction is therefore visible rather than silently amortized away.

If a paper or artifact exposes only a preprocessed representation and no neutral-input path can be reproduced, the result must be labelled as a **steady-state/preprocessed comparison**, not an end-to-end comparison.

## Symmetry rule

A cost excluded for one system must also be excluded for the other system or the difference must be shown explicitly as a separate component. Examples:

- Do not include VeloGraphX CSR construction while starting a baseline from an already-built proprietary index.
- Do not include baseline update ingestion while timing only VeloGraphX repair.
- Do not include checksum validation for only one system.
- Do not compare one-thread VeloGraphX with unrestricted baseline parallelism unless the thread mismatch is the experiment being studied.

## Reporting

Accepted results should provide both:

1. **end-to-end latency**, starting at the agreed standard input boundary; and
2. **steady-state/kernel latency**, where useful for explaining internal algorithmic behavior.

The two numbers answer different questions and must remain separately labelled in tables, CSV/JSON artifacts and README claims.

## Existing evidence

Historical hosted-CI evidence remains valid for the timing contract under which it was originally produced, but it must not be retroactively relabelled as preprocessing-inclusive if preprocessing was outside the measured envelope. New publication-grade campaigns follow this ledger.

See also [`benchmark-methodology.md`](benchmark-methodology.md) and [`../datasets/publication-benchmark-contract.json`](../datasets/publication-benchmark-contract.json).
