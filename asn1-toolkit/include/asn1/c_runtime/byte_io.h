/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/include/asn1/c_runtime/byte_io.h
**   $Version: 0.1
**   $Date:   2026-10-17
**   $Author: tina.cao
***************************************************************************/
/**  File Description:

    P2-4 (--lang c) byte-level I/O and Result helper macros.

    C11 port of asn1-toolkit/include/asn1/runtime/byte_io.hpp. Decoders
    return asn1_status_t (defined in asn1_c_common.h); this header
    provides the helper macro ASN1_RETURN_IF_ERROR(expr) so codec
    implementation code can write a one-liner early-return without an
    explicit temporary or goto on every decode step.

    Design Spec: asn1-toolkit/docs/todo.md anchor p2-4-c-lang (see
    P2-4 sub-task 1 entry)
***************************************************************************/
#ifndef ASN1_C_BYTE_IO_H
#define ASN1_C_BYTE_IO_H

#include <asn1/c_runtime/asn1_c_common.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Early-return helper. Use only inside a function whose return type is
   asn1_status_t and which has a label named `fail` (the C port uses
   a do-while-0 / explicit return-value dance to avoid GCC's
   "jump skips variable initialization" warning). Sub-task 2 will
   flesh out the canonical pattern. */
#define ASN1_RETURN_IF_ERROR(expr) \
  do { \
    asn1_status_t _s = (expr); \
    if (_s != ASN1_OK) return _s; \
  } while (0)

/* Decode-time byte reader (wraps a bit reader on octet-aligned
   sub-streams). Lean stub in sub-task 1; sub-task 2 will mirror
   asn1::byte_io's full surface. */
typedef struct asn1_byte_reader asn1_byte_reader_t;

asn1_status_t asn1_byte_reader_create(asn1_byte_reader_t** out,
                                      asn1_byte_view_t src);
void          asn1_byte_reader_destroy(asn1_byte_reader_t* r);

/* TODO sub-task 2: byte-aligned read helpers for BER/DER/CER. */

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* ASN1_C_BYTE_IO_H */