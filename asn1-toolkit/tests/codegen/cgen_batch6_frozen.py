"""Replay independently captured asn1tools wires without an oracle installation."""
import json
import subprocess
import sys
from pathlib import Path

types='Pdu Huge Text Bits Blob Numbers Pick Extension Reading Identifier Pair Real32 Real64 Records Labels Modes Flags'.split()
codecs={'oer':0,'jer':2,'xer':4}
vectors=json.loads(Path(sys.argv[2]).read_text())
assert vectors['asn1tools']=='0.169.0' and len(vectors['cases'])==72
for case in vectors['cases']:
    for source in ('input','output'):
        result=subprocess.run([sys.argv[1],str(types.index(case['type'])),str(codecs[case['codec']]),case[source]],capture_output=True,text=True)
        assert result.returncode==0,(case,result.stderr)
        assert result.stdout.strip()==case['output'],(case,result.stdout)
print('PASS 72 independent frozen vectors, both external and C-normalized inputs')
