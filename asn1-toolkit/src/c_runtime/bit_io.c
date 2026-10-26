/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/src/c_runtime/bit_io.c
**   $Version: 0.1
**   $Date:   2026-10-17
**   $Author: tina.cao
***************************************************************************/
/**  File Description:

    P2-4 (--lang c) MSB-first bit stream reader / writer.

    Direct port of asn1-toolkit/src/runtime/bit_io.cpp (the C++ class
    BitWriter / BitReader). The byte-equivalence to the C++ runtime is
    the test that proves the C runtime wires correctly: emit_c's
    generated stubs call into these helpers, and the resulting bytes
    must match the C++ emitted bytes for the same ASN.1 input.

    Storage:
      BitWriter: heap-owned `buf` (uint8_t*), `bit_count`, `cap`.
      BitReader: heap-owned `buf` (uint8_t*) so a caller-owned buffer
                 is always copied at construction time (the C runtime
                 never borrows; emit-generated stubs cannot reason about
                 C++ lifetimes). `bit_pos` is the read cursor in bits.

    P2-1 bulk bitmap fast path is preserved (pack up to 64 source bits
    into one put_bits call).

    Specification: ITU-T X.691 (PER) bit-level primitives
    Design Spec:   asn1-toolkit/include/asn1/c_runtime/bit_io.h
***************************************************************************/

#include <asn1/c_runtime/bit_io.h>
#include <asn1/c_runtime/limits.h>

#include <stdlib.h>
#include "alloc_internal.h"
#include <string.h>

/* --------------------------------------------------------------------- */
/* Internal: mask low-n bits of a uint64_t (n in [0, 64]).                */
/* --------------------------------------------------------------------- */

static uint64_t mask_bits(size_t n) {
  return n >= 64 ? ~(uint64_t)0 : (((uint64_t)1 << n) - 1u);
}

/* --------------------------------------------------------------------- */
/* BitWriter                                                              */
/* --------------------------------------------------------------------- */

struct asn1_bit_writer {
  uint8_t* buf;
  size_t   cap;        /* byte capacity */
  size_t   bit_count;  /* total bits written */
};

asn1_status_t asn1_bit_writer_create(asn1_bit_writer_t** out) {
  if (!out) {
    return ASN1_ERR_INVALID_ARGUMENT;
  }
  asn1_bit_writer_t* w = (asn1_bit_writer_t*)calloc(1, sizeof(*w));
  if (!w) {
    return ASN1_ERR_OUT_OF_MEMORY;
  }
  *out = w;
  return ASN1_OK;
}

void asn1_bit_writer_destroy(asn1_bit_writer_t* w) {
  if (!w) {
    return;
  }
  free(w->buf);
  free(w);
}

/* Internal: ensure `nbits` more bits can be written. */
static asn1_status_t bit_writer_ensure(asn1_bit_writer_t* w, size_t nbits) {
  if (nbits > SIZE_MAX - w->bit_count) {
    return ASN1_ERR_LENGTH_OVERFLOW;
  }
  const size_t total = w->bit_count + nbits;
  const size_t need = total / 8u + (total % 8u != 0u);
  if (w->cap < need) {
    size_t new_cap = w->cap == 0 ? 16u : w->cap;
    while (new_cap < need) {
      if (new_cap > SIZE_MAX / 2u) {
        new_cap = need;
        break;
      }
      new_cap *= 2u;
    }
    uint8_t* nb = (uint8_t*)realloc(w->buf, new_cap);
    if (!nb) return ASN1_ERR_OUT_OF_MEMORY;
    memset(nb + w->cap, 0, new_cap - w->cap);
    w->buf = nb;
    w->cap = new_cap;
  }
  return ASN1_OK;
}

asn1_status_t asn1_bit_writer_put_bit(asn1_bit_writer_t* w, int bit) {
  if (!w) {
    return ASN1_ERR_INVALID_ARGUMENT;
  }
  const size_t byte_index = w->bit_count >> 3;
  asn1_status_t status = bit_writer_ensure(w, 1u);
  if (status != ASN1_OK) return status;
  if (bit) {
    const size_t bit_index = 7u - (w->bit_count & 7u);
    w->buf[byte_index] = (uint8_t)(w->buf[byte_index] | (uint8_t)(1u << bit_index));
  }
  ++w->bit_count;
  return ASN1_OK;
}

