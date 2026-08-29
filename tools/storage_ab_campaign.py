#!/usr/bin/env python3
import argparse
import json
import statistics
import subprocess
from pathlib import Path


def run_case(binary, mode, edges, degree, updates, probes, timeout):
    cmd = [str(binary), mode, str(edges), str(degree), str(updates), str(probes)]
    cp = subprocess.run(cmd, text=True, capture_output=True, timeout=timeout, check=False)
    if cp.returncode != 0:
        raise RuntimeError(f"benchmark failed: {' '.join(cmd)}\n{cp.stderr}\n{cp.stdout}")
    lines = [line for line in cp.stdout.splitlines() if line.strip()]
    if not lines:
        raise RuntimeError(f"benchmark emitted no JSON: {' '.join(cmd)}")
    return json.loads(lines[-1])


def median_rows(rows):
    numeric = {}
    for key in rows[0]:
        if isinstance(rows[0][key], (int, float)) and not isinstance(rows[0][key], bool):
            numeric[key] = statistics.median(row[key] for row in rows)
    out = dict(rows[0])
    out.update(numeric)
    out["repeats"] = len(rows)
    return out


def ratio(a, b):
    return a / b if b else None


def fmt_ratio(value):
    return "n/a" if value is None else f"{value:.3f}x"


def fmt_mib(kb):
    return f"{kb / 1024.0:.1f} MiB"


