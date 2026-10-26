/* Shared C11 UPER/APER core. X.691 (2021) clauses 10, 11, 13, 15-19, 23, 30.
 * No compiler/IR or C++ runtime dependency. Legacy wrappers are in per.c. */
#include <asn1/c_runtime/per.h>
#include <asn1/c_runtime/value_content.h>
#include <asn1/c_runtime/limits.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include "alloc_internal.h"

#define TRY(expr) do { asn1_status_t status_ = (expr); if (status_ != ASN1_OK) return status_; } while (0)
static int valid_variant(asn1_per_variant_t v) {
  return v == ASN1_PER_UNALIGNED || v == ASN1_PER_ALIGNED;
}
static size_t width(uint64_t max) {
  size_t bits = 0;
  while (max) { ++bits; max >>= 1; }
  return bits;
}
static size_t octets(uint64_t value) { size_t n = (width(value) + 7u) / 8u; return n ? n : 1u; }
static asn1_status_t align_write(asn1_bit_writer_t* w, asn1_per_variant_t v) {
  return v == ASN1_PER_ALIGNED ? asn1_bit_writer_align_to_octet(w) : ASN1_OK;
}
static asn1_status_t align_read(asn1_bit_reader_t* r, asn1_per_variant_t v) {
  return v == ASN1_PER_ALIGNED ? asn1_bit_reader_align_to_octet(r) : ASN1_OK;
}
static int64_t signed_bits(uint64_t value) {
  return value <= (uint64_t)INT64_MAX ? (int64_t)value
                                    : -1 - (int64_t)(UINT64_MAX - value);
}

/* max is the largest offset, not cardinality: UINT64_MAX covers 2^64 values. */
static asn1_status_t put_offset(asn1_bit_writer_t* w, asn1_per_variant_t v, uint64_t max, uint64_t value) {
  if (value > max) return ASN1_ERR_CONSTRAINT_VIOLATION;
  if (!max) return ASN1_OK;
  if (v == ASN1_PER_UNALIGNED || max < 255u)
    return asn1_bit_writer_put_bits(w, value, width(max));
  if (max <= 65535u) {
    TRY(align_write(w, v));
    return asn1_bit_writer_put_bits(w, value, max == 255u ? 8u : 16u);
  }
  size_t used = octets(value), capacity = octets(max);
  TRY(put_offset(w, v, capacity - 1u, used - 1u));
  TRY(align_write(w, v));
  return asn1_bit_writer_put_bits(w, value, used * 8u);
}
static asn1_status_t get_offset(asn1_bit_reader_t* r, asn1_per_variant_t v, uint64_t max, uint64_t* out) {
  uint64_t value = 0;
  if (!max) { *out = 0; return ASN1_OK; }
  if (v == ASN1_PER_UNALIGNED || max < 255u) {
    TRY(asn1_bit_reader_get_bits(r, width(max), &value));
  } else if (max <= 65535u) {
    TRY(align_read(r, v));
    TRY(asn1_bit_reader_get_bits(r, max == 255u ? 8u : 16u, &value));
  } else {
    uint64_t used_m1 = 0;
    TRY(get_offset(r, v, octets(max) - 1u, &used_m1));
    TRY(align_read(r, v));
    TRY(asn1_bit_reader_get_bits(r, (size_t)(used_m1 + 1u) * 8u, &value));
  }
  if (value > max) return ASN1_ERR_CONSTRAINT_VIOLATION;
  *out = value;
  return ASN1_OK;
}

asn1_status_t asn1_per_encode_length_fragment(asn1_bit_writer_t* w, asn1_per_variant_t v, size_t n, int more) {
  if (!w || !valid_variant(v) || (more != 0 && more != 1)) return ASN1_ERR_INVALID_ARGUMENT;
  if ((more && (n < 16384u || n > 65536u || n % 16384u)) || (!more && n >= 16384u))
    return ASN1_ERR_INVALID_ARGUMENT;
  TRY(align_write(w, v));
  if (more) return asn1_bit_writer_put_octet(w, (uint8_t)(0xC0u | n / 16384u));
  if (n < 128u) return asn1_bit_writer_put_octet(w, (uint8_t)n);
  TRY(asn1_bit_writer_put_octet(w, (uint8_t)(0x80u | (n >> 8))));
  return asn1_bit_writer_put_octet(w, (uint8_t)n);
}
asn1_status_t asn1_per_decode_length_fragment(asn1_bit_reader_t* r, asn1_per_variant_t v, size_t* out, int* more) {
  if (!r || !out || !more || !valid_variant(v)) return ASN1_ERR_INVALID_ARGUMENT;
  TRY(align_read(r, v));
  uint8_t first = 0, second = 0;
  TRY(asn1_bit_reader_get_octet(r, &first));
  size_t n; int continues = 0;
  if (first < 128u) n = first;
  else if (first < 192u) {
    TRY(asn1_bit_reader_get_octet(r, &second));
    n = ((size_t)(first & 63u) << 8) | second;
    if (n < 128u) return ASN1_ERR_NON_CANONICAL;
  } else {
    if (first < 0xC1u || first > 0xC4u) return ASN1_ERR_INVALID_ARGUMENT;
    n = (size_t)(first & 63u) * 16384u; continues = 1;
  }
  *out = n; *more = continues;
  return ASN1_OK;
}

