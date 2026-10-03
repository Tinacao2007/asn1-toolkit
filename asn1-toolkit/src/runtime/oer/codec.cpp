#include <asn1/runtime/oer/codec.hpp>

#include <asn1/runtime/ber/codec.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace asn1 {
namespace oer {
namespace {

Error bad(std::size_t offset, std::string message) {
  return make_error(Error::Code::InvalidArgument, offset, std::move(message));
}

/// Fixed-width OER integer: 1/2/4/8 bytes. Returns 0 if variable-length.
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
  auto len = decode_length(in);
  if (!len.ok()) {
    return len.error();
  }
  return ber::decode_big_integer_content(in, len.value());
}

Result<std::int64_t> decode_variable_signed_integer(ByteReader& in) {
  auto big = decode_variable_signed_big_integer(in);
  if (!big.ok()) {
    return big.error();
  }
  auto v = big.value().as_i64();
  if (!v) {
    return bad(in.offset(), "OER INTEGER wider than 64 bits; use decode_big_integer");
  }
  return *v;
}

void encode_variable_unsigned_integer(ByteWriter& out, const BigInteger& value) {
  auto bytes = value.to_unsigned_bytes();
  if (!bytes.ok()) {
    // Should not happen for non-negative values used by callers.
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
  auto len = decode_length(in);
  if (!len.ok()) {
    return len.error();
  }
  if (len.value() == 0) {
    return bad(in.offset(), "OER unsigned INTEGER length out of range");
  }
  auto bytes = in.read(len.value());
  if (!bytes.ok()) {
    return bytes.error();
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
    return bad(in.offset(), "OER unsigned INTEGER wider than 64 bits; use decode_big_integer");
  }
  return *v;
}

}  // namespace

void encode_length(ByteWriter& out, std::size_t length) {
  if (length < 128) {
    out.put(static_cast<std::uint8_t>(length));
    return;
  }
  std::uint8_t tmp[8];
  std::size_t n = 0;
  std::size_t v = length;
  while (v > 0) {
    tmp[n++] = static_cast<std::uint8_t>(v & 0xFFu);
    v >>= 8;
  }
  out.put(static_cast<std::uint8_t>(0x80u | n));
  for (std::size_t i = 0; i < n; ++i) {
    out.put(tmp[n - 1 - i]);
  }
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
    return bad(in.offset(), "invalid OER length determinant");
  }
  std::size_t value = 0;
  for (std::size_t i = 0; i < n; ++i) {
    auto b = in.get();
    if (!b.ok()) {
      return b.error();
    }
    value = (value << 8) | b.value();
  }
  return value;
}

void encode_boolean(ByteWriter& out, bool value) {
  out.put(value ? 0xFFu : 0x00u);
}

Result<bool> decode_boolean(ByteReader& in) {
  auto b = in.get();
  if (!b.ok()) {
    return b.error();
  }
  return b.value() != 0;
}

void encode_null(ByteWriter&) {}

Result<void> decode_null(ByteReader&) {
  return Result<void>::success();
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
    encode_integer(out, *narrow, constraint);
    return;
  }
  if (use_unsigned_variable(constraint)) {
    encode_variable_unsigned_integer(out, value);
    return;
  }
  encode_variable_signed_integer(out, value);
}

