/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/include/asn1/c_runtime/asn1_c_common.h
**   $Version: 0.1
**   $Date:   2026-10-17
**   $Author: tina.cao
***************************************************************************/
/**  File Description:

    P2-4 (--lang c) shared C11 runtime types and status codes.

    Companion to asn1-toolkit/include/asn1/common/{result,span}.hpp (C++).
    Pure C11, no C++ dependencies, no <optional> <span> <vector>. Every
    function returning a value uses an explicit out parameter; failure
    paths return an asn1_status_t integer (0 = Ok).

    Conventions (P2-4 sub-task 1 interface skeleton — implementation
    arrives in sub-task 2):

      - prefix `asn1_<codec>_` (e.g. asn1_per_, asn1_ber_, asn1_jer_)
        replaces the C++ `namespace asn1::<codec>::` block. No nested
        namespaces — flat prefix is more idiomatic for C.
      - all buffers are `(const uint8_t* data, size_t size)` pairs; the
        older C++ `Span<const uint8_t>` collapses into two args at call
        sites (mirrors what asn1c / libasn1 do).
      - all decode functions return an asn1_status_t (0 = Ok, non-zero =
        the asn1_error_code_t value). Out parameters are populated only
        when the return is ASN1_OK.
      - encode functions return bytes-written via `size_t*` out param
        (or, for fixed-size outputs, the buffer pointer).
      - all multi-byte int fields are stored little-endian on the wire
        for BER/DER/CER/OER families; PER families use MSB-first per
        X.691 — see codec-specific headers.

    Why a parallel runtime (not extern "C" wrapping the C++ one):
    the C++ runtime uses std::optional, std::span, std::variant, and a
    self-defined BigInteger backed by std::vector<uint32_t> — none of
    which can be exposed across a C ABI without forcing every C user to
    pull in a C++ stdlib symbol or carry a C++ exception personality.
    A clean C11 rewrite is small enough (~1k LOC across all 5 codec
    families per the existing per-family files) and gives us a true
    `gcc -std=c11 -Wall -Wextra -c` build that any C consumer can drop
    into their project.

    Layout of the rest of asn1/c_runtime/:

      asn1_c_common.h     — this file (status codes, byte_span, bigint)
      bit_io.h            — bit reader / writer (C11 port of bit_io.hpp)
      byte_io.h           — error / span helpers (C11 port of byte_io.hpp)
      bigint.h            — big-integer INTEGER (C11 port of bigint.hpp)
      bit_string.h        — BIT STRING operations (C11 port of bit_string.hpp)
      per.h               — UPER/APER primitives (port of per/codec.hpp + uper.hpp + aper.hpp)
      ber.h               — BER TLV + DER/CER (port of ber/tlv.hpp + ber/codec.hpp + der/codec.hpp + cer/codec.hpp)
      oer.h               — OER/COER (port of oer/codec.hpp + coer/codec.hpp)
      jer.h               — JER/JERI (port of jer/codec.hpp + jeri/codec.hpp)
      xer.h               — XER/EXER/CXER (port of xer/codec.hpp + exer/codec.hpp + cxer/codec.hpp)

    Implementation arrives in P2-4 sub-task 2. Until then, every header
    in asn1/c_runtime/ is a pure interface skeleton: types + function
    declarations + doc comments. No function bodies — linking this
    library today is not meaningful; the goal of sub-task 1 is to lock
    down the surface area so sub-task 2 can fill it in without breaking
    callers.

    Specification: ITU-T X.680 / X.681 / X.683 / X.690 / X.691 / X.692 /
                    X.693 / X.694 / X.695 / X.696 (any subset covered by
                    the parallel C++ runtime today).
    Design Spec:   asn1-toolkit/docs/todo.md#p2-4--c-语言代码生成器-lang-c
***************************************************************************/
#ifndef ASN1_C_COMMON_H
#define ASN1_C_COMMON_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --------------------------------------------------------------------- */
/* Status / error codes (mirrors asn1::Error::Code)                      */
/* --------------------------------------------------------------------- */