static int valid_size(const asn1_per_length_constraint_t* c) {
  if (!c || (c->extensible != 0 && c->extensible != 1)) return 0;
  if (c->kind == ASN1_PER_LENGTH_CONSTRAINED) return c->lower <= c->upper;
  return c->kind == ASN1_PER_LENGTH_UNCONSTRAINED || c->kind == ASN1_PER_LENGTH_SEMI_CONSTRAINED;
}
static int in_size(const asn1_per_length_constraint_t* c, size_t n) {
  if (c->kind == ASN1_PER_LENGTH_UNCONSTRAINED) return 1;
  return n >= c->lower && (c->kind != ASN1_PER_LENGTH_CONSTRAINED || n <= c->upper);
}
static int bounded_size(const asn1_per_length_constraint_t* c, int extension) {
  return !extension && c->kind == ASN1_PER_LENGTH_CONSTRAINED && c->upper < 65536u;
}
asn1_status_t asn1_per_encode_sequence_preamble(asn1_bit_writer_t* w,
                                                int extensible,
                                                int extensions_present,
                                                const uint8_t* optional_bits,
                                                size_t optional_count) {
  if (!w || (extensible != 0 && extensible != 1) ||
      (extensions_present != 0 && extensions_present != 1) ||
      (!extensible && extensions_present) ||
      (optional_count && !optional_bits)) return ASN1_ERR_INVALID_ARGUMENT;
  if (optional_count > ASN1_MAX_STRING_BYTES) return ASN1_ERR_RESOURCE_LIMIT;
  for (size_t i = 0; i < optional_count; ++i)
    if (optional_bits[i] > 1u) return ASN1_ERR_INVALID_ARGUMENT;
  if (extensible) TRY(asn1_bit_writer_put_bit(w, extensions_present));
  for (size_t i = 0; i < optional_count; ++i)
    TRY(asn1_bit_writer_put_bit(w, optional_bits[i]));
  return ASN1_OK;
}
asn1_status_t asn1_per_decode_sequence_preamble(asn1_bit_reader_t* r,
                                                int extensible,
                                                size_t optional_count,
                                                asn1_per_sequence_preamble_t* out) {
  if (!r || !out || (extensible != 0 && extensible != 1))
    return ASN1_ERR_INVALID_ARGUMENT;
  if (optional_count > ASN1_MAX_STRING_BYTES) return ASN1_ERR_RESOURCE_LIMIT;
  asn1_per_sequence_preamble_t tmp = {0, NULL, 0};
  if (extensible) {
    asn1_status_t status = asn1_bit_reader_get_bit(r, &tmp.extensions_present);
    if (status != ASN1_OK) return status;
  }
  if (optional_count) {
    asn1_status_t budget = asn1_bit_reader_reserve(r, optional_count, 0);
    if (budget != ASN1_OK) return budget;
    tmp.optional_bits = (uint8_t*)malloc(optional_count);
    if (!tmp.optional_bits) return ASN1_ERR_OUT_OF_MEMORY;
    for (size_t i = 0; i < optional_count; ++i) {
      int bit = 0;
      asn1_status_t status = asn1_bit_reader_get_bit(r, &bit);
      if (status != ASN1_OK) { free(tmp.optional_bits); return status; }
      tmp.optional_bits[i] = (uint8_t)bit;
    }
    tmp.optional_count = optional_count;
  }
  *out = tmp;
  return ASN1_OK;
}
void asn1_per_sequence_preamble_free(asn1_per_sequence_preamble_t* value) {
  if (!value) return;
  free(value->optional_bits);
  value->optional_bits = NULL;
  value->optional_count = 0;
  value->extensions_present = 0;
}
asn1_status_t asn1_per_encode_extension_additions(asn1_bit_writer_t* w, asn1_per_variant_t v,
    const uint8_t* presence, size_t count, const asn1_byte_view_t* values) {
  if(!w||!valid_variant(v)||(count&&(!presence||!values)))return ASN1_ERR_INVALID_ARGUMENT;
  if(!count||count>ASN1_MAX_CHOICE_ALTS)return ASN1_ERR_RESOURCE_LIMIT;
  int any=0;for(size_t i=0;i<count;++i){if(presence[i]>1u)return ASN1_ERR_INVALID_ARGUMENT;any|=presence[i];}
  if(!any)return ASN1_ERR_INVALID_ARGUMENT;
  TRY(asn1_per_encode_normally_small_uint_ex(w,v,(uint64_t)(count-1u)));
  for(size_t i=0;i<count;++i)TRY(asn1_bit_writer_put_bit(w,presence[i]));
  for(size_t i=0;i<count;++i)if(presence[i])TRY(asn1_per_encode_open_type(w,v,values[i]));
  return ASN1_OK;
}
void asn1_per_extension_additions_free(asn1_per_extension_additions_t* out) {
  if(!out)return;
  if(out->open_type_data)for(size_t i=0;i<out->count;++i)free(out->open_type_data[i]);
  free(out->open_type_data);free(out->open_type_sizes);free(out->presence);
  out->open_type_data=NULL;out->open_type_sizes=NULL;out->presence=NULL;out->count=0;
}
asn1_status_t asn1_per_decode_extension_additions(asn1_bit_reader_t* r, asn1_per_variant_t v,
    asn1_per_extension_additions_t* out) {
  if(!r||!out||!valid_variant(v))return ASN1_ERR_INVALID_ARGUMENT;
  uint64_t last=0;TRY(asn1_per_decode_normally_small_uint_ex(r,v,&last));
  if(last>=ASN1_MAX_CHOICE_ALTS)return ASN1_ERR_RESOURCE_LIMIT;
  asn1_per_extension_additions_t tmp={0};tmp.count=(size_t)last+1u;
  if(tmp.count>SIZE_MAX/(sizeof(*tmp.open_type_data)+sizeof(*tmp.open_type_sizes)+sizeof(*tmp.presence)))return ASN1_ERR_LENGTH_OVERFLOW;
  asn1_status_t budget=asn1_bit_reader_reserve(r,tmp.count*(sizeof(*tmp.open_type_data)+sizeof(*tmp.open_type_sizes)+sizeof(*tmp.presence)),tmp.count);if(budget!=ASN1_OK)return budget;
  tmp.presence=(uint8_t*)calloc(tmp.count,1u);tmp.open_type_data=(uint8_t**)calloc(tmp.count,sizeof(*tmp.open_type_data));tmp.open_type_sizes=(size_t*)calloc(tmp.count,sizeof(*tmp.open_type_sizes));
  if(!tmp.presence||!tmp.open_type_data||!tmp.open_type_sizes){asn1_per_extension_additions_free(&tmp);return ASN1_ERR_OUT_OF_MEMORY;}
  int any=0;for(size_t i=0;i<tmp.count;++i){int bit=0;asn1_status_t s=asn1_bit_reader_get_bit(r,&bit);if(s!=ASN1_OK){asn1_per_extension_additions_free(&tmp);return s;}tmp.presence[i]=(uint8_t)bit;any|=bit;}
  if(!any){asn1_per_extension_additions_free(&tmp);return ASN1_ERR_INVALID_ARGUMENT;}
  for(size_t i=0;i<tmp.count;++i)if(tmp.presence[i]){asn1_status_t s=asn1_per_decode_open_type(r,v,&tmp.open_type_data[i],&tmp.open_type_sizes[i]);if(s!=ASN1_OK){asn1_per_extension_additions_free(&tmp);return s;}}
  *out=tmp;return ASN1_OK;
}
asn1_status_t asn1_per_encode_size_ex(asn1_bit_writer_t* w, asn1_per_variant_t v,
                                    const asn1_per_length_constraint_t* c, size_t n) {
  if (!w || !valid_variant(v) || !valid_size(c)) return ASN1_ERR_INVALID_ARGUMENT;
  int ext = !in_size(c, n);
  if (ext && !c->extensible) return ASN1_ERR_CONSTRAINT_VIOLATION;
  int bounded = bounded_size(c, ext);
  if (!bounded && n >= 16384u) return ASN1_ERR_UNSUPPORTED;
  if (c->extensible) TRY(asn1_bit_writer_put_bit(w, ext));
  return bounded ? put_offset(w, v, c->upper - c->lower, n - c->lower)
                 : asn1_per_encode_length_fragment(w, v, n, 0);
}
asn1_status_t asn1_per_decode_size_ex(asn1_bit_reader_t* r, asn1_per_variant_t v,
                                    const asn1_per_length_constraint_t* c, size_t* out) {
  if (!r || !out || !valid_variant(v) || !valid_size(c)) return ASN1_ERR_INVALID_ARGUMENT;
  int ext = 0; size_t n;
  if (c->extensible) TRY(asn1_bit_reader_get_bit(r, &ext));
  if (bounded_size(c, ext)) {
    uint64_t offset = 0;
    TRY(get_offset(r, v, c->upper - c->lower, &offset));
    n = c->lower + (size_t)offset;
  } else {
    int more = 0;
    TRY(asn1_per_decode_length_fragment(r, v, &n, &more));
    if (more) return ASN1_ERR_UNSUPPORTED;
  }
  if ((!ext && !in_size(c, n)) || (ext && in_size(c, n))) return ASN1_ERR_CONSTRAINT_VIOLATION;
  *out = n; return ASN1_OK;
}

