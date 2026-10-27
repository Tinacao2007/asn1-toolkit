/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/tests/c_lang/c_runtime_smoke.c
**   $Version: 0.1
**   $Date:   2026-10-17
**   $Author: tina.cao
***************************************************************************/
/**  File Description:

    P2-4 sub-task 2 link-and-run gate for the parallel C runtime.

    Sub-task 1 only checked that every header parses under
    `gcc -std=c11 -fsyntax-only`. Sub-task 2 fills the bodies in
    the C sources in src/c_runtime, so the gate upgrades: link against
    `asn1::c_runtime`, call representative legacy functions with a sample
    input, and check bytes/round-trips. This is not declaration-complete
    coverage; see docs/development.md. A failure means a C-runtime
    body returns an unexpected status or produces bytes that don't
    match the expected wire format.

    What this driver does NOT exercise:

      - Per-codec family emit_c stubs (`asn1_per_encode_<Type>` /
        `asn1_ber_decode_<Type>`) — sub-task 3+. These are generated
        by emit_c, not part of the runtime.
      - Full P1-3 protocol byte-equality — sub-task 5+ drives the
        C-runtime + emit_c pipeline against the existing per_golden /
        tlv_golden / P1-3 oracle vectors.
      - CER indefinite-form length decoding — declared as
        ASN1_ERR_LENGTH_OVERFLOW by `asn1_ber_length_decode` for now
        (CER constructed types add a dedicated decoder in sub-task 5+).

    Each test is a tiny sub-driver that returns 0 on success and 1 on
    failure; the main driver aggregates them and returns the count.
    The CTest wrapper asserts the return code is 0.

    Specification: asn1-toolkit/docs/changelog.md#roadmap--p2-4--c-语言代码生成器-lang-c
    Design Spec:   asn1-toolkit/include/asn1/c_runtime/
***************************************************************************/

#include <asn1/c_runtime/asn1_c_common.h>
#include <asn1/c_runtime/bigint.h>
#include <asn1/c_runtime/bit_io.h>
#include <asn1/c_runtime/bit_string.h>
#include <asn1/c_runtime/byte_io.h>
#include <asn1/c_runtime/ber_tlv.h>
#include <asn1/c_runtime/per.h>
#include <asn1/c_runtime/jer.h>
#include <asn1/c_runtime/xer.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --------------------------------------------------------------------- */
/* Tiny test helpers                                                      */
/* --------------------------------------------------------------------- */

static int g_failures = 0;

#define EXPECT(expr) do { \
  asn1_status_t _s = (expr); \
  if (_s != ASN1_OK) { \
    ++g_failures; \
    fprintf(stderr, "FAIL %s:%d: expected ASN1_OK, got %s\n", \
            __FILE__, __LINE__, asn1_status_name(_s)); \
    return; \
  } \
} while (0)

#define EXPECT_OK(call) EXPECT(call)

/* Status-returning call expected to fail with the given status. */
#define EXPECT_STATUS(call, expected) do { \
  asn1_status_t _s = (call); \
  if (_s != (expected)) { \
    ++g_failures; \
    fprintf(stderr, "FAIL %s:%d: expected %s, got %s\n", \
            __FILE__, __LINE__, asn1_status_name(expected), \
            asn1_status_name(_s)); \
    return; \
  } \
} while (0)

/* Boolean condition expected to be true. */
#define EXPECT_TRUE(cond) do { \
  if (!(cond)) { \
    ++g_failures; \
    fprintf(stderr, "FAIL %s:%d: %s was false\n", \
            __FILE__, __LINE__, #cond); \
    return; \
  } \
} while (0)

#define EXPECT_EQ_I(a, b) do { \
  long long _a = (long long)(a); \
  long long _b = (long long)(b); \
  if (_a != _b) { \
    ++g_failures; \
    fprintf(stderr, "FAIL %s:%d: %s (%lld) != %s (%lld)\n", \
            __FILE__, __LINE__, #a, _a, #b, _b); \
    return; \
  } \
} while (0)

