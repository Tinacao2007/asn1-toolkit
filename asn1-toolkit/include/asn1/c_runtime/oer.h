/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/include/asn1/c_runtime/oer.h
**   $Version: 0.1
**   $Date:   2026-10-17
**   $Author: tina.cao
***************************************************************************/
/**  File Description:

    P2-4 (--lang c) Octet Encoding Rules (X.696) entry points.

    C11 port of asn1::oer:: codec surface. OER uses tag-preamble bit
    patterns (rather than explicit tag octets like BER) and length
    determinants that vary by type (X.696 8.7).

    Sub-task 1 = interface skeleton only.

    Design Spec: asn1-toolkit/docs/todo.md#p2-4--c-语言代码生成器-lang-c
***************************************************************************/
#ifndef ASN1_C_OER_H
#define ASN1_C_OER_H

#include <asn1/c_runtime/asn1_c_common.h>
#include <asn1/c_runtime/bigint.h>
#include <asn1/c_runtime/bit_string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* TODO sub-task 2: asn1_oer_encode_<Type> / asn1_oer_decode_<Type> per
   generated stub; preamble-bit helpers (per X.696 8.7 / 20.1); OER
   SET component canonical ordering (X.696 18.2 — same as the C++
   runtime's P1-1 fix). */

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* ASN1_C_OER_H */