asn1_status_t asn1_per_encode_collection_fragment_ex(asn1_bit_writer_t* w,
    asn1_per_variant_t v, const asn1_per_length_constraint_t* c,
    size_t remaining, int first, int* is_extension, size_t* chunk, int* more) {
  if (!w || !valid_variant(v) || !valid_size(c) || !is_extension || !chunk || !more ||
      (first != 0 && first != 1)) return ASN1_ERR_INVALID_ARGUMENT;
  if (first) {
    const int ext = !in_size(c, remaining);
    if (ext && !c->extensible) return ASN1_ERR_CONSTRAINT_VIOLATION;
    *is_extension = ext;
    if (c->extensible) TRY(asn1_bit_writer_put_bit(w, ext));
    if (bounded_size(c, ext)) {
      *chunk = remaining; *more = 0;
      return put_offset(w, v, c->upper - c->lower, remaining - c->lower);
    }
  } else if (*is_extension != 0 && *is_extension != 1) return ASN1_ERR_INVALID_ARGUMENT;
  if (bounded_size(c, *is_extension)) return ASN1_ERR_INVALID_ARGUMENT;
  size_t part = remaining; int continues = 0;
  if (remaining >= 16384u) {
    part = remaining >= 65536u ? 65536u : (remaining / 16384u) * 16384u;
    continues = 1;
  }
  TRY(asn1_per_encode_length_fragment(w, v, part, continues));
  *chunk = part; *more = continues;
  return ASN1_OK;
}

asn1_status_t asn1_per_decode_collection_fragment_ex(asn1_bit_reader_t* r,
    asn1_per_variant_t v, const asn1_per_length_constraint_t* c,
    int first, int* is_extension, size_t consumed, size_t* chunk, int* more) {
  if (!r || !valid_variant(v) || !valid_size(c) || !is_extension || !chunk || !more ||
      (first != 0 && first != 1)) return ASN1_ERR_INVALID_ARGUMENT;
  if (first) {
    int ext = 0;
    if (c->extensible) TRY(asn1_bit_reader_get_bit(r, &ext));
    *is_extension = ext;
    if (bounded_size(c, ext)) {
      uint64_t offset = 0;
      TRY(get_offset(r, v, c->upper - c->lower, &offset));
      *chunk = c->lower + (size_t)offset; *more = 0;
      if (!in_size(c, *chunk)) return ASN1_ERR_CONSTRAINT_VIOLATION;
      return ASN1_OK;
    }
  } else if (*is_extension != 0 && *is_extension != 1) return ASN1_ERR_INVALID_ARGUMENT;
  if (bounded_size(c, *is_extension)) return ASN1_ERR_INVALID_ARGUMENT;
  TRY(asn1_per_decode_length_fragment(r, v, chunk, more));
  if (consumed > SIZE_MAX - *chunk) return ASN1_ERR_LENGTH_OVERFLOW;
  if (!*more) {
    const size_t total = consumed + *chunk;
    if (*is_extension ? in_size(c, total) : !in_size(c, total))
      return ASN1_ERR_CONSTRAINT_VIOLATION;
  }
  return ASN1_OK;
}

