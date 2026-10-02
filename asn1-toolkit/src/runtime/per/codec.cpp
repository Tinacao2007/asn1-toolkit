#include <asn1/runtime/per/codec.hpp>
#include <asn1/runtime/ber/codec.hpp>
#include <asn1/runtime/byte_io.hpp>

namespace asn1 {
namespace per {
namespace {

bool in_root_integer(std::int64_t value, const IntegerConstraint& c) {
  if (c.lower && value < *c.lower) {
    return false;
  }
  if (c.upper && value > *c.upper) {
    return false;
  }
  return true;
}

bool in_root_size(std::size_t n, const SizeConstraint& size) {
  if (size.lower && n < *size.lower) {
    return false;
  }
  if (size.upper && n > *size.upper) {
    return false;
  }
  return true;
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
    encode_constrained_whole_number(out, variant, static_cast<std::int64_t>(n),
                                    static_cast<std::int64_t>(*size.lower),
                                    static_cast<std::int64_t>(*size.upper));
    return;
  }
  // Unbound / semi: length determinant of n (semi with lower uses n as absolute length
  // in the common encoding; X.691 semi-constrained length is n - lb as non-neg).
  if (size.lower && !size.upper) {
    encode_semi_constrained_whole_number(out, variant, static_cast<std::int64_t>(n),
                                         static_cast<std::int64_t>(*size.lower));
    return;
  }
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
    return static_cast<std::size_t>(v.value());
  }
  if (size.lower && !size.upper) {
    auto v = decode_semi_constrained_whole_number(in, variant,
                                                  static_cast<std::int64_t>(*size.lower));
    if (!v) {
      return v.error();
    }
    return static_cast<std::size_t>(v.value());
  }
  if (auto a = maybe_align(in, variant); !a) {
    return a.error();
  }
  return decode_length_determinant(in, variant);
}

}  // namespace

void encode_boolean(BitWriter& out, Variant /*variant*/, bool value) {
  out.put_bit(value);
}

Result<bool> decode_boolean(BitReader& in, Variant /*variant*/) {
  return in.get_bit();
}

void encode_null(BitWriter& /*out*/, Variant /*variant*/) {}

Result<void> decode_null(BitReader& /*in*/, Variant /*variant*/) {
  return Result<void>::success();
}

void encode_integer(BitWriter& out, Variant variant, std::int64_t value,
                    const IntegerConstraint& constraint) {
  if (constraint.extensible) {
    const bool root = in_root_integer(value, constraint);
    out.put_bit(!root);
    if (!root) {
      encode_unconstrained_whole_number(out, variant, value);
      return;
    }
  }

  if (constraint.lower && constraint.upper) {
    encode_constrained_whole_number(out, variant, value, *constraint.lower,
                                    *constraint.upper);
    return;
  }
  if (constraint.lower) {
    encode_semi_constrained_whole_number(out, variant, value, *constraint.lower);
    return;
  }
  encode_unconstrained_whole_number(out, variant, value);
}

Result<std::int64_t> decode_integer(BitReader& in, Variant variant,
                                    const IntegerConstraint& constraint) {
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
    return decode_constrained_whole_number(in, variant, *constraint.lower,
                                           *constraint.upper);
  }
  if (constraint.lower) {
    return decode_semi_constrained_whole_number(in, variant, *constraint.lower);
  }
  return decode_unconstrained_whole_number(in, variant);
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
      encode_constrained_whole_number(out, variant,
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
      encode_constrained_whole_number(out, variant,
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
                              Span<const std::uint8_t> optionals_present) {
  if (extensible) {
    out.put_bit(false);  // no extension additions in Phase 9 encode path
  }
  encode_bitmap(out, optionals_present);
}

Result<std::vector<std::uint8_t>> decode_sequence_preamble(BitReader& in,
                                                           Variant /*variant*/,
                                                           bool extensible,
                                                           std::size_t n_optionals) {
  if (extensible) {
    auto ext = in.get_bit();
    if (!ext) {
      return ext.error();
    }
    if (ext.value()) {
      return make_error(Error::Code::Unsupported, in.bit_offset(),
                        "SEQUENCE extension additions not supported in Phase 9");
    }
  }
  return decode_bitmap(in, n_optionals);
}

void encode_choice_root(BitWriter& out, Variant variant, std::size_t index,
                        std::size_t root_count, bool extensible) {
  if (extensible) {
    out.put_bit(false);
  }
  encode_choice_index(out, variant, index, root_count);
}

Result<std::size_t> decode_choice_root(BitReader& in, Variant variant,
                                       std::size_t root_count, bool extensible) {
  if (extensible) {
    auto ext = in.get_bit();
    if (!ext) {
      return ext.error();
    }
    if (ext.value()) {
      return make_error(Error::Code::Unsupported, in.bit_offset(),
                        "CHOICE extension alternative not supported in Phase 9");
    }
  }
  return decode_choice_index(in, variant, root_count);
}

void encode_sequence_of_length(BitWriter& out, Variant variant, std::size_t count,
                               const SizeConstraint& size) {
  encode_size(out, variant, count, size);
}

Result<std::size_t> decode_sequence_of_length(BitReader& in, Variant variant,
                                              const SizeConstraint& size) {
  return decode_size(in, variant, size);
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

Result<std::size_t> decode_enumerated(BitReader& in, Variant variant, std::size_t root_count,
                                      bool extensible) {
  (void)variant;
  if (extensible) {
    auto ext = in.get_bit();
    if (!ext) {
      return ext.error();
    }
    if (ext.value()) {
      return make_error(Error::Code::Unsupported, in.bit_offset(),
                        "ENUMERATED extension value not supported in Phase 12");
    }
  }
  if (root_count <= 1) {
    return std::size_t{0};
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
  return static_cast<std::size_t>(idx.value());
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

}  // namespace per
}  // namespace asn1