/* --------------------------------------------------------------------- */
/* Status + view constructors                                             */
/* --------------------------------------------------------------------- */

static void test_status_names(void) {
  /* Every status code must return a stable non-NULL name. */
  EXPECT_TRUE(asn1_status_name(ASN1_OK) != NULL);
  EXPECT_TRUE(strcmp(asn1_status_name(ASN1_OK), "ASN1_OK") == 0);
  EXPECT_TRUE(strcmp(asn1_status_name(ASN1_ERR_TRUNCATED),
                     "ASN1_ERR_TRUNCATED") == 0);
  EXPECT_TRUE(strcmp(asn1_status_name(ASN1_ERR_TAG_MISMATCH),
                     "ASN1_ERR_TAG_MISMATCH") == 0);
  /* Out-of-range enum returns a fallback (not a crash). */
  EXPECT_TRUE(asn1_status_name((asn1_status_t)9999) != NULL);
}

static void test_byte_view(void) {
  const uint8_t data[3] = {0xAA, 0xBB, 0xCC};
  asn1_byte_view_t v = asn1_byte_view(data, 3);
  EXPECT_EQ_I(v.size, 3);
  EXPECT_EQ_I(v.data[0], 0xAA);
  EXPECT_EQ_I(v.data[2], 0xCC);

  uint8_t buf[2] = {0, 0};
  asn1_byte_buf_t b = asn1_byte_buf(buf, 2);
  EXPECT_EQ_I(b.size, 2);
  b.data[0] = 0x55;
  EXPECT_EQ_I(buf[0], 0x55);
}

/* --------------------------------------------------------------------- */
/* BigInteger                                                             */
/* --------------------------------------------------------------------- */

static void test_bigint_decimal_round_trip(void) {
  const char* samples[] = {
      "0", "1", "-1", "127", "128", "-128", "255",
      "9223372036854775807", "-9223372036854775808",
      "18446744073709551616",
  };
  for (size_t i = 0; i < sizeof(samples) / sizeof(*samples); ++i) {
    asn1_bigint_t b;
    EXPECT(asn1_bigint_init_from_decimal(&b, samples[i], 0));
    /* sign must be 0 for "0" and ±1 otherwise. */
    if (strcmp(samples[i], "0") == 0) {
      EXPECT_EQ_I(asn1_bigint_sign(&b), 0);
      EXPECT_TRUE(asn1_bigint_is_zero(&b));
    } else {
      EXPECT_TRUE(asn1_bigint_sign(&b) != 0);
      EXPECT_TRUE(!asn1_bigint_is_zero(&b));
    }
    asn1_bigint_free(&b);
  }
}

static void test_bigint_to_int64(void) {
  asn1_bigint_t b;
  int64_t v = -42;

  EXPECT(asn1_bigint_init_from_decimal(&b, "0", 0));
  EXPECT(asn1_bigint_to_int64(&b, &v));
  EXPECT_EQ_I(v, 0);
  asn1_bigint_free(&b);

  EXPECT(asn1_bigint_init_from_decimal(&b, "-42", 0));
  EXPECT(asn1_bigint_to_int64(&b, &v));
  EXPECT_EQ_I(v, -42);
  asn1_bigint_free(&b);

  /* 2^64 does not fit in int64 → must return LENGTH_OVERFLOW. */
  EXPECT(asn1_bigint_init_from_decimal(&b, "18446744073709551616", 0));
  asn1_status_t s = asn1_bigint_to_int64(&b, &v);
  EXPECT_EQ_I(s, ASN1_ERR_LENGTH_OVERFLOW);
  asn1_bigint_free(&b);
}

