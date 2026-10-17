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

    Sub-task 1 = interface skeleton only.

    Design Spec: asn1-toolkit/docs/todo.md#p2-4--c-语言代码生成器-lang-c
***************************************************************************/
#ifndef ASN1_C_DER_H
#define ASN1_C_DER_H

#include <asn1/c_runtime/ber.h>

#ifdef __cplusplus
extern "C" {
#endif

/* TODO sub-task 2: asn1_der_encode_<Type> / asn1_der_decode_<Type> per
   generated stub; canonical SET comparator `asn1_der_set_cmp`; INTEGER
   minimal-length validation per X.690 11.3. */

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* ASN1_C_DER_H */