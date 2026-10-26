"""Independent asn1tools interoperability. Run using a test-only oracle venv."""
import argparse
import json
import subprocess
from pathlib import Path
import asn1tools

p=argparse.ArgumentParser()
p.add_argument('--driver', required=True)
p.add_argument('--root', required=True)
p.add_argument('--evidence', required=True)
a=p.parse_args()
schema=Path(a.root)/'tests/fixtures/cgen_batch6.asn'
cases=[('Pdu',0,{'flag':True,'count':3}),
       ('Pdu',0,{'flag':False,'count':7,'note':'a<&"\r\n'}),
       ('Huge',1,0),('Huge',1,-129),('Huge',1,2**128),('Huge',1,-2**127-1),
       ('Text',2,'中😀<&\r\n'),('Bits',3,(b'\xa0',3)),('Blob',4,b'\x00\xff\x80'),
       ('Numbers',5,[3,-10,1000]),('Numbers',5,[]),
       ('Pick',6,('small',7)),('Pick',6,('text','hi')),('Pick',6,('future',True)),
       ('Extension',7,{'root':4}),('Extension',7,{'root':4,'extra':True}),
       ('Extension',7,{'root':4,'extra':True,'required':7,'optionalValue':False}),
       ('Reading',8,1.5),('Identifier',9,'1.2.840.113549'),
       ('Pair',10,{'later':7,'earlier':True}),('Real32',11,1.5),('Real64',12,-1.5),('Records',13,[{'flag':True,'count':3}]),('Labels',14,['a','b']),('Modes',15,['red','blue']),('Flags',16,[True,False,True])]
results=[]
gaps=[]
for codec,index in [('oer',0),('jer',2),('xer',4)]:
    definitions=asn1tools.parse_files(str(schema))
    if codec=='oer':
        # This external compiler cannot order an untagged CHOICE SET member;
        # that unrelated declaration has separate standard-derived vectors.
        del definitions['C6']['types']['Ordered']
        gaps.append('asn1tools OER compiler cannot sort the untagged CHOICE member of Ordered SET; standard frozen vector covers this declaration')
    oracle=asn1tools.compile_dict(definitions,codec)
    for name,type_index,value in cases:
        if name=='Extension' and (codec=='oer' and 'extra' in value or codec!='oer' and 'required' not in value):
            gaps.append('asn1tools 0.169.0 flattens extension addition groups (OER bitmap, JSON/XML group absence); excluded cases covered by standard frozen group vectors')
            continue
        wire=oracle.encode(name,value)
        r=subprocess.run([a.driver,str(type_index),str(index),wire.hex()],capture_output=True,text=True)
        assert r.returncode==0,(codec,name,value,wire.hex(),r.stderr)
        encoded=bytes.fromhex(r.stdout.strip())
        before,after=oracle.decode(name,wire),oracle.decode(name,encoded)
        assert before==after,(codec,name,value,before,after,encoded)
        results.append({'codec':codec,'type':name,'input':wire.hex(),'output':encoded.hex()})
Path(a.evidence).write_text(json.dumps({'asn1tools':asn1tools.__version__,'cases':results,'oracle_gaps':gaps},indent=2)+'\n')
print('PASS',len(results),'independent asn1tools OER/JER/XER exchanges; COER/CXER not claimed as independently canonical-certified')
