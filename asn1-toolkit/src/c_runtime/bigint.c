/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/src/c_runtime/bigint.c
**   $Version: 0.1
**   $Date:   2026-10-17
**   $Author: tina.cao
***************************************************************************/
/**  File Description:

    P2-4 (--lang c) BigInteger bodies.

    Sign-plus-limbs storage (matches the C++ asn1::BigInteger layout so
    per_golden / tlv_golden / P1-3 protocol vectors stay byte-equal
    across both runtimes):

      sign  : -1 / 0 / +1 (0 = zero)
      limbs : heap array of uint32_t in little-endian magnitude order;
              no leading zero limbs except for zero (empty array).
      nlimbs: count of valid limbs
      cap   : capacity of limbs

    Sub-task 2 surface (matches bigint.h declarations):

      asn1_bigint_init / free
      asn1_bigint_init_from_decimal
      asn1_bigint_is_zero
      asn1_bigint_sign  (declared in asn1_c_common.h to avoid duplicate
                         declaration with conflicting signatures)
      asn1_bigint_cmp   (declared in asn1_c_common.h)
      asn1_bigint_to_int64
      asn1_bigint_to_bytes  (unsigned big-endian magnitude only;
                              not BER TLV or signed INTEGER content)

    Algorithm notes (mirror asn1::BigInteger in src/runtime/bigint.cpp):

      - Decimal parse: mul_add_u32(10, digit) per char.
      - Compare: sign first, then magnitude (longest wins, then MSB-down
        lexical).
      - to_bytes: minimal big-endian byte form (no leading zeros except
        for zero itself → {0x00}); the BER length-prefixed form is the
        caller's responsibility in the C++ runtime too.

    Specification: ITU-T X.680 INTEGER type
    Design Spec:   asn1-toolkit/include/asn1/c_runtime/bigint.h
***************************************************************************/

#include <asn1/c_runtime/bigint.h>

#include <stdint.h>
#include <stdlib.h>
#include "alloc_internal.h"
#include <string.h>

/* --------------------------------------------------------------------- */
/* Internal helpers                                                       */
/* --------------------------------------------------------------------- */

/* Append one limb, propagating allocator and configured-limit failures. */
static asn1_status_t bigint_push_limb(asn1_bigint_t* b, uint32_t limb) {
  if (b->nlimbs >= ASN1_MAX_BIGINT_LIMBS) return ASN1_ERR_RESOURCE_LIMIT;
  if (b->nlimbs == b->cap) {
    size_t new_cap = b->cap == 0 ? 4u : b->cap;
    if (new_cap <= SIZE_MAX / 2u && b->cap != 0) new_cap *= 2u;
    if (new_cap > ASN1_MAX_BIGINT_LIMBS) new_cap = ASN1_MAX_BIGINT_LIMBS;
    if (new_cap <= b->nlimbs || new_cap > SIZE_MAX / sizeof(uint32_t))
      return ASN1_ERR_LENGTH_OVERFLOW;
    uint32_t* nl = (uint32_t*)realloc(b->limbs, new_cap * sizeof(uint32_t));
    if (!nl) return ASN1_ERR_OUT_OF_MEMORY;
    b->limbs = nl;
    b->cap = new_cap;
  }
  b->limbs[b->nlimbs++] = limb;
  return ASN1_OK;
}

static asn1_status_t bigint_mul_add_u32(asn1_bigint_t* b, uint32_t mul, uint32_t add) {
  uint64_t carry = add;
  for (size_t i = 0; i < b->nlimbs; ++i) {
    const uint64_t prod = (uint64_t)b->limbs[i] * mul + carry;
    b->limbs[i] = (uint32_t)prod;
    carry = prod >> 32;
  }
  if (carry != 0) return bigint_push_limb(b, (uint32_t)carry);
  return ASN1_OK;
}

/* Three-way magnitude compare: longest wins, then MSB-down. */
static int bigint_cmp_mag(const asn1_bigint_t* a, const asn1_bigint_t* b) {
  if (a->nlimbs != b->nlimbs) {
    return a->nlimbs < b->nlimbs ? -1 : 1;
  }
  for (size_t i = a->nlimbs; i-- > 0;) {
    if (a->limbs[i] != b->limbs[i]) {
      return a->limbs[i] < b->limbs[i] ? -1 : 1;
    }
  }
  return 0;
}

