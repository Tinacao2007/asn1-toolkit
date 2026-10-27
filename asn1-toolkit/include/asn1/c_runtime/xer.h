/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/include/asn1/c_runtime/xer.h
**   $Version: 0.1
**   $Date:   2026-10-17
**   $Author: tina.cao
***************************************************************************/
/**  File Description:

    P2-4 (--lang c) XML Encoding Rules (X.693) entry points.

    C11 port of asn1::xer:: codec surface. XER produces XML; the C
    runtime mirrors JER's (buf, cap, written) shape.

    Legacy bounded text helpers; XML parsing uses text_tree.h and generated XER/CXER/E-XER uses schema_codec.h.

    Design Spec: asn1-toolkit/docs/changelog.md#roadmap--p2-4--c-语言代码生成器-lang-c
***************************************************************************/
#ifndef ASN1_C_XER_H
#define ASN1_C_XER_H

#include <asn1/c_runtime/asn1_c_common.h>
#include <asn1/c_runtime/bigint.h>
#include <asn1/c_runtime/bit_string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The XER writer / reader mirror the JER shape but operate on XML
   fragments. emit_c generates per-type stubs that emit element open /
   close tags around the field-specific payload. */
typedef struct {
    char*  buf;
    size_t cap;
    size_t used;
} asn1_xer_writer_t;

asn1_status_t asn1_xer_writer_init(asn1_xer_writer_t* w,
                                   char* buf, size_t cap);
asn1_status_t asn1_xer_writer_puts(asn1_xer_writer_t* w, const char* s);

typedef struct {
    const char* data;
    size_t pos;
    size_t size;
} asn1_xer_reader_t;

/* Borrows data for its lifetime. NULL is allowed only with size zero.
   init leaves r unchanged on error and does not parse XML. Writer puts
   leaves used and the committed prefix unchanged on error. */

asn1_status_t asn1_xer_reader_init(asn1_xer_reader_t* r,
                                   const char* data, size_t size);

/* TODO sub-task 2: asn1_xer_encode_<Type> / asn1_xer_decode_<Type> per
   generated stub; element-name + attribute + whitespace handling. */

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* ASN1_C_XER_H */
