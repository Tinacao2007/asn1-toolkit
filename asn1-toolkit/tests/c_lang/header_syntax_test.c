/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/tests/c_lang/header_syntax_test.c
**   $Version: 0.1
**   $Date:   2026-10-17
**   $Author: tina.cao
***************************************************************************/
/**  File Description:

    P2-4 (--lang c) sub-task 1 sanity test: every C runtime header must
    parse cleanly under `gcc -std=c11 -fsyntax-only` with `-Wall -Wextra`.

    Sub-task 2 will fill in the bodies; sub-task 1 only needs the headers
    to compile. Each header is `#include`d here exactly once so a
    duplicate-typedef or forward-declaration error surfaces immediately.

    Specification: asn1-toolkit/docs/todo.md#p2-4--c-语言代码生成器-lang-c
    Design Spec:   asn1-toolkit/include/asn1/c_runtime/
***************************************************************************/
#include <asn1/c_runtime/asn1_c_common.h>
#include <asn1/c_runtime/limits.h>
#include <asn1/c_runtime/bigint.h>
#include <asn1/c_runtime/bit_string.h>
#include <asn1/c_runtime/bit_io.h>
#include <asn1/c_runtime/byte_io.h>
#include <asn1/c_runtime/ber_tlv.h>
#include <asn1/c_runtime/ber.h>
#include <asn1/c_runtime/der.h>
#include <asn1/c_runtime/cer.h>
#include <asn1/c_runtime/oer.h>
#include <asn1/c_runtime/coer.h>
#include <asn1/c_runtime/per.h>
#include <asn1/c_runtime/uper.h>
#include <asn1/c_runtime/aper.h>
#include <asn1/c_runtime/jer.h>
#include <asn1/c_runtime/jeri.h>
#include <asn1/c_runtime/xer.h>
#include <asn1/c_runtime/exer.h>
#include <asn1/c_runtime/cxer.h>

/* Trivial stub so the compilation unit is non-empty. Sub-task 1 has no
   bodies to test — just the surface area. */
