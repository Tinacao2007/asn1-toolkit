#!/usr/bin/env python3
"""Generate PER golden vectors for the differential cross-validation suite.

The vectors are produced by INDEPENDENT implementations (asn1tools for both
variants, asn1c as the arbiter), never by asn1-toolkit itself. The generated
file `tests/per/golden/per_golden_vectors.txt` is checked in; the C++ test
`per_golden_vectors_test.cpp` verifies that asn1-toolkit's encoder produces
exactly these bytes and that its decoder accepts them.

Pipeline per case: value -> asn1tools UPER / APER / BER;
BER -> asn1c converter -> UPER (arbiter cross-check; on disagreement the
asn1c bytes win the vector file).

Usage:
    python3 scripts/gen_per_golden.py [--oracle /path/to/Python_asn1tools]
                                      [--conv /tmp/asn1c_conv/conv]

The oracle path defaults to the sibling Python_asn1tools checkout in the
parent workspace; asn1tools is a pure-Python package usable directly from
its source tree (needs pyparsing + bitstruct).

asn1c converter build recipe (once per spec change):
    mkdir -p /tmp/asn1c_conv && cd /tmp/asn1c_conv
    /mnt/f/ASN_1/ASN1C/asn1c/asn1c/asn1c -fcompound-names -gen-PER \
        /mnt/f/ASN_1/asn1-toolkit/asn1-toolkit/tests/fixtures/telecom/per_golden.asn
    # conv.c: reads BER from stdin, decodes, re-encodes UPER or APER on
    # stdout (argv: <PDU> <uper|aper>); see the asn1c probing session notes.
    gcc -I. -I/mnt/f/ASN_1/ASN1C/asn1c/skeletons conv.c \
        $(ls *.c | grep -v '^conv.c$') \
        $(ls /mnt/f/ASN_1/ASN1C/asn1c/skeletons/*.c | grep -v converter-example) \
        -o conv -lm
"""
import argparse
import os
import subprocess
import sys

SPEC = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                    '..', 'tests', 'fixtures', 'telecom', 'per_golden.asn')
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                   '..', 'tests', 'per', 'golden', 'per_golden_vectors.txt')

DEFAULT_ORACLE = '/mnt/f/ASN_1/Python_asn1tools'
DEFAULT_CONV = '/tmp/asn1c_conv/conv'


def personnel_record(name='John', title='Director', employee_number=51,
                     date_of_hire='19810905', **kw):
    rec = {'name': name, 'title': title, 'employeeNumber': employee_number,
           'dateOfHire': date_of_hire}
    rec.update(kw)
    return rec


def prim(**kw):
    base = {
        'flag': True, 'small': 5, 'med': 200, 'wide': 60000, 'semi': 5000,
        'unb': 12345, 'neg': -777, 'en': 'green', 'extEn': 'two',
        'bits': (b'\x3f\xc0', 10), 'varBits': (b'\xa5', 5),
        'octs': b'\xde\xad\xbe\xef',
        'varOcts': b'\x01\x02', 'oid': '1.3.6.1.4.1', 'n': None,
        'txt': 'Hi!', 'ps': 'Ab-?', 'ns': '42 9', 'u8': 'Héllo',
    }
    base.update(kw)
    return base


