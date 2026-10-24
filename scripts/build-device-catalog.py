"""Regenerate the offline factual model-name lookup from checksum-pinned inputs.

No network requests, imported upstream modules or execution of the GYB template.
Pass the two downloaded source files explicitly; ordinary builds use the checked-in TSV.
"""
import argparse
import ast
import csv
import hashlib
import io
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def fold(value):
    return ''.join(chr(ord(c) + 32) if 'A' <= c <= 'Z' else c for c in value)


def clean(value):
    value = value.strip(' \r\n\t\0')
    if not value or len(value.encode('utf-8')) > 256 or any(ord(c) < 32 for c in value):
        return None
    return value


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--google', type=Path, required=True)
    parser.add_argument('--apple', type=Path, required=True)
    args = parser.parse_args()
    manifest = json.loads((ROOT / 'data/device-models.sources.json').read_text(encoding='utf-8'))
    inputs = {}
    for key, path in [('google', args.google), ('apple', args.apple)]:
        data = path.read_bytes()
        if hashlib.sha256(data).hexdigest() != manifest[key]['sha256']:
            raise ValueError(f'{key}: input does not match the chosen snapshot; update intentionally')
        inputs[key] = data
    records = {}

    def add(source, brand, model, name):
        values = [clean(v) for v in (brand, model, name)]
        if any(v is None for v in values):
            return
        key = (source, *(fold(v) for v in values))
        records.setdefault(key, (source, *values))

    for row in csv.DictReader(io.StringIO(inputs['google'].decode('utf-16'))):
        # Device codenames can cover several variants: only the Model column is indexed.
        add('google', row['Retail Branding'], row['Model'], row['Marketing Name'])

    source = inputs['apple'].decode('utf-8')
    start = source.index('%{') + 2
    tree = ast.parse(source[start:source.index('}%', start)])
    for node in ast.walk(tree):
        if not (isinstance(node, ast.Call) and isinstance(node.func, ast.Name)
                and node.func.id == 'Device' and len(node.args) > 6):
            continue
        case = ast.literal_eval(node.args[0])
        if not case.startswith(('iPhone', 'iPad', 'iPod')):
            continue
        for model in ast.literal_eval(node.args[3]):
            add('apple', 'Apple', model, ast.literal_eval(node.args[6]))

    lines = ['# File Analysis device catalogue v1; source\tbrand\tmodel\tname']
    lines.extend('\t'.join(records[k]) for k in sorted(records))
    data = ('\n'.join(lines) + '\n').encode('utf-8')
    (ROOT / 'data/device-models.tsv').write_bytes(data)
    digest = hashlib.sha256(data).hexdigest()
    header = ('#pragma once\n#include <cstddef>\n#include <string_view>\nnamespace fa {\n'
              f'inline constexpr std::string_view deviceCatalogSha256 = "{digest}";\n'
              f'inline constexpr std::size_t deviceCatalogRows = {len(records)};\n'
              f'inline constexpr std::string_view deviceCatalogDate = "{manifest["snapshot_date"]}";\n'
              '}\n')
    (ROOT / 'src/device_catalog_snapshot.hpp').write_text(header, encoding='utf-8', newline='\n')
    print(f'{len(records)} factual model/name records, {len(data)} bytes, SHA-256 {digest}')


if __name__ == '__main__':
    main()
