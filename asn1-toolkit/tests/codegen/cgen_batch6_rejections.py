"""Unsupported instructions fail before publishing or replacing artifacts."""
import pathlib
import subprocess
import sys
import tempfile

with tempfile.TemporaryDirectory(prefix='cgen6-reject-') as temporary:
    root=pathlib.Path(temporary)
    cases=[('jer','JER','Bad ::= [ARRAY] SEQUENCE { optionalValue NULL OPTIONAL }','nullable'),
           ('jer','JER','Bad ::= [OBJECT] SET OF SEQUENCE { key INTEGER, value BOOLEAN }','OBJECT'),
           ('jer','JER','Bad ::= SEQUENCE { first [NAME AS "same"] BOOLEAN, second [NAME AS "same"] BOOLEAN }','duplicate'),
           ('exer','XER','Bad ::= [UNTAGGED] SEQUENCE { value BOOLEAN }','UNTAGGED'),
           ('exer','XER','Bad ::= SEQUENCE { value [NAME AS "bad name"] BOOLEAN }','NAME')]
    for codec,instructions,body,message in cases:
        schema=root/'input.asn'
        schema.write_text(f'M DEFINITIONS {instructions} INSTRUCTIONS ::= BEGIN {body} END\n')
        output=root/'output'
        output.mkdir(exist_ok=True)
        marker=output/'keep.txt'
        marker.write_text('keep')
        r=subprocess.run([sys.argv[1],'--lang','c','--codec',codec,'--emit-dir',str(output),str(schema)],capture_output=True,text=True)
        assert r.returncode and message in r.stderr,(body,r.stdout,r.stderr)
        assert list(output.iterdir())==[marker] and marker.read_text()=='keep'
print('PASS unsupported instruction diagnostics, publication untouched')
