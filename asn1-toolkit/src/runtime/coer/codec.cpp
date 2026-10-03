#include <asn1/runtime/coer/codec.hpp>

#include <asn1/runtime/ber/codec.hpp>

#include <algorithm>

namespace asn1 {
namespace coer {
namespace {

Error bad(std::size_t offset, std::string message) {
  return make_error(Error::Code::InvalidArgument, offset, std::move(message));
}

Error non_canonical(std::size_t offset, std::string message) {
  return make_error(Error::Code::NonCanonical, offset, std::move(message));
}

/// Fixed-width OER/COER integer: 1/2/4/8 bytes. Returns 0 if variable-length.
std::size_t fixed_integer_width(const IntegerConstraint& c, bool& is_signed) {
  is_signed = true;
  if (c.extensible || !c.lower.has_value() || !c.upper.has_value()) {
    return 0;
  }
  const std::int64_t lo = *c.lower;
  const std::int64_t hi = *c.upper;
  if (lo >= 0) {
    is_signed = false;
    const std::uint64_t top = static_cast<std::uint64_t>(hi);
    if (top <= 0xFFull) {
      return 1;
    }
    if (top <= 0xFFFFull) {
      return 2;
    }
    if (top <= 0xFFFFFFFFull) {
      return 4;
    }
    return 8;
  }
  if (lo >= -128 && hi <= 127) {
    return 1;
  }
  if (lo >= -32768 && hi <= 32767) {
    return 2;
  }
  if (lo >= -2147483648LL && hi <= 2147483647LL) {
    return 4;
  }
  return 8;
}

bool use_unsigned_variable(const IntegerConstraint& c) {
  return c.lower.has_value() && *c.lower >= 0 &&
         (!c.upper.has_value() || c.extensible);
}

void write_be(ByteWriter& out, std::uint64_t value, std::size_t width) {
  for (std::size_t i = 0; i < width; ++i) {
    const std::size_t shift = 8 * (width - 1 - i);
    out.put(static_cast<std::uint8_t>((value >> shift) & 0xFFu));
  }
}

Result<std::uint64_t> read_be(ByteReader& in, std::size_t width) {
  std::uint64_t value = 0;
  for (std::size_t i = 0; i < width; ++i) {
    auto b = in.get();
    if (!b.ok()) {
      return b.error();
    }
    value = (value << 8) | b.value();
  }
  return value;
}

Result<void> check_signed_integer_minimal(Span<const std::uint8_t> bytes, std::size_t offset) {
  if (bytes.empty()) {
    return bad(offset, "COER INTEGER content empty");
  }
  if (bytes.size() > 1) {
    if (bytes[0] == 0x00u && (bytes[1] & 0x80u) == 0) {
      return non_canonical(offset, "COER INTEGER must use minimal encoding");
    }
    if (bytes[0] == 0xFFu && (bytes[1] & 0x80u) != 0) {
      return non_canonical(offset, "COER INTEGER must use minimal encoding");
    }
  }
  return Result<void>::success();
}

Result<void> check_unsigned_integer_minimal(Span<const std::uint8_t> bytes, std::size_t offset) {
  if (bytes.empty()) {
    return bad(offset, "COER unsigned INTEGER content empty");
  }
  if (bytes.size() > 1 && bytes[0] == 0x00u) {
    return non_canonical(offset, "COER unsigned INTEGER must use minimal encoding");
  }
  return Result<void>::success();
}

Result<void> check_bit_string_unused_zero(const BitStringValue& v, std::size_t offset) {
  if (v.bit_length % 8 == 0 || v.bits.empty()) {
    return Result<void>::success();
  }
  const std::size_t rem = v.bit_length % 8;
  const std::uint8_t unused = static_cast<std::uint8_t>(8 - rem);
  const std::uint8_t mask = static_cast<std::uint8_t>((1u << unused) - 1u);
  if ((v.bits.back() & mask) != 0) {
    return non_canonical(offset, "COER BIT STRING unused bits must be zero");
  }
  return Result<void>::success();
}

void encode_variable_signed_integer(ByteWriter& out, const BigInteger& value) {
  ByteWriter content;
  ber::encode_integer_content(content, value);
  encode_length(out, content.size());
  out.write(Span<const std::uint8_t>(content.buffer().data(), content.size()));
}

void encode_variable_signed_integer(ByteWriter& out, std::int64_t value) {
  encode_variable_signed_integer(out, BigInteger::from_i64(value));
}

Result<BigInteger> decode_variable_signed_big_integer(ByteReader& in) {
  const std::size_t start = in.offset();
  auto len = decode_length(in);
  if (!len.ok()) {
    return len.error();
  }
  auto bytes = in.read(len.value());
  if (!bytes.ok()) {
    return bytes.error();
  }
  if (auto chk = check_signed_integer_minimal(bytes.value(), start); !chk.ok()) {
    return chk.error();
  }
  return BigInteger::from_twos_complement(bytes.value());
}

Result<std::int64_t> decode_variable_signed_integer(ByteReader& in) {
  auto big = decode_variable_signed_big_integer(in);
  if (!big.ok()) {
    return big.error();
  }
  auto v = big.value().as_i64();
  if (!v) {
    return bad(in.offset(), "COER INTEGER wider than 64 bits; use decode_big_integer");
  }
  return *v;
}

void encode_variable_unsigned_integer(ByteWriter& out, const BigInteger& value) {
  auto bytes = value.to_unsigned_bytes();
  if (!bytes.ok()) {
    encode_length(out, 1);
    out.put(0);
    return;
  }
  encode_length(out, bytes.value().size());
  out.write(Span<const std::uint8_t>(bytes.value().data(), bytes.value().size()));
}

void encode_variable_unsigned_integer(ByteWriter& out, std::uint64_t value) {
  encode_variable_unsigned_integer(out, BigInteger::from_u64(value));
}

Result<BigInteger> decode_variable_unsigned_big_integer(ByteReader& in) {
  const std::size_t start = in.offset();
  auto len = decode_length(in);
  if (!len.ok()) {
    return len.error();
  }
  if (len.value() == 0) {
    return bad(in.offset(), "COER unsigned INTEGER length out of range");
  }
  auto bytes = in.read(len.value());
  if (!bytes.ok()) {
    return bytes.error();
  }
  if (auto chk = check_unsigned_integer_minimal(bytes.value(), start); !chk.ok()) {
    return chk.error();
  }
  return BigInteger::from_unsigned_bytes(bytes.value());
}

Result<std::uint64_t> decode_variable_unsigned_integer(ByteReader& in) {
  auto big = decode_variable_unsigned_big_integer(in);
  if (!big.ok()) {
    return big.error();
  }
  auto v = big.value().as_u64();
  if (!v) {
    return bad(in.offset(), "COER unsigned INTEGER wider than 64 bits; use decode_big_integer");
  }
  return *v;
}

}  // namespace

void encode_length(ByteWriter& out, std::size_t length) {
  // Already shortest-form (same as BASIC-OER encoder).
  oer::encode_length(out, length);
}

Result<std::size_t> decode_length(ByteReader& in) {
  auto first = in.get();
  if (!first.ok()) {
    return first.error();
  }
  if ((first.value() & 0x80u) == 0) {
    return static_cast<std::size_t>(first.value());
  }
  const std::size_t n = first.value() & 0x7Fu;
  if (n == 0 || n > sizeof(std::size_t)) {
    return bad(in.offset(), "invalid COER length determinant");
  }
  std::size_t value = 0;
  for (std::size_t i = 0; i < n; ++i) {
    auto b = in.get();
    if (!b.ok()) {
      return b.error();
    }
    if (i == 0 && b.value() == 0) {
      return non_canonical(in.offset(), "COER length determinant must be minimal");
    }
    value = (value << 8) | b.value();
  }
  // Long form must not be used when short form would suffice.
  if (value < 128) {
    return non_canonical(in.offset(),
                         "COER length determinant must use short form for values < 128");
  }
  // Also reject over-long form: n must be the minimum octets for `value`.
  std::size_t need = 1;
  std::size_t tmp = value;
  while (tmp > 0xFFu) {
    tmp >>= 8;
    ++need;
  }
  if (n != need) {
    return non_canonical(in.offset(), "COER length determinant must use minimal octets");
  }
  return value;
}

void encode_boolean(ByteWriter& out, bool value) {
  oer::encode_boolean(out, value);
}

Result<bool> decode_boolean(ByteReader& in) {
  auto b = in.get();
  if (!b.ok()) {
    return b.error();
  }
  if (b.value() == 0x00u) {
    return false;
  }
  if (b.value() == 0xFFu) {
    return true;
  }
  return non_canonical(in.offset(), "COER BOOLEAN TRUE must be 0xFF");
}

void encode_null(ByteWriter& out) {
  oer::encode_null(out);
}

Result<void> decode_null(ByteReader& in) {
  return oer::decode_null(in);
}

void encode_integer(ByteWriter& out, std::int64_t value, const IntegerConstraint& constraint) {
  bool is_signed = true;
  const std::size_t width = fixed_integer_width(constraint, is_signed);
  if (width != 0) {
    write_be(out, static_cast<std::uint64_t>(value), width);
    return;
  }
  if (use_unsigned_variable(constraint)) {
    encode_variable_unsigned_integer(out, static_cast<std::uint64_t>(value));
    return;
  }
  encode_variable_signed_integer(out, value);
}

void encode_integer(ByteWriter& out, const BigInteger& value,
                    const IntegerConstraint& constraint) {
  auto narrow = value.as_i64();
  if (narrow && (constraint.lower || constraint.upper)) {
    coer::encode_integer(out, *narrow, constraint);
    return;
  }
  if (use_unsigned_variable(constraint)) {
    encode_variable_unsigned_integer(out, value);
    return;
  }
  encode_variable_signed_integer(out, value);
}

Result<std::int64_t> decode_integer(ByteReader& in, const IntegerConstraint& constraint) {
  bool is_signed = true;
  const std::size_t width = fixed_integer_width(constraint, is_signed);
  if (width != 0) {
    auto raw = read_be(in, width);
    if (!raw.ok()) {
      return raw.error();
    }
    if (!is_signed) {
      return static_cast<std::int64_t>(raw.value());
    }
    std::int64_t value = static_cast<std::int64_t>(raw.value());
    if (width < 8) {
      const std::uint64_t sign_bit = 1ull << (8 * width - 1);
      if (raw.value() & sign_bit) {
        const std::uint64_t mask = ~((1ull << (8 * width)) - 1ull);
        value = static_cast<std::int64_t>(raw.value() | mask);
      }
    }
    return value;
  }
  if (use_unsigned_variable(constraint)) {
    auto u = decode_variable_unsigned_integer(in);
    if (!u.ok()) {
      return u.error();
    }
    return static_cast<std::int64_t>(u.value());
  }
  return decode_variable_signed_integer(in);
}

Result<BigInteger> decode_big_integer(ByteReader& in, const IntegerConstraint& constraint) {
  bool is_signed = true;
  const std::size_t width = fixed_integer_width(constraint, is_signed);
  if (width != 0) {
    auto v = coer::decode_integer(in, constraint);
    if (!v.ok()) {
      return v.error();
    }
    return BigInteger::from_i64(v.value());
  }
  if (use_unsigned_variable(constraint)) {
    return decode_variable_unsigned_big_integer(in);
  }
  return decode_variable_signed_big_integer(in);
}

void encode_octet_string(ByteWriter& out, Span<const std::uint8_t> value,
                         const SizeConstraint& size) {
  if (size.is_fixed()) {
    out.write(value);
    return;
  }
  encode_length(out, value.size());
  out.write(value);
}

Result<std::vector<std::uint8_t>> decode_octet_string(ByteReader& in,
                                                      const SizeConstraint& size) {
  std::size_t n = 0;
  if (size.is_fixed()) {
    n = *size.lower;
  } else {
    auto len = decode_length(in);
    if (!len.ok()) {
      return len.error();
    }
    n = len.value();
  }
  auto bytes = in.read(n);
  if (!bytes.ok()) {
    return bytes.error();
  }
  return std::vector<std::uint8_t>(bytes.value().begin(), bytes.value().end());
}

void encode_bit_string(ByteWriter& out, Span<const std::uint8_t> bits, std::size_t bit_length,
                       const SizeConstraint& size) {
  // OER encoder already clears unused trailing bits.
  oer::encode_bit_string(out, bits, bit_length, size);
}

Result<BitStringValue> decode_bit_string(ByteReader& in, const SizeConstraint& size) {
  const std::size_t start = in.offset();
  BitStringValue out;
  if (size.is_fixed()) {
    const std::size_t bit_length = *size.lower;
    const std::size_t n = (bit_length + 7) / 8;
    auto bytes = in.read(n);
    if (!bytes.ok()) {
      return bytes.error();
    }
    out.bits.assign(bytes.value().begin(), bytes.value().end());
    out.bit_length = bit_length;
    if (auto chk = check_bit_string_unused_zero(out, start); !chk.ok()) {
      return chk.error();
    }
    return out;
  }
  auto len = decode_length(in);
  if (!len.ok()) {
    return len.error();
  }
  if (len.value() < 1) {
    return bad(in.offset(), "COER BIT STRING length too small");
  }
  auto unused = in.get();
  if (!unused.ok()) {
    return unused.error();
  }
  if (unused.value() > 7) {
    return bad(in.offset(), "COER BIT STRING unused bits must be 0..7");
  }
  const std::size_t n = len.value() - 1;
  if (n == 0 && unused.value() != 0) {
    return bad(in.offset(), "empty COER BIT STRING must have 0 unused bits");
  }
  auto bytes = in.read(n);
  if (!bytes.ok()) {
    return bytes.error();
  }
  out.bits.assign(bytes.value().begin(), bytes.value().end());
  out.bit_length = 8 * n - unused.value();
  if (auto chk = check_bit_string_unused_zero(out, start); !chk.ok()) {
    return chk.error();
  }
  return out;
}

void encode_utf8_string(ByteWriter& out, const std::string& value,
                        const SizeConstraint& size) {
  coer::encode_octet_string(
      out,
      Span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(value.data()),
                               value.size()),
      size);
}

Result<std::string> decode_utf8_string(ByteReader& in, const SizeConstraint& size) {
  auto bytes = coer::decode_octet_string(in, size);
  if (!bytes.ok()) {
    return bytes.error();
  }
  return std::string(reinterpret_cast<const char*>(bytes.value().data()), bytes.value().size());
}

void encode_enumerated(ByteWriter& out, std::int64_t value) {
  oer::encode_enumerated(out, value);
}

Result<std::int64_t> decode_enumerated(ByteReader& in) {
  auto peek = in.peek();
  if (!peek.ok()) {
    return peek.error();
  }
  if ((peek.value() & 0x80u) == 0) {
    auto b = in.get();
    if (!b.ok()) {
      return b.error();
    }
    return static_cast<std::int64_t>(b.value());
  }
  // Long form: distinguishing bit on length octet, low 7 bits = content length
  // (Phase 16: content length 1..127, sufficient for int64 enumeration values).
  auto first = in.get();
  if (!first.ok()) {
    return first.error();
  }
  const std::size_t length = first.value() & 0x7Fu;
  if (length == 0) {
    return bad(in.offset(), "invalid COER ENUMERATED length");
  }
  const std::size_t content_start = in.offset();
  auto bytes = in.read(length);
  if (!bytes.ok()) {
    return bytes.error();
  }
  if (auto chk = check_signed_integer_minimal(bytes.value(), content_start); !chk.ok()) {
    return chk.error();
  }
  ByteReader cr(bytes.value());
  auto value = ber::decode_integer_content(cr, length);
  if (!value.ok()) {
    return value.error();
  }
  if (value.value() >= 0 && value.value() <= 127) {
    return non_canonical(content_start,
                         "COER ENUMERATED 0..127 must use short form");
  }
  return value;
}

void encode_object_identifier(ByteWriter& out, Span<const std::uint64_t> arcs, bool relative) {
  ByteWriter content;
  if (relative) {
    ber::encode_relative_oid_content(content, arcs);
  } else {
    ber::encode_object_identifier_content(content, arcs);
  }
  encode_length(out, content.size());
  out.write(Span<const std::uint8_t>(content.buffer().data(), content.size()));
}

Result<std::vector<std::uint64_t>> decode_object_identifier(ByteReader& in, bool relative) {
  auto len = decode_length(in);
  if (!len.ok()) {
    return len.error();
  }
  auto bytes = in.read(len.value());
  if (!bytes.ok()) {
    return bytes.error();
  }
  ByteReader cr(bytes.value());
  if (relative) {
    return ber::decode_relative_oid_content(cr, len.value());
  }
  return ber::decode_object_identifier_content(cr, len.value());
}

Result<void> encode_real(ByteWriter& out, double value, RealIeeeForm form) {
  // Unconstrained REAL uses DER content octets; IEEE forms are already canonical.
  return oer::encode_real(out, value, form);
}

Result<double> decode_real(ByteReader& in, RealIeeeForm form) {
  return oer::decode_real(in, form);
}

void encode_sequence_preamble(ByteWriter& out, bool extensible, bool extensions_present,
                              Span<const bool> optionals_present) {
  // Padding bits are already zero in the OER encoder.
  oer::encode_sequence_preamble(out, extensible, extensions_present, optionals_present);
}

Result<SequencePreamble> decode_sequence_preamble(ByteReader& in, bool extensible,
                                                   std::size_t n_optionals) {
  SequencePreamble out;
  const std::size_t n_bits = (extensible ? 1u : 0u) + n_optionals;
  if (n_bits == 0) {
    return out;
  }
  const std::size_t n_bytes = (n_bits + 7) / 8;
  const std::size_t start = in.offset();
  auto bytes = in.read(n_bytes);
  if (!bytes.ok()) {
    return bytes.error();
  }
  // Padding bits (after the used preamble bits) must be zero.
  const std::size_t pad_bits = n_bytes * 8 - n_bits;
  if (pad_bits > 0) {
    const std::uint8_t last = bytes.value()[bytes.value().size() - 1];
    const std::uint8_t mask = static_cast<std::uint8_t>((1u << pad_bits) - 1u);
    if ((last & mask) != 0) {
      return non_canonical(start, "COER SEQUENCE preamble padding bits must be zero");
    }
  }
  std::size_t bit = 0;
  auto get_bit = [&]() -> bool {
    const bool v = (bytes.value()[bit / 8] & (0x80u >> (bit % 8))) != 0;
    ++bit;
    return v;
  };
  if (extensible) {
    out.extensions_present = get_bit();
  }
  out.optionals.reserve(n_optionals);
  for (std::size_t i = 0; i < n_optionals; ++i) {
    out.optionals.push_back(get_bit());
  }
  return out;
}

void encode_choice_tag(ByteWriter& out, std::uint64_t tag_number, bool constructed) {
  oer::encode_choice_tag(out, tag_number, constructed);
}

Result<std::uint64_t> decode_choice_tag(ByteReader& in) {
  // Tag encodings from BASIC-OER encoder are already canonical (minimal).
  return oer::decode_choice_tag(in);
}

void encode_sequence_of_length(ByteWriter& out, std::size_t count) {
  encode_length(out, count);
}

Result<std::size_t> decode_sequence_of_length(ByteReader& in) {
  return decode_length(in);
}

void encode_set_of(ByteWriter& out, std::vector<std::vector<std::uint8_t>> components) {
  std::sort(components.begin(), components.end());
  encode_length(out, components.size());
  for (const auto& c : components) {
    out.write(Span<const std::uint8_t>(c.data(), c.size()));
  }
}

Result<void> require_set_of_order(Span<const std::vector<std::uint8_t>> components) {
  for (std::size_t i = 1; i < components.size(); ++i) {
    if (components[i] < components[i - 1]) {
      return non_canonical(0, "COER SET OF components must be in ascending order");
    }
  }
  return Result<void>::success();
}

}  // namespace coer
}  // namespace asn1