static void test_bigint_to_bytes(void) {
  asn1_bigint_t b;
  EXPECT(asn1_bigint_init_from_decimal(&b, "127", 0));
  uint8_t* bytes = NULL;
  size_t n = 0;
  EXPECT(asn1_bigint_to_bytes(&b, &bytes, &n));
  EXPECT_EQ_I(n, 1);
  EXPECT_EQ_I(bytes[0], 0x7F);
  free(bytes);
  asn1_bigint_free(&b);

  /* Zero → {0x00}. */
  EXPECT(asn1_bigint_init_from_decimal(&b, "0", 0));
  bytes = NULL;
  n = 0;
  EXPECT(asn1_bigint_to_bytes(&b, &bytes, &n));
  EXPECT_EQ_I(n, 1);
  EXPECT_EQ_I(bytes[0], 0x00);
  free(bytes);
  asn1_bigint_free(&b);
}

static void test_bigint_cmp(void) {
  asn1_bigint_t a, c;
  EXPECT(asn1_bigint_init_from_decimal(&a, "10", 0));
  EXPECT(asn1_bigint_init_from_decimal(&c, "20", 0));
  EXPECT_EQ_I(asn1_bigint_cmp(&a, &c), -1);
  EXPECT_EQ_I(asn1_bigint_cmp(&c, &a), 1);
  EXPECT_EQ_I(asn1_bigint_cmp(&a, &a), 0);
  asn1_bigint_free(&a);
  asn1_bigint_free(&c);
}

static void test_bigint_rejects_invalid(void) {
  asn1_bigint_t b;
  EXPECT_EQ_I(asn1_bigint_init_from_decimal(&b, "", 0),
              ASN1_ERR_INVALID_ARGUMENT);
  EXPECT_EQ_I(asn1_bigint_init_from_decimal(&b, "-", 0),
              ASN1_ERR_INVALID_ARGUMENT);
  EXPECT_EQ_I(asn1_bigint_init_from_decimal(&b, "12x", 0),
              ASN1_ERR_INVALID_ARGUMENT);
  /* Free is safe on a never-initialized struct. */
  asn1_bigint_init(&b);
  asn1_bigint_free(&b);
}

/* --------------------------------------------------------------------- */
/* BitWriter / BitReader                                                 */
/* --------------------------------------------------------------------- */

static void test_bit_writer_reader_round_trip(void) {
  /* Pack a few bytes and read them back via get_octet. */
  asn1_bit_writer_t* w = NULL;
  EXPECT(asn1_bit_writer_create(&w));
  EXPECT(asn1_bit_writer_put_octet(w, 0xDE));
  EXPECT(asn1_bit_writer_put_octet(w, 0xAD));
  EXPECT(asn1_bit_writer_put_octet(w, 0xBE));
  EXPECT(asn1_bit_writer_put_octet(w, 0xEF));
  EXPECT(asn1_bit_writer_align_to_octet(w));
  EXPECT_EQ_I(asn1_bit_writer_byte_size(w), 4);

  uint8_t* bytes = NULL;
  size_t n = 0;
  EXPECT(asn1_bit_writer_take(w, &bytes, &n));
  EXPECT_EQ_I(n, 4);
  EXPECT_EQ_I(bytes[0], 0xDE);
  EXPECT_EQ_I(bytes[1], 0xAD);
  EXPECT_EQ_I(bytes[2], 0xBE);
  EXPECT_EQ_I(bytes[3], 0xEF);

  asn1_bit_reader_t* r = NULL;
  EXPECT(asn1_bit_reader_create(&r, asn1_byte_view(bytes, n)));
  uint8_t oc = 0;
  EXPECT(asn1_bit_reader_get_octet(r, &oc));
  EXPECT_EQ_I(oc, 0xDE);
  EXPECT(asn1_bit_reader_get_octet(r, &oc));
  EXPECT_EQ_I(oc, 0xAD);
  EXPECT(asn1_bit_reader_get_octet(r, &oc));
  EXPECT_EQ_I(oc, 0xBE);
  EXPECT(asn1_bit_reader_get_octet(r, &oc));
  EXPECT_EQ_I(oc, 0xEF);
  EXPECT_EQ_I(asn1_bit_reader_eof(r), 1);
  free(bytes);
  asn1_bit_reader_destroy(r);
  asn1_bit_writer_destroy(w);
}

