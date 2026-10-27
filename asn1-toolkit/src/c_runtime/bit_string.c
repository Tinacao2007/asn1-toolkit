/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/src/c_runtime/bit_string.c
**   $Version: 0.1
**   $Date:   2026-10-17
**   $Author: tina.cao
***************************************************************************/
/**  File Description:

    P2-4 (--lang c) BIT STRING value bodies.

    C11 port of asn1-toolkit/src/runtime/bit_string.cpp
    (asn1::BitStringValue). The C struct mirrors the C++ one: `bits`
    holds the octet array (MSB-first within each byte, bits beyond
    bit_length are zero-padded), and `bit_length` is the count of valid
    bits (may be a non-multiple of 8).

    Sub-task 2 surface: init / free / resize. cmp + NamedBitList view +
    canonical-DER trailing-zero helper are deferred to a later sub-task
    (per the bit_string.h TODO).

    Specification: ITU-T X.680 / X.691 BIT STRING type
    Design Spec:   asn1-toolkit/include/asn1/c_runtime/bit_string.h
***************************************************************************/

#include <asn1/c_runtime/bit_string.h>

#include <stdlib.h>
#include "alloc_internal.h"
#include <string.h>
/* ASN1_MAX_STRING_BYTES is defined in limits.h; bit_string.h doesn't
   pull it in transitively, so include it directly. */
#include <asn1/c_runtime/limits.h>

void asn1_bitstring_init(asn1_bitstring_t* s) {
  if (!s) {
    return;
  }
  s->bits = NULL;
  s->bit_length = 0;
  s->cap_bytes = 0;
}

void asn1_bitstring_free(asn1_bitstring_t* s) {
  if (!s) {
    return;
  }
  free(s->bits);
  s->bits = NULL;
  s->bit_length = 0;
  s->cap_bytes = 0;
}

asn1_status_t asn1_bitstring_resize(asn1_bitstring_t* s, size_t bit_count) {
  if (!s) {
    return ASN1_ERR_INVALID_ARGUMENT;
  }
  if (bit_count > (size_t)ASN1_MAX_STRING_BYTES * 8u) {
    return ASN1_ERR_RESOURCE_LIMIT;
  }
  const size_t need_bytes = bit_count / 8u + (bit_count % 8u != 0u);
  if (need_bytes == 0u) {
    /* Empty: retain storage and capacity for reuse. */
    s->bit_length = 0;
    return ASN1_OK;
  }
  if (s->cap_bytes < need_bytes) {
    size_t new_cap = s->cap_bytes == 0u ? 8u : s->cap_bytes;
    while (new_cap < need_bytes) {
      if (new_cap > SIZE_MAX / 2u) { new_cap = need_bytes; break; }
      new_cap *= 2u;
    }
    uint8_t* nb = (uint8_t*)realloc(s->bits, new_cap);
    if (!nb) {
      return ASN1_ERR_OUT_OF_MEMORY;
    }
    /* Zero the freshly grown tail so bit_length padding is 0. */
    memset(nb + s->cap_bytes, 0, new_cap - s->cap_bytes);
    s->bits = nb;
    s->cap_bytes = new_cap;
  }
  s->bit_length = bit_count;
  return ASN1_OK;
}
asn1_status_t asn1_bitstring_significant(const asn1_bitstring_t* s,size_t* out) {
  if(!s||!out)return ASN1_ERR_INVALID_ARGUMENT;
  const size_t n=s->bit_length/8u+(s->bit_length%8u!=0);
  if(n>s->cap_bytes||(n&&!s->bits))return ASN1_ERR_INVALID_ARGUMENT;
  if(n>ASN1_MAX_STRING_BYTES)return ASN1_ERR_RESOURCE_LIMIT;
  if(n&&s->bit_length%8u&&(s->bits[n-1]&((1u<<(8u-s->bit_length%8u))-1u)))return ASN1_ERR_CONSTRAINT_VIOLATION;
  size_t bits=s->bit_length;
  while(bits&&!(s->bits[(bits-1)/8u]&(128u>>((bits-1)%8u))))--bits;
  *out=bits;return ASN1_OK;
}
asn1_status_t asn1_bitstring_normalize(const asn1_bitstring_t* src,size_t count,asn1_bitstring_t* out){
  if(!out||src==out)return ASN1_ERR_INVALID_ARGUMENT;
  size_t significant=0;asn1_status_t s=asn1_bitstring_significant(src,&significant);
  if(s!=ASN1_OK)return s;if(count<significant)return ASN1_ERR_CONSTRAINT_VIOLATION;
  asn1_bitstring_t tmp;asn1_bitstring_init(&tmp);s=asn1_bitstring_resize(&tmp,count);if(s!=ASN1_OK)return s;
  const size_t n=significant/8u+(significant%8u!=0);if(n)memcpy(tmp.bits,src->bits,n);
  *out=tmp;return ASN1_OK;
}
