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

/* Owns an input copy. NULL data is valid only for size zero. Creation
   leaves *out unchanged on failure; destroy(NULL) is safe, repeated
   destruction of a non-NULL handle is not. */

asn1_status_t asn1_byte_reader_create(asn1_byte_reader_t** out,
                                      asn1_byte_view_t src);
void          asn1_byte_reader_destroy(asn1_byte_reader_t* r);

/* Read/skip are transactional: failure preserves cursor and output. The view
   borrows reader storage and expires when the reader is destroyed. */
size_t asn1_byte_reader_position(const asn1_byte_reader_t* r);
size_t asn1_byte_reader_remaining(const asn1_byte_reader_t* r);
asn1_byte_view_t asn1_byte_reader_view(const asn1_byte_reader_t* r);
asn1_status_t asn1_byte_reader_read(asn1_byte_reader_t* r, size_t count, asn1_byte_view_t* out);
asn1_status_t asn1_byte_reader_skip(asn1_byte_reader_t* r, size_t count);

typedef struct asn1_byte_writer asn1_byte_writer_t;
asn1_status_t asn1_byte_writer_create(asn1_byte_writer_t** out);
void asn1_byte_writer_destroy(asn1_byte_writer_t* w);
asn1_status_t asn1_byte_writer_append(asn1_byte_writer_t* w, asn1_byte_view_t bytes);
asn1_byte_view_t asn1_byte_writer_view(const asn1_byte_writer_t* w);
/* Transfers ownership; use the same C allocator/CRT to free the result. */
asn1_status_t asn1_byte_writer_take(asn1_byte_writer_t* w, uint8_t** out, size_t* size);

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* ASN1_C_BYTE_IO_H */