static void test_bit_writer_put_bits_aligned(void) {
  /* Pack 12 bits — exercises the bulk path with a tail of 4 bits.
     put_bits(value, nbits) writes the bottom `nbits` bits of value
     MSB-first (matches the C++ BitWriter::put_bits convention used by
     asn1c_per_encode_int / per_primitives). For value=0x123 and
     nbits=12 the bottom 12 bits = 0x123 = 0001 0010 0011. MSB-first
     written: byte[0] = 0001 0010 = 0x12, byte[1] = 0011 padded to
     0011 0000 = 0x30. After align_to_octet, output is {0x12, 0x30}. */
  asn1_bit_writer_t* w = NULL;
  EXPECT(asn1_bit_writer_create(&w));
  EXPECT(asn1_bit_writer_put_bits(w, 0x123u, 12));
  EXPECT_EQ_I(asn1_bit_writer_bit_size(w), 12);
  EXPECT(asn1_bit_writer_align_to_octet(w));
  uint8_t* bytes = NULL;
  size_t n = 0;
  EXPECT(asn1_bit_writer_take(w, &bytes, &n));
  EXPECT_EQ_I(n, 2);
  EXPECT_EQ_I(bytes[0], 0x12);
  EXPECT_EQ_I(bytes[1], 0x30);
  free(bytes);
  asn1_bit_writer_destroy(w);
}

static void test_bit_writer_bitmap(void) {
  /* 8-bit bitmap via the P2-1 bulk path. */
  const uint8_t bits[8] = {1, 0, 1, 1, 0, 0, 1, 0};
  asn1_bit_writer_t* w = NULL;
  EXPECT(asn1_bit_writer_create(&w));
  EXPECT(asn1_bit_writer_put_bitmap(w, asn1_byte_view(bits, 8)));
  EXPECT(asn1_bit_writer_align_to_octet(w));
  uint8_t* bytes = NULL;
  size_t n = 0;
  EXPECT(asn1_bit_writer_take(w, &bytes, &n));
  EXPECT_EQ_I(n, 1);
  EXPECT_EQ_I(bytes[0], 0xB2);  /* 10110010 */
  free(bytes);
  asn1_bit_writer_destroy(w);
}

static void test_bit_reader_truncated(void) {
  const uint8_t buf[2] = {0xAA, 0xBB};
  asn1_bit_reader_t* r = NULL;
  EXPECT(asn1_bit_reader_create(&r, asn1_byte_view(buf, 2)));
  /* ask for 17 bits but only 16 are available → TRUNCATED. */
  uint64_t v = 0;
  asn1_status_t s = asn1_bit_reader_get_bits(r, 17, &v);
  EXPECT_EQ_I(s, ASN1_ERR_TRUNCATED);
  asn1_bit_reader_destroy(r);
}

/* --------------------------------------------------------------------- */
/* BER TLV tag + length                                                  */
/* --------------------------------------------------------------------- */

static void test_ber_tag_encode_decode(void) {
  /* Universal INTEGER (tag 0x02 = cls=universal, prim, number=2). */
  asn1_ber_tag_t tag = {0, 0, 2};
  uint8_t buf[8] = {0};
  size_t out = 0;
  EXPECT(asn1_ber_tag_encode(buf, sizeof(buf), &tag, &out));
  EXPECT_EQ_I(out, 1);
  EXPECT_EQ_I(buf[0], 0x02);

  asn1_ber_tag_t dec = {0, 0, 0};
  size_t consumed = 0;
  EXPECT(asn1_ber_tag_decode(asn1_byte_view(buf, out), &dec, &consumed));
  EXPECT_EQ_I(dec.cls, 0);
  EXPECT_EQ_I(dec.constructed, 0);
  EXPECT_EQ_I(dec.number, 2);
  EXPECT_EQ_I(consumed, 1);

  /* High-tag-number form (number >= 31): write 100, expect 0x1F 0x64. */
  asn1_ber_tag_t big = {0, 1, 100};
  out = 0;
  EXPECT(asn1_ber_tag_encode(buf, sizeof(buf), &big, &out));
  EXPECT_EQ_I(out, 2);
  EXPECT_EQ_I(buf[0], 0x3F);  /* constructed=1, low 5 = 0x1F */
  EXPECT_EQ_I(buf[1], 0x64);  /* 100 in base-128 with no continuation */

  dec.cls = 9; dec.constructed = 9; dec.number = 999;
  consumed = 0;
  EXPECT(asn1_ber_tag_decode(asn1_byte_view(buf, out), &dec, &consumed));
  EXPECT_EQ_I(dec.cls, 0);
  EXPECT_EQ_I(dec.constructed, 1);
  EXPECT_EQ_I(dec.number, 100);
  EXPECT_EQ_I(consumed, 2);
}