/* Whole strings and open types share one chunk engine. For BIT STRING each
 * non-final chunk is octet-sized, so aggregate offsets remain byte-aligned. */
static asn1_status_t put_string(asn1_bit_writer_t* w, asn1_per_variant_t v,
                                const asn1_per_length_constraint_t* c,
                                asn1_byte_view_t data, size_t units, int bits) {
  if (!w || !valid_variant(v) || !valid_size(c) || (!data.data && data.size))
    return ASN1_ERR_INVALID_ARGUMENT;
  size_t need = bits ? units / 8u + (units % 8u != 0) : units;
  if (need > data.size) return ASN1_ERR_INVALID_ARGUMENT;
  int ext = !in_size(c, units);
  if (ext && !c->extensible) return ASN1_ERR_CONSTRAINT_VIOLATION;
  if (c->extensible) TRY(asn1_bit_writer_put_bit(w, ext));
  if (bounded_size(c, ext)) {
    TRY(put_offset(w, v, c->upper - c->lower, units - c->lower));
    if (units && (c->lower != c->upper || units > (bits ? 16u : 2u))) TRY(align_write(w, v));
    return bits ? asn1_bit_writer_put_bits_from_bytes(w, data, units)
                : asn1_bit_writer_put_octets(w, asn1_byte_view(data.data, units));
  }
  size_t offset = 0, remaining = units;
  for (;;) {
    int more = remaining >= 16384u;
    size_t chunk = more ? (remaining >= 65536u ? 65536u : (remaining / 16384u) * 16384u) : remaining;
    TRY(asn1_per_encode_length_fragment(w, v, chunk, more));
    const uint8_t* p = chunk ? data.data + (bits ? offset / 8u : offset) : NULL;
    if (bits) TRY(asn1_bit_writer_put_bits_from_bytes(w, asn1_byte_view(p, chunk / 8u + (chunk % 8u != 0)), chunk));
    else TRY(asn1_bit_writer_put_octets(w, asn1_byte_view(p, chunk)));
    offset += chunk; remaining -= chunk;
    if (!more) return ASN1_OK;
  }
}
static asn1_status_t get_string(asn1_bit_reader_t* r, asn1_per_variant_t v,
                                const asn1_per_length_constraint_t* c,
                                uint8_t** out, size_t* out_units, int bits) {
  if (!r || !out || !out_units || !valid_variant(v) || !valid_size(c)) return ASN1_ERR_INVALID_ARGUMENT;
  int ext = 0;
  if (c->extensible) TRY(asn1_bit_reader_get_bit(r, &ext));
  int bounded = bounded_size(c, ext);
  size_t fixed = 0;
  if (bounded) {
    uint64_t offset = 0;
    TRY(get_offset(r, v, c->upper - c->lower, &offset));
    fixed = c->lower + (size_t)offset;
    if (fixed && (c->lower != c->upper || fixed > (bits ? 16u : 2u))) TRY(align_read(r, v));
  }
  size_t total = 0;
  uint8_t* result = NULL;
  asn1_status_t s = ASN1_OK;
  do {
    size_t chunk = fixed; int more = 0;
    if (!bounded) {
      s = asn1_per_decode_length_fragment(r, v, &chunk, &more);
      if (s != ASN1_OK) break;
    }
    size_t limit = (size_t)ASN1_MAX_STRING_BYTES * (bits ? 8u : 1u);
    if (total > limit || chunk > limit - total) { s = ASN1_ERR_RESOURCE_LIMIT; break; }
    size_t n = bits ? chunk / 8u + (chunk % 8u != 0) : chunk;
    s = asn1_bit_reader_reserve(r, n, 0);
    if (s != ASN1_OK) break;
    uint8_t* part = NULL; size_t read = 0;
    s = bits ? asn1_bit_reader_get_bits_as_bytes(r, chunk, &part, &read)
             : asn1_bit_reader_get_octets(r, chunk, &part, &read);
    if (s != ASN1_OK) break;
    size_t position = bits ? total / 8u : total;
    s = asn1_bit_reader_reserve(r, position + n ? position + n : 1u, 0);
    if (s != ASN1_OK) { free(part); break; }
    uint8_t* grown = (uint8_t*)realloc(result, position + n ? position + n : 1u);
    if (!grown) { free(part); s = ASN1_ERR_OUT_OF_MEMORY; break; }
    result = grown;
    if (n) memcpy(result + position, part, n);
    free(part); total += chunk;
    if (!more) break;
  } while (1);
  if (s == ASN1_OK && ((!ext && !in_size(c, total)) || (ext && in_size(c, total))))
    s = ASN1_ERR_CONSTRAINT_VIOLATION;
  if (s != ASN1_OK) { free(result); return s; }
  *out = result; *out_units = total;
  return ASN1_OK;
}
asn1_status_t asn1_per_encode_octetstring_ex(asn1_bit_writer_t* w, asn1_per_variant_t v, const asn1_per_length_constraint_t* c, asn1_byte_view_t data) {
  return put_string(w, v, c, data, data.size, 0);
}
asn1_status_t asn1_per_decode_octetstring_ex(asn1_bit_reader_t* r, asn1_per_variant_t v, const asn1_per_length_constraint_t* c, uint8_t** out, size_t* n) {
  return get_string(r, v, c, out, n, 0);
}

