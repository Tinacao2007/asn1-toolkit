/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/include/asn1/c_runtime/jer.h
**   $Version: 0.1
**   $Date:   2026-10-17
**   $Author: tina.cao
***************************************************************************/
/**  File Description:

    P2-4 (--lang c) JSON Encoding Rules (X.697) entry points.

    C11 port of asn1::jer:: codec surface. JER produces JSON, not
    bytes; the C port uses (char* buf, size_t cap) + size_t* written
    out-param rather than bit_io. The C standard library gives us
    snprintf for INTEGER formatting and memcpy for raw-string copy.

    Sub-task 1 = interface skeleton only.

    Design Spec: asn1-toolkit/docs/todo.md#p2-4--c-语言代码生成器-lang-c
***************************************************************************/
#ifndef ASN1_C_JER_H
#define ASN1_C_JER_H

#include <asn1/c_runtime/asn1_c_common.h>
#include <asn1/c_runtime/bigint.h>
#include <asn1/c_runtime/bit_string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Encode into a caller-provided buffer. Returns ASN1_ERR_LENGTH_OVERFLOW
   when the output would exceed cap; on success *out_size is set. */
typedef struct {
    char*  buf;
    size_t cap;
    size_t used;
} asn1_jer_writer_t;

asn1_status_t asn1_jer_writer_init(asn1_jer_writer_t* w,
                                   char* buf, size_t cap);
asn1_status_t asn1_jer_writer_puts(asn1_jer_writer_t* w, const char* s);
asn1_status_t asn1_jer_writer_printf(asn1_jer_writer_t* w, const char* fmt, ...);

/* Decode from a borrowed (or owned) buffer. */
typedef struct {
    const char* data;
    size_t pos;
    size_t size;
} asn1_jer_reader_t;

asn1_status_t asn1_jer_reader_init(asn1_jer_reader_t* r,
                                   const char* data, size_t size);

/* TODO sub-task 2: asn1_jer_encode_<Type> / asn1_jer_decode_<Type> per
   generated stub; JSON token skipping; name-form transform helpers
   (AsIs / Capitalized / Uppercased / Lowercased / Literal). */

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* ASN1_C_JER_H */