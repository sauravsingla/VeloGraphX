# Security Policy

VeloGraphX treats security, reproducibility, and supply-chain integrity as part of the engineering contract.

## Supported versions

Security fixes are applied to the latest source on `main` and, when practical, to the latest published release. Pre-1.0 APIs may evolve; enterprise consumers should pin a release tag or commit SHA and validate it in their own environment.

## Reporting a vulnerability

Do **not** publish exploit details, credentials, proprietary data, or personally identifiable information in a public issue.

Report suspected vulnerabilities privately through GitHub's private vulnerability reporting/security-advisory mechanism when available. Include only the minimum information needed to reproduce the issue: affected revision, platform/toolchain, impact, reproduction steps, and a minimal non-sensitive proof of concept.

If private reporting is unavailable, contact the repository owner privately and request a secure reporting channel before sending exploit details.

## Coordinated disclosure

Please allow reasonable time to investigate and prepare a fix before public disclosure. Valid reports will be assessed for impact and reproducibility. A security advisory and patched release may be published when appropriate.

## Security boundaries

The core VeloGraphX library is a local C++20 graph-processing library. Its normal core execution path does not require credentials or a remote service. Optional capabilities, benchmark tooling, dataset preparation, Python interoperability, and CI workflows can introduce additional build-time or evaluation-time dependencies and must be reviewed separately by consumers.

VeloGraphX does not claim that cloning the repository automatically satisfies any company's security policy. Enterprise users should run their normal SAST, SCA, secret, malware, license, and binary/provenance controls before deployment.

## Supply-chain controls in this repository

The repository maintains:

- build and correctness CI;
- ASan/UBSan testing;
- checksum validation for prepared dataset fixtures;
- CodeQL static analysis;
- repository secret-pattern scanning;
- an SPDX SBOM generation step for source-intake review;
- Dependabot updates for GitHub Actions dependencies.

See `docs/enterprise-security.md` for the enterprise intake model, dependency boundaries, SBOM scope, and runtime-impact statement.

## Sensitive data

Never commit or attach secrets, access tokens, private keys, credentials, proprietary datasets, production payment/customer data, or personally identifiable information to issues, tests, fixtures, benchmarks, examples, or vulnerability reports.
