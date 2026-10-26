"""Compile/run single and combined C/C++ entries, all emission forms."""
import argparse, json, subprocess
from pathlib import Path
p=argparse.ArgumentParser()
for name in ('compiler','cc','cxx','root','build','work'):p.add_argument('--'+name,required=True)
p.add_argument('--sanitizers',action='store_true')
a=p.parse_args();root=Path(a.root);build=Path(a.build);work=Path(a.work);work.mkdir(parents=True,exist_ok=True)
san=['-fsanitize=address,undefined','-fno-sanitize-recover=undefined'] if a.sanitizers else []
def run(args):
 r=subprocess.run(list(map(str,args)),capture_output=True,text=True)
 if r.returncode:raise RuntimeError(str(args)+'\n'+r.stdout+r.stderr)
 return r.stdout
schema=work/'matrix.asn';schema.write_text('M DEFINITIONS IMPLICIT TAGS ::= BEGIN Pdu ::= SEQUENCE { flag [0] BOOLEAN, count [1] INTEGER (0..7) } END')
c_driver=r'''#include "generated.h"
#include <stdlib.h>
#include <stdio.h>
int main(void){mx_mM_tPdu v,b;mx_mM_tPdu_init(&v);mx_mM_tPdu_init(&b);v.fflag.value=1;v.fcount.value=3;
#ifdef HAS_UPER
asn1_bit_writer_t* w=NULL;if(asn1_bit_writer_create(&w)||mx_mM_tPdu_encode_uper(w,&v))return 1;uint8_t* data=NULL;size_t n=0;if(asn1_bit_writer_take(w,&data,&n))return 2;for(size_t i=0;i<n;++i)printf("%02x",data[i]);puts("");asn1_bit_reader_t* r=NULL;if(asn1_bit_reader_create(&r,asn1_byte_view(data,n))||mx_mM_tPdu_decode_uper(r,&b)||b.fcount.value!=3||!b.fflag.value)return 3;asn1_bit_reader_destroy(r);asn1_bit_writer_destroy(w);free(data);
#endif
#ifdef HAS_BER
asn1_byte_writer_t* bw=NULL;if(asn1_byte_writer_create(&bw)||mx_mM_tPdu_encode_ber(bw,&v))return 4;asn1_byte_view_t bytes=asn1_byte_writer_view(bw);for(size_t i=0;i<bytes.size;++i)printf("%02x",bytes.data[i]);puts("");asn1_byte_reader_t* br=NULL;if(asn1_byte_reader_create(&br,bytes)||mx_mM_tPdu_decode_ber(br,&b)||b.fcount.value!=3||!b.fflag.value)return 5;asn1_byte_reader_destroy(br);asn1_byte_writer_destroy(bw);
#endif
mx_mM_tPdu_free(&v);mx_mM_tPdu_free(&b);return 0;}
'''
cpp_driver=r'''#include "generated.hpp"
#include <cstdio>
int main(){asn1_gen::Pdu v{true,3},b{};
#ifdef HAS_UPER
asn1::BitWriter w;if(!asn1_gen::encode_uper(w,v))return 1;for(auto x:w.buffer())std::printf("%02x",x);std::puts("");asn1::BitReader r(w.buffer());if(!asn1_gen::decode_uper(r,b)||!b.flag||b.count!=3)return 2;
#endif
#ifdef HAS_BER
asn1::ByteWriter bw;if(!asn1_gen::encode_ber(bw,v))return 3;for(auto x:bw.buffer())std::printf("%02x",x);std::puts("");asn1::ByteReader br(bw.buffer());if(!asn1_gen::decode_ber(br,b)||!b.flag||b.count!=3)return 4;
#endif
return 0;}
'''
results={}
for lang in ('c','cpp'):
 for codec in ('uper','ber','uper,ber'):
  for shape in (('outline','inline','split','shards') if lang=='c' else ('outline','inline','split')):
   out=work/(lang+'-'+codec.replace(',','-')+'-'+shape)
   args=[a.compiler,'--no-emit-cache','--codec',codec,'--emit-dir',out,schema]
   if lang=='c':args+=['--lang','c','--c-prefix','mx']
   if shape=='inline':args+=['--emit-inline']
   if shape in ('split','shards'):args+=['--emit-split']
   if shape=='shards':args+=['--split-max-types','1']
   run(args)
   driver=out/('main.c' if lang=='c' else 'main.cpp');driver.write_text(c_driver if lang=='c' else cpp_driver.replace('asn1_gen::','asn1_gen::M::') if shape=='split' else cpp_driver)
   sources=list(out.glob('*.c' if lang=='c' else '*.cpp'));exe=out/'run'
   libs=[build/'libasn1_c_runtime.a'] if lang=='c' else [build/('libasn1_'+n+'.a') for n in ('per','ber','runtime')]
   cmd=[a.cc if lang=='c' else a.cxx,'-std=c11' if lang=='c' else '-std=c++17','-pedantic-errors',*san,'-I',root/'include','-I',out]
   cmd+=['-DHAS_'+n.upper() for n in codec.split(',')]
   run([*cmd,*sources,*libs,'-lm','-o',exe]);wire=run([exe]).splitlines()
   expected=[{'uper':'b0','ber':'30068001ff810103'}[n] for n in codec.split(',')]
   assert wire==expected,(lang,codec,shape,wire,expected)
   results[str(out.name)]=wire
(work/'evidence.json').write_text(json.dumps(results,indent=2)+'\n');print('PASS',len(results),'compiled consumers, C11/C++17, single/combined, all shapes and C shards')
