/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/src/runtime/per/codec.cpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   PER structured-type orchestration.
**
** Specification: ITU-T X.691 — ASN.1 encoding rules: Packed Encoding
**                 Rules (PER); UPER/APER variants.
** Design Spec:   asn1-toolkit/docs/ARCHITECTURE.md
**                 asn1-toolkit/README.md
***************************************************************************/
#include <asn1/runtime/per/codec.hpp>
#include <asn1/runtime/ber/codec.hpp>
#include <asn1/runtime/byte_io.hpp>

namespace asn1 {
namespace per {
namespace {

/**
 *  Function    : in_root_integer
 *  Description : Returns a boolean result from value, c.
 *  Parameters  : value — std::int64_t value; c — const IntegerConstraint& c
 *  Returns     : bool
 */
bool in_root_integer(std::int64_t value, const IntegerConstraint& c) {
  return c.contains(value);
}

/**
 *  Function    : in_root_size
 *  Description : Returns a boolean result from n, size.
 *  Parameters  : n — std::size_t n; size — const SizeConstraint& size
 *  Returns     : bool
 */
bool in_root_size(std::size_t n, const SizeConstraint& size) {
  return size.contains(n);
}

void encode_length_chunks(BitWriter& out, Variant variant, std::size_t length,
                          Span<const std::uint8_t> bytes) {
  std::size_t remaining = length;
  std::size_t offset = 0;
  for (;;) {
    const std::size_t chunk = encode_length_determinant(out, variant, remaining);
    out.put_octets(Span<const std::uint8_t>(bytes.data() + offset, chunk));
    offset += chunk;
    remaining -= chunk;
    if (chunk < 16384) {
      break;
    }
  }
}

/**
 *  Function    : decode_length_chunks
 *  Description : Returns success or an error from decode length chunks.
 *  Parameters  : in — BitReader& in; variant — Variant variant
 *  Returns     : Result<std::vector<std::uint8_t>>
 */
Result<std::vector<std::uint8_t>> decode_length_chunks(BitReader& in, Variant variant) {
  std::vector<std::uint8_t> out;
  for (;;) {
    auto len = decode_length_determinant(in, variant);
    if (!len) {
      return len.error();
    }
    auto part = in.get_octets(len.value());
    if (!part) {
      return part.error();
    }
    out.insert(out.end(), part.value().begin(), part.value().end());
    if (len.value() < 16384) {
      break;
    }
  }
  return out;
}

void encode_bit_length_chunks(BitWriter& out, Variant variant, std::size_t bit_length,
                              Span<const std::uint8_t> bits) {
  if (bit_length > bits.size() * 8) {
    bit_length = bits.size() * 8;
  }
  std::size_t remaining = bit_length;
  std::size_t bit_offset = 0;
  for (;;) {
    const std::size_t chunk = encode_length_determinant(out, variant, remaining);
    // Write `chunk` bits starting at bit_offset from `bits`.
    for (std::size_t i = 0; i < chunk; ++i) {
      const std::size_t abs = bit_offset + i;
      const std::uint8_t byte = bits[abs / 8];
      const bool bit = ((byte >> (7 - (abs % 8))) & 1u) != 0;
      out.put_bit(bit);
    }
    bit_offset += chunk;
    remaining -= chunk;
    if (chunk < 16384) {
      break;
    }
  }
}

/**
 *  Function    : decode_bit_length_chunks
 *  Description : Returns success or an error from decode bit length chunks.
 *  Parameters  : in — BitReader& in; variant — Variant variant
 *  Returns     : Result<BitStringValue>
 */
Result<BitStringValue> decode_bit_length_chunks(BitReader& in, Variant variant) {
  BitStringValue v;
  for (;;) {
    auto len = decode_length_determinant(in, variant);
    if (!len) {
      return len.error();
    }
    auto part = in.get_bits_as_bytes(len.value());
    if (!part) {
      return part.error();
    }
    // Append bits into v.bits starting at v.bit_length.
    for (std::size_t i = 0; i < len.value(); ++i) {
      const std::uint8_t byte = part.value()[i / 8];
      const bool bit = ((byte >> (7 - (i % 8))) & 1u) != 0;
      const std::size_t abs = v.bit_length + i;
      if (v.bits.size() < abs / 8 + 1) {
        v.bits.push_back(0);
      }
      if (bit) {
        v.bits[abs / 8] =
            static_cast<std::uint8_t>(v.bits[abs / 8] | (1u << (7 - (abs % 8))));
      }
    }
    v.bit_length += len.value();
    if (len.value() < 16384) {
      break;
    }
  }
  return v;
}

void encode_size(BitWriter& out, Variant variant, std::size_t n,
                 const SizeConstraint& size) {
  if (size.extensible) {
    out.put_bit(!in_root_size(n, size));
    if (!in_root_size(n, size)) {
      maybe_align(out, variant);
      encode_length_determinant(out, variant, n);
      return;
    }
  }
  if (size.is_fully_constrained()) {
    if (size.is_fixed()) {
      return;
    }
    (void)encode_constrained_whole_number(out, variant, static_cast<std::int64_t>(n),
                                          static_cast<std::int64_t>(*size.lower),
                                          static_cast<std::int64_t>(*size.upper));
    return;
  }
  // Unbound or upper >= 64K: X.691 11.9.3.5--11.9.3.8 uses general length determinant of n.
  maybe_align(out, variant);
  encode_length_determinant(out, variant, n);
}

Result<std::size_t> decode_size(BitReader& in, Variant variant,
                                const SizeConstraint& size) {
  if (size.extensible) {
    auto ext = in.get_bit();
    if (!ext) {
      return ext.error();
    }
    if (ext.value()) {
      if (auto a = maybe_align(in, variant); !a) {
        return a.error();
      }
      return decode_length_determinant(in, variant);
    }
  }
  if (size.is_fully_constrained()) {
    if (size.is_fixed()) {
      return *size.lower;
    }
    auto v = decode_constrained_whole_number(in, variant,
                                             static_cast<std::int64_t>(*size.lower),
                                             static_cast<std::int64_t>(*size.upper));
    if (!v) {
      return v.error();
    }
    const std::size_t sz = static_cast<std::size_t>(v.value());
    if (!size.contains(sz)) {
      return make_error(Error::Code::ConstraintViolation, in.bit_offset(),
                        "decoded size violates constraint");
    }
    return sz;
  }
  if (auto a = maybe_align(in, variant); !a) {
    return a.error();
  }
  auto res = decode_length_determinant(in, variant);
  if (!res) {
    return res.error();
  }
  if (!size.contains(res.value())) {
    return make_error(Error::Code::ConstraintViolation, in.bit_offset(),
                      "decoded size violates constraint");
  }
  return res;
}

}  // namespace

/**
 *  Function    : encode_boolean
 *  Description : Performs encode boolean (definition).
 *  Parameters  : out — BitWriter& out; / — Variant /*variant*/; value — bool value
 *  Returns     : void
 */
void encode_boolean(BitWriter& out, Variant /*variant*/, bool value) {
  out.put_bit(value);
}

/**
 *  Function    : decode_boolean
 *  Description : Returns a boolean result from in, /.
 *  Parameters  : in — BitReader& in; / — Variant /*variant*/
 *  Returns     : Result<bool>
 */
Result<bool> decode_boolean(BitReader& in, Variant /*variant*/) {
  return in.get_bit();
}

/**
 *  Function    : encode_null
 *  Description : Performs encode null (definition).
 *  Parameters  : / — BitWriter& /*out*/; / — Variant /*variant*/
 *  Returns     : void
 */
void encode_null(BitWriter& /*out*/, Variant /*variant*/) {}

/**
 *  Function    : decode_null
 *  Description : Returns success or an error from decode null.
 *  Parameters  : / — BitReader& /*in*/; / — Variant /*variant*/
 *  Returns     : Result<void>
 */
Result<void> decode_null(BitReader& /*in*/, Variant /*variant*/) {
  return Result<void>::success();
}

Result<void> encode_integer(BitWriter& out, Variant variant, std::int64_t value,
                            const IntegerConstraint& constraint) {
  if (constraint.extensible) {
    const bool root = in_root_integer(value, constraint);
    out.put_bit(!root);
    if (!root) {
      encode_unconstrained_whole_number(out, variant, value);
      return Result<void>::success();
    }
  } else if (!constraint.contains(value)) {
    return make_error(Error::Code::ConstraintViolation, out.bit_size(),
                      "integer value violates constraint");
  }

  if (constraint.lower && constraint.upper) {
    return encode_constrained_whole_number(out, variant, value, *constraint.lower,
                                           *constraint.upper);
  }
  if (constraint.lower) {
    if (value < *constraint.lower) {
      return make_error(Error::Code::ConstraintViolation, out.bit_size(),
                        "value violates semi-constrained lower bound");
    }
    encode_semi_constrained_whole_number(out, variant, value, *constraint.lower);
    return Result<void>::success();
  }
  encode_unconstrained_whole_number(out, variant, value);
  return Result<void>::success();
}

Result<void> encode_integer(BitWriter& out, Variant variant, const BigInteger& value,
                            const IntegerConstraint& constraint) {
  // Constrained host paths stay on int64; BigInteger uses the unconstrained form
  // (or extension addition) unless the value fits the int64 root range.
  if (constraint.lower || constraint.upper || constraint.extensible) {
    auto v = value.as_i64();
    if (v) {
      return encode_integer(out, variant, *v, constraint);
    }
    if (constraint.extensible) {
      out.put_bit(true);
      encode_unconstrained_whole_number(out, variant, value);
      return Result<void>::success();
    }
    // Unconstrained with leftover bounds that don't fit int64 → length+octets.
  }
  encode_unconstrained_whole_number(out, variant, value);
  return Result<void>::success();
}

Result<std::int64_t> decode_integer(BitReader& in, Variant variant,
                                    const IntegerConstraint& constraint) {
  std::int64_t res_val = 0;
  if (constraint.extensible) {
    auto ext = in.get_bit();
    if (!ext) {
      return ext.error();
    }
    if (ext.value()) {
      return decode_unconstrained_whole_number(in, variant);
    }
  }

  if (constraint.lower && constraint.upper) {
    auto v = decode_constrained_whole_number(in, variant, *constraint.lower,
                                             *constraint.upper);
    if (!v) return v.error();
    res_val = v.value();
  } else if (constraint.lower) {
    auto res = decode_semi_constrained_whole_number(in, variant, *constraint.lower);
    if (!res) {
      return res.error();
    }
    if (res.value() < *constraint.lower) {
      return make_error(Error::Code::ConstraintViolation, in.bit_offset(),
                        "decoded integer below semi-constrained lower bound");
    }
    res_val = res.value();
  } else {
    auto res = decode_unconstrained_whole_number(in, variant);
    if (!res) return res.error();
    res_val = res.value();
  }

  if (!constraint.contains(res_val)) {
    return make_error(Error::Code::ConstraintViolation, in.bit_offset(),
                      "decoded integer violates constraint");
  }
  return res_val;
}

Result<BigInteger> decode_big_integer(BitReader& in, Variant variant,
                                      const IntegerConstraint& constraint) {
  if (constraint.extensible) {
    auto ext = in.get_bit();
    if (!ext) {
      return ext.error();
    }
    if (ext.value()) {
      return decode_unconstrained_big_integer(in, variant);
    }
  }
  if (constraint.lower && constraint.upper) {
    auto v = decode_constrained_whole_number(in, variant, *constraint.lower,
                                            *constraint.upper);
    if (!v) {
      return v.error();
    }
    return BigInteger::from_i64(v.value());
  }
  if (constraint.lower) {
    auto v = decode_semi_constrained_whole_number(in, variant, *constraint.lower);
    if (!v) {
      return v.error();
    }
    return BigInteger::from_i64(v.value());
  }
  return decode_unconstrained_big_integer(in, variant);
}

void encode_octet_string(BitWriter& out, Variant variant,
                         Span<const std::uint8_t> value,
                         const SizeConstraint& size) {
  if (size.extensible) {
    out.put_bit(!in_root_size(value.size(), size));
    if (!in_root_size(value.size(), size)) {
      maybe_align(out, variant);
      encode_length_chunks(out, variant, value.size(), value);
      return;
    }
  }

  if (size.is_fully_constrained()) {
    if (!size.is_fixed()) {
      (void)encode_constrained_whole_number(out, variant,
                                            static_cast<std::int64_t>(value.size()),
                                            static_cast<std::int64_t>(*size.lower),
                                            static_cast<std::int64_t>(*size.upper));
      maybe_align(out, variant);
    } else if (*size.lower > 2) {
      maybe_align(out, variant);
    }
    out.put_octets(value);
    return;
  }

  maybe_align(out, variant);
  encode_length_chunks(out, variant, value.size(), value);
}

Result<std::vector<std::uint8_t>> decode_octet_string(BitReader& in, Variant variant,
                                                      const SizeConstraint& size) {
  if (size.extensible) {
    auto ext = in.get_bit();
    if (!ext) {
      return ext.error();
    }
    if (ext.value()) {
      if (auto a = maybe_align(in, variant); !a) {
        return a.error();
      }
      return decode_length_chunks(in, variant);
    }
  }

  if (size.is_fully_constrained()) {
    std::size_t length = *size.lower;
    if (!size.is_fixed()) {
      auto len = decode_constrained_whole_number(in, variant,
                                                 static_cast<std::int64_t>(*size.lower),
                                                 static_cast<std::int64_t>(*size.upper));
      if (!len) {
        return len.error();
      }
      length = static_cast<std::size_t>(len.value());
      if (auto a = maybe_align(in, variant); !a) {
        return a.error();
      }
    } else if (*size.lower > 2) {
      if (auto a = maybe_align(in, variant); !a) {
        return a.error();
      }
    }
    return in.get_octets(length);
  }

  if (auto a = maybe_align(in, variant); !a) {
    return a.error();
  }
  return decode_length_chunks(in, variant);
}

void encode_bit_string(BitWriter& out, Variant variant,
                       Span<const std::uint8_t> bits, std::size_t bit_length,
                       const SizeConstraint& size) {
  if (size.extensible) {
    out.put_bit(!in_root_size(bit_length, size));
    if (!in_root_size(bit_length, size)) {
      maybe_align(out, variant);
      encode_bit_length_chunks(out, variant, bit_length, bits);
      return;
    }
  }

  if (size.is_fully_constrained()) {
    if (!size.is_fixed()) {
      (void)encode_constrained_whole_number(out, variant,
                                            static_cast<std::int64_t>(bit_length),
                                            static_cast<std::int64_t>(*size.lower),
                                            static_cast<std::int64_t>(*size.upper));
      maybe_align(out, variant);
    } else if (*size.lower > 16) {
      maybe_align(out, variant);
    }
    out.put_bits(bits, bit_length);
    return;
  }

  maybe_align(out, variant);
  encode_bit_length_chunks(out, variant, bit_length, bits);
}

Result<BitStringValue> decode_bit_string(BitReader& in, Variant variant,
                                         const SizeConstraint& size) {
  if (size.extensible) {
    auto ext = in.get_bit();
    if (!ext) {
      return ext.error();
    }
    if (ext.value()) {
      if (auto a = maybe_align(in, variant); !a) {
        return a.error();
      }
      return decode_bit_length_chunks(in, variant);
    }
  }

  if (size.is_fully_constrained()) {
    std::size_t bit_length = *size.lower;
    if (!size.is_fixed()) {
      auto len = decode_constrained_whole_number(in, variant,
                                                 static_cast<std::int64_t>(*size.lower),
                                                 static_cast<std::int64_t>(*size.upper));
      if (!len) {
        return len.error();
      }
      bit_length = static_cast<std::size_t>(len.value());
      if (auto a = maybe_align(in, variant); !a) {
        return a.error();
      }
    } else if (*size.lower > 16) {
      if (auto a = maybe_align(in, variant); !a) {
        return a.error();
      }
    }
    auto bytes = in.get_bits_as_bytes(bit_length);
    if (!bytes) {
      return bytes.error();
    }
    return BitStringValue{bytes.value(), bit_length};
  }

  if (auto a = maybe_align(in, variant); !a) {
    return a.error();
  }
  return decode_bit_length_chunks(in, variant);
}

void encode_utf8_string(BitWriter& out, Variant variant, const std::string& value,
                        const SizeConstraint& size) {
  // UTF8String SIZE constrains characters; Phase 9 treats SIZE as octet length when set,
  // matching common runtime usage for unconstrained UTF8 (length in octets).
  const auto bytes = Span<const std::uint8_t>(
      reinterpret_cast<const std::uint8_t*>(value.data()), value.size());
  encode_octet_string(out, variant, bytes, size);
}

Result<std::string> decode_utf8_string(BitReader& in, Variant variant,
                                       const SizeConstraint& size) {
  auto bytes = decode_octet_string(in, variant, size);
  if (!bytes) {
    return bytes.error();
  }
  return std::string(reinterpret_cast<const char*>(bytes.value().data()),
                     bytes.value().size());
}

void encode_sequence_preamble(BitWriter& out, Variant /*variant*/, bool extensible,
                              bool extensions_present,
                              Span<const std::uint8_t> optionals_present) {
  if (extensible) {
    out.put_bit(extensions_present);
  }
  encode_bitmap(out, optionals_present);
}

Result<SequencePreamble> decode_sequence_preamble(BitReader& in, Variant /*variant*/,
                                                  bool extensible,
                                                  std::size_t n_optionals) {
  SequencePreamble pre;
  if (extensible) {
    auto ext = in.get_bit();
    if (!ext) {
      return ext.error();
    }
    pre.extensions_present = ext.value();
  }
  auto bm = decode_bitmap(in, n_optionals);
  if (!bm) {
    return bm.error();
  }
  pre.optionals = std::move(bm.value());
  return pre;
}

void encode_extension_additions(BitWriter& out, Variant variant,
                                Span<const std::uint8_t> presence,
                                const std::vector<std::vector<std::uint8_t>>& open_types) {
  const std::size_t n = presence.size();
  if (n == 0) {
    return;
  }
  encode_normally_small_length(out, variant, n);
  encode_bitmap(out, presence);
  std::size_t ot_i = 0;
  for (std::size_t i = 0; i < n; ++i) {
    if (!presence[i]) {
      continue;
    }
    if (ot_i >= open_types.size()) {
      break;
    }
    encode_open_type(out, variant, open_types[ot_i]);
    ++ot_i;
  }
}

/**
 *  Function    : decode_extension_additions
 *  Description : Returns success or an error from decode extension additions.
 *  Parameters  : in — BitReader& in; variant — Variant variant
 *  Returns     : Result<ExtensionAdditions>
 */
Result<ExtensionAdditions> decode_extension_additions(BitReader& in, Variant variant) {
  auto n = decode_normally_small_length(in, variant);
  if (!n) {
    return n.error();
  }
  auto bm = decode_bitmap(in, n.value());
  if (!bm) {
    return bm.error();
  }
  ExtensionAdditions out;
  out.presence = std::move(bm.value());
  for (std::uint8_t bit : out.presence) {
    if (!bit) {
      continue;
    }
    auto ot = decode_open_type(in, variant);
    if (!ot) {
      return ot.error();
    }
    out.open_types.push_back(std::move(ot.value()));
  }
  return out;
}

void encode_choice_root(BitWriter& out, Variant variant, std::size_t index,
                        std::size_t root_count, bool extensible) {
  if (extensible) {
    out.put_bit(false);
  }
  encode_choice_index(out, variant, index, root_count);
}

void encode_choice_extension(BitWriter& out, Variant variant, std::size_t ext_index,
                             Span<const std::uint8_t> open_type_content) {
  out.put_bit(true);
  encode_normally_small_non_negative_whole_number(out, variant, ext_index);
  encode_open_type(out, variant, open_type_content);
}

Result<ExtensionIndex> decode_choice(BitReader& in, Variant variant, std::size_t root_count,
                                     bool extensible) {
  ExtensionIndex idx;
  if (extensible) {
    auto ext = in.get_bit();
    if (!ext) {
      return ext.error();
    }
    if (ext.value()) {
      idx.extension = true;
      auto n = decode_normally_small_non_negative_whole_number(in, variant);
      if (!n) {
        return n.error();
      }
      idx.index = static_cast<std::size_t>(n.value());
      return idx;
    }
  }
  auto root = decode_choice_index(in, variant, root_count);
  if (!root) {
    return root.error();
  }
  idx.index = root.value();
  return idx;
}

Result<std::size_t> decode_choice_root(BitReader& in, Variant variant,
                                       std::size_t root_count, bool extensible) {
  auto idx = decode_choice(in, variant, root_count, extensible);
  if (!idx) {
    return idx.error();
  }
  if (idx.value().extension) {
    return make_error(Error::Code::Unsupported, in.bit_offset(),
                      "CHOICE extension alternative; use decode_choice");
  }
  return idx.value().index;
}

void encode_sequence_of_length(BitWriter& out, Variant variant, std::size_t count,
                               const SizeConstraint& size) {
  encode_size(out, variant, count, size);
}

Result<std::size_t> decode_sequence_of_length(BitReader& in, Variant variant,
                                              const SizeConstraint& size) {
  return decode_size(in, variant, size);
}

std::size_t encode_sequence_of_chunk(BitWriter& out, Variant variant,
                                     std::size_t remaining, bool is_first,
                                     const SizeConstraint& size) {
  if (is_first) {
    if (size.extensible) {
      const bool in_root = in_root_size(remaining, size);
      out.put_bit(!in_root);
      if (in_root) {
        if (size.is_fixed()) {
          return remaining;
        }
        (void)encode_constrained_whole_number(out, variant, static_cast<std::int64_t>(remaining),
                                              static_cast<std::int64_t>(*size.lower),
                                              static_cast<std::int64_t>(*size.upper));
        return remaining;
      }
    } else if (size.is_fully_constrained()) {
      if (size.is_fixed()) {
        return remaining;
      }
      (void)encode_constrained_whole_number(out, variant, static_cast<std::int64_t>(remaining),
                                            static_cast<std::int64_t>(*size.lower),
                                            static_cast<std::int64_t>(*size.upper));
      return remaining;
    }
  }

  maybe_align(out, variant);
  return encode_length_determinant(out, variant, remaining);
}

Result<std::size_t> decode_sequence_of_chunk(BitReader& in, Variant variant,
                                             bool is_first,
                                             const SizeConstraint& size) {
  if (is_first) {
    if (size.extensible) {
      auto ext = in.get_bit();
      if (!ext) {
        return ext.error();
      }
      if (!ext.value()) {
        if (size.is_fixed()) {
          return *size.lower;
        }
        auto v = decode_constrained_whole_number(in, variant,
                                                 static_cast<std::int64_t>(*size.lower),
                                                 static_cast<std::int64_t>(*size.upper));
        if (!v) {
          return v.error();
        }
        return static_cast<std::size_t>(v.value());
      }
    } else if (size.is_fully_constrained()) {
      if (size.is_fixed()) {
        return *size.lower;
      }
      auto v = decode_constrained_whole_number(in, variant,
                                               static_cast<std::int64_t>(*size.lower),
                                               static_cast<std::int64_t>(*size.upper));
      if (!v) {
        return v.error();
      }
      return static_cast<std::size_t>(v.value());
    }
  }

  if (auto a = maybe_align(in, variant); !a) {
    return a.error();
  }
  return decode_length_determinant(in, variant);
}

void encode_enumerated(BitWriter& out, Variant variant, std::size_t root_index,
                       std::size_t root_count, bool extensible) {
  (void)variant;
  if (extensible) {
    out.put_bit(false);  // root value
  }
  if (root_count <= 1) {
    return;
  }
  const std::size_t nbits = bits_for_range(root_count);
  out.put_bits(static_cast<std::uint64_t>(root_index), nbits);
}

/**
 *  Function    : encode_enumerated_extension
 *  Description : Performs encode enumerated extension (definition).
 *  Parameters  : out — BitWriter& out; variant — Variant variant; ext_index — std::size_t ext_index
 *  Returns     : void
 */
void encode_enumerated_extension(BitWriter& out, Variant variant, std::size_t ext_index) {
  out.put_bit(true);
  encode_normally_small_non_negative_whole_number(out, variant, ext_index);
}

Result<ExtensionIndex> decode_enumerated(BitReader& in, Variant variant,
                                         std::size_t root_count, bool extensible) {
  ExtensionIndex result;
  if (extensible) {
    auto ext = in.get_bit();
    if (!ext) {
      return ext.error();
    }
    if (ext.value()) {
      result.extension = true;
      auto n = decode_normally_small_non_negative_whole_number(in, variant);
      if (!n) {
        return n.error();
      }
      result.index = static_cast<std::size_t>(n.value());
      return result;
    }
  }
  if (root_count <= 1) {
    result.index = 0;
    return result;
  }
  const std::size_t nbits = bits_for_range(root_count);
  auto idx = in.get_bits(nbits);
  if (!idx) {
    return idx.error();
  }
  if (idx.value() >= root_count) {
    return make_error(Error::Code::InvalidArgument, in.bit_offset(),
                      "ENUMERATED index out of range");
  }
  result.index = static_cast<std::size_t>(idx.value());
  return result;
}

void encode_object_identifier(BitWriter& out, Variant variant, Span<const std::uint64_t> arcs,
                              bool relative) {
  ByteWriter content;
  if (relative) {
    ber::encode_relative_oid_content(content, arcs);
  } else {
    ber::encode_object_identifier_content(content, arcs);
  }
  maybe_align(out, variant);
  encode_length_chunks(out, variant, content.buffer().size(), content.buffer());
}

Result<std::vector<std::uint64_t>> decode_object_identifier(BitReader& in, Variant variant,
                                                           bool relative) {
  if (auto a = maybe_align(in, variant); !a) {
    return a.error();
  }
  auto bytes = decode_length_chunks(in, variant);
  if (!bytes) {
    return bytes.error();
  }
  ByteReader r(bytes.value());
  if (relative) {
    return ber::decode_relative_oid_content(r, bytes.value().size());
  }
  return ber::decode_object_identifier_content(r, bytes.value().size());
}

/**
 *  Function    : encode_real
 *  Description : Performs encode real (definition).
 *  Parameters  : out — BitWriter& out; variant — Variant variant; value — double value
 *  Returns     : void
 */
void encode_real(BitWriter& out, Variant variant, double value) {
  ByteWriter content;
  ber::encode_real_content(content, value);
  maybe_align(out, variant);
  encode_length_chunks(out, variant, content.buffer().size(), content.buffer());
}

/**
 *  Function    : decode_real
 *  Description : Returns success or an error from decode real.
 *  Parameters  : in — BitReader& in; variant — Variant variant
 *  Returns     : Result<double>
 */
Result<double> decode_real(BitReader& in, Variant variant) {
  if (auto a = maybe_align(in, variant); !a) {
    return a.error();
  }
  auto bytes = decode_length_chunks(in, variant);
  if (!bytes) {
    return bytes.error();
  }
  ByteReader r(bytes.value());
  return ber::decode_real_content(r, bytes.value().size());
}

}  // namespace per
}  // namespace asn1