/* Minimal big-endian magnitude byte form: zero → {0x00}; positive
   non-zero → strip leading zero bytes (none should exist after normalize);
   negative handled by caller. */
static asn1_status_t bigint_magnitude_be(const asn1_bigint_t* b,
                                uint8_t** out_buf, size_t* out_size) {
  if (b->nlimbs == 0) {
    uint8_t* z = (uint8_t*)malloc(1);
    if (!z) {
      return ASN1_ERR_OUT_OF_MEMORY;
    }
    z[0] = 0x00;
    *out_buf = z;
    *out_size = 1;
    return ASN1_OK;
  }
  if (b->nlimbs > ASN1_MAX_BIGINT_LIMBS) return ASN1_ERR_RESOURCE_LIMIT;
  if (b->nlimbs > SIZE_MAX / 4u) return ASN1_ERR_LENGTH_OVERFLOW;
  uint8_t* buf = (uint8_t*)malloc(b->nlimbs * 4u);
  if (!buf) {
    return ASN1_ERR_OUT_OF_MEMORY;
  }
  for (size_t i = 0; i < b->nlimbs; ++i) {
    const uint32_t w = b->limbs[b->nlimbs - 1 - i];
    const size_t o = i * 4u;
    buf[o + 0] = (uint8_t)((w >> 24) & 0xFFu);
    buf[o + 1] = (uint8_t)((w >> 16) & 0xFFu);
    buf[o + 2] = (uint8_t)((w >> 8) & 0xFFu);
    buf[o + 3] = (uint8_t)(w & 0xFFu);
  }
  /* Strip leading zero bytes (keep at least one). */
  size_t start = 0;
  while (start + 1u < b->nlimbs * 4u && buf[start] == 0u) {
    ++start;
  }
  size_t out_n = b->nlimbs * 4u - start;
  uint8_t* trimmed = (uint8_t*)malloc(out_n);
  if (!trimmed) {
    free(buf);
    return ASN1_ERR_OUT_OF_MEMORY;
  }
  memcpy(trimmed, buf + start, out_n);
  free(buf);
  *out_buf = trimmed;
  *out_size = out_n;
  return ASN1_OK;
}

/* --------------------------------------------------------------------- */
/* Lifecycle                                                              */
/* --------------------------------------------------------------------- */

void asn1_bigint_init(asn1_bigint_t* b) {
  if (!b) {
    return;
  }
  b->sign = 0;
  b->limbs = NULL;
  b->nlimbs = 0;
  b->cap = 0;
}

void asn1_bigint_free(asn1_bigint_t* b) {
  if (!b) {
    return;
  }
  free(b->limbs);
  b->limbs = NULL;
  b->nlimbs = 0;
  b->cap = 0;
  b->sign = 0;
}

asn1_status_t asn1_bigint_init_from_decimal(asn1_bigint_t* b,
                                             const char* text,
                                             int negative) {
  if (!b || !text) {
    return ASN1_ERR_INVALID_ARGUMENT;
  }
  /* Constructor: destination must be uninitialized or previously freed.
   * Do not inspect its old bytes; construct privately and publish once. */
  asn1_bigint_t tmp;
  asn1_bigint_init(&tmp);
  size_t i = 0;
  int neg = negative != 0;
  if (!neg && (text[0] == '-' || text[0] == '+')) {
    neg = text[0] == '-';
    i = 1;
  }
  if (text[i] == '\0') return ASN1_ERR_INVALID_ARGUMENT;
  for (; text[i] != '\0'; ++i) {
    if (text[i] < '0' || text[i] > '9') {
      asn1_bigint_free(&tmp);
      return ASN1_ERR_INVALID_ARGUMENT;
    }
    asn1_status_t status = bigint_mul_add_u32(&tmp, 10u, (uint32_t)(text[i] - '0'));
    if (status != ASN1_OK) {
      asn1_bigint_free(&tmp);
      return status;
    }
  }
  tmp.sign = tmp.nlimbs == 0 ? 0 : (neg ? -1 : 1);
  *b = tmp;
  return ASN1_OK;
}