asn1_status_t asn1_bit_writer_put_bits(asn1_bit_writer_t* w,
                                       uint64_t value, size_t nbits) {
  if (!w) {
    return ASN1_ERR_INVALID_ARGUMENT;
  }
  if (nbits > 64) {
    return ASN1_ERR_INVALID_ARGUMENT;
  }
  if (nbits == 0) {
    return ASN1_OK;
  }
  asn1_status_t status = bit_writer_ensure(w, nbits);
  if (status != ASN1_OK) return status;
  size_t remaining = nbits;
  size_t bit_off = w->bit_count & 7u;
  size_t byte_index = w->bit_count >> 3;
  /* Partial current octet first (MSB-first). */
  if (bit_off != 0) {
    const size_t fill =
        (8u - bit_off < remaining) ? (8u - bit_off) : remaining;
    const uint64_t top = (value >> (remaining - fill)) & mask_bits(fill);
    w->buf[byte_index] = (uint8_t)(
        w->buf[byte_index] | (uint8_t)(top << (8u - bit_off - fill)));
    remaining -= fill;
    w->bit_count += fill;
    ++byte_index;
  }
  /* Octet-aligned bulk path: top of value goes to current byte. */
  while (remaining >= 8u) {
    w->buf[byte_index] =
        (uint8_t)((value >> (remaining - 8u)) & 0xFFu);
    ++byte_index;
    remaining -= 8u;
    w->bit_count += 8u;
  }
  if (remaining > 0) {
    w->buf[byte_index] =
        (uint8_t)((value & mask_bits(remaining)) << (8u - remaining));
    w->bit_count += remaining;
  }
  return ASN1_OK;
}

asn1_status_t asn1_bit_writer_put_bits_from_bytes(asn1_bit_writer_t* w,
                                                  asn1_byte_view_t bytes,
                                                  size_t nbits) {
  if (!w) {
    return ASN1_ERR_INVALID_ARGUMENT;
  }
  if ((!bytes.data && bytes.size != 0u) ||
      nbits / 8u + (nbits % 8u != 0u) > bytes.size) {
    return ASN1_ERR_INVALID_ARGUMENT;
  }
  asn1_status_t status = bit_writer_ensure(w, nbits);
  if (status != ASN1_OK) return status;
  /* Fast path: octet-aligned writer + whole-octet request copies bytes
     directly; a sub-octet tail lands in a fresh zeroed octet (same shape
     as the C++ runtime). Sub-octet tail needs one extra source byte;
     source size and full destination capacity were checked above. */
  if ((w->bit_count & 7u) == 0u) {
    const size_t whole = nbits / 8u;
    if (whole < bytes.size ||
        (whole == bytes.size && (nbits & 7u) == 0u)) {
      const size_t base = w->bit_count >> 3;
      if (whole > 0 && bytes.data) {
        memcpy(w->buf + base, bytes.data, whole);
      }
      w->bit_count += whole * 8u;
      nbits -= whole * 8u;
      if (nbits == 0u) {
        return ASN1_OK;
      }
      const size_t byte_index = w->bit_count >> 3;
      w->buf[byte_index] = (uint8_t)(bytes.data[whole]
                                   & (uint8_t)(0xFFu << (8u - nbits)));
      w->bit_count += nbits;
      return ASN1_OK;
    }
  }
  /* Per-bit loop (MSB of each byte first). */
  size_t written = 0;
  for (size_t i = 0; i < bytes.size && written < nbits; ++i) {
    for (int b = 7; b >= 0 && written < nbits; --b) {
      asn1_status_t s = asn1_bit_writer_put_bit(w, ((bytes.data[i] >> b) & 1u) != 0);
      if (s != ASN1_OK) {
        return s;
      }
      ++written;
    }
  }
  return ASN1_OK;
}

