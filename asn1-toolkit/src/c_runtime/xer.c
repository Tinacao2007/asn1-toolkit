/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/src/c_runtime/xer.c
**   $Version: 0.1
**   $Date:   2026-10-17
**   $Author: tina.cao
***************************************************************************/
/**  File Description:

    P2-4 (--lang c) XER (X.693) writer / reader bodies.

    XER shapes are the same as JER: caller-provided buffer + cursor +
    size. Sub-task 2 mirrors jer.c for the bare writer/reader surface
    declared in xer.h.

    Sub-task 2 surface:
      asn1_xer_writer_init / puts
      asn1_xer_reader_init

    (The XER-specific element-name + attribute + whitespace helpers
    land in a later sub-task alongside the per-type emit_c stubs.)

    Specification: ITU-T X.693 — XML Encoding Rules
    Design Spec:   asn1-toolkit/include/asn1/c_runtime/xer.h
***************************************************************************/

#include <asn1/c_runtime/xer.h>

#include <string.h>

asn1_status_t asn1_xer_writer_init(asn1_xer_writer_t* w,
                                    char* buf, size_t cap) {
  if (!w || (!buf && cap > 0u)) {
    return ASN1_ERR_INVALID_ARGUMENT;
  }
  w->buf = buf;
  w->cap = cap;
  w->used = 0;
  return ASN1_OK;
}

asn1_status_t asn1_xer_writer_puts(asn1_xer_writer_t* w, const char* s) {
  if (!w || !s) {
    return ASN1_ERR_INVALID_ARGUMENT;
  }
  const size_t n = strlen(s);
  /* Need room for n data bytes; the trailing NUL is written only if
     space remains (a caller-sized buffer that fits the string exactly
     does NOT get a NUL — matches the C++ runtime's writer behavior). */
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

asn1_status_t asn1_xer_reader_init(asn1_xer_reader_t* r,
                                    const char* data, size_t size) {
  if (!r || (!data && size != 0u)) {
    return ASN1_ERR_INVALID_ARGUMENT;
  }
  r->data = data;
  r->pos = 0;
  r->size = size;
  return ASN1_OK;
}