/* --------------------------------------------------------------------- */
/* Inspection                                                             */
/* --------------------------------------------------------------------- */

int asn1_bigint_is_zero(const asn1_bigint_t* b) {
  if (!b) {
    return 1;
  }
  return b->nlimbs == 0;
}

int asn1_bigint_sign(const asn1_bigint_t* b) {
  if (!b) {
    return 0;
  }
  if (b->nlimbs == 0) {
    return 0;
  }
  return b->sign < 0 ? -1 : +1;
}

int asn1_bigint_cmp(const asn1_bigint_t* a, const asn1_bigint_t* b) {
  if (!a || !b) {
    /* Null pointers compare unequal; consistent direction so a > NULL
       yields -1 below (a is non-null → a is "smaller" → -1 means "less"). */
    return (a == b) ? 0 : (a ? -1 : 1);
  }
  const int a_zero = (a->nlimbs == 0);
  const int b_zero = (b->nlimbs == 0);
  if (a_zero && b_zero) {
    return 0;
  }
  if ((a->sign < 0) != (b->sign < 0)) {
    return (a->sign < 0) ? -1 : 1;
  }
  const int mag = bigint_cmp_mag(a, b);
  return (a->sign < 0) ? -mag : mag;
}

asn1_status_t asn1_bigint_to_int64(const asn1_bigint_t* b, int64_t* out) {
  if (!b || !out) {
    return ASN1_ERR_INVALID_ARGUMENT;
  }
  if (b->nlimbs == 0) {
    *out = 0;
    return ASN1_OK;
  }
  if (b->nlimbs > 2u) {
    return ASN1_ERR_LENGTH_OVERFLOW;
  }
  uint64_t mag = b->limbs[0];
  if (b->nlimbs == 2u) {
    mag |= (uint64_t)b->limbs[1] << 32;
  }
  if (b->sign >= 0) {
    if (mag > (uint64_t)INT64_MAX) {
      return ASN1_ERR_LENGTH_OVERFLOW;
    }
    *out = (int64_t)mag;
    return ASN1_OK;
  }
  /* Negative: INT64_MIN has magnitude INT64_MAX+1. */
  const uint64_t min_mag = (uint64_t)INT64_MAX + 1u;
  if (mag > min_mag) {
    return ASN1_ERR_LENGTH_OVERFLOW;
  }
  if (mag == min_mag) {
    *out = INT64_MIN;
    return ASN1_OK;
  }
  *out = -(int64_t)mag;
  return ASN1_OK;
}

/* --------------------------------------------------------------------- */
/* Serialization                                                          */
/* --------------------------------------------------------------------- */

asn1_status_t asn1_bigint_to_bytes(const asn1_bigint_t* b,
                                    uint8_t** out_buf, size_t* out_size) {
  if (!b || !out_buf || !out_size) {
    return ASN1_ERR_INVALID_ARGUMENT;
  }
  if (b->sign < 0) {
    /* Mirrors asn1::BigInteger::to_unsigned_bytes: refuse negative. */
    return ASN1_ERR_CONSTRAINT_VIOLATION;
  }
  return bigint_magnitude_be(b, out_buf, out_size);
}

asn1_status_t asn1_bigint_to_twos_complement(const asn1_bigint_t* b,
                                           uint8_t** out_buf, size_t* out_size) {
  if (!b || !out_buf || !out_size) return ASN1_ERR_INVALID_ARGUMENT;
  uint8_t* magnitude = NULL;
  size_t n = 0;
  asn1_status_t s = bigint_magnitude_be(b, &magnitude, &n);
  if (s != ASN1_OK) return s;
  if (n == SIZE_MAX) { free(magnitude); return ASN1_ERR_LENGTH_OVERFLOW; }
  uint8_t* bytes = (uint8_t*)malloc(n + 1u);
  if (!bytes) { free(magnitude); return ASN1_ERR_OUT_OF_MEMORY; }
  bytes[0] = 0;
  memcpy(bytes + 1u, magnitude, n);
  free(magnitude);
  ++n;
  if (b->sign < 0) {
    unsigned carry = 1;
    for (size_t i = n; i-- > 0;) {
      unsigned v = (unsigned)(uint8_t)~bytes[i] + carry;
      bytes[i] = (uint8_t)v;
      carry = v >> 8;
    }
  }
  size_t start = 0;
  while (start + 1u < n &&
         ((bytes[start] == 0 && !(bytes[start + 1u] & 0x80u)) ||
          (bytes[start] == 0xFF && (bytes[start + 1u] & 0x80u)))) ++start;
  n -= start;
  memmove(bytes, bytes + start, n);
  *out_buf = bytes; *out_size = n;
  return ASN1_OK;
}