static void test_ber_length_encode_decode(void) {
  uint8_t buf[16] = {0};
  size_t out = 0;

  /* Short form: 5 → {0x05}. */
  EXPECT(asn1_ber_length_encode(buf, sizeof(buf), 5, &out));
  EXPECT_EQ_I(out, 1);
  EXPECT_EQ_I(buf[0], 0x05);

  size_t value = 0;
  size_t consumed = 0;
  EXPECT(asn1_ber_length_decode(asn1_byte_view(buf, out), &value, &consumed));
  EXPECT_EQ_I(value, 5);
  EXPECT_EQ_I(consumed, 1);

  /* Long form: 200 → 0x81 0xC8. */
  out = 0;
  EXPECT(asn1_ber_length_encode(buf, sizeof(buf), 200, &out));
  EXPECT_EQ_I(out, 2);
  EXPECT_EQ_I(buf[0], 0x81);
  EXPECT_EQ_I(buf[1], 0xC8);

  value = 0;
  consumed = 0;
  EXPECT(asn1_ber_length_decode(asn1_byte_view(buf, out), &value, &consumed));
  EXPECT_EQ_I(value, 200);
  EXPECT_EQ_I(consumed, 2);

  /* Indefinite (0x80) is reported as LENGTH_OVERFLOW by this primitive
     (CER constructed decoders handle 0x80 separately). */
  uint8_t indef = 0x80;
  value = 0;
  consumed = 0;
  asn1_status_t s = asn1_ber_length_decode(
      asn1_byte_view(&indef, 1), &value, &consumed);
  EXPECT_EQ_I(s, ASN1_ERR_LENGTH_OVERFLOW);
}

/* --------------------------------------------------------------------- */
/* PER primitives (round-trip checks against canonical bytes)            */
/* --------------------------------------------------------------------- */

static void test_per_bool(void) {
  asn1_bit_writer_t* w = NULL;
  EXPECT(asn1_bit_writer_create(&w));
  EXPECT(asn1_per_encode_bool(w, 1));
  EXPECT(asn1_per_encode_bool(w, 0));

  uint8_t* bytes = NULL;
  size_t n = 0;
  EXPECT(asn1_bit_writer_take(w, &bytes, &n));
  EXPECT_EQ_I(n, 1);
  EXPECT_EQ_I(bytes[0], 0x80);  /* 1 << 7, then 0 << 6 */
  free(bytes);
  asn1_bit_writer_destroy(w);
}

static void test_per_constrained_int(void) {
  /* INTEGER (0..7) — 3-bit field. */
  asn1_bit_writer_t* w = NULL;
  EXPECT(asn1_bit_writer_create(&w));
  asn1_per_int_range_t range = {0, 1, 7, 1, 0};
  EXPECT(asn1_per_encode_int(w, &range, 5));
  EXPECT(asn1_bit_writer_align_to_octet(w));

  uint8_t* bytes = NULL;
  size_t n = 0;
  EXPECT(asn1_bit_writer_take(w, &bytes, &n));
  /* 5 in 3 bits = 101 → 0b101_00000 = 0xA0. */
  EXPECT_EQ_I(n, 1);
  EXPECT_EQ_I(bytes[0], 0xA0);
  free(bytes);
  asn1_bit_writer_destroy(w);

  /* Decode: feed 0xA0 and expect 5. */
  const uint8_t in = 0xA0;
  asn1_bit_reader_t* r = NULL;
  EXPECT(asn1_bit_reader_create(&r, asn1_byte_view(&in, 1)));
  int64_t v = -1;
  EXPECT(asn1_per_decode_int(r, &range, &v));
  EXPECT_EQ_I(v, 5);
  asn1_bit_reader_destroy(r);
}

