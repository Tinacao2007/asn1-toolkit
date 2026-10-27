/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/src/c_runtime/c_common.c
**   $Version: 0.1
**   $Date:   2026-10-17
**   $Author: tina.cao
***************************************************************************/
/**  File Description:

    P2-4 (--lang c) shared C11 runtime bodies for the asn1_c_common.h
    declarations: status code names + byte view / byte buf constructors.

    No bodies for asn1_bigint_t here — that struct is fully declared in
    the header (sign + limbs + cap) and the lifecycle / arithmetic
    operations live in bigint.c.

    Specification: asn1-toolkit/docs/todo.md#p2-4--c-语言代码生成器-lang-c
    Design Spec:   asn1-toolkit/include/asn1/c_runtime/asn1_c_common.h
***************************************************************************/

#include <asn1/c_runtime/asn1_c_common.h>

#include <stddef.h>

/* --------------------------------------------------------------------- */
/* Status code names                                                     */
/* --------------------------------------------------------------------- */

const char* asn1_status_name(asn1_status_t s) {
  switch (s) {
    case ASN1_ERR_OUT_OF_MEMORY: return "ASN1_ERR_OUT_OF_MEMORY";
    case ASN1_ERR_RESOURCE_LIMIT: return "ASN1_ERR_RESOURCE_LIMIT";
    case ASN1_OK: return "ASN1_OK";
    case ASN1_ERR_TRUNCATED: return "ASN1_ERR_TRUNCATED";
    case ASN1_ERR_TAG_MISMATCH: return "ASN1_ERR_TAG_MISMATCH";
    case ASN1_ERR_LENGTH_OVERFLOW: return "ASN1_ERR_LENGTH_OVERFLOW";
    case ASN1_ERR_CONSTRAINT_VIOLATION: return "ASN1_ERR_CONSTRAINT_VIOLATION";
    case ASN1_ERR_NON_CANONICAL: return "ASN1_ERR_NON_CANONICAL";
    case ASN1_ERR_INVALID_ARGUMENT: return "ASN1_ERR_INVALID_ARGUMENT";
    case ASN1_ERR_UNSUPPORTED: return "ASN1_ERR_UNSUPPORTED";
  }
  /* Out-of-range enum value: be loud, not silent. */
  return "ASN1_ERR_INVALID_ARGUMENT";
}

/* --------------------------------------------------------------------- */
/* Byte view / byte buf constructors                                     */
/* --------------------------------------------------------------------- */

asn1_byte_view_t asn1_byte_view(const uint8_t* data, size_t size) {
  asn1_byte_view_t v;
  v.data = data;
  v.size = size;
  return v;
}

asn1_byte_buf_t asn1_byte_buf(uint8_t* data, size_t size) {
  asn1_byte_buf_t b;
  b.data = data;
  b.size = size;
  return b;
}