Result<std::int64_t> decode_integer(ByteReader& in, const IntegerConstraint& constraint) {
  std::int64_t val = 0;
  bool is_signed = true;
  const std::size_t width = fixed_integer_width(constraint, is_signed);
  if (width != 0) {
    auto raw = read_be(in, width);
    if (!raw.ok()) {
      return raw.error();
    }
    if (!is_signed) {
      val = static_cast<std::int64_t>(raw.value());
    } else {
      std::int64_t value = static_cast<std::int64_t>(raw.value());
      if (width < 8) {
        const std::uint64_t sign_bit = 1ull << (8 * width - 1);
        if (raw.value() & sign_bit) {
          const std::uint64_t mask = ~((1ull << (8 * width)) - 1ull);
          value = static_cast<std::int64_t>(raw.value() | mask);
        }
      }
      val = value;
    }
  } else if (use_unsigned_variable(constraint)) {
    auto u = decode_variable_unsigned_integer(in);
    if (!u.ok()) {
      return u.error();
    }
    val = static_cast<std::int64_t>(u.value());
  } else {
    auto s = decode_variable_signed_integer(in);
    if (!s.ok()) {
      return s.error();
    }
    val = s.value();
  }
  if (!constraint.contains(val)) {
    return make_error(Error::Code::ConstraintViolation, in.offset(),
                      "decoded integer violates constraint");
  }
  return val;
}

Result<BigInteger> decode_big_integer(ByteReader& in, const IntegerConstraint& constraint) {
  bool is_signed = true;
  const std::size_t width = fixed_integer_width(constraint, is_signed);
  if (width != 0) {
    auto v = decode_integer(in, constraint);
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
  if (!size.contains(n)) {
    return make_error(Error::Code::ConstraintViolation, in.offset(),
                      "decoded octet string length violates constraint");
  }
  auto bytes = in.read(n);
  if (!bytes.ok()) {
    return bytes.error();
  }
  return std::vector<std::uint8_t>(bytes.value().begin(), bytes.value().end());
}

void encode_bit_string(ByteWriter& out, Span<const std::uint8_t> bits, std::size_t bit_length,
                       const SizeConstraint& size) {
  const std::size_t full_bytes = bit_length / 8;
  const std::size_t rem = bit_length % 8;
  std::vector<std::uint8_t> data;
  data.reserve(full_bytes + (rem ? 1 : 0));
  for (std::size_t i = 0; i < full_bytes; ++i) {
    data.push_back(i < bits.size() ? bits[i] : 0);
  }
  std::uint8_t unused = 0;
  if (rem != 0) {
    std::uint8_t last = full_bytes < bits.size() ? bits[full_bytes] : 0;
    last &= static_cast<std::uint8_t>(0xFFu << (8 - rem));
    data.push_back(last);
    unused = static_cast<std::uint8_t>(8 - rem);
  }
  if (size.is_fixed()) {
    out.write(Span<const std::uint8_t>(data.data(), data.size()));
    return;
  }
  encode_length(out, data.size() + 1);
  out.put(unused);
  out.write(Span<const std::uint8_t>(data.data(), data.size()));
}

Result<BitStringValue> decode_bit_string(ByteReader& in, const SizeConstraint& size) {
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
    return out;
  }
  auto len = decode_length(in);
  if (!len.ok()) {
    return len.error();
  }
  if (len.value() < 1) {
    return bad(in.offset(), "OER BIT STRING length too small");
  }
  auto unused = in.get();
  if (!unused.ok()) {
    return unused.error();
  }
  if (unused.value() > 7) {
    return bad(in.offset(), "OER BIT STRING unused bits must be 0..7");
  }
  const std::size_t n = len.value() - 1;
  if (n == 0 && unused.value() != 0) {
    return bad(in.offset(), "empty OER BIT STRING must have 0 unused bits");
  }
  auto bytes = in.read(n);
  if (!bytes.ok()) {
    return bytes.error();
  }
  out.bits.assign(bytes.value().begin(), bytes.value().end());
  out.bit_length = 8 * n - unused.value();
  return out;
}

void encode_utf8_string(ByteWriter& out, const std::string& value,
                        const SizeConstraint& size) {
  encode_octet_string(out,
                      Span<const std::uint8_t>(
                          reinterpret_cast<const std::uint8_t*>(value.data()), value.size()),
                      size);
}

