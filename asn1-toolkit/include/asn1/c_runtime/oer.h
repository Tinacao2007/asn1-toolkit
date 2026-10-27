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

    C11 OER primitive surface. Presence preambles describe optional fields;
    CHOICE tags use X.696 8.7 and lengths use X.696 8.6.

    OER/COER primitives; generated structures use schema_codec.h.

    Design Spec: asn1-toolkit/docs/changelog.md#roadmap--p2-4--c-语言代码生成器-lang-c
***************************************************************************/
#ifndef ASN1_C_OER_H
#define ASN1_C_OER_H

#include <asn1/c_runtime/asn1_c_common.h>
#include <asn1/c_runtime/bigint.h>
#include <asn1/c_runtime/bit_string.h>
#include <asn1/c_runtime/byte_io.h>
#include <asn1/c_runtime/ber_tlv.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { ASN1_OER_BASIC, ASN1_OER_CANONICAL } asn1_oer_policy_t;
/* Effective constraint after OER-visible constraint planning. A zero width
   uses length-prefixed integer contents; unsigned_form selects unsigned
   contents. Nonzero width is exactly 1/2/4/8 octets. Schema validation remains
   the caller's job; these helpers reject any value not representable on wire. */
typedef struct { unsigned width; int unsigned_form; } asn1_oer_integer_form_t;
asn1_status_t asn1_oer_encode_length(asn1_byte_writer_t*,size_t);
asn1_status_t asn1_oer_decode_length(asn1_byte_reader_t*,asn1_oer_policy_t,size_t*);
asn1_status_t asn1_oer_encode_boolean(asn1_byte_writer_t*,int);
asn1_status_t asn1_oer_decode_boolean(asn1_byte_reader_t*,asn1_oer_policy_t,int*);
asn1_status_t asn1_oer_encode_integer(asn1_byte_writer_t*,asn1_oer_integer_form_t,const asn1_bigint_t*);
asn1_status_t asn1_oer_decode_integer(asn1_byte_reader_t*,asn1_oer_policy_t,asn1_oer_integer_form_t,asn1_bigint_t*);
asn1_status_t asn1_oer_encode_enumerated(asn1_byte_writer_t*,int64_t);
asn1_status_t asn1_oer_decode_enumerated(asn1_byte_reader_t*,asn1_oer_policy_t,int64_t*);
asn1_status_t asn1_oer_encode_tag(asn1_byte_writer_t*,asn1_ber_tag_t);
asn1_status_t asn1_oer_decode_tag(asn1_byte_reader_t*,asn1_ber_tag_t*);
/* Complete-encoding open payload. Decode returns a borrowed reader view.
   Every primitive is transactional for reader/writer and output parameters.
   Integer destination must be an empty initialized BigInt. */
asn1_status_t asn1_oer_encode_open(asn1_byte_writer_t*,asn1_byte_view_t);
asn1_status_t asn1_oer_decode_open(asn1_byte_reader_t*,asn1_oer_policy_t,asn1_byte_view_t*);

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* ASN1_C_OER_H */
