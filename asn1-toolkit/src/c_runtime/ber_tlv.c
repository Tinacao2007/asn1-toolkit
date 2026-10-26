/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/src/c_runtime/ber_tlv.c
**   $Version: 0.1
**   $Date:   2026-10-17
**   $Author: tina.cao
***************************************************************************/
/**  File Description:

    P2-4 (--lang c) BER TLV tag / length helpers (X.690 8).

    Direct port of asn1::ber::encode_tag / decode_tag / encode_length /
    decode_length from asn1-toolkit/src/runtime/ber/tlv.cpp. The
    byte-level wire format must match so any sub-task 5+ oracle tests
    reusing the existing per_golden / tlv_golden / P1-3 vectors stay
    byte-equal between the C and C++ runtimes.

    Sub-task 2 surface:
      asn1_ber_tag_encode  — X.690 8.1.2 identifier octets
      asn1_ber_tag_decode  — same, decoder
      asn1_ber_length_encode — X.690 8.1.3 short form + 8.1.4 long form
      asn1_ber_length_decode — same, decoder

    Notes on indefinite length:
      `length_decode` rejects the 0x80 indefinite-form indicator with
      ASN1_ERR_LENGTH_OVERFLOW — the C BER decoder's per-TLV decode path
      handles the indefinite wrapper separately (BER constructed types).
      The C++ runtime's Length struct carries an `indefinite` flag the
      decoder honours directly; the C runtime's `out_length` is plain
      size_t, so the indefinite-form case is reported as an error from
      this primitive. Indefinite-form support (CER + BER constructed
      types) is a sub-task 5+ concern that adds a dedicated decoder
      path, not a change to the length-encode/decode primitives.

    Specification: ITU-T X.690 8 — BER identifier / length octets
    Design Spec:   asn1-toolkit/include/asn1/c_runtime/ber_tlv.h
***************************************************************************/

#include <asn1/c_runtime/ber_tlv.h>
#include <asn1/c_runtime/limits.h>

#include <stdbool.h>
#include <string.h>

asn1_status_t asn1_ber_tag_encode(uint8_t* out_buf, size_t out_cap,
                                  const asn1_ber_tag_t* tag, size_t* out_size) {
  if (!out_buf || !tag || !out_size) {
    return ASN1_ERR_INVALID_ARGUMENT;
  }
  if (tag->cls > 3u || (tag->constructed != 0u && tag->constructed != 1u)) {
    return ASN1_ERR_INVALID_ARGUMENT;
  }
  const uint8_t first = (uint8_t)(((uint8_t)(tag->cls & 0x03u) << 6)
                                  | (tag->constructed ? 0x20u : 0u));
  if (tag->number < 31u) {
    if (out_cap < 1u) {
      return ASN1_ERR_LENGTH_OVERFLOW;
    }
    out_buf[0] = (uint8_t)(first | (uint8_t)(tag->number & 0x1Fu));
    *out_size = 1;
    return ASN1_OK;
  }
  if (out_cap < 2u) {
    return ASN1_ERR_LENGTH_OVERFLOW;
  }
  /* Base-128 high-tag-number form, MSB-first, no leading 0x80. */
  uint8_t stack[10];
  int sp = 0;
  uint64_t n = tag->number;
  stack[sp++] = (uint8_t)(n & 0x7Fu);
  n >>= 7;
  while (n > 0) {
    if (sp >= 10) {
      return ASN1_ERR_LENGTH_OVERFLOW;
    }
    stack[sp++] = (uint8_t)(0x80u | (n & 0x7Fu));
    n >>= 7;
  }
  if (out_cap < (size_t)(1 + sp)) {
    return ASN1_ERR_LENGTH_OVERFLOW;
  }
  out_buf[0] = (uint8_t)(first | 0x1Fu);
  for (int i = 1; i <= sp; ++i) {
    out_buf[i] = stack[sp - i];
  }
  *out_size = (size_t)(1 + sp);
  return ASN1_OK;
}