static void test_per_size_encoded(void) {
  /* Unconstrained length: value 5 → 0x05 single byte. */
  asn1_bit_writer_t* w = NULL;
  EXPECT(asn1_bit_writer_create(&w));
  asn1_per_size_range_t range = {0, 0};
  EXPECT(asn1_per_encode_size(w, &range, 5));
  EXPECT(asn1_bit_writer_align_to_octet(w));

  uint8_t* bytes = NULL;
  size_t n = 0;
  EXPECT(asn1_bit_writer_take(w, &bytes, &n));
  EXPECT_EQ_I(bytes[0], 0x05);
  asn1_bit_reader_t* r = NULL;
  EXPECT(asn1_bit_reader_create(&r, asn1_byte_view(bytes, n)));
  size_t decoded = 0;
  EXPECT(asn1_per_decode_size(r, &range, &decoded));
  EXPECT_EQ_I(decoded, 5);
  asn1_bit_reader_destroy(r);
  free(bytes);
  asn1_bit_writer_destroy(w);
}

static void test_per_constrained_size(void) {
  /* SIZE (0..7) — 3-bit field, value 5 → 101 in 3 bits = 0xA0 byte. */
  asn1_bit_writer_t* w = NULL;
  EXPECT(asn1_bit_writer_create(&w));
  asn1_per_size_range_t range = {0, 7};
  EXPECT(asn1_per_encode_constrained_size(w, &range, 5));
  EXPECT(asn1_bit_writer_align_to_octet(w));
  uint8_t* bytes = NULL;
  size_t n = 0;
  EXPECT(asn1_bit_writer_take(w, &bytes, &n));
  EXPECT_EQ_I(bytes[0], 0xA0);
  asn1_bit_reader_t* r = NULL;
  EXPECT(asn1_bit_reader_create(&r, asn1_byte_view(bytes, n)));
  size_t decoded = 0;
  EXPECT(asn1_per_decode_constrained_size(r, &range, &decoded));
  EXPECT_EQ_I(decoded, 5);
  asn1_bit_reader_destroy(r);
  free(bytes);
  asn1_bit_writer_destroy(w);
}

static void test_per_octetstring_round_trip(void) {
  const uint8_t data[3] = {0x11, 0x22, 0x33};
  asn1_bit_writer_t* w = NULL;
  EXPECT(asn1_bit_writer_create(&w));
  EXPECT(asn1_per_encode_octetstring(w, asn1_byte_view(data, 3)));
  EXPECT(asn1_bit_writer_align_to_octet(w));

  uint8_t* bytes = NULL;
  size_t n = 0;
  EXPECT(asn1_bit_writer_take(w, &bytes, &n));
  /* Length prefix 0x03 + 3 data bytes = {03 11 22 33}. */
  EXPECT_EQ_I(n, 4);
  EXPECT_EQ_I(bytes[0], 0x03);
  EXPECT_EQ_I(bytes[1], 0x11);
  EXPECT_EQ_I(bytes[2], 0x22);
  EXPECT_EQ_I(bytes[3], 0x33);
  asn1_bit_reader_t* r = NULL;
  EXPECT(asn1_bit_reader_create(&r, asn1_byte_view(bytes, n)));
  asn1_per_size_range_t unused = {0, 0};
  size_t length = 0;
  EXPECT(asn1_per_decode_size(r, &unused, &length));
  uint8_t* decoded = NULL; size_t count = 0;
  EXPECT(asn1_per_decode_octetstring(r, length, &decoded, &count));
  EXPECT_TRUE(count == sizeof(data) && memcmp(decoded, data, count) == 0);
  free(decoded); asn1_bit_reader_destroy(r);
  free(bytes);
  asn1_bit_writer_destroy(w);
}

