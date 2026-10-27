/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/include/asn1/c_runtime/bigint.h
**   $Version: 0.1
**   $Date:   2026-10-17
**   $Author: tina.cao
***************************************************************************/
/**  File Description:

    P2-4 (--lang c) arbitrary-precision INTEGER (the C11 port of
    asn1::BigInteger).

    Sign-plus-limbs storage (mirrors the C++ layout so per_golden /
    tlv_golden vectors stay byte-equal across both runtimes): a `sign`
    field (0 for zero, +1 / -1 otherwise), a heap-allocated `limbs`
    array of uint32_t limbs in little-endian order, and a count.

    Implemented in src/c_runtime/bigint.c; lifecycle and signed/unsigned
    serialization contracts are documented below.

    Design Spec: asn1-toolkit/docs/changelog.md#roadmap--p2-4--c-语言代码生成器-lang-c
***************************************************************************/
#ifndef ASN1_C_BIGINT_H
#define ASN1_C_BIGINT_H

#include <asn1/c_runtime/asn1_c_common.h>
#include <asn1/c_runtime/limits.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Lifecycle. asn1_bigint_init sets to zero (sign=0, limbs=NULL,
   nlimbs=0, cap=0). asn1_bigint_free releases the limbs array and
   zeros the struct (so callers can safely call it again). */
void asn1_bigint_init(asn1_bigint_t* b);
void asn1_bigint_free(asn1_bigint_t* b);

/* Construct from ASCII decimal. With negative==0 a leading '+' or '-'
   is accepted; with negative!=0 supply unsigned digits and force a
   negative result (zero remains unsigned). No whitespace or other chars.
   Destination must be new, zero-initialized, or previously freed, never
   an owning live value. Failure leaves it unchanged; initialize to zero
   first if unconditional free on failure is desired. OOM and limb limit
   return OUT_OF_MEMORY and RESOURCE_LIMIT respectively. */
asn1_status_t asn1_bigint_init_from_decimal(asn1_bigint_t* b,
                                            const char* text,
                                            int negative);

/* Number is zero? non-zero if true. */
int asn1_bigint_is_zero(const asn1_bigint_t* b);

/* Sign: -1 / 0 / +1 (mirrors asn1::BigInteger::sign).
   Signature is defined in asn1_c_common.h to keep a single source of
   truth — this header only adds the rest of the surface. */
int asn1_bigint_sign(const asn1_bigint_t* b);

/* Three-way compare: -1 / 0 / +1. Defined in asn1_c_common.h. */

/* If the value fits in int64, write it and return ASN1_OK; otherwise
   return ASN1_ERR_LENGTH_OVERFLOW (matching the C++ runtime's
   `as_i64` returning std::nullopt semantics). */
asn1_status_t asn1_bigint_to_int64(const asn1_bigint_t* b,
                                   int64_t* out);
/* Exact, locale-independent ASCII decimal. Transfers a malloc-allocated,
   NUL-terminated string; size excludes NUL. Failure preserves both outputs. */
asn1_status_t asn1_bigint_to_decimal(const asn1_bigint_t*,char**,size_t*);

/* Serialize non-negative unsigned magnitude as minimal big-endian octets
   (zero -> {0x00}). No BER tag/length or signed sign-protection byte.
   Caller frees *out_buf with the same C allocator/CRT's free(). Failure
   leaves *out_buf and *out_size unchanged. */
asn1_status_t asn1_bigint_to_bytes(const asn1_bigint_t* b,
                                   uint8_t** out_buf,
                                   size_t* out_size);

/* Signed minimal big-endian two's complement (INTEGER content, not TLV).
   Constructor destination must be new/zero/freed. Empty/redundant signed
   input is rejected. Outputs are unchanged on failure. */
asn1_status_t asn1_bigint_to_twos_complement(const asn1_bigint_t* b,
                                           uint8_t** out_buf, size_t* out_size);
asn1_status_t asn1_bigint_init_from_twos_complement(asn1_bigint_t* b,
                                                  asn1_byte_view_t bytes);

/* TODO sub-task 2: asn1_bigint_cmp_i64 / from_i64 / add_i64 / sub_i64 /
   mul_i64 (NEG 7/8.6.4 super-large split + canonical CER compare use
   cases). The list above is the minimum the C runtime needs to be
   wire-compatible with the C++ runtime for the current per_golden /
   tlv_golden / P1-3 protocol-level vectors. */

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* ASN1_C_BIGINT_H */
