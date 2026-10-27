"""Generate and independently compile the unchanged seven-fixture C11 matrix.

Rejections and compilation errors are failures, never skipped combinations.
Evidence is written even on failure; no linked/wire conformance is implied.
"""
import argparse
import json
import pathlib
import subprocess
import sys

from validate_codec_matrix import FIXTURES, CODECS


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--compiler', required=True)
    p.add_argument('--cc', required=True)
    p.add_argument('--output', required=True, type=pathlib.Path)
    a = p.parse_args()
    root = pathlib.Path(__file__).resolve().parents[1]
    output = a.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    msvc = pathlib.Path(a.cc).stem.lower() in ('cl', 'clang-cl')
    evidence = {'compiler': a.compiler, 'cc': a.cc, 'results': []}
    for index, fixture in enumerate(FIXTURES):
        for codec in CODECS:
            dest = output / str(index) / codec
            commands = [[a.compiler, '--lang', 'c', '--no-emit-cache', '--codec', codec,
                         '--emit-dir', str(dest), str(root / 'tests/fixtures' / fixture)]]
            row = {'fixture': fixture, 'codec': codec, 'passed': False, 'steps': []}
            try:
                for command in commands:
                    result = subprocess.run(command, capture_output=True, text=True,
                                            encoding='utf-8', errors='replace')
                    row['steps'].append(dict(command=command, returncode=result.returncode,
                                             stdout=result.stdout, stderr=result.stderr))
                    if result.returncode:
                        break
                    if len(commands) == 1:
                        manifest = json.loads((dest / 'generated-sources.json').read_text(encoding='utf-8'))
                        # Use the declared source list; stale/unrelated files are not inputs.
                        sources = manifest['sources']
                        if not sources:
                            raise ValueError('out-of-line generation declared no C sources')
                        flags = (['/std:c11', '/Zs', '/TC', '/I' + str(root / 'include'), '/I' + str(dest)]
                                 if msvc else ['-std=c11', '-pedantic-errors', '-fsyntax-only',
                                               '-I' + str(root / 'include'), '-I' + str(dest)])
                        for source in sources:
                            commands.append([a.cc, *flags, str(dest / source)])
                else:
                    row['passed'] = True
            except (OSError, ValueError, KeyError) as error:
                row['error'] = str(error)
            evidence['results'].append(row)
            print(('PASS' if row['passed'] else 'FAIL'), fixture, codec, flush=True)
            (output / 'evidence.json').write_text(json.dumps(evidence, indent=2) + '\n', encoding='utf-8')
    count = sum(row['passed'] for row in evidence['results'])
    print(f'{count}/84 generated C11 combinations passed; syntax only', flush=True)
    return 0 if count == 84 else 1


if __name__ == '__main__':
    sys.exit(main())