asn1_status_t asn1_ber_tag_decode(asn1_byte_view_t src,
                                  asn1_ber_tag_t* out_tag,
                                  size_t* out_bytes_consumed) {
  if (!out_tag || !out_bytes_consumed || (!src.data && src.size != 0u)) {
    return ASN1_ERR_INVALID_ARGUMENT;
  }
  if (src.size < 1u || !src.data) {
    return ASN1_ERR_TRUNCATED;
  }
  const uint8_t first = src.data[0];
  asn1_ber_tag_t tmp;
  tmp.cls = (uint8_t)((first >> 6) & 0x03u);
  tmp.constructed = (uint8_t)((first & 0x20u) != 0u);
  const uint8_t low = (uint8_t)(first & 0x1Fu);
  if (low != 0x1Fu) {
    tmp.number = low;
    *out_bytes_consumed = 1;
    *out_tag = tmp;
    return ASN1_OK;
  }
  /* High-tag-number form. */
  uint64_t number = 0;
  bool first_octet = true;
  size_t i = 1;
  for (;;) {
    if (i >= src.size) {
      return ASN1_ERR_TRUNCATED;
    }
    const uint8_t b = src.data[i];
    if (first_octet && b == 0x80u) {
      return ASN1_ERR_INVALID_ARGUMENT;
    }
    first_octet = false;
    if (number > (UINT64_MAX >> 7)) {
      return ASN1_ERR_LENGTH_OVERFLOW;
    }
    number = (number << 7) | (uint64_t)(b & 0x7Fu);
    ++i;
    if ((b & 0x80u) == 0u) {
      break;
    }
  }
  if (number < 31u) return ASN1_ERR_INVALID_ARGUMENT;
  tmp.number = number;
  *out_tag = tmp;
  *out_bytes_consumed = i;
  return ASN1_OK;
}

asn1_status_t asn1_ber_length_encode(uint8_t* out_buf, size_t out_cap,
                                     size_t length, size_t* out_size) {
  if (!out_buf || !out_size) {
    return ASN1_ERR_INVALID_ARGUMENT;
  }
  if (length <= 0x7Fu) {
    if (out_cap < 1u) {
      return ASN1_ERR_LENGTH_OVERFLOW;
    }
    out_buf[0] = (uint8_t)(length & 0x7Fu);
    *out_size = 1;
    return ASN1_OK;
  }
  /* Long form: 0x80 | nbytes, then nbytes big-endian. */
  uint8_t bytes[sizeof(size_t)];
  int n = 0;
  size_t v = length;
  while (v > 0) {
    bytes[n++] = (uint8_t)(v & 0xFFu);
    v >>= 8;
  }
  if (out_cap < (size_t)(1 + n)) {
    return ASN1_ERR_LENGTH_OVERFLOW;
  }
  out_buf[0] = (uint8_t)(0x80u | (uint8_t)n);
  for (int i = 0; i < n; ++i) {
    out_buf[1 + i] = bytes[n - 1 - i];
  }
  *out_size = (size_t)(1 + n);
  return ASN1_OK;
}

asn1_status_t asn1_ber_length_decode(asn1_byte_view_t src,
                                     size_t* out_length,
                                     size_t* out_bytes_consumed) {
  if (!out_length || !out_bytes_consumed || (!src.data && src.size != 0u)) {
    return ASN1_ERR_INVALID_ARGUMENT;
  }
  if (src.size < 1u || !src.data) {
    return ASN1_ERR_TRUNCATED;
  }
  const uint8_t first = src.data[0];
  if (first == 0x80u) {
    /* Indefinite form is reported as overflow by this primitive — see
       header notes. CER/BER constructed-type decoders handle 0x80
       directly. */
    return ASN1_ERR_LENGTH_OVERFLOW;
  }
  if ((first & 0x80u) == 0u) {
    *out_length = (size_t)first;
    *out_bytes_consumed = 1;
    return ASN1_OK;
  }
  const uint8_t nbytes = (uint8_t)(first & 0x7Fu);
  if (nbytes == 0u || nbytes > sizeof(size_t)) {
    return ASN1_ERR_LENGTH_OVERFLOW;
  }
  if (src.size < (size_t)(1u + nbytes)) {
    return ASN1_ERR_TRUNCATED;
  }
  size_t value = 0;
  for (uint8_t i = 0; i < nbytes; ++i) {
    value = (value << 8) | (size_t)src.data[1u + i];
  }
  if (value > (size_t)ASN1_MAX_STRING_BYTES) {
    return ASN1_ERR_RESOURCE_LIMIT;
  }
  *out_length = value;
  *out_bytes_consumed = (size_t)(1u + nbytes);
  return ASN1_OK;
}