static size_t restricted_bits(asn1_per_variant_t v, asn1_per_string_kind_t kind) {
  size_t bits = kind == ASN1_PER_STRING_NUMERIC ? 4u :
      (kind == ASN1_PER_STRING_IA5 || kind == ASN1_PER_STRING_PRINTABLE || kind == ASN1_PER_STRING_VISIBLE) ? 7u :
      kind == ASN1_PER_STRING_BMP ? 16u : 32u;
  if (v == ASN1_PER_ALIGNED) { size_t b = 1u; while (b < bits) b <<= 1u; return b; }
  return bits;
}
static int printable_char(uint32_t cp) {
  static const char alphabet[] = " '()+,-./0123456789:=?ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
  if (cp > 0x7au) return 0;
  for (size_t i = 0; alphabet[i]; ++i) if ((uint32_t)(unsigned char)alphabet[i] == cp) return 1;
  return 0;
}
static asn1_status_t utf8_next(asn1_byte_view_t text, size_t* at, uint32_t* cp) {
  if (!text.data || !at || !cp || *at >= text.size) return ASN1_ERR_INVALID_ARGUMENT;
  const uint8_t b0 = text.data[(*at)++];
  if (b0 < 0x80u) { *cp = b0; return ASN1_OK; }
  size_t n = (b0 & 0xe0u) == 0xc0u ? 1u : (b0 & 0xf0u) == 0xe0u ? 2u : (b0 & 0xf8u) == 0xf0u ? 3u : 99u;
  if (n == 99u || n > text.size - *at) return ASN1_ERR_INVALID_ARGUMENT;
  uint32_t value = b0 & (n == 1u ? 0x1fu : n == 2u ? 0x0fu : 0x07u);
  for (size_t i = 0; i < n; ++i) { const uint8_t b = text.data[(*at)++]; if ((b & 0xc0u) != 0x80u) return ASN1_ERR_INVALID_ARGUMENT; value = (value << 6u) | (b & 0x3fu); }
  if ((n == 1u && value < 0x80u) || (n == 2u && value < 0x800u) || (n == 3u && value < 0x10000u) || value > 0x10ffffu || (value >= 0xd800u && value <= 0xdfffu)) return ASN1_ERR_INVALID_ARGUMENT;
  *cp = value; return ASN1_OK;
}
static int restricted_code(uint32_t cp, asn1_per_string_kind_t kind, uint32_t* code) {
  switch (kind) {
    case ASN1_PER_STRING_IA5: if (cp > 0x7fu) return 0; *code = cp; return 1;
    case ASN1_PER_STRING_PRINTABLE: if (!printable_char(cp)) return 0; *code = cp; return 1;
    case ASN1_PER_STRING_VISIBLE: if (cp < 0x20u || cp > 0x7eu) return 0; *code = cp; return 1;
    case ASN1_PER_STRING_NUMERIC: if (cp == ' ') { *code = 0; return 1; } if (cp >= '0' && cp <= '9') { *code = cp - '0' + 1u; return 1; } return 0;
    case ASN1_PER_STRING_BMP: if (cp > 0xffffu) return 0; *code = cp; return 1;
    case ASN1_PER_STRING_UNIVERSAL: *code = cp; return 1;
  }
  return 0;
}
static asn1_status_t utf8_put(uint8_t* out, size_t cap, size_t* n, uint32_t cp) {
  uint8_t b[4]; size_t k;
  if (cp < 0x80u) { b[0]=(uint8_t)cp; k=1; }
  else if (cp < 0x800u) { b[0]=(uint8_t)(0xc0u|(cp>>6u)); b[1]=(uint8_t)(0x80u|(cp&63u)); k=2; }
  else if (cp < 0x10000u) { b[0]=(uint8_t)(0xe0u|(cp>>12u)); b[1]=(uint8_t)(0x80u|((cp>>6u)&63u)); b[2]=(uint8_t)(0x80u|(cp&63u)); k=3; }
  else { b[0]=(uint8_t)(0xf0u|(cp>>18u)); b[1]=(uint8_t)(0x80u|((cp>>12u)&63u)); b[2]=(uint8_t)(0x80u|((cp>>6u)&63u)); b[3]=(uint8_t)(0x80u|(cp&63u)); k=4; }
  if (*n > cap || k > cap - *n) return ASN1_ERR_LENGTH_OVERFLOW;
  memcpy(out + *n, b, k); *n += k; return ASN1_OK;
}
static int char_data_align(asn1_per_variant_t v, asn1_per_string_kind_t kind, const asn1_per_length_constraint_t* c) {
  if (v != ASN1_PER_ALIGNED) return 0;
  const size_t bits = restricted_bits(v,kind);
  if (c->kind == ASN1_PER_LENGTH_CONSTRAINED && c->lower == c->upper) return c->lower > 16u / bits;
  if (c->kind == ASN1_PER_LENGTH_CONSTRAINED && !c->extensible) return c->upper >= 16u / bits;
  return 1;
}
asn1_status_t asn1_per_encode_restricted_string_ex(asn1_bit_writer_t* w, asn1_per_variant_t v,
    const asn1_per_length_constraint_t* c, asn1_per_string_kind_t kind, asn1_byte_view_t text) {
  if (!w || !valid_variant(v) || !valid_size(c) || (text.size && !text.data) || kind < ASN1_PER_STRING_IA5 || kind > ASN1_PER_STRING_UNIVERSAL) return ASN1_ERR_INVALID_ARGUMENT;
  if (text.size > ASN1_MAX_STRING_BYTES) return ASN1_ERR_RESOURCE_LIMIT;
  size_t cap = text.size ? text.size * 2u : 1u;
  if (cap < text.size) return ASN1_ERR_LENGTH_OVERFLOW;
  if (cap > SIZE_MAX / sizeof(uint32_t)) return ASN1_ERR_LENGTH_OVERFLOW;
  uint32_t* codes = (uint32_t*)malloc(cap * sizeof(*codes)); if (!codes) return ASN1_ERR_OUT_OF_MEMORY;
  size_t at=0,n=0; asn1_status_t s=ASN1_OK;
  while (at < text.size) {
    uint32_t cp=0,code=0; s=utf8_next(text,&at,&cp); if(s!=ASN1_OK) break;
    /* BMPString is restricted to the Basic Multilingual Plane. UTF-16
       surrogate pairs are not BMPString characters and must not be emitted. */
    if (kind==ASN1_PER_STRING_BMP && cp>0xffffu) { s=ASN1_ERR_CONSTRAINT_VIOLATION; break; }
    if(!restricted_code(cp,kind,&code)){s=ASN1_ERR_INVALID_ARGUMENT;break;}
    if(n==cap){s=ASN1_ERR_RESOURCE_LIMIT;break;} codes[n++]=code;
  }
  if(s==ASN1_OK) s=asn1_per_encode_size_ex(w,v,c,n);
  if(s==ASN1_OK && char_data_align(v,kind,c)) s=align_write(w,v);
  const size_t bits=restricted_bits(v,kind);
  for(size_t i=0;s==ASN1_OK && i<n;++i) s=asn1_bit_writer_put_bits(w,codes[i],bits);
  free(codes); return s;
}
asn1_status_t asn1_per_decode_restricted_string_ex(asn1_bit_reader_t* r, asn1_per_variant_t v,
    const asn1_per_length_constraint_t* c, asn1_per_string_kind_t kind, uint8_t** out, size_t* out_size) {
  if(!r||!out||!out_size||!valid_variant(v)||!valid_size(c)||kind<ASN1_PER_STRING_IA5||kind>ASN1_PER_STRING_UNIVERSAL) return ASN1_ERR_INVALID_ARGUMENT;
  size_t count=0; asn1_status_t s=asn1_per_decode_size_ex(r,v,c,&count); if(s!=ASN1_OK) return s;
  if(count>ASN1_MAX_STRING_BYTES) return ASN1_ERR_RESOURCE_LIMIT;
  if(char_data_align(v,kind,c)){s=align_read(r,v);if(s!=ASN1_OK)return s;}
  size_t cap=count<=SIZE_MAX/4u?count*4u:SIZE_MAX; if(cap>ASN1_MAX_STRING_BYTES*4u) return ASN1_ERR_RESOURCE_LIMIT;
  s=asn1_bit_reader_reserve(r,cap?cap:1u,0);if(s!=ASN1_OK)return s;
  uint8_t* text=(uint8_t*)malloc(cap?cap:1u); if(!text)return ASN1_ERR_OUT_OF_MEMORY; size_t used=0; const size_t bits=restricted_bits(v,kind); uint32_t high=0;
  for(size_t i=0;i<count;++i){uint64_t raw=0;s=asn1_bit_reader_get_bits(r,bits,&raw);if(s!=ASN1_OK)break;uint32_t cp=(uint32_t)raw;
    if(kind==ASN1_PER_STRING_NUMERIC){if(cp==0)cp=' ';else if(cp<=10)cp='0'+cp-1;else{s=ASN1_ERR_CONSTRAINT_VIOLATION;break;}}
    else if(kind==ASN1_PER_STRING_PRINTABLE&&!printable_char(cp)){s=ASN1_ERR_CONSTRAINT_VIOLATION;break;}
    else if(kind==ASN1_PER_STRING_VISIBLE&&(cp<0x20u||cp>0x7eu)){s=ASN1_ERR_CONSTRAINT_VIOLATION;break;}
    else if(kind==ASN1_PER_STRING_IA5&&cp>0x7fu){s=ASN1_ERR_CONSTRAINT_VIOLATION;break;}
    else if(kind==ASN1_PER_STRING_BMP&&(cp>=0xd800u&&cp<=0xdfffu)){s=ASN1_ERR_CONSTRAINT_VIOLATION;break;}
    else if(kind==ASN1_PER_STRING_UNIVERSAL&&(cp>0x10ffffu||(cp>=0xd800u&&cp<=0xdfffu))){s=ASN1_ERR_CONSTRAINT_VIOLATION;break;}
    s=utf8_put(text,cap,&used,cp);if(s!=ASN1_OK)break;
  }
  if(s==ASN1_OK&&high)s=ASN1_ERR_CONSTRAINT_VIOLATION;
  if(s!=ASN1_OK){free(text);return s;} *out=text;*out_size=used;return ASN1_OK;
}

