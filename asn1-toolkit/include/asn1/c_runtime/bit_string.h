/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/include/asn1/c_runtime/bit_string.h
**   $Version: 0.1
**   $Date:   2026-10-17
**   $Author: tina.cao
***************************************************************************/
/**  File Description:

    P2-4 (--lang c) BIT STRING value storage.

    C11 port of asn1::BitStringValue (see asn1-toolkit/include/asn1/runtime/
    bit_string.hpp). Two fields:
      - `bits`: byte array (MSB-first within each byte, like the C++
        runtime); bits beyond `bit_length` are zero-padded.
      - `bit_length`: count of valid bits (not bytes); may be a
        non-multiple of 8.

    Used by the C runtime for both PER (where BIT STRING sizes are
    constrained to a multiple of 8 in non-NamedBitList cases) and
    BER/DER/CER (where the initial octet carries the unused-bits
    count per X.690 8.6.3).

    Sub-task 1 = interface skeleton only; sub-task 2 fills the bodies.

    Design Spec: asn1-toolkit/docs/todo.md#p2-4--c-语言代码生成器-lang-c
***************************************************************************/
#ifndef ASN1_C_BIT_STRING_H
#define ASN1_C_BIT_STRING_H

#include <asn1/c_runtime/asn1_c_common.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  uint8_t* bits;       /* owned; malloc/free; empty values may retain storage */
  size_t   bit_length; /* valid bit count; 0 .. 8 * size_bytes(bits) */
  size_t   cap_bytes;  /* capacity of `bits` in bytes */
} asn1_bitstring_t;

/* init new/freed objects only; free initialized objects only (repeatable).
   Public structs own their pointers: do not shallow-copy then free twice. */
void asn1_bitstring_init(asn1_bitstring_t* s);
void asn1_bitstring_free(asn1_bitstring_t* s);

/* Resize an initialized object, retaining existing contents. Newly allocated
   capacity is zeroed. Failure leaves the object unchanged; returns RESOURCE_LIMIT
   above the configured cap and OUT_OF_MEMORY on allocation failure. */
asn1_status_t asn1_bitstring_resize(asn1_bitstring_t* s, size_t bit_count);
/* Validates storage/padding and returns one past the highest set bit. */
asn1_status_t asn1_bitstring_significant(const asn1_bitstring_t*,size_t*);
/* New/zero/freed output. Exact zero padding/trimming, no significant bit loss. */
asn1_status_t asn1_bitstring_normalize(const asn1_bitstring_t*,size_t,asn1_bitstring_t*);

/* TODO sub-task 2: bitstring cmp, NamedBitList view (so the PER / BER
   encoders can read individual named bits), canonical-DER trailing-
   zero-removal helper. */

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* ASN1_C_BIT_STRING_H */
