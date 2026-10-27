"""用真实生成消费者重放独立电信 PDU 向量；不依赖外部工具安装。"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess


def resolve(value, wires):
    if isinstance(value, dict):
        if set(value) == {'$wire'}:
            return wires[value['$wire']].upper()
        return {k: resolve(v, wires) for k, v in value.items()}
    if isinstance(value, list):
        return [resolve(v, wires) for v in value]
    return value


def normalize(value):
    if isinstance(value, str) and value and all(c in '0123456789abcdefABCDEF' for c in value):
        return value.upper()
    if isinstance(value, dict):
        return {k: normalize(v) for k, v in value.items()}
    if isinstance(value, list):
        return [normalize(v) for v in value]
    return value


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for key in ('driver', 'protocol', 'language', 'root', 'evidence'):
        p.add_argument('--' + key, required=True)
    a = p.parse_args()
    root = Path(a.root).resolve()
    catalog_path = root / 'tests/telecom/protocol_cases.json'
    catalog = json.loads(catalog_path.read_text(encoding='utf-8'))
    frozen = json.loads((root / 'tests/telecom/protocol_vectors.json').read_text(encoding='utf-8'))
    if hashlib.sha256(catalog_path.read_bytes()).hexdigest() != frozen['catalog_sha256']:
        raise RuntimeError('protocol catalog hash changed')
    for schema in frozen['schemas'].values():
        if hashlib.sha256((root / schema['path']).read_bytes()).hexdigest() != schema['sha256']:
            raise RuntimeError('protocol schema hash changed: ' + schema['path'])
    rows = [r for r in frozen['rows'] if r['protocol'] == a.protocol]
    if not rows:
        raise RuntimeError('no protocol rows')
    runs, negative_count = [], 0
    def invoke(row, codec, mode, value):
        return subprocess.run([a.driver, row['type'], codec, mode, value], capture_output=True,
                              text=True, encoding='utf-8', timeout=30)
    for codec in catalog['codecs']:
        wires = {}
        for row in rows:
            # Construct every open-type payload from earlier PROJECT outputs, never relay oracle octets.
            expected_input = resolve(row['input'], wires)
            encoded = invoke(row, codec, 'encode', json.dumps(expected_input, separators=(',', ':')))
            if encoded.returncode:
                raise RuntimeError(f"{row['id']}/{codec}/{a.language} encode: {encoded.stderr}")
            wire = encoded.stdout.strip()
            if wire != row['wire'][codec]:
                raise RuntimeError(f"{row['id']}/{codec}/{a.language}: got {wire}, oracle {row['wire'][codec]}")
            wires[row['id']] = wire
            decoded = invoke(row, codec, 'decode', row['wire'][codec])
            if decoded.returncode:
                raise RuntimeError(f"{row['id']}/{codec}/{a.language} decode: {decoded.stderr}")
            parts = decoded.stdout.splitlines()
            if len(parts) != 2 or normalize(json.loads(parts[0])) != normalize(expected_input) or parts[1] != wire:
                raise RuntimeError(f"semantic/reencode mismatch {row['id']}/{codec}/{a.language}: {decoded.stdout}, expected {expected_input}")
            for length in range(len(wire) // 2):
                truncated = invoke(row, codec, 'decode', wire[:2*length])
                if truncated.returncode != 1:
                    raise RuntimeError(f"truncation was not rejected normally: {row['id']}/{codec}/{length}: rc={truncated.returncode}")
                negative_count += 1
            trailing = invoke(row, codec, 'decode', wire + '00')
            if trailing.returncode != 1:
                raise RuntimeError('trailing octet was not rejected normally')
            negative_count += 1
            runs.append({'id': row['id'], 'protocol': a.protocol, 'type': row['type'], 'phase': row['phase'],
                         'codec': codec, 'language': a.language, 'wire': wire, 'decoded': json.loads(parts[0])})
    for invalid in catalog['invalid_values']:
        if invalid['protocol'] != a.protocol:
            continue
        result = invoke(invalid, 'uper', 'encode', json.dumps(invalid['value']))
        if result.returncode != 1:
            raise RuntimeError('invalid value not rejected normally: ' + str(invalid))
        negative_count += 1
    evidence = {'driver_sha256': hashlib.sha256(Path(a.driver).read_bytes()).hexdigest(),
                'catalog_sha256': frozen['catalog_sha256'], 'language': a.language,
                'protocol': a.protocol, 'negative_cases': negative_count, 'runs': runs}
    output = Path(a.evidence)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(evidence, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(f"PASS {a.protocol}/{a.language}: {len(runs)} independent rows, {negative_count} negative inputs")


if __name__ == '__main__':
    main()
