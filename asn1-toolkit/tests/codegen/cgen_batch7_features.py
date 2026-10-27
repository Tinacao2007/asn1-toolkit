"""Actual generated named-bit/time/sparse-root and typed text consumers."""
import argparse
import json
from pathlib import Path
import subprocess

p=argparse.ArgumentParser(description=__doc__)
for key in ('compiler','cc','runtime','root','work'):p.add_argument('--'+key,required=True)
p.add_argument('--sanitizers',action='store_true')
a=p.parse_args();root=Path(a.root).resolve();work=Path(a.work).resolve();work.mkdir(parents=True,exist_ok=True)
codecs='ber der cer uper aper oer coer xer cxer exer jer jeri'.split()
def run(command):
    r=subprocess.run(list(map(str,command)),capture_output=True,text=True)
    if r.returncode:raise RuntimeError(str(command)+'\n'+r.stdout+r.stderr)
    return r.stdout
results={}
for shape in ('outline','inline','split'):
    out=work/shape;flags=[] if shape=='outline' else ['--emit-'+shape]
    if shape=='split':flags+=['--split-max-types','4']
    run([a.compiler,'--lang','c','--c-prefix','c7','--codec',','.join(codecs),*flags,'--emit-dir',out,
         root/'tests/fixtures/cgen_batch7.asn',root/'tests/fixtures/telecom/table_constraint_ie.asn'])
    driver=['#include "generated.h"','#include <stdio.h>','#include <stdlib.h>','#include <string.h>',
            '#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s\\n",__LINE__,#x);return 1;}}while(0)',
            'int second(void);int main(void){CHECK(!second());']
    for codec in codecs:
        io='bit' if codec in ('uper','aper') else 'byte'
        for kind in ('Named','Fixed','Stamp','Sparse'):
            t='c7_mC7_t'+kind
            driver += [f'{{ {t} v,b;{t}_init(&v);{t}_init(&b);']
            if kind in ('Named','Fixed'):driver+=['CHECK(asn1_bitstring_resize(&v.value,8)==ASN1_OK);v.value.bits[0]=128;']
            elif kind=='Stamp':driver+=['v.count=v.capacity=15;v.data=malloc(15);CHECK(v.data);memcpy(v.data,"2401012330-0100",15);']
            else:driver+=['CHECK(asn1_bigint_init_from_decimal(&v.value,"4",0)==ASN1_OK);']
            driver += [f'asn1_{io}_writer_t* w=NULL;CHECK(asn1_{io}_writer_create(&w)==ASN1_OK);CHECK({t}_encode_{codec}(w,&v)==ASN1_OK);']
            if io=='bit':driver+=['uint8_t* owned=NULL;size_t size=0;CHECK(asn1_bit_writer_take(w,&owned,&size)==ASN1_OK);asn1_byte_view_t bytes=asn1_byte_view(owned,size);']
            else:driver+=['asn1_byte_view_t bytes=asn1_byte_writer_view(w);']
            driver+=['for(size_t i=0;i<bytes.size;++i)printf("%02x",bytes.data[i]);puts("");',
                     f'asn1_{io}_reader_t* r=NULL;CHECK(asn1_{io}_reader_create(&r,bytes)==ASN1_OK);CHECK({t}_decode_{codec}(r,&b)==ASN1_OK);']
            if kind in ('Named','Fixed'):driver+=['size_t significant=0;CHECK(asn1_bitstring_significant(&b.value,&significant)==ASN1_OK&&significant==1);CHECK(v.value.bit_length==8);',f'CHECK(b.value.bit_length=={16 if kind=="Fixed" else 1});']
            elif kind=='Stamp':driver+=['uint8_t canonical[13];CHECK(asn1_utctime_normalize(asn1_byte_view(b.data,b.count),canonical)==ASN1_OK&&!memcmp(canonical,"240102003000Z",13));']
            else:driver+=['CHECK(asn1_bigint_cmp(&v.value,&b.value)==0);']
            driver += [f'asn1_{io}_reader_destroy(r);asn1_{io}_writer_destroy(w);{t}_free(&v);{t}_free(&b);']
            if io=='bit':driver+=['free(owned);']
            driver+=['}']
    for codec in ('jer','jeri','xer','cxer','exer'):
        xml=codec not in ('jer','jeri')
        if xml:
            wire='<HandoverIE><id>0</id><criticality><ignore/></criticality><value><MME-UE-S1AP-ID>42</MME-UE-S1AP-ID></value></HandoverIE>'
        else:wire='{"value":42,"criticality":"ignore","id":0}' # governor may arrive last
        driver += ['{c7_mTCMod_tHandoverIE v,copy;c7_mTCMod_tHandoverIE_init(&v);c7_mTCMod_tHandoverIE_init(&copy);',
                   'const char* wire='+json.dumps(wire)+';asn1_byte_reader_t* r=NULL;',
                   'CHECK(asn1_byte_reader_create(&r,asn1_byte_view((const uint8_t*)wire,strlen(wire)))==ASN1_OK);',
                   f'CHECK(c7_mTCMod_tHandoverIE_decode_{codec}(r,&v)==ASN1_OK);',
                   'CHECK(c7_mTCMod_tHandoverIE_copy(&v,&copy)==ASN1_OK);asn1_byte_writer_t* w=NULL;CHECK(asn1_byte_writer_create(&w)==ASN1_OK);',
                   f'CHECK(c7_mTCMod_tHandoverIE_encode_{codec}(w,&copy)==ASN1_OK);asn1_byte_view_t bytes=asn1_byte_writer_view(w);',
                   'for(size_t i=0;i<bytes.size;++i)printf("%02x",bytes.data[i]);puts("");',
                   'asn1_byte_reader_destroy(r);asn1_byte_writer_destroy(w);c7_mTCMod_tHandoverIE_free(&v);c7_mTCMod_tHandoverIE_free(&copy);}']
    for wire in ('{"value":42,"criticality":"ignore","id":2}',
                 '{"value":4294967295,"criticality":"reject","id":1}',
                 '{"value":null,"criticality":"ignore","id":0}'):
        driver += ['{c7_mTCMod_tHandoverIE v,before;c7_mTCMod_tHandoverIE_init(&v);memcpy(&before,&v,sizeof(v));',
                   'const char* wire='+json.dumps(wire)+';asn1_byte_reader_t* r=NULL;CHECK(asn1_byte_reader_create(&r,asn1_byte_view((const uint8_t*)wire,strlen(wire)))==ASN1_OK);',
                   'CHECK(c7_mTCMod_tHandoverIE_decode_jer(r,&v)!=ASN1_OK);CHECK(!asn1_byte_reader_position(r)&&!memcmp(&before,&v,sizeof(v)));asn1_byte_reader_destroy(r);c7_mTCMod_tHandoverIE_free(&v);}']
    for codec in ('oer','coer'):
        driver += ['{c7_mC7_tStampSized v,b;c7_mC7_tStampSized_init(&v);c7_mC7_tStampSized_init(&b);v.data=malloc(13);CHECK(v.data);v.count=v.capacity=13;memcpy(v.data,"240102003000Z",13);',
                   f'asn1_byte_writer_t* w=NULL;CHECK(asn1_byte_writer_create(&w)==ASN1_OK);CHECK(c7_mC7_tStampSized_encode_{codec}(w,&v)==ASN1_OK);asn1_byte_view_t bytes=asn1_byte_writer_view(w);CHECK(bytes.size==14&&bytes.data[0]==13);',
                   'asn1_byte_reader_t* r=NULL;CHECK(asn1_byte_reader_create(&r,bytes)==ASN1_OK);',
                   f'CHECK(c7_mC7_tStampSized_decode_{codec}(r,&b)==ASN1_OK&&b.count==13&&!memcmp(v.data,b.data,13));',
                   'asn1_byte_reader_destroy(r);asn1_byte_writer_destroy(w);c7_mC7_tStampSized_free(&v);c7_mC7_tStampSized_free(&b);}']
    driver += ['{const uint8_t bad[]={3,2,0,128};c7_mC7_tNamed v;c7_mC7_tNamed_init(&v);asn1_byte_reader_t* r=NULL;',
               'CHECK(asn1_byte_reader_create(&r,asn1_byte_view(bad,sizeof(bad)))==ASN1_OK);CHECK(c7_mC7_tNamed_decode_der(r,&v)==ASN1_ERR_NON_CANONICAL);CHECK(!asn1_byte_reader_position(r)&&!v.value.bits);asn1_byte_reader_destroy(r);c7_mC7_tNamed_free(&v);}',
               'return 0;}']
    (out/'main.c').write_text('\n'.join(driver)+'\n')
    (out/'second.c').write_text('#include "generated.h"\nint second(void){c7_mC7_tNamed v;c7_mC7_tNamed_init(&v);c7_mC7_tNamed_free(&v);return 0;}\n')
    sources=json.loads((out/'generated-sources.json').read_text())['sources']
    san=['-fsanitize=address,undefined','-fno-sanitize-recover=undefined'] if a.sanitizers else []
    exe=out/'run';run([a.cc,'-std=c11','-pedantic-errors',*san,'-I',root/'include','-I',out,out/'main.c',out/'second.c',*[out/f for f in sources],a.runtime,'-lm','-o',exe])
    wires=run([exe]).splitlines();assert len(wires)==53
    if results:assert wires==results['outline']
    results[shape]=wires
    for codec,expected in [('der','03020780'),('cer','03020780'),('uper','10'),('aper','0080'),('cxer','3c4e616d65643e313c2f4e616d65643e')]:assert wires[codecs.index(codec)*4]==expected,(codec,wires)
    frozen=json.loads((root/'tests/standards/cgen7-asn1tools-vectors.json').read_text())
    for case in frozen['cases']:assert wires[case['index']]==case['wire'],case
    # UPER sparse-hole/root RANGE wire independently produced by asn1c 0.9.29.
    assert wires[codecs.index('uper')*4+3]=='18'
    print('PASS',shape,'named bits/time/sparse roots: 48 consumers; typed JSON/XML: 5',flush=True)
(work/'evidence.json').write_text(json.dumps(results,indent=2)+'\n')