def markdown(report):
    c = report["current"]
    l = report["legacy"]
    r = report["reverse_base"]
    d = report["derived"]
    lines = [
        f"# Storage A/B — {report['target_edges']:,} directed edges",
        "",
        "Historical baseline: the pre-upgrade `vector<vector<VertexId>>` base with per-vertex `unordered_set` add/delete deltas, reconstructed from commit `22d05c6`. Current design: segmented CSR + packed deltas + explicit reverse adjacency.",
        "",
        f"Synthetic graph: {int(c['vertices']):,} vertices, degree {int(c['degree'])}; mixed deterministic update batch: {int(c['updates']):,} operations. Both designs produced the same sampled-neighbor checksum and final directed edge count.",
        "",
        "| Metric | Historical | Current | Current / historical |",
        "|---|---:|---:|---:|",
        f"| Resident memory after load | {fmt_mib(l['loaded_rss_kb'])} | {fmt_mib(c['loaded_rss_kb'])} | {fmt_ratio(d['loaded_rss_ratio'])} |",
        f"| Bulk-load time | {l['bulk_load_ms']:.2f} ms | {c['bulk_load_ms']:.2f} ms | {fmt_ratio(d['bulk_load_time_ratio'])} |",
        f"| Update throughput | {l['updates_per_second']:.0f}/s | {c['updates_per_second']:.0f}/s | {fmt_ratio(d['update_throughput_ratio'])} |",
        f"| Neighbor materialization | {l['neighbor_ns_per_probe']:.1f} ns/probe | {c['neighbor_ns_per_probe']:.1f} ns/probe | {fmt_ratio(d['neighbor_latency_ratio'])} |",
        f"| Compaction time | {l['compaction_ms']:.2f} ms | {c['compaction_ms']:.2f} ms | {fmt_ratio(d['compaction_time_ratio'])} |",
        "",
        "## Explicit reverse-adjacency cost",
        "",
        f"Forward segmented CSR storage estimate: **{r['forward_storage_bytes'] / (1024**2):.1f} MiB**; transpose estimate: **{r['reverse_storage_bytes'] / (1024**2):.1f} MiB** ({100.0 * d['reverse_base_storage_fraction']:.1f}% of forward CSR).",
        f"Measured transpose construction time: **{r['transpose_build_ms']:.2f} ms**; resident-RSS increase after adding the transpose: **{fmt_mib(r['reverse_rss_delta_kb'])}**.",
        "",
        "## Interpretation boundary",
        "",
        "These are controlled synthetic hosted-runner engineering measurements, not universal performance claims. RSS is measured after releasing the generated input edge vector and requesting allocator trimming on Linux. The historical design did not maintain reverse adjacency, so the current total-memory number intentionally includes functionality absent from the baseline; the reverse section isolates that cost rather than concealing it.",
        "",
    ]
    return "\n".join(lines)


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--binary", required=True)
    p.add_argument("--edges", type=int, required=True)
    p.add_argument("--degree", type=int, default=20)
    p.add_argument("--updates", type=int)
    p.add_argument("--probes", type=int, default=8192)
    p.add_argument("--repeats", type=int, default=1)
    p.add_argument("--timeout-seconds", type=int, default=3600)
    p.add_argument("--output-dir", required=True)
    args = p.parse_args()
    if args.edges <= 0 or args.degree <= 0 or args.repeats <= 0:
        raise ValueError("edges, degree and repeats must be positive")
    updates = args.updates or max(1000, args.edges // 1000)

    current_rows = []
    legacy_rows = []
    reverse_rows = []
    for _ in range(args.repeats):
        current = run_case(args.binary, "current", args.edges, args.degree, updates, args.probes, args.timeout_seconds)
        legacy = run_case(args.binary, "legacy", args.edges, args.degree, updates, args.probes, args.timeout_seconds)
        reverse = run_case(args.binary, "reverse", args.edges, args.degree, updates, args.probes, args.timeout_seconds)
        if current["neighbor_checksum"] != legacy["neighbor_checksum"]:
            raise RuntimeError("correctness gate failed: sampled logical neighborhoods differ")
        if current["final_edges"] != legacy["final_edges"]:
            raise RuntimeError("correctness gate failed: final directed edge counts differ")
        current_rows.append(current)
        legacy_rows.append(legacy)
        reverse_rows.append(reverse)

    current = median_rows(current_rows)
    legacy = median_rows(legacy_rows)
    reverse = median_rows(reverse_rows)
    derived = {
        "loaded_rss_ratio": ratio(current["loaded_rss_kb"], legacy["loaded_rss_kb"]),
        "bulk_load_time_ratio": ratio(current["bulk_load_ms"], legacy["bulk_load_ms"]),
        "update_throughput_ratio": ratio(current["updates_per_second"], legacy["updates_per_second"]),
        "neighbor_latency_ratio": ratio(current["neighbor_ns_per_probe"], legacy["neighbor_ns_per_probe"]),
        "compaction_time_ratio": ratio(current["compaction_ms"], legacy["compaction_ms"]),
        "reverse_base_storage_fraction": ratio(reverse["reverse_storage_bytes"], reverse["forward_storage_bytes"]),
    }
    report = {
        "schema_version": 1,
        "artifact_type": "velographx-storage-ab-evidence",
        "research_claim": False,
        "target_edges": args.edges,
        "degree": args.degree,
        "updates": updates,
        "probes": args.probes,
        "current": current,
        "legacy": legacy,
        "reverse_base": reverse,
        "derived": derived,
        "raw": {"current": current_rows, "legacy": legacy_rows, "reverse_base": reverse_rows},
    }
    out = Path(args.output_dir)
    out.mkdir(parents=True, exist_ok=True)
    (out / "storage-ab.json").write_text(json.dumps(report, sort_keys=True, indent=2) + "\n")
    (out / "README.md").write_text(markdown(report))
    print(json.dumps({
        "target_edges": args.edges,
        "correctness": "passed",
        "loaded_rss_ratio": derived["loaded_rss_ratio"],
        "update_throughput_ratio": derived["update_throughput_ratio"],
        "compaction_time_ratio": derived["compaction_time_ratio"],
        "neighbor_latency_ratio": derived["neighbor_latency_ratio"],
        "reverse_base_storage_fraction": derived["reverse_base_storage_fraction"],
    }, sort_keys=True))


if __name__ == "__main__":
    main()