int asn1_p24_subtask1_header_syntax_ok(void) {
  /* Reference every typedef + enum value the headers expose, so a
     `-Wunused` build (in -Wall -Wextra) doesn't drop them. */
  asn1_status_t s = ASN1_OK;
  s = ASN1_ERR_TRUNCATED;
  s = ASN1_ERR_TAG_MISMATCH;
  s = ASN1_ERR_LENGTH_OVERFLOW;
  s = ASN1_ERR_CONSTRAINT_VIOLATION;
  s = ASN1_ERR_NON_CANONICAL;
  s = ASN1_ERR_INVALID_ARGUMENT;
  s = ASN1_ERR_UNSUPPORTED;
  (void)asn1_status_name(s);

  asn1_byte_view_t v = asn1_byte_view((const uint8_t*)0, 0);
  asn1_byte_buf_t  b = asn1_byte_buf((uint8_t*)0, 0);
  (void)v; (void)b;

  asn1_bigint_t bi;
  asn1_bigint_init(&bi);
  asn1_bigint_free(&bi);
  (void)asn1_bigint_init_from_decimal(&bi, "", 0);
  (void)asn1_bigint_is_zero(&bi);
  (void)asn1_bigint_sign(&bi);
  (void)asn1_bigint_cmp(&bi, &bi);
  int64_t i64 = 0;
  (void)asn1_bigint_to_int64(&bi, &i64);
  (void)asn1_bigint_to_bytes(&bi, (uint8_t**)0, (size_t*)0);

  asn1_bitstring_t bs;
  asn1_bitstring_init(&bs);
  asn1_bitstring_free(&bs);
  (void)asn1_bitstring_resize(&bs, 0);

  asn1_bit_writer_t* bw = (asn1_bit_writer_t*)0;
  asn1_bit_reader_t* br = (asn1_bit_reader_t*)0;
  (void)asn1_bit_writer_create(&bw);
  (void)asn1_bit_writer_destroy(bw);
  (void)asn1_bit_writer_put_bit(bw, 0);
  (void)asn1_bit_writer_put_bits(bw, 0, 0);
  (void)asn1_bit_writer_put_bits_from_bytes(bw, v, 0);
  (void)asn1_bit_writer_put_bitmap(bw, v);
  (void)asn1_bit_writer_put_octet(bw, 0);
  (void)asn1_bit_writer_put_octets(bw, v);
  (void)asn1_bit_writer_align_to_octet(bw);
  (void)asn1_bit_writer_bit_size(bw);
  (void)asn1_bit_writer_byte_size(bw);
  (void)asn1_bit_writer_octet_aligned(bw);
  (void)asn1_bit_writer_take(bw, (uint8_t**)0, (size_t*)0);
  (void)asn1_bit_writer_clear(bw);

  (void)asn1_bit_reader_create(&br, v);
  (void)asn1_bit_reader_destroy(br);
  int ib = 0; uint64_t uv = 0;
  (void)asn1_bit_reader_get_bit(br, &ib);
  (void)asn1_bit_reader_get_bits(br, 0, &uv);
  (void)asn1_bit_reader_get_bits_as_bytes(br, 0, (uint8_t**)0, (size_t*)0);
  uint8_t oc = 0;
  (void)asn1_bit_reader_get_octet(br, &oc);
  (void)asn1_bit_reader_get_octets(br, 0, (uint8_t**)0, (size_t*)0);
  (void)asn1_bit_reader_align_to_octet(br);
  (void)asn1_bit_reader_bit_offset(br);
  (void)asn1_bit_reader_bit_remaining(br);
  (void)asn1_bit_reader_octet_aligned(br);
  (void)asn1_bit_reader_eof(br);

  asn1_ber_tag_t tag = {0, 0, 0};
  (void)asn1_ber_tag_encode((uint8_t*)0, 0, &tag, (size_t*)0);
  (void)asn1_ber_tag_decode(v, &tag, (size_t*)0);
  (void)asn1_ber_length_encode((uint8_t*)0, 0, 0, (size_t*)0);
  (void)asn1_ber_length_decode(v, (size_t*)0, (size_t*)0);

  asn1_per_int_range_t ir = {0, 0, 0, 0, 0};
  asn1_per_size_range_t sr = {0, 0};
  (void)asn1_per_encode_bool(bw, 0);
  (void)asn1_per_decode_bool(br, &ib);
  (void)asn1_per_encode_int(bw, &ir, 0);
  (void)asn1_per_decode_int(br, &ir, &i64);
  (void)asn1_per_encode_unbounded_int(bw, &bi);
  (void)asn1_per_decode_unbounded_int(br, &bi);
  (void)asn1_per_encode_size(bw, &sr, 0);
  (void)asn1_per_decode_size(br, &sr, (size_t*)0);
  (void)asn1_per_encode_constrained_size(bw, &sr, 0);
  (void)asn1_per_decode_constrained_size(br, &sr, (size_t*)0);
  (void)asn1_per_encode_bitstring(bw, &sr, &bs);
  (void)asn1_per_decode_bitstring(br, &sr, &bs);
  (void)asn1_per_encode_octetstring(bw, v);
  (void)asn1_per_decode_octetstring(br, 0, (uint8_t**)0, (size_t*)0);

  asn1_jer_writer_t jw = {0, 0, 0};
  asn1_jer_reader_t jr = {0, 0, 0};
  (void)asn1_jer_writer_init(&jw, (char*)0, 0);
  (void)asn1_jer_writer_puts(&jw, "");
  (void)asn1_jer_writer_printf(&jw, "%s", "");
  (void)asn1_jer_reader_init(&jr, "", 0);

  asn1_xer_writer_t xw = {0, 0, 0};
  asn1_xer_reader_t xr = {0, 0, 0};
  (void)asn1_xer_writer_init(&xw, (char*)0, 0);
  (void)asn1_xer_writer_puts(&xw, "");
  (void)asn1_xer_reader_init(&xr, "", 0);

  return (int)s;
}