#!/usr/bin/env python3
"""Generate BER/DER/OER golden vectors for the differential cross-validation
suite (P1-1: extend byte-level golden vectors beyond PER).

The vectors are produced by INDEPENDENT implementations (asn1tools, asn1c),
never by asn1-toolkit itself. The generated file
`tests/tlv/golden/tlv_golden_vectors.txt` is checked in; the C++ test
`tests/tlv/tlv_golden_vectors_test.cpp` verifies that asn1-toolkit's encoders
produce exactly these bytes and that its decoders accept them.

Oracle policy per codec (every disagreement is adjudicated against the ITU
PDFs, never by picking "the reference compiler" by reputation):

- der: asn1c is the vector source (its der_encode arbitrates). asn1tools is
  known to deviate: it does not sort SET / SET OF components (X.690 11.6
  requires tag order per X.680 8.6 for SET and ascending octet-string order
  for SET OF), and emits SET components in reverse tag order.
- oer: asn1tools is the vector source. asn1c's oer_encode SEGFAULTS on any
  SET value (an asn1c bug, logged by the generator when it happens); the
  SET ordering rule comes from X.696 18.1/18.2 (X.680 8.6 order, same as
  DER/CER).
- ber: asn1tools is the vector source; asn1c exposes no ber_encode in this
  build (DER is a BER subset). BER has NO canonical SET order (X.690 8.14
  only requires the constructed form), so the BER column is '-' for
  SET-bearing cases — there is no byte layout to differentially prove there;
  the DER column carries the ordering proof.

Usage:
    python3 scripts/gen_tlv_golden.py [--oracle /path/to/Python_asn1tools]
                                       [--conv /tmp/asn1c_tlv/conv_tlv]

asn1c converter build recipe (once per spec change):
    mkdir -p /tmp/asn1c_tlv && cd /tmp/asn1c_tlv
    /mnt/f/ASN_1/ASN1C/asn1c/asn1c/asn1c -fcompound-names <fixture>.asn
    # conv_tlv.c: reads BER from stdin, decodes, re-encodes DER or OER on
    # stdout (argv: <TypeName> <der|oer>); see the asn1c probing notes.
    gcc -I. -I/mnt/f/ASN_1/ASN1C/asn1c/skeletons -I/mnt/f/ASN_1/ASN1C/asn1c \\
        conv_tlv.c $(ls *.c | grep -v '^conv_tlv.c$') \\
        $(ls /mnt/f/ASN_1/ASN1C/asn1c/skeletons/*.c | grep -v converter-example) \\
        -o conv_tlv -lm
"""
import argparse
import os
import subprocess
import sys

SPEC = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                    '..', 'tests', 'fixtures', 'telecom', 'tlv_golden.asn')
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                   '..', 'tests', 'tlv', 'golden', 'tlv_golden_vectors.txt')

DEFAULT_ORACLE = '/mnt/f/ASN_1/Python_asn1tools'
DEFAULT_CONV = '/tmp/asn1c_tlv/conv_tlv'

# BER has no canonical SET order; these cases carry '-' in the BER column.
BER_EXCLUDED = {'set_order', 'set_false'}

