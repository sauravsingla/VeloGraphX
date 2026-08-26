#!/usr/bin/env python3
import argparse
import hashlib
import json
import pathlib
import shutil
import sys
import urllib.parse
import urllib.request


def sha256_file(path: pathlib.Path) -> str:
    h = hashlib.sha256()
    with path.open('rb') as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b''):
            h.update(chunk)
    return h.hexdigest()


def load_manifest(path: pathlib.Path):
    data = json.loads(path.read_text())
    if data.get('schema_version') != 1:
        raise ValueError('unsupported manifest schema_version')
    datasets = data.get('datasets')
    if not isinstance(datasets, list):
        raise ValueError('manifest datasets must be a list')
    return datasets


def resolve_file_url(parsed: urllib.parse.ParseResult, manifest_dir: pathlib.Path) -> pathlib.Path:
    # file:///absolute/path -> absolute path
    # file:relative/path -> relative to the manifest directory
    # file://datasets/foo.txt -> repository-relative datasets/foo.txt
    # (the latter uses URL authority syntax, so urlparse places "datasets" in netloc).
    if parsed.netloc and parsed.netloc not in ('', 'localhost'):
        repo_root = manifest_dir.parent
        relative = pathlib.Path(parsed.netloc) / urllib.request.url2pathname(parsed.path.lstrip('/'))
        return (repo_root / relative).resolve()

    raw_path = urllib.request.url2pathname(parsed.path)
    source = pathlib.Path(raw_path)
    if source.is_absolute():
        return source
    return (manifest_dir / source).resolve()


def acquire(entry, manifest_dir: pathlib.Path, output_dir: pathlib.Path) -> pathlib.Path:
    name = entry['name']
    url = entry['url']
    expected = entry.get('sha256', '').lower()
    if len(expected) != 64 or any(c not in '0123456789abcdef' for c in expected):
        raise ValueError(f'{name}: sha256 must be a 64-character lowercase hex digest')

    parsed = urllib.parse.urlparse(url)
    output_dir.mkdir(parents=True, exist_ok=True)
    suffix = pathlib.Path(parsed.path).suffix
    target = output_dir / f'{name}{suffix}'

    if parsed.scheme == 'file':
        source = resolve_file_url(parsed, manifest_dir)
        if not source.is_file():
            raise ValueError(f'{name}: local dataset source is not a file: {source}')
        shutil.copyfile(source, target)
    elif parsed.scheme in ('http', 'https'):
        with urllib.request.urlopen(url) as response, target.open('wb') as out:
            shutil.copyfileobj(response, out)
    else:
        raise ValueError(f'{name}: unsupported URL scheme {parsed.scheme!r}')

    actual = sha256_file(target)
    if actual != expected:
        target.unlink(missing_ok=True)
        raise ValueError(f'{name}: sha256 mismatch: expected {expected}, got {actual}')
    return target


def main() -> int:
    parser = argparse.ArgumentParser(description='Prepare checksum-verified VeloGraphX benchmark datasets.')
    parser.add_argument('--manifest', type=pathlib.Path, required=True)
    parser.add_argument('--output-dir', type=pathlib.Path, default=pathlib.Path('datasets/cache'))
    parser.add_argument('--dataset', action='append', help='Dataset name to prepare; repeatable. Defaults to all.')
    args = parser.parse_args()

    datasets = load_manifest(args.manifest)
    selected = set(args.dataset or [d['name'] for d in datasets])
    known = {d['name'] for d in datasets}
    unknown = selected - known
    if unknown:
        raise ValueError(f'unknown datasets: {sorted(unknown)}')

    prepared = []
    for entry in datasets:
        if entry['name'] not in selected:
            continue
        path = acquire(entry, args.manifest.parent, args.output_dir)
        prepared.append({
            'name': entry['name'],
            'path': str(path),
            'sha256': entry['sha256'],
            'format': entry.get('format'),
            'directed': entry.get('directed'),
            'weighted': entry.get('weighted')
        })
    print(json.dumps({'schema_version': 1, 'prepared': prepared}, sort_keys=True))
    return 0


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f'error: {exc}', file=sys.stderr)
        raise SystemExit(2)
