# Enterprise security and supply-chain posture

This document is intended to help security, open-source governance, and engineering teams evaluate VeloGraphX before internal use.

## Runtime boundary

VeloGraphX core is a local C++20 graph-processing library. The core build does not require a remote service, credentials, or telemetry. The optional `VELOGRAPHX_ENABLE_IO_URING` path adds a Linux `liburing` dependency. Python bindings are disabled by default and, when enabled, use pybind11 at build time plus NumPy/SciPy/PyArrow interoperability in tests/examples.

Benchmark and dataset tooling is separate from the core runtime and may intentionally access local files or externally supplied datasets. Consumers should review those tools according to their own data-governance policy.

## Security controls

The repository uses layered controls rather than treating one scanner as a certification:

- normal Linux/macOS build and test CI;
- ASan/UBSan sanitizer CI;
- SHA-256 validation for prepared dataset fixtures;
- CodeQL static analysis for C/C++ and Python source;
- repository secret-pattern scanning for high-confidence credential/key signatures;
- automated SPDX JSON SBOM generation for source-intake review;
- Dependabot for GitHub Actions dependency updates.

These controls reduce avoidable risk but do not replace a consuming company's SAST, SCA, malware, license, provenance, container, or policy-specific checks.

## Dependency model

### Core C++ library

Mandatory third-party runtime dependencies: **none declared by the default CMake build**.

Optional dependency:

- `liburing` when `VELOGRAPHX_ENABLE_IO_URING=ON` on Linux.

### Optional Python interoperability

Python bindings are opt-in (`VELOGRAPHX_BUILD_PYTHON=ON`). CI interoperability uses:

- pybind11
- NumPy
- SciPy
- PyArrow

The source-intake SBOM records these components and resolves installed versions when available in the generation environment.

## SBOM

`tools/generate_spdx_sbom.py` emits an SPDX 2.3 JSON document. It is deliberately conservative: it records VeloGraphX, the optional `liburing` component, and optional Python interoperability packages. When Python package versions are installed, exact versions are captured; otherwise the version is reported as `NOASSERTION`.

For production intake, consumers should also generate an environment-specific SBOM after dependency resolution and package installation, because transitive dependencies depend on the platform and chosen optional features.

## Source provenance and pinning

For reproducible internal use:

1. Pin VeloGraphX to a release tag or immutable commit SHA.
2. Retain the commit SHA in internal build metadata.
3. Generate and archive an SBOM in the target build environment.
4. Verify all externally obtained datasets/artifacts with organization-approved checksums or signatures.
5. Re-run the organization's normal security and license scanners on the pinned revision.

GitHub Actions used by the dedicated security workflow are pinned to immutable commit SHAs where practical. Dependabot is configured to surface upstream action updates for review.

## Data handling

No production credentials, payment/customer data, proprietary datasets, or PII should be committed to this repository or included in tests, issues, examples, benchmark artifacts, or security reports.

## Performance impact

The enterprise hardening controls are CI/source-governance controls. They do **not** change graph algorithms, benchmark implementations, release compiler flags, or runtime execution paths. CodeQL, secret scanning, and SBOM generation only add CI time. Sanitizers remain confined to the dedicated debug/sanitizer build and are not enabled in normal Release builds.

## Enterprise intake checklist

A consuming company can use this minimal intake sequence:

1. Clone/pin an immutable revision.
2. Run secret, malware, license, SAST, and SCA scanners required by company policy.
3. Review `LICENSE`, `SECURITY.md`, this document, and the generated SPDX SBOM.
4. Build with optional features disabled unless required.
5. Run `ctest` and the sanitizer configuration in a controlled build environment.
6. Record accepted dependencies, compiler/toolchain, and revision in internal governance systems.

Passing these repository controls does not guarantee approval by every InfoSec organization; final approval depends on the consuming company's policies and deployment context.