Result<std::string> decode_utf8_string(ByteReader& in, const SizeConstraint& size) {
  auto bytes = decode_octet_string(in, size);
  if (!bytes.ok()) {
    return bytes.error();
  }
  return std::string(reinterpret_cast<const char*>(bytes.value().data()), bytes.value().size());
}

void encode_enumerated(ByteWriter& out, std::int64_t value) {
  if (value >= 0 && value <= 127) {
    out.put(static_cast<std::uint8_t>(value));
    return;
  }
  ByteWriter tmp;
  encode_variable_signed_integer(tmp, value);
  auto& buf = tmp.buffer();
  if (!buf.empty()) {
    buf[0] |= 0x80u;
  }
  out.write(Span<const std::uint8_t>(buf.data(), buf.size()));
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
  auto first = in.get();
  if (!first.ok()) {
    return first.error();
  }
  const std::uint8_t cleared = static_cast<std::uint8_t>(first.value() & 0x7Fu);
  if (cleared & 0x80u) {
    const std::size_t n = cleared & 0x7Fu;
    std::size_t length = 0;
    for (std::size_t i = 0; i < n; ++i) {
      auto b = in.get();
      if (!b.ok()) {
        return b.error();
      }
      length = (length << 8) | b.value();
    }
    return ber::decode_integer_content(in, length);
  }
  return ber::decode_integer_content(in, cleared);
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

namespace {

void write_be32(ByteWriter& out, std::uint32_t bits) {
  out.put(static_cast<std::uint8_t>((bits >> 24) & 0xFFu));
  out.put(static_cast<std::uint8_t>((bits >> 16) & 0xFFu));
  out.put(static_cast<std::uint8_t>((bits >> 8) & 0xFFu));
  out.put(static_cast<std::uint8_t>(bits & 0xFFu));
}

void write_be64(ByteWriter& out, std::uint64_t bits) {
  for (int shift = 56; shift >= 0; shift -= 8) {
    out.put(static_cast<std::uint8_t>((bits >> shift) & 0xFFu));
  }
}

Result<std::uint32_t> read_be32(ByteReader& in) {
  auto bytes = in.read(4);
  if (!bytes.ok()) {
    return bytes.error();
  }
  const auto b = bytes.value();
  return (static_cast<std::uint32_t>(b[0]) << 24) | (static_cast<std::uint32_t>(b[1]) << 16) |
         (static_cast<std::uint32_t>(b[2]) << 8) | static_cast<std::uint32_t>(b[3]);
}

Result<std::uint64_t> read_be64(ByteReader& in) {
  auto bytes = in.read(8);
  if (!bytes.ok()) {
    return bytes.error();
  }
  const auto b = bytes.value();
  std::uint64_t bits = 0;
  for (std::size_t i = 0; i < 8; ++i) {
    bits = (bits << 8) | b[i];
  }
  return bits;
}

}  // namespace

Result<void> encode_real(ByteWriter& out, double value, RealIeeeForm form) {
  if (form == RealIeeeForm::Unconstrained) {
    ByteWriter content;
    ber::encode_real_content(content, value);
    encode_length(out, content.size());
    out.write(Span<const std::uint8_t>(content.buffer().data(), content.size()));
    return Result<void>::success();
  }
  if (form == RealIeeeForm::Binary32) {
    const float f = static_cast<float>(value);
    if (std::isfinite(value) && !std::isfinite(f)) {
      return bad(0, "REAL value overflows IEEE 754 binary32");
    }
    std::uint32_t bits = 0;
    std::memcpy(&bits, &f, sizeof(bits));
    write_be32(out, bits);
    return Result<void>::success();
  }
  // Binary64: host double is already IEEE-754 binary64.
  if (std::isfinite(value)) {
    // No overflow possible into binary64 from double; still reject signaling issues.
  } else if (!std::isnan(value) && !std::isinf(value)) {
    return bad(0, "invalid REAL value for IEEE 754 binary64");
  }
  std::uint64_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  write_be64(out, bits);
  return Result<void>::success();
}

Result<double> decode_real(ByteReader& in, RealIeeeForm form) {
  if (form == RealIeeeForm::Unconstrained) {
    auto len = decode_length(in);
    if (!len.ok()) {
      return len.error();
    }
    return ber::decode_real_content(in, len.value());
  }
  if (form == RealIeeeForm::Binary32) {
    auto bits = read_be32(in);
    if (!bits.ok()) {
      return bits.error();
    }
    float f = 0.0f;
    const std::uint32_t u = bits.value();
    std::memcpy(&f, &u, sizeof(f));
    return static_cast<double>(f);
  }
  auto bits = read_be64(in);
  if (!bits.ok()) {
    return bits.error();
  }
  double d = 0.0;
  const std::uint64_t u = bits.value();
  std::memcpy(&d, &u, sizeof(d));
  return d;
}

void encode_sequence_preamble(ByteWriter& out, bool extensible, bool extensions_present,
                              Span<const bool> optionals_present) {
  const std::size_t n_bits = (extensible ? 1u : 0u) + optionals_present.size();
  if (n_bits == 0) {
    return;
  }
  const std::size_t n_bytes = (n_bits + 7) / 8;
  std::vector<std::uint8_t> buf(n_bytes, 0);
  std::size_t bit = 0;
  auto put_bit = [&](bool v) {
    if (v) {
      buf[bit / 8] |= static_cast<std::uint8_t>(0x80u >> (bit % 8));
    }
    ++bit;
  };
  if (extensible) {
    put_bit(extensions_present);
  }
  for (bool p : optionals_present) {
    put_bit(p);
  }
  out.write(Span<const std::uint8_t>(buf.data(), buf.size()));
}

Result<SequencePreamble> decode_sequence_preamble(ByteReader& in, bool extensible,
                                                  std::size_t n_optionals) {
  SequencePreamble out;
  const std::size_t n_bits = (extensible ? 1u : 0u) + n_optionals;
  if (n_bits == 0) {
    return out;
  }
  const std::size_t n_bytes = (n_bits + 7) / 8;
  auto bytes = in.read(n_bytes);
  if (!bytes.ok()) {
    return bytes.error();
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

void encode_open_type(ByteWriter& out, Span<const std::uint8_t> content) {
  encode_length(out, content.size());
  out.write(content);
}

Result<std::vector<std::uint8_t>> decode_open_type(ByteReader& in) {
  auto len = decode_length(in);
  if (!len) {
    return len.error();
  }
  auto bytes = in.read(len.value());
  if (!bytes) {
    return bytes.error();
  }
  return std::vector<std::uint8_t>(bytes.value().begin(), bytes.value().end());
}

void encode_extension_additions(ByteWriter& out, Span<const bool> presence,
                                const std::vector<std::vector<std::uint8_t>>& open_types) {
  const std::size_t n = presence.size();
  if (n == 0) {
    return;
  }
  const std::size_t unused = (8u - (n % 8u)) % 8u;
  const std::size_t n_subsequent = (n + 7u) / 8u;
  encode_length(out, 1u + n_subsequent);
  out.put(static_cast<std::uint8_t>(unused));
  std::vector<std::uint8_t> bm(n_subsequent, 0);
  for (std::size_t i = 0; i < n; ++i) {
    if (presence[i]) {
      bm[i / 8] |= static_cast<std::uint8_t>(0x80u >> (i % 8));
    }
  }
  out.write(Span<const std::uint8_t>(bm.data(), bm.size()));
  std::size_t ot_i = 0;
  for (std::size_t i = 0; i < n; ++i) {
    if (!presence[i]) {
      continue;
    }
    if (ot_i >= open_types.size()) {
      break;
    }
    encode_open_type(out, open_types[ot_i]);
    ++ot_i;
  }
}

Result<ExtensionAdditions> decode_extension_additions(ByteReader& in) {
  auto len = decode_length(in);
  if (!len) {
    return len.error();
  }
  if (len.value() < 1) {
    return make_error(Error::Code::InvalidArgument, in.offset(),
                      "OER extension bitmap length too short");
  }
  auto unused_r = in.get();
  if (!unused_r) {
    return unused_r.error();
  }
  const std::uint8_t unused = unused_r.value();
  if (unused > 7) {
    return make_error(Error::Code::InvalidArgument, in.offset(),
                      "OER extension bitmap unused bits must be 0..7");
  }
  const std::size_t n_subsequent = len.value() - 1u;
  auto bm_bytes = in.read(n_subsequent);
  if (!bm_bytes) {
    return bm_bytes.error();
  }
  if (n_subsequent == 0) {
    if (unused != 0) {
      return make_error(Error::Code::InvalidArgument, in.offset(),
                        "OER empty extension bitmap must have 0 unused bits");
    }
    return ExtensionAdditions{};
  }
  const std::size_t n_bits = n_subsequent * 8u - unused;
  // Unused bits in the final octet must be zero.
  if (unused > 0) {
    const std::uint8_t last = bm_bytes.value()[n_subsequent - 1];
    const std::uint8_t mask = static_cast<std::uint8_t>((1u << unused) - 1u);
    if ((last & mask) != 0) {
      return make_error(Error::Code::InvalidArgument, in.offset(),
                        "OER extension bitmap unused bits must be zero");
    }
  }
  ExtensionAdditions out;
  out.presence.reserve(n_bits);
  for (std::size_t i = 0; i < n_bits; ++i) {
    const bool bit = (bm_bytes.value()[i / 8] & (0x80u >> (i % 8))) != 0;
    out.presence.push_back(bit);
  }
  for (bool present : out.presence) {
    if (!present) {
      continue;
    }
    auto ot = decode_open_type(in);
    if (!ot) {
      return ot.error();
    }
    out.open_types.push_back(std::move(ot.value()));
  }
  return out;
}

void encode_choice_tag(ByteWriter& out, std::uint64_t tag_number, bool constructed) {
  std::uint8_t flags = 0x80u;
  if (constructed) {
    flags |= 0x20u;
  }
  if (tag_number < 63) {
    out.put(static_cast<std::uint8_t>(flags | tag_number));
    return;
  }
  out.put(static_cast<std::uint8_t>(flags | 0x3Fu));
  std::uint8_t tmp[10];
  std::size_t n = 0;
  std::uint64_t v = tag_number;
  do {
    tmp[n++] = static_cast<std::uint8_t>(0x80u | (v & 0x7Fu));
    v >>= 7;
  } while (v > 0);
  tmp[0] &= 0x7Fu;
  for (std::size_t i = 0; i < n; ++i) {
    out.put(tmp[n - 1 - i]);
  }
}

Result<std::uint64_t> decode_choice_tag(ByteReader& in) {
  auto first = in.get();
  if (!first.ok()) {
    return first.error();
  }
  const std::uint64_t low = first.value() & 0x3Fu;
  if (low != 0x3Fu) {
    return low;
  }
  std::uint64_t number = 0;
  for (;;) {
    auto b = in.get();
    if (!b.ok()) {
      return b.error();
    }
    number = (number << 7) | (b.value() & 0x7Fu);
    if ((b.value() & 0x80u) == 0) {
      break;
    }
  }
  return number;
}

void encode_sequence_of_length(ByteWriter& out, std::size_t count) {
  // ITU-T X.696 clause 17.2: quantity field is encoded as a variable-length unsigned integer
  // (a length determinant indicating the number of quantity octets, followed by the quantity).
  encode_variable_unsigned_integer(out, count);
}

Result<std::size_t> decode_sequence_of_length(ByteReader& in) {
  return decode_variable_unsigned_integer(in);
}

}  // namespace oer
}  // namespace asn1