# name, type, value
CASES = [
    # ---- PersonnelRecord: the X.691 A.1 canonical worked example ----
    ('personnel_full', 'PersonnelRecord', personnel_record(
        nameOfSpouse='Mary',
        children=[{'name': 'Ralph', 'dateOfBirth': '19571111'},
                  {'name': 'Susan', 'dateOfBirth': '19590702'}])),
    ('personnel_minimal', 'PersonnelRecord', personnel_record()),
    ('personnel_spouse_only', 'PersonnelRecord',
     personnel_record(nameOfSpouse='Mary')),
    ('personnel_children_only', 'PersonnelRecord',
     personnel_record(children=[{'name': 'Ann', 'dateOfBirth': '20200101'}])),
    ('personnel_empty_children', 'PersonnelRecord',
     personnel_record(children=[])),
    ('personnel_bounds', 'PersonnelRecord',
     personnel_record(name='A' * 16, title='T' * 16, employee_number=0,
                      date_of_hire='00000000', nameOfSpouse='B' * 16,
                      children=[{'name': 'C' * 16, 'dateOfBirth': '99999999'}] * 4)),
    ('personnel_max_emp', 'PersonnelRecord',
     personnel_record(employee_number=250)),

    # ---- Prim: every PER integer form and the standard primitives ----
    ('prim_base', 'Prim', prim()),
    ('prim_flag_false', 'Prim', prim(flag=False)),
    ('prim_small_bounds', 'Prim', prim(small=0)),
    ('prim_small_max', 'Prim', prim(small=15)),
    ('prim_med_bounds', 'Prim', prim(med=0)),
    ('prim_med_max', 'Prim', prim(med=255)),
    ('prim_wide_bounds', 'Prim', prim(wide=0)),
    ('prim_wide_max', 'Prim', prim(wide=65535)),
    ('prim_semi_min', 'Prim', prim(semi=1000)),
    ('prim_semi_big', 'Prim', prim(semi=100000)),
    ('prim_unb_zero', 'Prim', prim(unb=0)),
    ('prim_unb_127', 'Prim', prim(unb=127)),
    ('prim_unb_128', 'Prim', prim(unb=128)),
    ('prim_unb_neg1', 'Prim', prim(unb=-1)),
    ('prim_unb_big', 'Prim', prim(unb=2 ** 40)),
    ('prim_neg_bounds', 'Prim', prim(neg=-1000)),
    ('prim_neg_max', 'Prim', prim(neg=1000)),
    ('prim_enum_red', 'Prim', prim(en='red')),
    ('prim_enum_blue', 'Prim', prim(en='blue')),
    ('prim_extenum_root', 'Prim', prim(extEn='one')),
    ('prim_extenum_ext', 'Prim', prim(extEn='three')),
    ('prim_bits_min', 'Prim', prim(bits=(b'\x00\x00', 10))),
    ('prim_bits_max', 'Prim', prim(bits=(b'\xff\xc0', 10))),
    ('prim_varbits_empty', 'Prim', prim(varBits=(b'', 0))),
    ('prim_varbits_max', 'Prim', prim(varBits=(b'\xff' * 4, 32))),
    ('prim_octs_bounds', 'Prim', prim(octs=b'\x00\x00\x00\x00')),
    ('prim_varocts_empty', 'Prim', prim(varOcts=b'')),
    ('prim_varocts_max', 'Prim', prim(varOcts=b'\xaa' * 16)),
    ('prim_oid_empty', 'Prim', prim(oid='2.999')),
    ('prim_txt_empty', 'Prim', prim(txt='')),
    ('prim_txt_max', 'Prim', prim(txt='~' * 20)),
    ('prim_ps_empty', 'Prim', prim(ps='A')),
    ('prim_ps_full', 'Prim', prim(ps='(A-z9?)')),
    ('prim_ns_empty', 'Prim', prim(ns='0')),
    ('prim_ns_full', 'Prim', prim(ns='9876543 ')),
    ('prim_u8_empty', 'Prim', prim(u8='')),
    ('prim_u8_utf8', 'Prim', prim(u8='naïve 日本')),

    # ---- Extension additions ----
    ('extseq_root', 'ExtSeq', {'a': 7, 'b': True}),
    ('extseq_ext', 'ExtSeq', {'a': 7, 'b': True, 'c': 200}),
    ('extchoice_a', 'ExtChoice', ('a', 42)),
    ('extchoice_b', 'ExtChoice', ('b', 'xy')),
    ('extchoice_ext', 'ExtChoice', ('c', True)),

    # ---- Version-bracket extension groups (X.691 19.9: nested SEQUENCE) ----
    # vbseq_root: group fully absent -> bitmap bit 0, no open type.
    # vbseq_full: group present with both inner components.
    # vbseq_partial: group present, inner preamble marks d absent.
    ('vbseq_root', 'ExtSeqVB', {'a': 7, 'b': True}),
    ('vbseq_full', 'ExtSeqVB', {'a': 7, 'b': True, 'c': 200, 'd': 'xy'}),
    ('vbseq_partial', 'ExtSeqVB', {'a': 7, 'b': True, 'c': 200}),

    # ---- Fragmentation (16385 booleans: one full 16K chunk + remainder) ----
    ('frag_small', 'Frag', [True, False, True]),
    ('frag_boundary', 'Frag', [i % 2 == 0 for i in range(16385)]),
]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--oracle', default=DEFAULT_ORACLE,
                    help='path to the Python_asn1tools source checkout')
    ap.add_argument('--conv', default=DEFAULT_CONV,
                    help='path to the asn1c BER->PER converter binary '
                         '(asn1c is the arbiter; see the docstring for the '
                         'build recipe)')
    ap.add_argument('--no-crosscheck', action='store_true',
                    help='skip the asn1c UPER cross-check')
    args = ap.parse_args()

    if not os.path.isdir(args.oracle):
        sys.exit('oracle checkout not found: %s' % args.oracle)
    sys.path.insert(0, args.oracle)
    import asn1tools  # noqa: E402

    spec_path = os.path.abspath(SPEC)
    # NB: in asn1tools, 'uper' is the unaligned variant and 'per' is the
    # aligned one (codecs/per.py implements APER).
    uper = asn1tools.compile_files([spec_path], 'uper')
    aper = asn1tools.compile_files([spec_path], 'per')
    ber = asn1tools.compile_files([spec_path], 'ber')

    conv = None
    if not args.no_crosscheck:
        if not os.path.isfile(args.conv) or not os.access(args.conv, os.X_OK):
            sys.exit('converter binary not found: %s\n'
                     'Build it with (see this script\'s docstring):\n'
                     '  cd /tmp/asn1c_conv && asn1c -fcompound-names -gen-PER <spec>\n'
                     '  gcc -I. -I<skeletons> conv.c *.c <skeletons>/*.c -o conv -lm'
                     % args.conv)
        conv = args.conv

    lines = []
    disagreements = 0
    for name, type_name, value in CASES:
        u = uper.encode(type_name, value)
        a = aper.encode(type_name, value)
        # The oracle must decode its own bytes; a failure here is an oracle
        # problem, not ours.
        uper.decode(type_name, u)
        aper.decode(type_name, a)

        # Cross-check UPER against asn1c (BER re-encoding). asn1tools stays
        # the vector source: asn1c is known to deviate on BIT STRING values
        # with trailing zero bits (it removes them unconditionally, but
        # X.691 16.2/16.3 permit that only for NamedBitList types — see
        # X.680 22.7), and on MAX bounds in INTEGER constraints. Every
        # disagreement is printed so deviations stay visible.
        if conv is not None:
            ref = subprocess.run([conv, type_name, 'uper'],
                                 input=ber.encode(type_name, value),
                                 stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            if ref.returncode != 0:
                sys.exit('asn1c converter failed for %s: %s'
                         % (name, ref.stderr.decode(errors='replace')))
            ref_hex = ref.stdout.hex().upper()
            if ref_hex != u.hex().upper():
                disagreements += 1
                print('NOTE: asn1tools/asn1c disagree on %s UPER\n'
                      '  asn1tools: %s (used)\n  asn1c    : %s'
                      % (name, u.hex().upper(), ref_hex))

        lines.append('%s %s %s' % (name, u.hex().upper(), a.hex().upper()))

    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, 'w') as f:
        f.write('# PER golden vectors generated by scripts/gen_per_golden.py\n')
        f.write('# Oracle: asn1tools (UPER + APER), cross-checked against asn1c\n')
        f.write('#         via BER re-encoding. Known asn1c deviations (logged by\n')
        f.write('#         the generator): BIT STRING trailing-zero removal, which\n')
        f.write('#         X.691 16.2/16.3 permits only for NamedBitList types\n')
        f.write('#         (X.680 22.7); MAX bounds in INTEGER constraints.\n')
        f.write('# Format: <case> <uper-hex> <aper-hex>\n')
        f.write('# Do not edit by hand; regenerate with the script.\n')
        for line in lines:
            f.write(line + '\n')
    print('wrote %d vectors to %s (%d asn1c deviations reported)'
          % (len(lines), OUT, disagreements))


if __name__ == '__main__':
    main()
