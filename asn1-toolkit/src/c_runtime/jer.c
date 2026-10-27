/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/src/c_runtime/jer.c
**   $Version: 0.1
**   $Date:   2026-10-17
**   $Author: tina.cao
***************************************************************************/
/**  File Description:

    P2-4 (--lang c) JER (X.697) writer / reader bodies.

    The C runtime surfaces two helpers — `asn1_jer_writer_*` (writes
    into a caller-provided buffer) and `asn1_jer_reader_*` (cursor over
    a borrowed string). JER/XER per-type stubs are emitted by emit_c
    in sub-task 3+, so the writer's role is just "copy this JSON snippet
    into the output buffer with overflow detection".

    Sub-task 2 surface (matches jer.h declarations):

      asn1_jer_writer_init    — bind writer to (buf, cap)
      asn1_jer_writer_puts    — append a NUL-terminated string
      asn1_jer_writer_printf  — append a printf-formatted fragment
      asn1_jer_reader_init    — bind reader to (data, size)

    Specification: ITU-T X.697 — JSON Encoding Rules
    Design Spec:   asn1-toolkit/include/asn1/c_runtime/jer.h
***************************************************************************/

#include <asn1/c_runtime/jer.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

asn1_status_t asn1_jer_writer_init(asn1_jer_writer_t* w,
                                    char* buf, size_t cap) {
  if (!w || (!buf && cap > 0u)) {
    return ASN1_ERR_INVALID_ARGUMENT;
  }
  w->buf = buf;
  w->cap = cap;
  w->used = 0;
  return ASN1_OK;
}

asn1_status_t asn1_jer_writer_puts(asn1_jer_writer_t* w, const char* s) {
  if (!w || !s) {
    return ASN1_ERR_INVALID_ARGUMENT;
  }
  const size_t n = strlen(s);
  /* Need room for n data bytes; trailing NUL written only if space
     remains (a caller-sized buffer that fits the string exactly does
     NOT get a NUL — matches the C++ runtime's writer behavior). */
  if (w->used > w->cap || n > w->cap - w->used) {
    return ASN1_ERR_LENGTH_OVERFLOW;
  }
  if (n > 0u) {
    memcpy(w->buf + w->used, s, n);
  }
  w->used += n;
  if (w->used < w->cap) {
    w->buf[w->used] = '\0';
  }
  return ASN1_OK;
}

asn1_status_t asn1_jer_writer_printf(asn1_jer_writer_t* w, const char* fmt, ...) {
  if (!w || !fmt) {
    return ASN1_ERR_INVALID_ARGUMENT;
  }
  if (w->used > w->cap || w->cap == 0u) {
    return ASN1_ERR_LENGTH_OVERFLOW;
  }
  const size_t avail = w->cap - w->used;
  if (avail == 0u) {
    return ASN1_ERR_LENGTH_OVERFLOW;
  }
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(w->buf + w->used, avail, fmt, ap);
  va_end(ap);
  if (n < 0) {
    return ASN1_ERR_INVALID_ARGUMENT;
  }
  if ((size_t)n >= avail) {
    /* Truncated: roll back the partial write so the next put starts at
       the same offset (vsnprintf wrote what fit then NUL). */
    w->buf[w->used] = '\0';
    return ASN1_ERR_LENGTH_OVERFLOW;
  }
  w->used += (size_t)n;
  return ASN1_OK;
}

asn1_status_t asn1_jer_reader_init(asn1_jer_reader_t* r,
                                    const char* data, size_t size) {
  if (!r || (!data && size != 0u)) {
    return ASN1_ERR_INVALID_ARGUMENT;
  }
  r->data = data;
  r->pos = 0;
  r->size = size;
  return ASN1_OK;
}
