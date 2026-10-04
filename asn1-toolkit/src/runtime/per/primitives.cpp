/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/src/runtime/per/primitives.cpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   PER primitive encode/decode (integers, strings, etc.).
**
** Specification: ITU-T X.691 — ASN.1 encoding rules: Packed Encoding
**                 Rules (PER); UPER/APER variants.
** Design Spec:   asn1-toolkit/docs/ARCHITECTURE.md
**                 asn1-toolkit/README.md
***************************************************************************/
#include <asn1/runtime/per/primitives.hpp>
#include <asn1/runtime/byte_io.hpp>

namespace asn1 {
namespace per {
namespace {

/**
 *  Function    : to_u64
 *  Description : Computes to u64 from (v).
 *  Parameters  : v — std::int64_t v
 *  Returns     : std::uint64_t
 */
std::uint64_t to_u64(std::int64_t v) {
  return static_cast<std::uint64_t>(v);
}

/// Unsigned range size upper-lower+1; assumes upper >= lower.
/// Returns 0 if the span does not fit in uint64 (caller must treat as error).
/**
 *  Function    : range_size
 *  Description : Computes range size from (lower, upper).
 *  Parameters  : lower — std::int64_t lower; upper — std::int64_t upper
 *  Returns     : std::uint64_t
 */
std::uint64_t range_size(std::int64_t lower, std::int64_t upper) {
  const std::uint64_t u_lo = to_u64(lower);
  const std::uint64_t u_hi = to_u64(upper);
  const std::uint64_t diff = u_hi - u_lo;  // well-defined for uint64
  if (diff == UINT64_MAX) {
    return 0;  // lower..upper spans the full int64 domain; +1 overflows
  }
  return diff + 1u;
}

/**
 *  Function    : octet_count_for_nonneg
 *  Description : Computes octet count for nonneg from (value).
 *  Parameters  : value — std::uint64_t value
 *  Returns     : std::size_t
 */
std::size_t octet_count_for_nonneg(std::uint64_t value) {
  if (value == 0) {
    return 1;
  }
  std::size_t bits = 0;
  std::uint64_t x = value;
  while (x > 0) {
    ++bits;
    x >>= 1;
  }
  return (bits + 7) / 8;
}

/**
 *  Function    : encode_nonneg_octets
 *  Description : Performs encode nonneg octets (definition).
 *  Parameters  : out — BitWriter& out; value — std::uint64_t value; nbytes — std::size_t nbytes
 *  Returns     : void
 */
void encode_nonneg_octets(BitWriter& out, std::uint64_t value, std::size_t nbytes) {
  for (std::size_t i = 0; i < nbytes; ++i) {
    const std::size_t shift = 8 * (nbytes - 1 - i);
    out.put_octet(static_cast<std::uint8_t>((value >> shift) & 0xFFu));
  }
}

/**
 *  Function    : decode_nonneg_octets
 *  Description : Returns success or an error from decode nonneg octets.
 *  Parameters  : in — BitReader& in; nbytes — std::size_t nbytes
 *  Returns     : Result<std::uint64_t>
 */
Result<std::uint64_t> decode_nonneg_octets(BitReader& in, std::size_t nbytes) {
  if (nbytes > 8) {
    return make_error(Error::Code::Unsupported, in.bit_offset(),
                      "constrained INTEGER offset wider than 64 bits unsupported");
  }
  std::uint64_t value = 0;
  for (std::size_t i = 0; i < nbytes; ++i) {
    auto b = in.get_octet();
    if (!b) {
      return b.error();
    }
    value = (value << 8) | b.value();
  }
  return value;
}

/**
 *  Function    : max_octets_for_range
 *  Description : Computes max octets for range from (range).
 *  Parameters  : range — std::uint64_t range
 *  Returns     : std::size_t
 */
std::size_t max_octets_for_range(std::uint64_t range) {
  // octets needed to hold values 0..range-1
  if (range <= 1) {
    return 1;
  }
  return (bits_for_range(range) + 7) / 8;
}

}  // namespace

/**
 *  Function    : bits_for_range
 *  Description : Computes bits for range from (range).
 *  Parameters  : range — std::uint64_t range
 *  Returns     : std::size_t
 */
std::size_t bits_for_range(std::uint64_t range) {
  if (range <= 1) {
    return 0;
  }
  std::size_t bits = 0;
  std::uint64_t x = range - 1;
  while (x > 0) {
    ++bits;
    x >>= 1;
  }
  return bits;
}

Result<void> encode_constrained_whole_number(BitWriter& out, Variant variant,
                                             std::int64_t value, std::int64_t lower,
                                             std::int64_t upper) {
  if (upper < lower) {
    return make_error(Error::Code::InvalidArgument, out.bit_size(),
                      "constrained INTEGER upper < lower");
  }
  if (value < lower || value > upper) {
    return make_error(Error::Code::ConstraintViolation, out.bit_size(),
                      "INTEGER value outside constrained range");
  }
  const std::uint64_t range = range_size(lower, upper);
  if (range == 0) {
    return make_error(Error::Code::Unsupported, out.bit_size(),
                      "constrained INTEGER range does not fit in 64-bit encoding");
  }
  const std::uint64_t offset = to_u64(value) - to_u64(lower);

  if (range == 1) {
    return Result<void>::success();
  }
  if (range <= 1) {
    return Result<void>::success();
  }

  if (variant == Variant::Unaligned) {
    out.put_bits(offset, bits_for_range(range));
    return Result<void>::success();
  }

  // Variant::Aligned
  if (range <= 255) {
    out.put_bits(offset, bits_for_range(range));
    return Result<void>::success();
  }
  if (range == 256) {
    maybe_align(out, variant);
    out.put_bits(offset, 8);
    return Result<void>::success();
  }
  if (range <= 65536) {
    maybe_align(out, variant);
    out.put_bits(offset, 16);
    return Result<void>::success();
  }

  // range > 65536: length determinant of octet count of offset, then octets.
  const std::size_t max_octets = max_octets_for_range(range);
  const std::size_t used = octet_count_for_nonneg(offset);
  // Encode (used - 1) as constrained 0..(max_octets-1)
  if (auto er = encode_constrained_whole_number(out, variant,
                                                static_cast<std::int64_t>(used - 1), 0,
                                                static_cast<std::int64_t>(max_octets - 1));
      !er) {
    return er;
  }
  maybe_align(out, variant);
  encode_nonneg_octets(out, offset, used);
  return Result<void>::success();
}

Result<std::int64_t> decode_constrained_whole_number(BitReader& in, Variant variant,
                                                     std::int64_t lower,
                                                     std::int64_t upper) {
  if (upper < lower) {
    return make_error(Error::Code::InvalidArgument, in.bit_offset(),
                      "empty constrained whole number range");
  }
  const std::uint64_t range = range_size(lower, upper);
  if (range == 0) {
    return make_error(Error::Code::Unsupported, in.bit_offset(),
                      "constrained INTEGER range does not fit in 64-bit encoding");
  }
  if (range == 1) {
    return lower;
  }

  std::uint64_t offset = 0;
  if (variant == Variant::Unaligned) {
    auto v = in.get_bits(bits_for_range(range));
    if (!v) {
      return v.error();
    }
    offset = v.value();
  } else if (range <= 255) {
    auto v = in.get_bits(bits_for_range(range));
    if (!v) {
      return v.error();
    }
    offset = v.value();
  } else if (range == 256) {
    if (auto a = maybe_align(in, variant); !a) {
      return a.error();
    }
    auto v = in.get_bits(8);
    if (!v) {
      return v.error();
    }
    offset = v.value();
  } else if (range <= 65536) {
    if (auto a = maybe_align(in, variant); !a) {
      return a.error();
    }
    auto v = in.get_bits(16);
    if (!v) {
      return v.error();
    }
    offset = v.value();
  } else {
    const std::size_t max_octets = max_octets_for_range(range);
    auto used_m1 = decode_constrained_whole_number(
        in, variant, 0, static_cast<std::int64_t>(max_octets - 1));
    if (!used_m1) {
      return used_m1.error();
    }
    const std::size_t used = static_cast<std::size_t>(used_m1.value()) + 1;
    if (auto a = maybe_align(in, variant); !a) {
      return a.error();
    }
    auto v = decode_nonneg_octets(in, used);
    if (!v) {
      return v.error();
    }
    offset = v.value();
  }

  if (offset >= range) {
    return make_error(Error::Code::ConstraintViolation, in.bit_offset(),
                      "constrained whole number offset out of range");
  }
  return static_cast<std::int64_t>(to_u64(lower) + offset);
}

void encode_semi_constrained_whole_number(BitWriter& out, Variant variant,
                                          std::int64_t value, std::int64_t lower) {
  if (value < lower) {
    value = lower;
  }
  const std::uint64_t offset = to_u64(value) - to_u64(lower);
  const std::size_t nbytes = octet_count_for_nonneg(offset);
  maybe_align(out, variant);
  encode_length_determinant(out, variant, nbytes);
  encode_nonneg_octets(out, offset, nbytes);
}

Result<std::int64_t> decode_semi_constrained_whole_number(BitReader& in,
                                                          Variant variant,
                                                          std::int64_t lower) {
  if (auto a = maybe_align(in, variant); !a) {
    return a.error();
  }
  auto len = decode_length_determinant(in, variant);
  if (!len) {
    return len.error();
  }
  auto v = decode_nonneg_octets(in, len.value());
  if (!v) {
    return v.error();
  }
  return static_cast<std::int64_t>(to_u64(lower) + v.value());
}

void encode_unconstrained_whole_number(BitWriter& out, Variant variant,
                                       std::int64_t value) {
  encode_unconstrained_whole_number(out, variant, BigInteger::from_i64(value));
}

void encode_unconstrained_whole_number(BitWriter& out, Variant variant,
                                       const BigInteger& value) {
  const auto bytes = value.to_twos_complement();
  maybe_align(out, variant);
  encode_length_determinant(out, variant, bytes.size());
  out.put_octets(bytes);
}

/**
 *  Function    : decode_unconstrained_big_integer
 *  Description : Returns success or an error from decode unconstrained big integer.
 *  Parameters  : in — BitReader& in; variant — Variant variant
 *  Returns     : Result<BigInteger>
 */
Result<BigInteger> decode_unconstrained_big_integer(BitReader& in, Variant variant) {
  if (auto a = maybe_align(in, variant); !a) {
    return a.error();
  }
  auto len = decode_length_determinant(in, variant);
  if (!len) {
    return len.error();
  }
  auto bytes = in.get_octets(len.value());
  if (!bytes) {
    return bytes.error();
  }
  return BigInteger::from_twos_complement(bytes.value());
}

Result<std::int64_t> decode_unconstrained_whole_number(BitReader& in,
                                                       Variant variant) {
  auto big = decode_unconstrained_big_integer(in, variant);
  if (!big) {
    return big.error();
  }
  auto v = big.value().as_i64();
  if (!v) {
    return make_error(Error::Code::Unsupported, in.bit_offset(),
                      "INTEGER wider than 64 bits; use BigInteger API");
  }
  return *v;
}

void encode_normally_small_non_negative_whole_number(BitWriter& out, Variant variant,
                                                     std::uint64_t value) {
  if (value < 64) {
    out.put_bit(false);
    out.put_bits(value, 6);
    return;
  }
  out.put_bit(true);
  const std::size_t nbytes = octet_count_for_nonneg(value);
  encode_length_determinant(out, variant, nbytes);
  encode_nonneg_octets(out, value, nbytes);
}

Result<std::uint64_t> decode_normally_small_non_negative_whole_number(
    BitReader& in, Variant variant) {
  auto flag = in.get_bit();
  if (!flag) {
    return flag.error();
  }
  if (!flag.value()) {
    return in.get_bits(6);
  }
  auto len = decode_length_determinant(in, variant);
  if (!len) {
    return len.error();
  }
  return decode_nonneg_octets(in, len.value());
}

void encode_normally_small_length(BitWriter& out, Variant variant,
                                  std::size_t length) {
  if (length >= 1 && length <= 64) {
    out.put_bit(false);
    out.put_bits(static_cast<std::uint64_t>(length - 1), 6);
    return;
  }
  out.put_bit(true);
  encode_length_determinant(out, variant, length);
}

/**
 *  Function    : decode_normally_small_length
 *  Description : Returns success or an error from decode normally small length.
 *  Parameters  : in — BitReader& in; variant — Variant variant
 *  Returns     : Result<std::size_t>
 */
Result<std::size_t> decode_normally_small_length(BitReader& in, Variant variant) {
  auto b0 = in.get_bit();
  if (!b0) {
    return b0.error();
  }
  if (!b0.value()) {
    auto v = in.get_bits(6);
    if (!v) {
      return v.error();
    }
    return static_cast<std::size_t>(v.value() + 1);
  }
  auto len = decode_length_determinant(in, variant);
  if (!len) {
    return len.error();
  }
  return len.value();
}

std::size_t encode_length_determinant(BitWriter& out, Variant /*variant*/,
                                      std::size_t length) {
  if (length < 128) {
    out.put_octet(static_cast<std::uint8_t>(length));
    return length;
  }
  if (length < 16384) {
    out.put_octet(static_cast<std::uint8_t>(0x80u | ((length >> 8) & 0x3Fu)));
    out.put_octet(static_cast<std::uint8_t>(length & 0xFFu));
    return length;
  }
  // Fragmentation (X.691): signal 16K/32K/48K/64K chunk.
  std::size_t chunk = 16384;
  std::uint8_t code = 0xC1;
  if (length >= 65536) {
    chunk = 65536;
    code = 0xC4;
  } else if (length >= 49152) {
    chunk = 49152;
    code = 0xC3;
  } else if (length >= 32768) {
    chunk = 32768;
    code = 0xC2;
  }
  out.put_octet(code);
  return chunk;
}

/**
 *  Function    : decode_length_determinant
 *  Description : Returns success or an error from decode length determinant.
 *  Parameters  : in — BitReader& in; variant — Variant variant
 *  Returns     : Result<std::size_t>
 */
Result<std::size_t> decode_length_determinant(BitReader& in, Variant /*variant*/) {
  auto first = in.get_octet();
  if (!first) {
    return first.error();
  }
  const std::uint8_t v = first.value();
  if ((v & 0x80u) == 0) {
    return static_cast<std::size_t>(v);
  }
  if ((v & 0xC0u) == 0x80u) {
    auto second = in.get_octet();
    if (!second) {
      return second.error();
    }
    return (static_cast<std::size_t>(v & 0x3Fu) << 8) | second.value();
  }
  switch (v) {
    case 0xC1:
      return static_cast<std::size_t>(16384);
    case 0xC2:
      return static_cast<std::size_t>(32768);
    case 0xC3:
      return static_cast<std::size_t>(49152);
    case 0xC4:
      return static_cast<std::size_t>(65536);
    default:
      return make_error(Error::Code::InvalidArgument, in.bit_offset() - 8,
                        "bad length determinant fragmentation value");
  }
}

void encode_choice_index(BitWriter& out, Variant variant, std::size_t index,
                         std::size_t count) {
  if (count == 0) {
    return;
  }
  (void)encode_constrained_whole_number(out, variant, static_cast<std::int64_t>(index), 0,
                                         static_cast<std::int64_t>(count - 1));
}

Result<std::size_t> decode_choice_index(BitReader& in, Variant variant,
                                        std::size_t count) {
  if (count == 0) {
    return make_error(Error::Code::InvalidArgument, in.bit_offset(),
                      "CHOICE with zero alternatives");
  }
  auto v = decode_constrained_whole_number(in, variant, 0,
                                           static_cast<std::int64_t>(count - 1));
  if (!v) {
    return v.error();
  }
  return static_cast<std::size_t>(v.value());
}

/**
 *  Function    : encode_bitmap
 *  Description : Performs encode bitmap (definition).
 *  Parameters  : out — BitWriter& out; bits — Span<const std::uint8_t> bits
 *  Returns     : void
 */
void encode_bitmap(BitWriter& out, Span<const std::uint8_t> bits) {
  for (std::size_t i = 0; i < bits.size(); ++i) {
    out.put_bit(bits[i] != 0);
  }
}

/**
 *  Function    : decode_bitmap
 *  Description : Returns success or an error from decode bitmap.
 *  Parameters  : in — BitReader& in; nbits — std::size_t nbits
 *  Returns     : Result<std::vector<std::uint8_t>>
 */
Result<std::vector<std::uint8_t>> decode_bitmap(BitReader& in, std::size_t nbits) {
  std::vector<std::uint8_t> bits;
  bits.reserve(nbits);
  for (std::size_t i = 0; i < nbits; ++i) {
    auto b = in.get_bit();
    if (!b) {
      return b.error();
    }
    bits.push_back(b.value() ? 1 : 0);
  }
  return bits;
}

void encode_open_type(BitWriter& out, Variant variant,
                      Span<const std::uint8_t> content) {
  maybe_align(out, variant);
  // Fragment if needed; Phase 8 encodes single fragment when length < 16K.
  std::size_t remaining = content.size();
  std::size_t offset = 0;
  for (;;) {
    const std::size_t chunk =
        encode_length_determinant(out, variant, remaining);
    out.put_octets(Span<const std::uint8_t>(content.data() + offset, chunk));
    offset += chunk;
    remaining -= chunk;
    if (chunk < 16384) {
      break;
    }
  }
}

/**
 *  Function    : decode_open_type
 *  Description : Returns success or an error from decode open type.
 *  Parameters  : in — BitReader& in; variant — Variant variant
 *  Returns     : Result<std::vector<std::uint8_t>>
 */
Result<std::vector<std::uint8_t>> decode_open_type(BitReader& in, Variant variant) {
  if (auto a = maybe_align(in, variant); !a) {
    return a.error();
  }
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

}  // namespace per
}  // namespace asn1