asn1_status_t asn1_bit_writer_put_bitmap(asn1_bit_writer_t* w,
                                         asn1_byte_view_t bits) {
  if (!w) {
    return ASN1_ERR_INVALID_ARGUMENT;
  }
  if (!bits.data && bits.size != 0u) return ASN1_ERR_INVALID_ARGUMENT;
  asn1_status_t status = bit_writer_ensure(w, bits.size);
  if (status != ASN1_OK) return status;
  /* Pack up to 64 source bits at a time; bulk write matches a per-bit
     loop byte-for-byte (P2-1 fast path). */
  size_t i = 0;
  while (i < bits.size) {
    const size_t n = (64u < bits.size - i) ? 64u : (bits.size - i);
    uint64_t packed = 0;
    for (size_t j = 0; j < n; ++j) {
      packed = (packed << 1) | ((bits.data[i + j] != 0) ? 1u : 0u);
    }
    asn1_status_t s = asn1_bit_writer_put_bits(w, packed, n);
    if (s != ASN1_OK) {
      return s;
    }
    i += n;
  }
  return ASN1_OK;
}

asn1_status_t asn1_bit_writer_put_octet(asn1_bit_writer_t* w, uint8_t octet) {
  return asn1_bit_writer_put_bits(w, octet, 8u);
}

asn1_status_t asn1_bit_writer_put_octets(asn1_bit_writer_t* w,
                                         asn1_byte_view_t bytes) {
  if (!w) {
    return ASN1_ERR_INVALID_ARGUMENT;
  }
  if (!bytes.data && bytes.size != 0u) return ASN1_ERR_INVALID_ARGUMENT;
  if (bytes.size > SIZE_MAX / 8u) return ASN1_ERR_LENGTH_OVERFLOW;
  asn1_status_t status = bit_writer_ensure(w, bytes.size * 8u);
  if (status != ASN1_OK) return status;
  if ((w->bit_count & 7u) == 0u) {
    const size_t base = w->bit_count >> 3;
    if (bytes.size > 0 && bytes.data) {
      memcpy(w->buf + base, bytes.data, bytes.size);
    }
    w->bit_count += bytes.size * 8u;
    return ASN1_OK;
  }
  for (size_t i = 0; i < bytes.size; ++i) {
    asn1_status_t s = asn1_bit_writer_put_octet(w, bytes.data[i]);
    if (s != ASN1_OK) {
      return s;
    }
  }
  return ASN1_OK;
}

asn1_status_t asn1_bit_writer_align_to_octet(asn1_bit_writer_t* w) {
  if (!w) {
    return ASN1_ERR_INVALID_ARGUMENT;
  }
  const size_t rem = w->bit_count % 8u;
  if (rem == 0u) {
    return ASN1_OK;
  }
  return asn1_bit_writer_put_bits(w, 0, 8u - rem);
}

size_t asn1_bit_writer_bit_size(const asn1_bit_writer_t* w) {
  if (!w) {
    return 0;
  }
  return w->bit_count;
}

size_t asn1_bit_writer_byte_size(const asn1_bit_writer_t* w) {
  if (!w) {
    return 0;
  }
  return w->bit_count / 8u + (w->bit_count % 8u != 0u);
}

int asn1_bit_writer_octet_aligned(const asn1_bit_writer_t* w) {
  if (!w) {
    return 1;
  }
  return (w->bit_count & 7u) == 0u;
}

asn1_status_t asn1_bit_writer_take(asn1_bit_writer_t* w,
                                   uint8_t** out_buf, size_t* out_size) {
  if (!w || !out_buf || !out_size) {
    return ASN1_ERR_INVALID_ARGUMENT;
  }
  const size_t n = asn1_bit_writer_byte_size(w);
  uint8_t* out = (uint8_t*)malloc(n == 0 ? 1u : n);
  if (!out) {
    return ASN1_ERR_OUT_OF_MEMORY;
  }
  if (n > 0) {
    memcpy(out, w->buf, n);
  }
  *out_buf = out;
  *out_size = n;
  /* Reset storage as well: put_bit appends with OR. */
  asn1_bit_writer_clear(w);
  return ASN1_OK;
}

void asn1_bit_writer_clear(asn1_bit_writer_t* w) {
  if (!w) {
    return;
  }
  if (w->buf) memset(w->buf, 0, w->cap);
  w->bit_count = 0;
}

/* --------------------------------------------------------------------- */
/* BitReader                                                              */
/* --------------------------------------------------------------------- */

