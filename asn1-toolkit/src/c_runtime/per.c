/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/src/c_runtime/per.c
**   $Version: 0.1
**   $Date:   2026-10-17
**   $Author: tina.cao
***************************************************************************/
/* Legacy ABI: UPER wrappers around the single variant-parameterized core.
 * See per.h and docs/architecture.md for constructor/output contracts. */
#include <asn1/c_runtime/per.h>

static asn1_per_length_constraint_t bounded(const asn1_per_size_range_t* c) {
  asn1_per_length_constraint_t r = {ASN1_PER_LENGTH_CONSTRAINED, c->lower, c->upper, 0};
  return r;
}
asn1_status_t asn1_per_encode_bool(asn1_bit_writer_t* w, int value) {
  return asn1_bit_writer_put_bit(w, value != 0);
}
asn1_status_t asn1_per_decode_bool(asn1_bit_reader_t* r, int* out) {
  return asn1_bit_reader_get_bit(r, out);
}
asn1_status_t asn1_per_encode_int(asn1_bit_writer_t* w, const asn1_per_int_range_t* c, int64_t n) {
  return asn1_per_encode_int_ex(w, ASN1_PER_UNALIGNED, c, n);
}
asn1_status_t asn1_per_decode_int(asn1_bit_reader_t* r, const asn1_per_int_range_t* c, int64_t* out) {
  return asn1_per_decode_int_ex(r, ASN1_PER_UNALIGNED, c, out);
}
asn1_status_t asn1_per_encode_unbounded_int(asn1_bit_writer_t* w, const asn1_bigint_t* n) {
  return asn1_per_encode_unbounded_int_ex(w, ASN1_PER_UNALIGNED, n);
}
asn1_status_t asn1_per_decode_unbounded_int(asn1_bit_reader_t* r, asn1_bigint_t* out) {
  return asn1_per_decode_unbounded_int_ex(r, ASN1_PER_UNALIGNED, out);
}
asn1_status_t asn1_per_encode_size(asn1_bit_writer_t* w, const asn1_per_size_range_t* unused, size_t n) {
  if (!unused) return ASN1_ERR_INVALID_ARGUMENT;
  if (n >= 16384u) return ASN1_ERR_UNSUPPORTED;
  return asn1_per_encode_length_fragment(w, ASN1_PER_UNALIGNED, n, 0);
}
asn1_status_t asn1_per_decode_size(asn1_bit_reader_t* r, const asn1_per_size_range_t* unused, size_t* out) {
  if (!unused || !out) return ASN1_ERR_INVALID_ARGUMENT;
  size_t n = 0; int more = 0;
  asn1_status_t s = asn1_per_decode_length_fragment(r, ASN1_PER_UNALIGNED, &n, &more);
  if (s != ASN1_OK) return s;
  if (more) return ASN1_ERR_UNSUPPORTED;
  *out = n; return ASN1_OK;
}
asn1_status_t asn1_per_encode_constrained_size(asn1_bit_writer_t* w, const asn1_per_size_range_t* c, size_t n) {
  if (!c) return ASN1_ERR_INVALID_ARGUMENT;
  asn1_per_length_constraint_t constraint = bounded(c);
  return asn1_per_encode_size_ex(w, ASN1_PER_UNALIGNED, &constraint, n);
}
asn1_status_t asn1_per_decode_constrained_size(asn1_bit_reader_t* r, const asn1_per_size_range_t* c, size_t* out) {
  if (!c) return ASN1_ERR_INVALID_ARGUMENT;
  asn1_per_length_constraint_t constraint = bounded(c);
  return asn1_per_decode_size_ex(r, ASN1_PER_UNALIGNED, &constraint, out);
}
asn1_status_t asn1_per_encode_bitstring(asn1_bit_writer_t* w, const asn1_per_size_range_t* c, const asn1_bitstring_t* n) {
  if (!c) return ASN1_ERR_INVALID_ARGUMENT;
  asn1_per_length_constraint_t constraint = bounded(c);
  return asn1_per_encode_bitstring_ex(w, ASN1_PER_UNALIGNED, &constraint, n);
}
asn1_status_t asn1_per_decode_bitstring(asn1_bit_reader_t* r, const asn1_per_size_range_t* c, asn1_bitstring_t* out) {
  if (!c) return ASN1_ERR_INVALID_ARGUMENT;
  asn1_per_length_constraint_t constraint = bounded(c);
  return asn1_per_decode_bitstring_ex(r, ASN1_PER_UNALIGNED, &constraint, out);
}
asn1_status_t asn1_per_encode_octetstring(asn1_bit_writer_t* w, asn1_byte_view_t n) {
  const asn1_per_length_constraint_t c = {ASN1_PER_LENGTH_UNCONSTRAINED, 0, 0, 0};
  return asn1_per_encode_octetstring_ex(w, ASN1_PER_UNALIGNED, &c, n);
}
/* Legacy raw payload reader: no length determinant consumed. Prefer _ex for
 * complete values, especially fragments. Kept to avoid silently changing ABI. */
asn1_status_t asn1_per_decode_octetstring(asn1_bit_reader_t* r, size_t n, uint8_t** out, size_t* size) {
  return asn1_bit_reader_get_octets(r, n, out, size);
}
