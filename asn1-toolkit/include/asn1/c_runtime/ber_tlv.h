/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/include/asn1/c_runtime/ber_tlv.h
**   $Version: 0.1
**   $Date:   2026-10-17
**   $Author: tina.cao
***************************************************************************/
/**  File Description:

    P2-4 (--lang c) BER TLV tag / length helpers (X.690 8).

    C11 port of asn1::ber::tlv_tag / tlv_length helpers. Sub-task 1
    declares the surface; sub-task 2 fills the bodies.

    Design Spec: asn1-toolkit/docs/todo.md#p2-4--c-语言代码生成器-lang-c
***************************************************************************/
#ifndef ASN1_C_BER_TLV_H
#define ASN1_C_BER_TLV_H

#include <asn1/c_runtime/asn1_c_common.h>

#ifdef __cplusplus
extern "C" {
#endif

/* X.690 8.1.2 universal tag triple (class, constructed, number). */
typedef struct {
    uint8_t cls;            /* 0..3 */
    uint8_t constructed;    /* 0 or 1 */
    uint32_t number;        /* 0..2^31-1 */
} asn1_ber_tag_t;

/* Write / read the tag octets. `out_buf` for write must have at least
   4 bytes capacity (worst case: 0x7f long form). */
asn1_status_t asn1_ber_tag_encode(uint8_t* out_buf, size_t out_cap,
                                  const asn1_ber_tag_t* tag, size_t* out_size);
asn1_status_t asn1_ber_tag_decode(asn1_byte_view_t src,
                                  asn1_ber_tag_t* out_tag,
                                  size_t* out_bytes_consumed);

/* X.690 8.1.3 / 8.1.4 length octets. */
asn1_status_t asn1_ber_length_encode(uint8_t* out_buf, size_t out_cap,
                                     size_t length, size_t* out_size);
asn1_status_t asn1_ber_length_decode(asn1_byte_view_t src,
                                     size_t* out_length,
                                     size_t* out_bytes_consumed);

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* ASN1_C_BER_TLV_H */