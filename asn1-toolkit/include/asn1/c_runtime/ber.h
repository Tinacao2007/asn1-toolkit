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

    Generated naming: <qualified-Type>_encode_ber / _decode_ber.
    Shared value/schema operations are declared in tlv.h / tlv_schema.h.

    CGEN batch 5 = shared policy-based TLV runtime.

    Design Spec: asn1-toolkit/docs/todo.md#p2-4--c-语言代码生成器-lang-c
***************************************************************************/
#ifndef ASN1_C_BER_H
#define ASN1_C_BER_H

#include <asn1/c_runtime/asn1_c_common.h>
#include <asn1/c_runtime/ber_tlv.h>
#include <asn1/c_runtime/tlv_schema.h>
#include <asn1/c_runtime/bigint.h>
#include <asn1/c_runtime/bit_string.h>
#include <asn1/c_runtime/byte_io.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Generated entry points use asn1_tlv_schema_encode/decode with their
   own ASN1_TLV_BER policy. See tlv.h and tlv_schema.h for the
   shared implementation, ownership and transactional decode contract. */

/* Legacy comparator typedef retained for source compatibility. Generated
   BER keeps declaration order; DER/CER schema policies choose their own
   standard-specific SET / SET OF order internally. */
typedef int (*asn1_ber_set_cmp_fn)(const void* a, const void* b);

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* ASN1_C_BER_H */