asn1_status_t asn1_per_encode_real_ex(asn1_bit_writer_t* w, asn1_per_variant_t v, double value) {
  if(!w||!valid_variant(v))return ASN1_ERR_INVALID_ARGUMENT;
  uint8_t* data=NULL;size_t n=0;TRY(asn1_value_encode_real(value,&data,&n));
  const asn1_per_length_constraint_t unconstrained={ASN1_PER_LENGTH_UNCONSTRAINED,0,0,0};
  asn1_status_t s=asn1_per_encode_octetstring_ex(w,v,&unconstrained,asn1_byte_view(data,n));free(data);return s;
}

asn1_status_t asn1_per_decode_real_ex(asn1_bit_reader_t* r, asn1_per_variant_t v, double* out) {
  if(!r||!out||!valid_variant(v))return ASN1_ERR_INVALID_ARGUMENT;
  const asn1_per_length_constraint_t unconstrained={ASN1_PER_LENGTH_UNCONSTRAINED,0,0,0};
  uint8_t* data=NULL;size_t n=0;TRY(asn1_per_decode_octetstring_ex(r,v,&unconstrained,&data,&n));
  asn1_status_t s=asn1_value_decode_real(asn1_byte_view(data,n),out);free(data);return s;
}
asn1_status_t asn1_per_encode_bitstring_ex(asn1_bit_writer_t* w, asn1_per_variant_t v, const asn1_per_length_constraint_t* c, const asn1_bitstring_t* data) {
  if (!data) return ASN1_ERR_INVALID_ARGUMENT;
  return put_string(w, v, c, asn1_byte_view(data->bits, data->cap_bytes), data->bit_length, 1);
}
asn1_status_t asn1_per_decode_bitstring_ex(asn1_bit_reader_t* r, asn1_per_variant_t v, const asn1_per_length_constraint_t* c, asn1_bitstring_t* out) {
  if (!out) return ASN1_ERR_INVALID_ARGUMENT;
  uint8_t* data = NULL; size_t n = 0;
  TRY(get_string(r, v, c, &data, &n, 1));
  asn1_bitstring_free(out);
  out->bits = data; out->bit_length = n; out->cap_bytes = n / 8u + (n % 8u != 0);
  return ASN1_OK;
}
static const asn1_per_length_constraint_t unconstrained = {ASN1_PER_LENGTH_UNCONSTRAINED, 0, 0, 0};
asn1_status_t asn1_per_encode_open_type(asn1_bit_writer_t* w, asn1_per_variant_t v, asn1_byte_view_t data) {
  return asn1_per_encode_octetstring_ex(w, v, &unconstrained, data);
}
asn1_status_t asn1_per_decode_open_type(asn1_bit_reader_t* r, asn1_per_variant_t v, uint8_t** out, size_t* n) {
  if (!out || !n) return ASN1_ERR_INVALID_ARGUMENT;
  uint8_t* bytes = NULL; size_t size = 0;
  TRY(asn1_per_decode_octetstring_ex(r, v, &unconstrained, &bytes, &size));
  *out = bytes; *n = size;
  return ASN1_OK;
}