typedef enum {
  ASN1_OK = 0,
  ASN1_ERR_TRUNCATED = 1,           /* X.690/X.691 input ran out before done */
  ASN1_ERR_TAG_MISMATCH = 2,        /* BER/DER/CER tag mismatch */
  ASN1_ERR_LENGTH_OVERFLOW = 3,     /* BER/DER/CER length exceeds limits */
  ASN1_ERR_CONSTRAINT_VIOLATION = 4,/* X.680 subtype constraint failed */
  ASN1_ERR_NON_CANONICAL = 5,       /* CER strictness / DER canonical */
  ASN1_ERR_INVALID_ARGUMENT = 6,    /* NULL out param / bad size / etc */
  ASN1_ERR_UNSUPPORTED = 7,         /* feature not in C runtime (yet) */
} asn1_status_t;

/* Returns the human-readable name of a status code. Stable across
   builds; suitable for log messages and diagnostics. */
const char* asn1_status_name(asn1_status_t s);

/* --------------------------------------------------------------------- */
/* Byte span (replaces asn1::Span<const std::uint8_t>)                   */
/* --------------------------------------------------------------------- */

/* Non-owning view over a contiguous byte buffer. Mirrors the C++17
   std::span API in shape but stays a plain struct so it can be passed
   by value across translation units. */
typedef struct {
  const uint8_t* data;
  size_t size;
} asn1_byte_view_t;

/* Mutable counterpart for encode-time buffers. */
typedef struct {
  uint8_t* data;
  size_t size;
} asn1_byte_buf_t;

/* Constructors (return-by-value so callers can write
   `asn1_byte_view_t v = asn1_byte_view(ptr, n);`). */
asn1_byte_view_t asn1_byte_view(const uint8_t* data, size_t size);
asn1_byte_buf_t asn1_byte_buf(uint8_t* data, size_t size);

/* --------------------------------------------------------------------- */
/* Big integer (replaces asn1::BigInteger)                                */
/* --------------------------------------------------------------------- */

/* Sign-plus-limbs arbitrary-precision integer. Absolute value is
   little-endian base-2^32 limbs; no leading-zero limbs except zero.
   Mirrors asn1::BigInteger's storage layout so per_golden / tlv_golden
   vectors stay byte-equal across C++ / C runtimes. */
typedef struct {
  int sign;        /* 0 = zero, +1 = non-negative, -1 = negative */
  uint32_t* limbs; /* owned; alloc with malloc/free; NULL when nlimbs=0 */
  size_t nlimbs;   /* count of valid limbs */
  size_t cap;      /* capacity of `limbs`; nlimbs <= cap */
} asn1_bigint_t;

/* Lifecycle. asn1_bigint_init sets to zero; asn1_bigint_free releases
   the limbs array and zeros the struct. asn1_bigint_init_from_decimal
   parses an ASCII decimal string (mirrors asn1::BigInt::from_decimal). */
void asn1_bigint_init(asn1_bigint_t* b);
void asn1_bigint_free(asn1_bigint_t* b);
asn1_status_t asn1_bigint_init_from_decimal(asn1_bigint_t* b,
                                            const char* text, int negative);

/* Comparison + arithmetic (full operator set is NOT needed for the C
   runtime. INTEGER encode/decode only requires: cmp vs zero, add small
   constants, multiply by small constants for length computation, and
   cmp for sorted SET components. The set below is the minimum surface
   that the parallel C++ runtime exposes via asn1::BigInteger. Sub-task
   2 will add the rest if needed.)
   (Compare signature is finalized in asn1_c_common.h; do NOT re-declare
   here to avoid conflicting-types errors.) */
int asn1_bigint_sign(const asn1_bigint_t* b);                    /* -1 / 0 / +1 */
/* Three-way compare returning -1 / 0 / +1; no out-param because the
   C ABI makes int returns free. Caller checks the return directly.
   (Mirrors the C++ runtime's compare path: `a < b` etc.) */
int asn1_bigint_cmp(const asn1_bigint_t* a, const asn1_bigint_t* b);
asn1_status_t asn1_bigint_to_int64(const asn1_bigint_t* b, int64_t* out);

/* --------------------------------------------------------------------- */
/* Convenience: NULL-checks for out-param APIs                           */
/* --------------------------------------------------------------------- */

/* Most APIs check `if (!out) return ASN1_ERR_INVALID_ARGUMENT;` at the
   top. Centralized here so codec headers can reference it in doc
   comments without redefining the contract. */

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* ASN1_C_COMMON_H */