static void test_per_bitstring_round_trip(void) {
  /* 8-bit constrained BIT STRING with 0xA5 in it. */
  asn1_bitstring_t s;
  asn1_bitstring_init(&s);
  EXPECT(asn1_bitstring_resize(&s, 8));
  s.bits[0] = 0xA5;

  asn1_bit_writer_t* w = NULL;
  EXPECT(asn1_bit_writer_create(&w));
  asn1_per_size_range_t range = {8, 8};
  EXPECT(asn1_per_encode_bitstring(w, &range, &s));
  EXPECT(asn1_bit_writer_align_to_octet(w));
  uint8_t* bytes = NULL;
  size_t n = 0;
  EXPECT(asn1_bit_writer_take(w, &bytes, &n));
  EXPECT_EQ_I(n, 1);
  EXPECT_EQ_I(bytes[0], 0xA5);
  asn1_bit_reader_t* r = NULL;
  EXPECT(asn1_bit_reader_create(&r, asn1_byte_view(bytes, n)));
  asn1_bitstring_t decoded;
  asn1_bitstring_init(&decoded);
  EXPECT(asn1_per_decode_bitstring(r, &range, &decoded));
  EXPECT_TRUE(decoded.bit_length == 8 && decoded.bits[0] == 0xA5);
  asn1_bitstring_free(&decoded); asn1_bit_reader_destroy(r);
  free(bytes);
  asn1_bit_writer_destroy(w);
  asn1_bitstring_free(&s);
}

/* --------------------------------------------------------------------- */
/* JER / XER writer surface                                              */
/* --------------------------------------------------------------------- */

static void test_jer_writer(void) {
  char buf[64];
  asn1_jer_writer_t w;
  EXPECT(asn1_jer_writer_init(&w, buf, sizeof(buf)));
  EXPECT(asn1_jer_writer_puts(&w, "{\"name\":"));
  EXPECT(asn1_jer_writer_printf(&w, "\"%s\"", "Alice"));
  EXPECT(asn1_jer_writer_puts(&w, "}"));
  EXPECT_TRUE(strcmp(buf, "{\"name\":\"Alice\"}") == 0);
}

static void test_xer_writer_overflow(void) {
  char buf[4];
  asn1_xer_writer_t w;
  EXPECT(asn1_xer_writer_init(&w, buf, sizeof(buf)));
  EXPECT(asn1_xer_writer_puts(&w, "abcd"));
  /* Next put overflows → returns LENGTH_OVERFLOW. */
  EXPECT_STATUS(asn1_xer_writer_puts(&w, "x"), ASN1_ERR_LENGTH_OVERFLOW);
}

/* --------------------------------------------------------------------- */
/* Driver                                                                */
/* --------------------------------------------------------------------- */

int main(void) {
  test_status_names();
  test_byte_view();

  test_bigint_decimal_round_trip();
  test_bigint_to_int64();
  test_bigint_to_bytes();
  test_bigint_cmp();
  test_bigint_rejects_invalid();

  test_bit_writer_reader_round_trip();
  test_bit_writer_put_bits_aligned();
  test_bit_writer_bitmap();
  test_bit_reader_truncated();

  test_ber_tag_encode_decode();
  test_ber_length_encode_decode();

  test_per_bool();
  test_per_constrained_int();
  test_per_size_encoded();
  test_per_constrained_size();
  test_per_octetstring_round_trip();
  test_per_bitstring_round_trip();

  test_jer_writer();
  test_xer_writer_overflow();

  if (g_failures != 0) {
    fprintf(stderr, "FAILED: %d sub-check(s) failed\n", g_failures);
    return 1;
  }
  printf("OK: all asn1_c_runtime sub-task 2 sub-checks passed\n");
  return 0;
}
