/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/include/asn1/c_runtime/der.h
**   $Version: 0.1
**   $Date:   2026-10-17
**   $Author: tina.cao
***************************************************************************/
/**  File Description:

    P2-4 (--lang c) Distinguished Encoding Rules (X.690 11).

    C11 port of asn1::der:: codec surface. DER is BER + canonical SET
    ordering (X.690 11.6) + minimal-length INTEGER (11.3) +
    minimal-length BIT STRING (11.5) + definitive length form (11.2).

    The C port reuses asn1_ber_* primitives where possible (BER is
    a superset of DER) and adds a thin DER-specific correctness check
    layer for the canonical rules. The decoder rejects non-canonical
    encodings with ASN1_ERR_NON_CANONICAL.

    CGEN batch 5 = shared policy-based TLV runtime.

    Design Spec: asn1-toolkit/docs/todo.md#p2-4--c-语言代码生成器-lang-c
***************************************************************************/
#ifndef ASN1_C_DER_H
#define ASN1_C_DER_H

#include <asn1/c_runtime/ber.h>
#include <asn1/c_runtime/tlv_schema.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Generated entry points use asn1_tlv_schema_encode/decode with their
   own ASN1_TLV_DER policy. See tlv.h and tlv_schema.h for the
   shared implementation, ownership and transactional decode contract. */

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* ASN1_C_DER_H */