# name, type, value — the SAME values the C++ test encodes (a byte diff
# caused by different input values is not a codec bug).
CASES = [
    ('tagged_base', 'Tagged', {'plain': 7, 'impl': 8, 'expl': 9}),
    ('tagged_neg', 'Tagged', {'plain': -300, 'impl': -1, 'expl': 0}),
    ('set_order', 'TaggedSet', {'second': 7, 'first': True}),
    ('set_false', 'TaggedSet', {'second': 300, 'first': False}),
    ('setof_order', 'IntSet', [7, 3, 256]),
    ('setof_empty', 'IntSet', []),
    ('def_default', 'WithDefault', {'kind': 42, 'name': 'anon'}),
    ('def_other', 'WithDefault', {'kind': 51, 'name': 'mary'}),
    ('prims_base', 'Prims', {'n': None, 'neg': -42, 'r': 1.5}),
    ('prims_real2', 'Prims', {'n': None, 'neg': 0, 'r': 2.25}),
    ('color_root', 'Color', 'green'),
    ('color_ext', 'Color', 'yellow'),
    ('inner_num', 'Inner', ('num', 7)),
    ('inner_seq', 'Inner', ('seq', {'x': 5})),
    ('ext_base', 'Ext', {'a': 1, 'b': True}),
    ('ext_add', 'Ext', {'a': 1, 'b': True, 'c': 9}),
]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--oracle', default=DEFAULT_ORACLE,
                    help='path to the Python_asn1tools source checkout')
    ap.add_argument('--conv', default=DEFAULT_CONV,
                    help='path to the asn1c BER->DER/OER converter binary '
                         '(asn1c is the DER arbiter; see the docstring for '
                         'the build recipe)')
    ap.add_argument('--no-crosscheck', action='store_true',
                    help='skip the asn1c cross-check')
    args = ap.parse_args()

    if not os.path.isdir(args.oracle):
        sys.exit('oracle checkout not found: %s' % args.oracle)
    sys.path.insert(0, args.oracle)
    import asn1tools  # noqa: E402

    spec_path = os.path.abspath(SPEC)
    specs = {c: asn1tools.compile_files([spec_path], c)
             for c in ('ber', 'der', 'oer')}

    conv = None
    if not args.no_crosscheck:
        if not os.path.isfile(args.conv) or not os.access(args.conv, os.X_OK):
            sys.exit('converter binary not found: %s\n'
                     'Build it with (see this script\'s docstring).' % args.conv)
        conv = args.conv

    def asn1c_bytes(type_name, codec, ber_hex):
        # conv_tlv prints the re-encoded bytes AS HEX TEXT already; do not
        # re-hex the stdout (that would double-encode the reference bytes).
        out = subprocess.run([conv, type_name, codec], input=ber_hex,
                             stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        return out.returncode, out.stdout.decode().strip(), out.stderr

    lines = []
    notes = []
    for name, type_name, value in CASES:
        cols = {}
        for codec in ('ber', 'der', 'oer'):
            enc = specs[codec].encode(type_name, value)
            # The oracle must decode its own bytes; a failure here is an
            # oracle problem, not ours.
            specs[codec].decode(type_name, enc)
            hexv = enc.hex().upper()
            cols[codec] = hexv

        # ---- DER: asn1c arbitrates (asn1tools deviates on SET ordering) ----
        # SET cases keep their DER cross-check: the DER column carries the
        # ordering proof even though the BER column is '-' (X.690 8.14).
        if conv is not None:
            rc, ref_hex, err = asn1c_bytes(type_name, 'der',
                                           cols['ber'].encode())
            if rc != 0:
                notes.append('%s der: asn1c FAILED (%s) — asn1tools kept'
                             % (name, err.decode(errors='replace').strip()[:60]))
            elif ref_hex != cols['der']:
                notes.append('%s der: asn1c wins (asn1tools does not sort SET/'
                             'SET OF, X.690 11.6)\n  asn1tools: %s\n  asn1c    : %s'
                             % (name, cols['der'], ref_hex))
                cols['der'] = ref_hex

        # ---- OER: asn1tools is the source; asn1c cross-checks ----
        if conv is not None:
            rc, ref_hex, err = asn1c_bytes(type_name, 'oer',
                                           cols['ber'].encode())
            if rc != 0:
                notes.append('%s oer: asn1c FAILED (%s) — asn1tools kept '
                             '(asn1c oer_encode segfaults on SET values)'
                             % (name, err.decode(errors='replace').strip()[:40]
                                or 'segfault'))
            elif ref_hex != cols['oer']:
                notes.append('%s oer: asn1tools/asn1c disagree\n'
                             '  asn1tools: %s (kept)\n  asn1c    : %s'
                             % (name, cols['oer'], ref_hex))

        if name in BER_EXCLUDED:
            cols['ber'] = '-'

        lines.append('%s %s %s %s' % (name, cols['ber'], cols['der'], cols['oer']))

    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, 'w') as f:
        f.write('# BER/DER/OER golden vectors generated by '
                'scripts/gen_tlv_golden.py\n')
        f.write('# Oracle: asn1tools (all three codecs); asn1c arbitrates DER\n')
        f.write('#         (asn1tools does not sort SET/SET OF — X.690 11.6)\n')
        f.write('#         and cross-checks OER (its oer_encode segfaults on\n')
        f.write('#         SET values, an asn1c bug). BER has no canonical SET\n')
        f.write('#         order (X.690 8.14), so SET cases carry \'-\'.\n')
        f.write('# Format: <case> <ber-hex|-> <der-hex> <oer-hex>\n')
        f.write('# Do not edit by hand; regenerate with the script.\n')
        for line in lines:
            f.write(line + '\n')
    print('wrote %d vectors to %s' % (len(lines), OUT))
    for n in notes:
        print('NOTE:', n)


if __name__ == '__main__':
    main()