struct asn1_bit_reader {
  uint8_t* buf;     /* owned copy of source */
  size_t   size;    /* byte count */
  size_t   bit_pos; /* read cursor in bits */
  size_t   budget_bytes;
  size_t   budget_elements;
  struct asn1_bit_reader* budget_owner;
};

asn1_status_t asn1_bit_reader_create(asn1_bit_reader_t** out,
                                     asn1_byte_view_t src) {
  if (!out || (!src.data && src.size != 0u)) {
    return ASN1_ERR_INVALID_ARGUMENT;
  }
  if (src.size > SIZE_MAX / 8u) return ASN1_ERR_LENGTH_OVERFLOW;
  asn1_bit_reader_t* r = (asn1_bit_reader_t*)calloc(1, sizeof(*r));
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
  r->budget_owner = r;
  *out = r;
  return ASN1_OK;
}

asn1_status_t asn1_bit_reader_begin_decode(asn1_bit_reader_t* r) {
  if (!r) return ASN1_ERR_INVALID_ARGUMENT;
  asn1_bit_reader_t* owner = r->budget_owner ? r->budget_owner : r;
  if (owner != r) return ASN1_ERR_INVALID_ARGUMENT;
  owner->budget_bytes = 0;
  owner->budget_elements = 0;
  return ASN1_OK;
}
asn1_status_t asn1_bit_reader_share_budget(asn1_bit_reader_t* child,
                                           asn1_bit_reader_t* parent) {
  if (!child || !parent) return ASN1_ERR_INVALID_ARGUMENT;
  child->budget_owner = parent->budget_owner ? parent->budget_owner : parent;
  return ASN1_OK;
}
asn1_status_t asn1_bit_reader_reserve(asn1_bit_reader_t* r,
                                      size_t bytes, size_t elements) {
  if (!r) return ASN1_ERR_INVALID_ARGUMENT;
  asn1_bit_reader_t* owner = r->budget_owner ? r->budget_owner : r;
  if (owner->budget_bytes > ASN1_MAX_PDU_ALLOCATION_BYTES ||
      bytes > ASN1_MAX_PDU_ALLOCATION_BYTES - owner->budget_bytes ||
      owner->budget_elements > ASN1_MAX_PDU_ELEMENTS ||
      elements > ASN1_MAX_PDU_ELEMENTS - owner->budget_elements)
    return ASN1_ERR_RESOURCE_LIMIT;
  owner->budget_bytes += bytes;
  owner->budget_elements += elements;
  return ASN1_OK;
}

void asn1_bit_reader_destroy(asn1_bit_reader_t* r) {
  if (!r) {
    return;
  }
  free(r->buf);
  free(r);
}

size_t asn1_bit_reader_bit_offset(const asn1_bit_reader_t* r) {
  if (!r) {
    return 0;
  }
  return r->bit_pos;
}

size_t asn1_bit_reader_bit_remaining(const asn1_bit_reader_t* r) {
  if (!r) {
    return 0;
  }
  const size_t total = r->size * 8u;
  return r->bit_pos < total ? total - r->bit_pos : 0u;
}

int asn1_bit_reader_octet_aligned(const asn1_bit_reader_t* r) {
  if (!r) {
    return 1;
  }
  return (r->bit_pos & 7u) == 0u;
}

int asn1_bit_reader_eof(const asn1_bit_reader_t* r) {
  if (!r) {
    return 1;
  }
  return asn1_bit_reader_bit_remaining(r) == 0u;
}

asn1_status_t asn1_bit_reader_get_bit(asn1_bit_reader_t* r, int* out_bit) {
  if (!r || !out_bit) {
    return ASN1_ERR_INVALID_ARGUMENT;
  }
  if (asn1_bit_reader_bit_remaining(r) == 0u) {
    return ASN1_ERR_TRUNCATED;
  }
  const size_t byte_index = r->bit_pos >> 3;
  const size_t bit_index = 7u - (r->bit_pos & 7u);
  *out_bit = ((r->buf[byte_index] >> bit_index) & 1u) != 0;
  ++r->bit_pos;
  return ASN1_OK;
}

