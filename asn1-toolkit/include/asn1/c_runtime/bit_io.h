/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/include/asn1/c_runtime/bit_io.h
**   $Version: 0.1
**   $Date:   2026-10-17
**   $Author: tina.cao
***************************************************************************/
/**  File Description:

    P2-4 (--lang c) bit-level I/O primitives.

    MSB-first bit stream reader / writer, byte-equivalent to
    asn1-toolkit/include/asn1/runtime/bit_io.hpp. Used by the PER family
    (UPER / APER), by BER/DER/CER primitive encoders, and by the C
    port of OER's integer length determinant.

    Conventions (P2-4 sub-task 1 interface skeleton; bodies in sub-task 2):

      - all positions are bit offsets from the start of the buffer.
      - put/get fail with ASN1_ERR_TRUNCATED when the buffer runs out.
      - put_bitmap writes a presence bitmap given as one byte per bit
        (0/1); packed into octets and written in bulk — the same
        P2-1 fast path the C++ runtime uses.
      - the writer's `take()` equivalent is `asn1_bit_writer_take()`
        returning a malloc'd buffer; caller frees with free().

    Design Spec: asn1-toolkit/docs/todo.md#p2-4--c-语言代码生成器-lang-c
***************************************************************************/
#ifndef ASN1_C_BIT_IO_H
#define ASN1_C_BIT_IO_H

#include <asn1/c_runtime/asn1_c_common.h>

#ifdef __cplusplus
extern "C" {
#endif

/* BitWriter: append bits to an internal byte buffer. */
typedef struct asn1_bit_writer asn1_bit_writer_t;

asn1_status_t asn1_bit_writer_create(asn1_bit_writer_t** out);
void          asn1_bit_writer_destroy(asn1_bit_writer_t* w);

asn1_status_t asn1_bit_writer_put_bit(asn1_bit_writer_t* w, int bit);
asn1_status_t asn1_bit_writer_put_bits(asn1_bit_writer_t* w,
                                       uint64_t value, size_t nbits);
asn1_status_t asn1_bit_writer_put_bits_from_bytes(asn1_bit_writer_t* w,
                                                  asn1_byte_view_t bytes,
                                                  size_t nbits);
asn1_status_t asn1_bit_writer_put_bitmap(asn1_bit_writer_t* w,
                                         asn1_byte_view_t bits);
asn1_status_t asn1_bit_writer_put_octet(asn1_bit_writer_t* w, uint8_t octet);
asn1_status_t asn1_bit_writer_put_octets(asn1_bit_writer_t* w,
                                         asn1_byte_view_t bytes);
asn1_status_t asn1_bit_writer_align_to_octet(asn1_bit_writer_t* w);
size_t        asn1_bit_writer_bit_size(const asn1_bit_writer_t* w);
size_t        asn1_bit_writer_byte_size(const asn1_bit_writer_t* w);
int           asn1_bit_writer_octet_aligned(const asn1_bit_writer_t* w);

/* Move out the internal buffer. `*out_buf` receives a malloc'd pointer
   that the caller must free(); `*out_size` receives the byte count
   (rounded up for incomplete trailing octets). After take(), the
   writer is empty and reusable. */
asn1_status_t asn1_bit_writer_take(asn1_bit_writer_t* w,
                                   uint8_t** out_buf, size_t* out_size);
void          asn1_bit_writer_clear(asn1_bit_writer_t* w);

/* BitReader: consume bits from a borrowed byte buffer. */
typedef struct asn1_bit_reader asn1_bit_reader_t;

asn1_status_t asn1_bit_reader_create(asn1_bit_reader_t** out,
                                     asn1_byte_view_t src);
void          asn1_bit_reader_destroy(asn1_bit_reader_t* r);

asn1_status_t asn1_bit_reader_get_bit(asn1_bit_reader_t* r, int* out_bit);
asn1_status_t asn1_bit_reader_get_bits(asn1_bit_reader_t* r,
                                       size_t nbits, uint64_t* out_value);
/* Reads `nbits` and copies them into a freshly allocated byte buffer
   (MSB of bytes[0] = first bit). Caller frees with free(). */
asn1_status_t asn1_bit_reader_get_bits_as_bytes(asn1_bit_reader_t* r,
                                                size_t nbits,
                                                uint8_t** out_buf,
                                                size_t* out_size);
asn1_status_t asn1_bit_reader_get_octet(asn1_bit_reader_t* r,
                                        uint8_t* out_octet);
/* Reads `n` octets into a freshly allocated byte buffer (MSB-first). */
asn1_status_t asn1_bit_reader_get_octets(asn1_bit_reader_t* r, size_t n,
                                        uint8_t** out_buf, size_t* out_size);
asn1_status_t asn1_bit_reader_align_to_octet(asn1_bit_reader_t* r);

size_t        asn1_bit_reader_bit_offset(const asn1_bit_reader_t* r);
size_t        asn1_bit_reader_bit_remaining(const asn1_bit_reader_t* r);
int           asn1_bit_reader_octet_aligned(const asn1_bit_reader_t* r);
int           asn1_bit_reader_eof(const asn1_bit_reader_t* r);

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* ASN1_C_BIT_IO_H */