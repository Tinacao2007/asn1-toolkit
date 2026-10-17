/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/include/asn1/c_runtime/cer.h
**   $Version: 0.1
**   $Date:   2026-10-17
**   $Author: tina.cao
***************************************************************************/
/**  File Description:

    P2-4 (--lang c) Canonical Encoding Rules (X.690 9).

    C11 port of asn1::cer:: codec surface. CER is BER + canonical SET
    ordering (X.690 9.3) + indefinite-length form for constructed
    types (9.1) + 1000-octet fragmentation for constructed OCTET STRING
    (9.2 + 8.6.4) + last-octet zero-pad for BIT STRING (11.2).

    No external CER oracle exists (asn1tools / asn1c lack CER); the C
    runtime's correctness is established via "条文推导 + DER 值等价"
    cross-validation, same as the C++ runtime (see status.md known
    limitations entry 4).

    Sub-task 1 = interface skeleton only.

    Design Spec: asn1-toolkit/docs/todo.md#p2-4--c-语言代码生成器-lang-c
***************************************************************************/
#ifndef ASN1_C_CER_H
#define ASN1_C_CER_H

#include <asn1/c_runtime/ber.h>

#ifdef __cplusplus
extern "C" {
#endif

/* TODO sub-task 2: asn1_cer_encode_<Type> / asn1_cer_decode_<Type>
   per generated stub; CER-specific indefinite-length and 1000-octet
   chunk helpers. */

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* ASN1_C_CER_H */