asn1_status_t asn1_bit_reader_get_bits(asn1_bit_reader_t* r,
                                       size_t nbits, uint64_t* out_value) {
  if (!r || !out_value) {
    return ASN1_ERR_INVALID_ARGUMENT;
  }
  if (nbits > 64u) {
    return ASN1_ERR_INVALID_ARGUMENT;
  }
  if (asn1_bit_reader_bit_remaining(r) < nbits) {
    return ASN1_ERR_TRUNCATED;
  }
  uint64_t value = 0;
  for (size_t i = 0; i < nbits; ++i) {
    int b = 0;
    asn1_status_t s = asn1_bit_reader_get_bit(r, &b);
    if (s != ASN1_OK) {
      return s;
    }
    value = (value << 1) | (b ? 1u : 0u);
  }
  *out_value = value;
  return ASN1_OK;
}

asn1_status_t asn1_bit_reader_get_bits_as_bytes(asn1_bit_reader_t* r,
                                                size_t nbits,
                                                uint8_t** out_buf,
                                                size_t* out_size) {
  if (!r || !out_buf || !out_size) {
    return ASN1_ERR_INVALID_ARGUMENT;
  }
  if (asn1_bit_reader_bit_remaining(r) < nbits) {
    return ASN1_ERR_TRUNCATED;
  }
  const size_t nbytes = nbits / 8u + (nbits % 8u != 0u);
  if (nbytes > ASN1_MAX_STRING_BYTES) return ASN1_ERR_RESOURCE_LIMIT;
  uint8_t* out = (uint8_t*)calloc(nbytes == 0 ? 1u : nbytes, 1);
  if (!out) {
    return ASN1_ERR_OUT_OF_MEMORY;
  }
  for (size_t i = 0; i < nbits; ++i) {
    int b = 0;
    asn1_status_t s = asn1_bit_reader_get_bit(r, &b);
    if (s != ASN1_OK) {
      free(out);
      return s;
    }
    if (b) {
      const size_t byte_index = i >> 3;
      const size_t bit_index = 7u - (i & 7u);
      out[byte_index] = (uint8_t)(out[byte_index] | (uint8_t)(1u << bit_index));
    }
  }
  *out_buf = out;
  *out_size = nbytes;
  return ASN1_OK;
}

asn1_status_t asn1_bit_reader_get_octet(asn1_bit_reader_t* r,
                                        uint8_t* out_octet) {
  if (!r || !out_octet) {
    return ASN1_ERR_INVALID_ARGUMENT;
  }
  uint64_t v = 0;
  asn1_status_t s = asn1_bit_reader_get_bits(r, 8u, &v);
  if (s != ASN1_OK) {
    return s;
  }
  *out_octet = (uint8_t)v;
  return ASN1_OK;
}

asn1_status_t asn1_bit_reader_get_octets(asn1_bit_reader_t* r, size_t n,
                                        uint8_t** out_buf, size_t* out_size) {
  if (!r || !out_buf || !out_size) {
    return ASN1_ERR_INVALID_ARGUMENT;
  }
  if (n > (size_t)ASN1_MAX_STRING_BYTES) {
    return ASN1_ERR_RESOURCE_LIMIT;
  }
  if (n > SIZE_MAX / 8u) return ASN1_ERR_LENGTH_OVERFLOW;
  if (n * 8u > asn1_bit_reader_bit_remaining(r)) return ASN1_ERR_TRUNCATED;
  uint8_t* out = (uint8_t*)malloc(n == 0 ? 1u : n);
  if (!out) {
    return ASN1_ERR_OUT_OF_MEMORY;
  }
  for (size_t i = 0; i < n; ++i) {
    uint8_t oc = 0;
    asn1_status_t s = asn1_bit_reader_get_octet(r, &oc);
    if (s != ASN1_OK) {
      free(out);
      return s;
    }
    out[i] = oc;
  }
  *out_buf = out;
  *out_size = n;
  return ASN1_OK;
}

asn1_status_t asn1_bit_reader_align_to_octet(asn1_bit_reader_t* r) {
  if (!r) {
    return ASN1_ERR_INVALID_ARGUMENT;
  }
  const size_t rem = r->bit_pos & 7u;
  if (rem == 0u) {
    return ASN1_OK;
  }
  uint64_t skip = 0;
  return asn1_bit_reader_get_bits(r, 8u - rem, &skip);
}