asn1_status_t asn1_per_encode_unbounded_int_ex(asn1_bit_writer_t* w, asn1_per_variant_t v, const asn1_bigint_t* value) {
  if (!w || !value || !valid_variant(v)) return ASN1_ERR_INVALID_ARGUMENT;
  uint8_t* data = NULL; size_t n = 0;
  TRY(asn1_bigint_to_twos_complement(value, &data, &n));
  asn1_status_t s = asn1_per_encode_open_type(w, v, asn1_byte_view(data, n));
  free(data); return s;
}
asn1_status_t asn1_per_decode_unbounded_int_ex(asn1_bit_reader_t* r, asn1_per_variant_t v, asn1_bigint_t* out) {
  if (!r || !out || !valid_variant(v)) return ASN1_ERR_INVALID_ARGUMENT;
  uint8_t* data = NULL; size_t n = 0;
  TRY(asn1_per_decode_open_type(r, v, &data, &n));
  asn1_status_t budget = asn1_bit_reader_reserve(r, n, 0);
  if (budget != ASN1_OK) { free(data); return budget; }
  asn1_status_t s = asn1_bigint_init_from_twos_complement(out, asn1_byte_view(data, n));
  free(data); return s;
}
static int valid_int(const asn1_per_int_range_t* c) {
  return c && (!c->lower_set || !c->upper_set || c->lower <= c->upper) &&
         (c->extensible == 0 || c->extensible == 1);
}

asn1_status_t asn1_per_encode_normally_small_uint_ex(asn1_bit_writer_t* w,
                                                      asn1_per_variant_t v,
                                                      uint64_t value) {
  if (!w || !valid_variant(v)) return ASN1_ERR_INVALID_ARGUMENT;
  if (value < 64u) {
    TRY(asn1_bit_writer_put_bit(w, 0));
    return asn1_bit_writer_put_bits(w, value, 6u);
  }
  TRY(asn1_bit_writer_put_bit(w, 1));
  size_t used = octets(value);
  TRY(asn1_per_encode_length_fragment(w, v, used, 0));
  return asn1_bit_writer_put_bits(w, value, used * 8u);
}

asn1_status_t asn1_per_decode_normally_small_uint_ex(asn1_bit_reader_t* r,
                                                      asn1_per_variant_t v,
                                                      uint64_t* out) {
  if (!r || !out || !valid_variant(v)) return ASN1_ERR_INVALID_ARGUMENT;
  int large = 0;
  TRY(asn1_bit_reader_get_bit(r, &large));
  if (!large) {
    uint64_t value = 0;
    TRY(asn1_bit_reader_get_bits(r, 6u, &value));
    *out = value;
    return ASN1_OK;
  }
  size_t used = 0;
  int more = 0;
  TRY(asn1_per_decode_length_fragment(r, v, &used, &more));
  if (more || used == 0u || used > 8u) return ASN1_ERR_UNSUPPORTED;
  uint64_t value = 0;
  TRY(asn1_bit_reader_get_bits(r, used * 8u, &value));
  if (used > 1u && value < (UINT64_C(1) << ((used - 1u) * 8u)))
    return ASN1_ERR_NON_CANONICAL;
  if (value < 64u) return ASN1_ERR_NON_CANONICAL;
  *out = value;
  return ASN1_OK;
}

asn1_status_t asn1_per_encode_enumerated_ex(asn1_bit_writer_t* w,
                                             asn1_per_variant_t v,
                                             size_t root_index,
                                             size_t root_count,
                                             int extensible,
                                             int is_extension,
                                             uint64_t extension_index) {
  if (!w || !valid_variant(v) || root_count == 0u ||
      (extensible != 0 && extensible != 1) ||
      (is_extension != 0 && is_extension != 1) ||
      (!extensible && is_extension) || (!is_extension && root_index >= root_count))
    return ASN1_ERR_INVALID_ARGUMENT;
  if (extensible) TRY(asn1_bit_writer_put_bit(w, is_extension));
  if (is_extension) return asn1_per_encode_normally_small_uint_ex(w, v, extension_index);
  return put_offset(w, v, (uint64_t)(root_count - 1u), (uint64_t)root_index);
}

asn1_status_t asn1_per_decode_enumerated_ex(asn1_bit_reader_t* r,
                                             asn1_per_variant_t v,
                                             size_t root_count,
                                             int extensible,
                                             int* is_extension,
                                             uint64_t* ordinal) {
  if (!r || !is_extension || !ordinal || !valid_variant(v) || root_count == 0u ||
      (extensible != 0 && extensible != 1)) return ASN1_ERR_INVALID_ARGUMENT;
  int extension = 0;
  if (extensible) TRY(asn1_bit_reader_get_bit(r, &extension));
  uint64_t value = 0;
  if (extension) {
    TRY(asn1_per_decode_normally_small_uint_ex(r, v, &value));
  } else {
    TRY(get_offset(r, v, (uint64_t)(root_count - 1u), &value));
    if (value >= root_count) return ASN1_ERR_CONSTRAINT_VIOLATION;
  }
  *is_extension = extension;
  *ordinal = value;
  return ASN1_OK;
}

