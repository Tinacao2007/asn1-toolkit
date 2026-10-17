/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/include/asn1/c_runtime/ber.h
**   $Version: 0.1
**   $Date:   2026-10-17
**   $Author: tina.cao
***************************************************************************/
/**  File Description:

    P2-4 (--lang c) Basic Encoding Rules (X.690) entry points.

    C11 port of asn1::ber:: codec surface. BER is the superset; DER/CER
    are the canonical restrictions, declared in der.h / cer.h.

    Naming: asn1_ber_encode_<Type> / asn1_ber_decode_<Type>. Per-type
    wrappers are emitted by emit_c (sub-task 3).

    Sub-task 1 = interface skeleton only.

    Design Spec: asn1-toolkit/docs/todo.md#p2-4--c-语言代码生成器-lang-c
***************************************************************************/
#ifndef ASN1_C_BER_H
#define ASN1_C_BER_H

#include <asn1/c_runtime/asn1_c_common.h>
#include <asn1/c_runtime/ber_tlv.h>
#include <asn1/c_runtime/bigint.h>
#include <asn1/c_runtime/bit_string.h>
#include <asn1/c_runtime/byte_io.h>

#ifdef __cplusplus
extern "C" {
#endif

/* TODO sub-task 2: asn1_ber_encode_bool / decode_bool, INTEGER
   (unconstrained + tagged), BIT STRING (with trailing-zero handling
   per X.690 8.6.3 — note BER is non-canonical so trailing zeros are
   preserved on encode), OCTET STRING, NULL, OBJECT IDENTIFIER,
   ENUMERATED, SEQUENCE / SEQUENCE OF / SET / SET OF (with the
   untagged-CHOICE dispatch path the C++ runtime handles in
   src/runtime/ber/codec.cpp — see asn1-codegen-verification skill
   reference for the trap). */

/* BER has no canonical SET ordering (X.690 8.14); DER/CER enforce it.
   `asn1_ber_set_sort_cmp` is exposed for the caller to override the
   comparison function used during SET / SET OF decode-ordering, but
   the BER path passes NULL (preserve insertion order). The DER/CER
   paths pass their own canonical comparator (sub-task 2). */
typedef int (*asn1_ber_set_cmp_fn)(const void* a, const void* b);

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* ASN1_C_BER_H */