asn1_status_t asn1_bigint_init_from_twos_complement(asn1_bigint_t* b,
                                                  asn1_byte_view_t bytes) {
  if (!b || !bytes.data || bytes.size == 0) return ASN1_ERR_INVALID_ARGUMENT;
  const int negative = (bytes.data[0] & 0x80u) != 0;
  if (bytes.size > 1u &&
      ((bytes.data[0] == 0 && !(bytes.data[1] & 0x80u)) ||
       (bytes.data[0] == 0xFF && (bytes.data[1] & 0x80u))))
    return ASN1_ERR_NON_CANONICAL;
  if (bytes.size > (size_t)ASN1_MAX_BIGINT_LIMBS * 4u + 1u)
    return ASN1_ERR_RESOURCE_LIMIT;
  asn1_bigint_t tmp;
  asn1_bigint_init(&tmp);
  /* Decode from the least significant byte, complementing with carry.
     Append a limb only when nonzero: sign-protection bytes cost no limb. */
  unsigned carry = negative ? 1u : 0u;
  uint32_t limb = 0;
  size_t index = 0;
  for (size_t i = bytes.size; i-- > 0; ++index) {
    unsigned v = negative ? (unsigned)(uint8_t)~bytes.data[i] + carry : bytes.data[i];
    carry = negative ? v >> 8 : 0;
    limb |= (uint32_t)(v & 0xFFu) << ((index % 4u) * 8u);
    if (index % 4u == 3u || i == 0) {
      if (limb != 0 || i != 0) {
        asn1_status_t s = bigint_push_limb(&tmp, limb);
        if (s != ASN1_OK) { asn1_bigint_free(&tmp); return s; }
      }
      limb = 0;
    }
  }
  while (tmp.nlimbs && tmp.limbs[tmp.nlimbs - 1u] == 0) --tmp.nlimbs;
  tmp.sign = tmp.nlimbs ? (negative ? -1 : 1) : 0;
  *b = tmp;
  return ASN1_OK;
}

/* Bounded base-2^32 long division; no host floating point or locale. */
asn1_status_t asn1_bigint_to_decimal(const asn1_bigint_t* b,char** out,size_t* size) {
  if(!b||!out||!size||b->nlimbs>b->cap||(b->nlimbs&&(!b->limbs||!b->limbs[b->nlimbs-1]||(b->sign!=1&&b->sign!=-1)))||(!b->nlimbs&&b->sign))return ASN1_ERR_INVALID_ARGUMENT;
  if(b->nlimbs>ASN1_MAX_BIGINT_LIMBS)return ASN1_ERR_RESOURCE_LIMIT;
  const size_t capacity=b->nlimbs*10u+3u;char* text=(char*)malloc(capacity);if(!text)return ASN1_ERR_OUT_OF_MEMORY;
  if(!b->nlimbs){text[0]='0';text[1]=0;*out=text;*size=1;return ASN1_OK;}
  uint32_t* words=(uint32_t*)malloc(b->nlimbs*sizeof(*words));if(!words){free(text);return ASN1_ERR_OUT_OF_MEMORY;}
  memcpy(words,b->limbs,b->nlimbs*sizeof(*words));size_t n=b->nlimbs,used=0;
  while(n){uint64_t remainder=0;for(size_t i=n;i-- >0;){const uint64_t value=(remainder<<32)|words[i];words[i]=(uint32_t)(value/10u);remainder=value%10u;}text[used++]=(char)('0'+remainder);while(n&&!words[n-1])--n;}
  if(b->sign<0)text[used++]='-';for(size_t i=0;i<used/2u;++i){char c=text[i];text[i]=text[used-1-i];text[used-1-i]=c;}text[used]=0;free(words);*out=text;*size=used;return ASN1_OK;
}
