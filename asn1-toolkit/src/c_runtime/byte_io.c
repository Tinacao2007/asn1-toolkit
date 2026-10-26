/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/src/c_runtime/byte_io.c
**   $Version: 0.1
**   $Date:   2026-10-17
**   $Author: tina.cao
***************************************************************************/
/**  File Description:

    P2-4 (--lang c) byte reader bodies.

    C11 port of asn1::ByteReader (asn1-toolkit/src/runtime/byte_io.cpp).
    The C runtime's byte reader owns a heap copy of the source buffer
    (mirroring the C++ owning-ctor overload that keeps the bytes alive
    across function boundaries; the C++ borrowing ctor's lifetime
    reasoning does not survive a C ABI translation).

    Sub-task 2 surface: create / destroy. The byte-aligned read helpers
    are deferred — bit_io's get_octet / get_octets already cover the
    octet-aligned PER / BER decode path, and the dedicated byte reader
    is only consumed by CER indefinite-length parsing (sub-task 14+).

    Specification: Internal byte-aligned decode helpers
    Design Spec:   asn1-toolkit/include/asn1/c_runtime/byte_io.h
***************************************************************************/

#include <asn1/c_runtime/byte_io.h>
#include <asn1/c_runtime/limits.h>

#include <stdlib.h>
#include "alloc_internal.h"
#include <string.h>

struct asn1_byte_reader {
  uint8_t* buf;   /* owned copy of source */
  size_t   size;
  size_t   pos;   /* byte cursor */
};

asn1_status_t asn1_byte_reader_create(asn1_byte_reader_t** out,
                                      asn1_byte_view_t src) {
  if (!out || (!src.data && src.size != 0u)) {
    return ASN1_ERR_INVALID_ARGUMENT;
  }
  if (src.size > ASN1_MAX_PDU_ALLOCATION_BYTES) return ASN1_ERR_RESOURCE_LIMIT;
  asn1_byte_reader_t* r = (asn1_byte_reader_t*)calloc(1, sizeof(*r));
  if (!r) {
    return ASN1_ERR_OUT_OF_MEMORY;
  }
  if (src.size > 0 && src.data) {
    r->buf = (uint8_t*)malloc(src.size);
    if (!r->buf) {
      free(r);
      return ASN1_ERR_OUT_OF_MEMORY;
    }
    memcpy(r->buf, src.data, src.size);
    r->size = src.size;
  }
  *out = r;
  return ASN1_OK;
}

void asn1_byte_reader_destroy(asn1_byte_reader_t* r) {
  if (!r) {
    return;
  }
  free(r->buf);
  free(r);
}

size_t asn1_byte_reader_position(const asn1_byte_reader_t* r) { return r ? r->pos : 0; }
size_t asn1_byte_reader_remaining(const asn1_byte_reader_t* r) { return r ? r->size-r->pos : 0; }
asn1_byte_view_t asn1_byte_reader_view(const asn1_byte_reader_t* r) {
  return asn1_byte_view(r && r->buf ? r->buf+r->pos : NULL, asn1_byte_reader_remaining(r));
}
asn1_status_t asn1_byte_reader_read(asn1_byte_reader_t* r,size_t count,asn1_byte_view_t* out) {
  if(!r || !out) return ASN1_ERR_INVALID_ARGUMENT;
  if(count>r->size-r->pos) return ASN1_ERR_TRUNCATED;
  *out=asn1_byte_view(r->buf ? r->buf+r->pos : NULL,count);r->pos+=count;return ASN1_OK;
}
asn1_status_t asn1_byte_reader_skip(asn1_byte_reader_t* r,size_t count) {
  asn1_byte_view_t ignored;return asn1_byte_reader_read(r,count,&ignored);
}
struct asn1_byte_writer { uint8_t* data; size_t size,capacity; };
asn1_status_t asn1_byte_writer_create(asn1_byte_writer_t** out) {
  if(!out)return ASN1_ERR_INVALID_ARGUMENT;
  asn1_byte_writer_t* w=(asn1_byte_writer_t*)calloc(1,sizeof(*w));
  if(!w)return ASN1_ERR_OUT_OF_MEMORY;*out=w;return ASN1_OK;
}
void asn1_byte_writer_destroy(asn1_byte_writer_t* w) { if(w){free(w->data);free(w);} }
asn1_byte_view_t asn1_byte_writer_view(const asn1_byte_writer_t* w) {
  return asn1_byte_view(w?w->data:NULL,w?w->size:0);
}
asn1_status_t asn1_byte_writer_append(asn1_byte_writer_t* w,asn1_byte_view_t bytes) {
  if(!w || (bytes.size&&!bytes.data))return ASN1_ERR_INVALID_ARGUMENT;
  if(bytes.size>ASN1_MAX_PDU_ALLOCATION_BYTES || w->size>ASN1_MAX_PDU_ALLOCATION_BYTES-bytes.size)return ASN1_ERR_RESOURCE_LIMIT;
  const uintptr_t source=(uintptr_t)bytes.data,base=(uintptr_t)w->data;
  const int self=bytes.size && w->data && source>=base && source-base<w->size;
  const size_t offset=self?(size_t)(source-base):0;
  if(self && bytes.size>w->size-offset)return ASN1_ERR_INVALID_ARGUMENT;
  if(bytes.size>w->capacity-w->size) {
    size_t capacity=w->capacity?w->capacity:(ASN1_MAX_PDU_ALLOCATION_BYTES<64u?ASN1_MAX_PDU_ALLOCATION_BYTES:64u);
    while(capacity-w->size<bytes.size) {
      if(capacity>ASN1_MAX_PDU_ALLOCATION_BYTES/2u){capacity=ASN1_MAX_PDU_ALLOCATION_BYTES;break;}
      capacity*=2u;
    }
    uint8_t* p=(uint8_t*)realloc(w->data,capacity);
    if(!p)return ASN1_ERR_OUT_OF_MEMORY;w->data=p;w->capacity=capacity;
  }
  if(bytes.size)memmove(w->data+w->size,self?w->data+offset:bytes.data,bytes.size);
  w->size+=bytes.size;return ASN1_OK;
}
asn1_status_t asn1_byte_writer_take(asn1_byte_writer_t* w,uint8_t** out,size_t* size) {
  if(!w||!out||!size)return ASN1_ERR_INVALID_ARGUMENT;
  *out=w->data;*size=w->size;w->data=NULL;w->size=w->capacity=0;return ASN1_OK;
}