asn1_status_t asn1_per_encode_oid_ex(asn1_bit_writer_t* w, asn1_per_variant_t v,const uint64_t* arcs,size_t count,int relative) {
  if(!w||!valid_variant(v))return ASN1_ERR_INVALID_ARGUMENT;
  uint8_t* data=NULL;size_t n=0;TRY(asn1_value_encode_oid(arcs,count,relative,&data,&n));
  const asn1_per_length_constraint_t unconstrained={ASN1_PER_LENGTH_UNCONSTRAINED,0,0,0};
  asn1_status_t s=asn1_per_encode_octetstring_ex(w,v,&unconstrained,asn1_byte_view(data,n));free(data);return s;
}

asn1_status_t asn1_per_decode_oid_ex(asn1_bit_reader_t* r, asn1_per_variant_t v,uint64_t** out_arcs,size_t* out_count,int relative) {
  if(!r||!out_arcs||!out_count||!valid_variant(v)||(relative!=0&&relative!=1))return ASN1_ERR_INVALID_ARGUMENT;
  const asn1_per_length_constraint_t unconstrained={ASN1_PER_LENGTH_UNCONSTRAINED,0,0,0};
  uint8_t* data=NULL;size_t n=0;TRY(asn1_per_decode_octetstring_ex(r,v,&unconstrained,&data,&n));
  const size_t capacity=n+(relative?0u:1u);
  asn1_status_t s=asn1_bit_reader_reserve(r,capacity*sizeof(uint64_t),capacity);
  if(s==ASN1_OK)s=asn1_value_decode_oid(asn1_byte_view(data,n),out_arcs,out_count,relative);free(data);return s;
}

static int in_int(const asn1_per_int_range_t* c, int64_t n) {
  return (!c->lower_set || n >= c->lower) && (!c->upper_set || n <= c->upper);
}
static asn1_status_t put_i64(asn1_bit_writer_t* w, asn1_per_variant_t v, int64_t n) {
  uint8_t data[8]; uint64_t value = (uint64_t)n;
  for (size_t i = 0; i < 8; ++i) data[7u - i] = (uint8_t)(value >> (i * 8u));
  size_t start = 0;
  while (start < 7u && ((data[start] == 0 && !(data[start+1] & 128u)) ||
                       (data[start] == 255u && (data[start+1] & 128u)))) ++start;
  return asn1_per_encode_open_type(w, v, asn1_byte_view(data + start, 8u - start));
}
static asn1_status_t get_i64(asn1_bit_reader_t* r, asn1_per_variant_t v, int64_t* out) {
  asn1_bigint_t tmp; asn1_bigint_init(&tmp);
  TRY(asn1_per_decode_unbounded_int_ex(r, v, &tmp));
  asn1_status_t s = asn1_bigint_to_int64(&tmp, out);
  asn1_bigint_free(&tmp); return s;
}
asn1_status_t asn1_per_encode_int_ex(asn1_bit_writer_t* w, asn1_per_variant_t v, const asn1_per_int_range_t* c, int64_t n) {
  if (!w || !valid_variant(v) || !valid_int(c)) return ASN1_ERR_INVALID_ARGUMENT;
  int ext = !in_int(c, n);
  if (ext && !c->extensible) return ASN1_ERR_CONSTRAINT_VIOLATION;
  if (c->extensible) TRY(asn1_bit_writer_put_bit(w, ext));
  if (!ext && c->lower_set && c->upper_set)
    return put_offset(w, v, (uint64_t)c->upper - (uint64_t)c->lower, (uint64_t)n - (uint64_t)c->lower);
  if (!ext && c->lower_set) {
    uint64_t offset = (uint64_t)n - (uint64_t)c->lower;
    size_t used = octets(offset);
    TRY(asn1_per_encode_length_fragment(w, v, used, 0));
    return asn1_bit_writer_put_bits(w, offset, used * 8u);
  }
  return put_i64(w, v, n);
}
asn1_status_t asn1_per_decode_int_ex(asn1_bit_reader_t* r, asn1_per_variant_t v, const asn1_per_int_range_t* c, int64_t* out) {
  if (!r || !out || !valid_variant(v) || !valid_int(c)) return ASN1_ERR_INVALID_ARGUMENT;
  int ext = 0; int64_t n;
  if (c->extensible) TRY(asn1_bit_reader_get_bit(r, &ext));
  if (!ext && c->lower_set && c->upper_set) {
    uint64_t offset = 0;
    TRY(get_offset(r, v, (uint64_t)c->upper - (uint64_t)c->lower, &offset));
    n = signed_bits((uint64_t)c->lower + offset);
  } else if (!ext && c->lower_set) {
    size_t used = 0; int more = 0;
    TRY(asn1_per_decode_length_fragment(r, v, &used, &more));
    if (more || used > 8) return ASN1_ERR_UNSUPPORTED;
    if (!used) return ASN1_ERR_INVALID_ARGUMENT;
    uint64_t offset = 0;
    TRY(asn1_bit_reader_get_bits(r, used * 8u, &offset));
    if (used > 1u && offset < (UINT64_C(1) << ((used - 1u) * 8u))) return ASN1_ERR_NON_CANONICAL;
    if (offset > (uint64_t)INT64_MAX - (uint64_t)c->lower) return ASN1_ERR_LENGTH_OVERFLOW;
    n = signed_bits((uint64_t)c->lower + offset);
  } else {
    TRY(get_i64(r, v, &n));
  }
  if ((!ext && !in_int(c, n)) || (ext && in_int(c, n))) return ASN1_ERR_CONSTRAINT_VIOLATION;
  *out = n; return ASN1_OK;
}
