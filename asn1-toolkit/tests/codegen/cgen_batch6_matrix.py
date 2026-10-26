"""Real C11 consumers: all twelve single entries versus a combined mask."""
import argparse
import json
import subprocess
from pathlib import Path

p = argparse.ArgumentParser()
for name in ('compiler', 'cc', 'runtime', 'root', 'work'):
    p.add_argument('--' + name, required=True)
p.add_argument('--sanitizers', action='store_true')
a = p.parse_args()
root, work = Path(a.root), Path(a.work)
work.mkdir(parents=True, exist_ok=True)
codecs = 'ber der cer uper aper oer coer xer cxer exer jer jeri'.split()

def run(args):
    r = subprocess.run(list(map(str, args)), capture_output=True, text=True)
    if r.returncode:
        raise RuntimeError(str(args) + '\n' + r.stdout + r.stderr)
    return r.stdout

schema = work / 'matrix.asn'
schema.write_text('M DEFINITIONS AUTOMATIC TAGS ::= BEGIN Pdu ::= SEQUENCE { flag BOOLEAN, count INTEGER (0..7) } END\n')
results = {}
for shape in ('outline', 'inline', 'split'):
    for selected in [[x] for x in codecs] + [codecs]:
        out = work / (shape + '-' + '-'.join(selected))
        flags = [] if shape == 'outline' else ['--emit-' + shape]
        run([a.compiler, '--lang', 'c', '--c-prefix', 'mx', '--no-emit-cache',
             '--codec', ','.join(selected), *flags, '--emit-dir', out, schema])
        driver = ['#include "generated.h"', '#include <stdio.h>', '#include <stdlib.h>',
                  'int main(void){mx_mM_tPdu v,b;mx_mM_tPdu_init(&v);mx_mM_tPdu_init(&b);v.fflag.value=1;v.fcount.value=3;']
        for codec in selected:
            bit = codec in ('uper', 'aper')
            io = 'bit' if bit else 'byte'
            driver.append('{asn1_' + io + '_writer_t* w=NULL;')
            driver.append(f'if(asn1_{io}_writer_create(&w)||mx_mM_tPdu_encode_{codec}(w,&v))return 1;')
            if bit:
                driver.append('uint8_t* owned=NULL;size_t n=0;if(asn1_bit_writer_take(w,&owned,&n))return 3;asn1_byte_view_t bytes=asn1_byte_view(owned,n);')
            else:
                driver.append('asn1_byte_view_t bytes=asn1_byte_writer_view(w);')
            driver.append('for(size_t i=0;i<bytes.size;++i)printf("%02x",bytes.data[i]);puts("");')
            driver.append(f'asn1_{io}_reader_t* r=NULL;if(asn1_{io}_reader_create(&r,bytes)||mx_mM_tPdu_decode_{codec}(r,&b)||!b.fflag.value||b.fcount.value!=3)return 2;asn1_{io}_reader_destroy(r);asn1_{io}_writer_destroy(w);}}')
            if bit:
                driver[-1] = driver[-1][:-1] + 'free(owned);}'
        driver.append('mx_mM_tPdu_free(&v);mx_mM_tPdu_free(&b);return 0;}')
        (out / 'main.c').write_text('\n'.join(driver))
        exe = out / 'run'
        san = ['-fsanitize=address,undefined', '-fno-sanitize-recover=undefined'] if a.sanitizers else []
        run([a.cc, '-std=c11', '-pedantic-errors', *san, '-I', root / 'include', '-I', out,
             *sorted(out.glob('*.c')), a.runtime, '-lm', '-o', exe])
        wire = run([exe]).splitlines()
        assert len(wire) == len(selected)
        for codec, value in zip(selected, wire):
            if codec in results:
                assert results[codec] == value, (shape, codec, results[codec], value)
            results[codec] = value
        print('PASS', shape, ','.join(selected), flush=True)
(work / 'evidence.json').write_text(json.dumps(results, indent=2) + '\n')
print('PASS 39 actual C11 consumers; twelve single/combined entries, three shapes')
