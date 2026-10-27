"""Generated owning INTEGER DEFAULTs across all entries and output shapes."""
import argparse
import json
from pathlib import Path
import subprocess

p=argparse.ArgumentParser(description=__doc__)
for key in ('compiler','cc','runtime','root','work'):p.add_argument('--'+key,required=True)
p.add_argument('--sanitizers',action='store_true')
a=p.parse_args();root=Path(a.root);work=Path(a.work);work.mkdir(parents=True,exist_ok=True)
schema=work/'defaults.asn'
schema.write_text('D DEFINITIONS AUTOMATIC TAGS ::= BEGIN Pdu ::= SEQUENCE { small INTEGER DEFAULT 42, huge INTEGER DEFAULT -340282366920938463463374607431768211456, text UTF8String DEFAULT "hi" } END\n')
codecs='ber der cer uper aper oer coer xer cxer exer jer jeri'.split()
def run(command):
    r=subprocess.run(list(map(str,command)),capture_output=True,text=True)
    if r.returncode:raise RuntimeError(str(command)+'\n'+r.stdout+r.stderr)
    return r.stdout
results={}
for shape in ('outline','inline','split'):
    out=work/shape
    flags=[] if shape=='outline' else ['--emit-'+shape]
    if shape=='split':flags+=['--split-max-types','1']
    run([a.compiler,'--lang','c','--c-prefix','d7','--codec',','.join(codecs),*flags,'--emit-dir',out,schema])
    driver=['#include "generated.h"','#include <stdio.h>','#include <stdlib.h>','#include <string.h>',
            '#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d\\n",__LINE__);return 1;}}while(0)',
            'int second(void);int main(void){CHECK(!second());']
    for codec in codecs:
        bit=codec in ('uper','aper');io='bit' if bit else 'byte'
        for changed in (False,True):
            driver+=['{ d7_mD_tPdu value,back;d7_mD_tPdu_init(&value);d7_mD_tPdu_init(&back);',
                     'CHECK(d7_mD_tPdu_reset_defaults(&value)==ASN1_OK);']
            if changed:
                driver+=['asn1_bigint_free(&value.fsmall.value);CHECK(asn1_bigint_init_from_decimal(&value.fsmall.value,"43",0)==ASN1_OK);']
            driver += [f'asn1_{io}_writer_t* w=NULL;CHECK(asn1_{io}_writer_create(&w)==ASN1_OK);',
                       f'CHECK(d7_mD_tPdu_encode_{codec}(w,&value)==ASN1_OK);']
            if bit:driver+=['uint8_t* owned=NULL;size_t n=0;CHECK(asn1_bit_writer_take(w,&owned,&n)==ASN1_OK);asn1_byte_view_t bytes=asn1_byte_view(owned,n);']
            else:driver+=['asn1_byte_view_t bytes=asn1_byte_writer_view(w);']
            driver+=['for(size_t i=0;i<bytes.size;++i)printf("%02x",bytes.data[i]);puts("");',
                     f'asn1_{io}_reader_t* r=NULL;CHECK(asn1_{io}_reader_create(&r,bytes)==ASN1_OK);',
                     f'CHECK(d7_mD_tPdu_decode_{codec}(r,&back)==ASN1_OK);',
                     'CHECK(!asn1_bigint_cmp(&value.fsmall.value,&back.fsmall.value));CHECK(!asn1_bigint_cmp(&value.fhuge.value,&back.fhuge.value));',
                     'CHECK(back.ftext.count==2&&!memcmp(back.ftext.data,"hi",2));',
                     f'asn1_{io}_reader_destroy(r);asn1_{io}_writer_destroy(w);',
                     'd7_mD_tPdu_free(&value);d7_mD_tPdu_free(&back);']
            if bit:driver+=['free(owned);']
            driver+=['}']
    driver+=['return 0;}']
    (out/'main.c').write_text('\n'.join(driver)+'\n')
    (out/'second.c').write_text('#include "generated.h"\nint second(void){d7_mD_tPdu v;d7_mD_tPdu_init(&v);int s=d7_mD_tPdu_reset_defaults(&v);d7_mD_tPdu_free(&v);return s;}\n')
    sources=json.loads((out/'generated-sources.json').read_text())['sources']
    san=['-fsanitize=address,undefined','-fno-sanitize-recover=undefined'] if a.sanitizers else []
    exe=out/'run'
    run([a.cc,'-std=c11','-pedantic-errors',*san,'-I',root/'include','-I',out,
         out/'main.c',out/'second.c',*[out/f for f in sources],a.runtime,'-lm','-o',exe])
    wire=run([exe]).splitlines();assert len(wire)==24
    if results:assert wire==results['outline']
    results[shape]=wire
    # Standard-derived absent DEFAULT wires; CXER must explicitly carry values.
    for codec in ('der','uper','aper','oer','coer','jer','jeri'):
        expected='3000' if codec=='der' else '7b7d' if codec in ('jer','jeri') else '00'
        assert wire[codecs.index(codec)*2]==expected,(codec,wire)
    print('PASS owning DEFAULTs',shape,'12 codecs, absent/present, two TUs',flush=True)
(work/'evidence.json').write_text(json.dumps(results,indent=2)+'\n')
