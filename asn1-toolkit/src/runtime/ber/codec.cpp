#include <asn1/runtime/ber/codec.hpp>
#include <asn1/common/float_conv.hpp>

#include <cmath>
#include <cstring>
#include <climits>
#include <limits>
#include <string>

namespace asn1 {
namespace ber {
namespace {

Result<void> require_tag(ByteReader& in, Tag expected, Tag got) {
  if (got != expected) {
    return make_error(Error::Code::TagMismatch, in.offset(), "BER tag mismatch");
  }
  return Result<void>::success();
}

Result<std::size_t> require_definite(ByteReader& in, const Length& len) {
  if (len.indefinite) {
    return make_error(Error::Code::Unsupported, in.offset(),
                      "indefinite length not valid for this primitive");
  }
  return len.value;
}

template <typename T>
Result<T> decode_primitive_tlv(ByteReader& in, Tag expected,
                               Result<T> (*decode_content)(ByteReader&, std::size_t)) {
  auto hdr = decode_tlv_header(in);
  if (!hdr) {
    return hdr.error();
  }
  if (auto r = require_tag(in, expected, hdr.value().tag); !r) {
    return r.error();
  }
  auto len = require_definite(in, hdr.value().length);
  if (!len) {
    return len.error();
  }
  if (in.remaining() < len.value()) {
    return make_error(Error::Code::Truncated, in.offset(), "truncated BER value");
  }
  return decode_content(in, len.value());
}

}  // namespace

void encode_boolean_content(ByteWriter& out, bool value) {
  out.put(value ? 0xFFu : 0x00u);
}

Result<bool> decode_boolean_content(ByteReader& in, std::size_t length) {
  if (length != 1) {
    return make_error(Error::Code::InvalidArgument, in.offset(),
                      "BOOLEAN content must be 1 octet");
  }
  auto b = in.get();
  if (!b) {
    return b.error();
  }
  return b.value() != 0;
}

void encode_integer_content(ByteWriter& out, std::int64_t value) {
  encode_integer_content(out, BigInteger::from_i64(value));
}

void encode_integer_content(ByteWriter& out, const BigInteger& value) {
  const auto bytes = value.to_twos_complement();
  out.write(Span<const std::uint8_t>(bytes.data(), bytes.size()));
}

Result<BigInteger> decode_big_integer_content(ByteReader& in, std::size_t length) {
  if (length == 0) {
    return make_error(Error::Code::InvalidArgument, in.offset(),
                      "INTEGER content must not be empty");
  }
  auto bytes_r = in.read(length);
  if (!bytes_r) {
    return bytes_r.error();
  }
  return BigInteger::from_twos_complement(bytes_r.value());
}

Result<std::int64_t> decode_integer_content(ByteReader& in, std::size_t length) {
  auto big = decode_big_integer_content(in, length);
  if (!big) {
    return big.error();
  }
  auto v = big.value().as_i64();
  if (!v) {
    return make_error(Error::Code::Unsupported, in.offset(),
                      "INTEGER wider than 64 bits; use decode_big_integer");
  }
  return *v;
}

void encode_null_content(ByteWriter&) {}

Result<void> decode_null_content(ByteReader& in, std::size_t length) {
  if (length != 0) {
    return make_error(Error::Code::InvalidArgument, in.offset(),
                      "NULL content must be empty");
  }
  return Result<void>::success();
}

void encode_octet_string_content(ByteWriter& out, Span<const std::uint8_t> value) {
  out.write(value);
}

Result<std::vector<std::uint8_t>> decode_octet_string_content(ByteReader& in,
                                                              std::size_t length) {
  auto bytes_r = in.read(length);
  if (!bytes_r) {
    return bytes_r.error();
  }
  auto view = bytes_r.value();
  return std::vector<std::uint8_t>(view.begin(), view.end());
}

void encode_bit_string_content(ByteWriter& out, Span<const std::uint8_t> bits,
                               std::uint8_t unused_bits) {
  if (unused_bits > 7) {
    unused_bits = 7;
  }
  if (bits.empty() && unused_bits != 0) {
    unused_bits = 0;
  }
  out.put(unused_bits);
  out.write(bits);
}

Result<BitStringValue> decode_bit_string_content(ByteReader& in, std::size_t length) {
  if (length < 1) {
    return make_error(Error::Code::InvalidArgument, in.offset(),
                      "BIT STRING content must include unused-bits octet");
  }
  auto unused_r = in.get();
  if (!unused_r) {
    return unused_r.error();
  }
  const std::uint8_t unused = unused_r.value();
  if (unused > 7) {
    return make_error(Error::Code::InvalidArgument, in.offset() - 1,
                      "BIT STRING unused bits must be 0..7");
  }
  if (length == 1) {
    if (unused != 0) {
      return make_error(Error::Code::InvalidArgument, in.offset() - 1,
                        "empty BIT STRING must have 0 unused bits");
    }
    return BitStringValue{};
  }
  auto data_r = in.read(length - 1);
  if (!data_r) {
    return data_r.error();
  }
  BitStringValue v;
  auto view = data_r.value();
  v.bits.assign(view.begin(), view.end());
  v.bit_length = v.bits.size() * 8u - unused;
  return v;
}

void encode_utf8_string_content(ByteWriter& out, const std::string& value) {
  out.write(reinterpret_cast<const std::uint8_t*>(value.data()), value.size());
}

Result<std::string> decode_utf8_string_content(ByteReader& in, std::size_t length) {
  auto bytes_r = in.read(length);
  if (!bytes_r) {
    return bytes_r.error();
  }
  auto view = bytes_r.value();
  return std::string(reinterpret_cast<const char*>(view.data()), view.size());
}

void encode_boolean(ByteWriter& out, bool value, Tag tag) {
  ByteWriter content;
  encode_boolean_content(content, value);
  encode_tlv(out, tag, content.buffer());
}

Result<bool> decode_boolean(ByteReader& in, Tag expected) {
  return decode_primitive_tlv<bool>(in, expected, decode_boolean_content);
}

void encode_integer(ByteWriter& out, std::int64_t value, Tag tag) {
  ByteWriter content;
  encode_integer_content(content, value);
  encode_tlv(out, tag, content.buffer());
}

void encode_integer(ByteWriter& out, const BigInteger& value, Tag tag) {
  ByteWriter content;
  encode_integer_content(content, value);
  encode_tlv(out, tag, content.buffer());
}

Result<std::int64_t> decode_integer(ByteReader& in, Tag expected) {
  return decode_primitive_tlv<std::int64_t>(in, expected, decode_integer_content);
}

Result<BigInteger> decode_big_integer(ByteReader& in, Tag expected) {
  return decode_primitive_tlv<BigInteger>(in, expected, decode_big_integer_content);
}

void encode_null(ByteWriter& out, Tag tag) {
  encode_tlv(out, tag, Span<const std::uint8_t>());
}

Result<void> decode_null(ByteReader& in, Tag expected) {
  auto hdr = decode_tlv_header(in);
  if (!hdr) {
    return hdr.error();
  }
  if (auto r = require_tag(in, expected, hdr.value().tag); !r) {
    return r;
  }
  auto len = require_definite(in, hdr.value().length);
  if (!len) {
    return len.error();
  }
  return decode_null_content(in, len.value());
}

void encode_octet_string(ByteWriter& out, Span<const std::uint8_t> value, Tag tag) {
  encode_tlv(out, tag, value);
}

Result<std::vector<std::uint8_t>> decode_octet_string(ByteReader& in, Tag expected) {
  return decode_primitive_tlv<std::vector<std::uint8_t>>(in, expected,
                                                         decode_octet_string_content);
}

void encode_bit_string(ByteWriter& out, Span<const std::uint8_t> bits, std::uint8_t unused_bits,
                       Tag tag) {
  ByteWriter content;
  encode_bit_string_content(content, bits, unused_bits);
  encode_tlv(out, tag, content.buffer());
}

Result<BitStringValue> decode_bit_string(ByteReader& in, Tag expected) {
  return decode_primitive_tlv<BitStringValue>(in, expected, decode_bit_string_content);
}

void encode_utf8_string(ByteWriter& out, const std::string& value, Tag tag) {
  ByteWriter content;
  encode_utf8_string_content(content, value);
  Tag t = tag;
  // UTF8String is primitive by default.
  encode_tlv(out, t, content.buffer());
}

Result<std::string> decode_utf8_string(ByteReader& in, Tag expected) {
  return decode_primitive_tlv<std::string>(in, expected, decode_utf8_string_content);
}

void encode_constructed(ByteWriter& out, Tag tag, Span<const std::uint8_t> components) {
  Tag t = tag;
  t.constructed = true;
  encode_tlv(out, t, components);
}

Result<std::vector<std::uint8_t>> decode_constructed(ByteReader& in, Tag expected) {
  auto hdr = decode_tlv_header(in);
  if (!hdr) {
    return hdr.error();
  }
  Tag want = expected;
  want.constructed = true;
  if (hdr.value().tag != want) {
    return make_error(Error::Code::TagMismatch, in.offset(),
                      "BER constructed tag mismatch");
  }
  if (!hdr.value().length.indefinite) {
    return decode_octet_string_content(in, hdr.value().length.value);
  }

  // Indefinite: collect TLVs until EOC.
  ByteWriter acc;
  for (;;) {
    auto peek0 = in.peek(0);
    auto peek1 = in.peek(1);
    if (peek0 && peek1 && peek0.value() == 0x00 && peek1.value() == 0x00) {
      auto eoc = decode_end_of_contents(in);
      if (!eoc) {
        return eoc.error();
      }
      break;
    }
    // Read one complete TLV (definite only for nested in this Phase 6 helper).
    const std::size_t start = in.offset();
    auto nested = decode_tlv_header(in);
    if (!nested) {
      return nested.error();
    }
    if (nested.value().length.indefinite) {
      return make_error(Error::Code::Unsupported, in.offset(),
                        "nested indefinite length not supported in Phase 6 helper");
    }
    auto content = in.read(nested.value().length.value);
    if (!content) {
      return content.error();
    }
    // Copy from start to current.
    // Re-read is awkward; rebuild from header+content.
    ByteWriter one;
    encode_tag(one, nested.value().tag);
    encode_length(one, nested.value().length.value);
    one.write(content.value());
    acc.write(one.buffer());
    (void)start;
  }
  return acc.take();
}

void encode_enumerated_content(ByteWriter& out, std::int64_t value) {
  encode_integer_content(out, value);
}

Result<std::int64_t> decode_enumerated_content(ByteReader& in, std::size_t length) {
  return decode_integer_content(in, length);
}

void encode_enumerated(ByteWriter& out, std::int64_t value, Tag tag) {
  ByteWriter content;
  encode_enumerated_content(content, value);
  encode_tlv(out, tag, content.buffer());
}

Result<std::int64_t> decode_enumerated(ByteReader& in, Tag expected) {
  return decode_primitive_tlv<std::int64_t>(in, expected, decode_enumerated_content);
}

namespace {

void encode_oid_subidentifier(ByteWriter& out, std::uint64_t value) {
  std::uint8_t stack[10];
  int n = 0;
  stack[n++] = static_cast<std::uint8_t>(value & 0x7Fu);
  value >>= 7;
  while (value > 0) {
    stack[n++] = static_cast<std::uint8_t>(0x80u | (value & 0x7Fu));
    value >>= 7;
  }
  while (n > 0) {
    out.put(stack[--n]);
  }
}

Result<std::uint64_t> decode_oid_subidentifier(ByteReader& in, std::size_t& remaining) {
  std::uint64_t value = 0;
  for (;;) {
    if (remaining == 0) {
      return make_error(Error::Code::Truncated, in.offset(), "truncated OID subidentifier");
    }
    auto b = in.get();
    if (!b) {
      return b.error();
    }
    --remaining;
    if (value > (UINT64_MAX >> 7)) {
      return make_error(Error::Code::Unsupported, in.offset(), "OID arc too large");
    }
    value = (value << 7) | (b.value() & 0x7Fu);
    if ((b.value() & 0x80u) == 0) {
      return value;
    }
  }
}

}  // namespace

void encode_object_identifier_content(ByteWriter& out, Span<const std::uint64_t> arcs) {
  if (arcs.size() < 2) {
    return;
  }
  const std::uint64_t first = 40ull * arcs[0] + arcs[1];
  encode_oid_subidentifier(out, first);
  for (std::size_t i = 2; i < arcs.size(); ++i) {
    encode_oid_subidentifier(out, arcs[i]);
  }
}

Result<std::vector<std::uint64_t>> decode_object_identifier_content(ByteReader& in,
                                                                   std::size_t length) {
  if (length == 0) {
    return make_error(Error::Code::InvalidArgument, in.offset(),
                      "OBJECT IDENTIFIER content must not be empty");
  }
  std::size_t remaining = length;
  auto first = decode_oid_subidentifier(in, remaining);
  if (!first) {
    return first.error();
  }
  std::vector<std::uint64_t> arcs;
  if (first.value() < 80) {
    arcs.push_back(first.value() / 40);
    arcs.push_back(first.value() % 40);
  } else {
    arcs.push_back(2);
    arcs.push_back(first.value() - 80);
  }
  while (remaining > 0) {
    auto arc = decode_oid_subidentifier(in, remaining);
    if (!arc) {
      return arc.error();
    }
    arcs.push_back(arc.value());
  }
  return arcs;
}

void encode_object_identifier(ByteWriter& out, Span<const std::uint64_t> arcs, Tag tag) {
  ByteWriter content;
  encode_object_identifier_content(content, arcs);
  encode_tlv(out, tag, content.buffer());
}

Result<std::vector<std::uint64_t>> decode_object_identifier(ByteReader& in, Tag expected) {
  return decode_primitive_tlv<std::vector<std::uint64_t>>(in, expected,
                                                          decode_object_identifier_content);
}

void encode_relative_oid_content(ByteWriter& out, Span<const std::uint64_t> arcs) {
  for (std::uint64_t a : arcs) {
    encode_oid_subidentifier(out, a);
  }
}

Result<std::vector<std::uint64_t>> decode_relative_oid_content(ByteReader& in,
                                                              std::size_t length) {
  std::size_t remaining = length;
  std::vector<std::uint64_t> arcs;
  while (remaining > 0) {
    auto arc = decode_oid_subidentifier(in, remaining);
    if (!arc) {
      return arc.error();
    }
    arcs.push_back(arc.value());
  }
  return arcs;
}

void encode_relative_oid(ByteWriter& out, Span<const std::uint64_t> arcs, Tag tag) {
  ByteWriter content;
  encode_relative_oid_content(content, arcs);
  encode_tlv(out, tag, content.buffer());
}

Result<std::vector<std::uint64_t>> decode_relative_oid(ByteReader& in, Tag expected) {
  return decode_primitive_tlv<std::vector<std::uint64_t>>(in, expected,
                                                          decode_relative_oid_content);
}

namespace {

void encode_real_exponent(ByteWriter& out, std::int32_t exponent, bool negative_mantissa) {
  // Binary encoding, base 2, scale 0. Exponent two's-complement, minimal length.
  std::uint8_t control = 0x80u | (negative_mantissa ? 0x40u : 0x00u);
  if (exponent >= -128 && exponent <= 127) {
    out.put(control);  // ee = 00 → 1-octet exponent
    out.put(static_cast<std::uint8_t>(static_cast<std::int8_t>(exponent)));
    return;
  }
  if (exponent >= -32768 && exponent <= 32767) {
    out.put(static_cast<std::uint8_t>(control | 0x01u));  // ee = 01 → 2 octets
    out.put(static_cast<std::uint8_t>((exponent >> 8) & 0xFF));
    out.put(static_cast<std::uint8_t>(exponent & 0xFF));
    return;
  }
  // 3-octet exponent (ee = 10). Sufficient for IEEE-754 double exponents.
  out.put(static_cast<std::uint8_t>(control | 0x02u));
  out.put(static_cast<std::uint8_t>((exponent >> 16) & 0xFF));
  out.put(static_cast<std::uint8_t>((exponent >> 8) & 0xFF));
  out.put(static_cast<std::uint8_t>(exponent & 0xFF));
}

Result<double> decode_real_binary(Span<const std::uint8_t> data, std::size_t err_offset) {
  if (data.empty()) {
    return make_error(Error::Code::InvalidArgument, err_offset,
                      "binary REAL missing control octet");
  }
  const std::uint8_t control = data[0];
  const std::uint8_t base_bits = static_cast<std::uint8_t>((control >> 4) & 0x03u);
  if (base_bits != 0) {
    return make_error(Error::Code::Unsupported, err_offset,
                      "REAL base other than 2 not supported");
  }
  const std::uint8_t scale = static_cast<std::uint8_t>((control >> 2) & 0x03u);
  const std::uint8_t ee = static_cast<std::uint8_t>(control & 0x03u);
  std::size_t offset = 1;
  std::int32_t exponent = 0;
  if (ee == 0x00u) {
    if (data.size() < 2) {
      return make_error(Error::Code::Truncated, err_offset, "truncated REAL exponent");
    }
    exponent = static_cast<std::int8_t>(data[1]);
    offset = 2;
  } else if (ee == 0x01u) {
    if (data.size() < 3) {
      return make_error(Error::Code::Truncated, err_offset, "truncated REAL exponent");
    }
    exponent = static_cast<std::int16_t>((static_cast<std::uint16_t>(data[1]) << 8) |
                                        data[2]);
    offset = 3;
  } else if (ee == 0x02u) {
    if (data.size() < 4) {
      return make_error(Error::Code::Truncated, err_offset, "truncated REAL exponent");
    }
    std::int32_t e = (static_cast<std::int32_t>(data[1]) << 16) |
                     (static_cast<std::int32_t>(data[2]) << 8) | data[3];
    if (e & 0x800000) {
      e |= static_cast<std::int32_t>(0xFF000000u);
    }
    exponent = e;
    offset = 4;
  } else {
    return make_error(Error::Code::Unsupported, err_offset,
                      "REAL exponent length octet form not supported");
  }
  if (offset >= data.size()) {
    return make_error(Error::Code::InvalidArgument, err_offset,
                      "REAL binary encoding missing mantissa");
  }
  // Mantissa as unsigned big-endian integer.
  double mant = 0.0;
  for (std::size_t i = offset; i < data.size(); ++i) {
    mant = mant * 256.0 + static_cast<double>(data[i]);
  }
  // Apply scale F: multiply mantissa by 2^F (X.690).
  double value = std::ldexp(mant, exponent + static_cast<int>(scale));
  if ((control & 0x40u) != 0) {
    value = -value;
  }
  return value;
}

Result<double> decode_real_special(std::uint8_t control, std::size_t offset) {
  switch (control) {
    case 0x40u:
      return std::numeric_limits<double>::infinity();
    case 0x41u:
      return -std::numeric_limits<double>::infinity();
    case 0x42u:
      return std::numeric_limits<double>::quiet_NaN();
    case 0x43u:
      return -0.0;
    default:
      return make_error(Error::Code::Unsupported, offset,
                        "unsupported special REAL control word");
  }
}

Result<double> decode_real_decimal(Span<const std::uint8_t> data, std::size_t offset) {
  if (data.size() < 2) {
    return make_error(Error::Code::InvalidArgument, offset,
                      "decimal REAL missing digits");
  }
  // First octet is NR form (1/2/3); remainder is ISO 6093 character data.
  std::string text;
  text.reserve(data.size() - 1);
  for (std::size_t i = 1; i < data.size(); ++i) {
    char c = static_cast<char>(data[i]);
    if (c == ',') {
      c = '.';
    }
    text.push_back(c);
  }
  double v = 0.0;
  if (!parse_double_c_locale(text, v)) {
    return make_error(Error::Code::InvalidArgument, offset, "invalid decimal REAL");
  }
  return v;
}

}  // namespace

void encode_real_content(ByteWriter& out, double value) {
  if (std::isnan(value)) {
    out.put(0x42u);
    return;
  }
  if (std::isinf(value)) {
    out.put(value > 0.0 ? 0x40u : 0x41u);
    return;
  }
  // Canonical zero (including -0.0) is empty content.
  if (value == 0.0) {
    return;
  }

  std::uint64_t bits = 0;
  static_assert(sizeof(double) == sizeof(std::uint64_t), "IEEE-754 binary64 required");
  std::memcpy(&bits, &value, sizeof(bits));
  const bool negative = (bits >> 63) != 0;
  const int biased = static_cast<int>((bits >> 52) & 0x7FFull);
  const std::uint64_t frac = bits & ((1ull << 52) - 1ull);

  std::uint64_t mant = 0;
  int exponent = 0;
  if (biased == 0) {
    // Subnormal: value = ± frac * 2^-1074
    mant = frac;
    exponent = -1074;
  } else {
    // Normal: ± (1.frac) * 2^(biased-1023)
    mant = (1ull << 52) | frac;
    exponent = biased - 1023 - 52;
  }
  while ((mant & 1ull) == 0ull) {
    mant >>= 1;
    ++exponent;
  }

  encode_real_exponent(out, exponent, negative);

  // Mantissa octets, big-endian, no leading zero.
  std::uint8_t bytes[8];
  int n = 0;
  std::uint64_t tmp = mant;
  do {
    bytes[n++] = static_cast<std::uint8_t>(tmp & 0xFFu);
    tmp >>= 8;
  } while (tmp != 0);
  while (n > 0) {
    out.put(bytes[--n]);
  }
}

Result<double> decode_real_content(ByteReader& in, std::size_t length) {
  if (length == 0) {
    return 0.0;
  }
  if (in.remaining() < length) {
    return make_error(Error::Code::Truncated, in.offset(), "truncated REAL value");
  }
  auto bytes_r = in.read(length);
  if (!bytes_r) {
    return bytes_r.error();
  }
  const auto data = bytes_r.value();
  const std::uint8_t control = data[0];
  if ((control & 0x80u) != 0) {
    return decode_real_binary(data, in.offset());
  }
  if ((control & 0x40u) != 0) {
    if (length != 1) {
      return make_error(Error::Code::InvalidArgument, in.offset(),
                        "special REAL must be a single control octet");
    }
    return decode_real_special(control, in.offset());
  }
  return decode_real_decimal(data, in.offset());
}

void encode_real(ByteWriter& out, double value, Tag tag) {
  ByteWriter content;
  encode_real_content(content, value);
  encode_tlv(out, tag, content.buffer());
}

Result<double> decode_real(ByteReader& in, Tag expected) {
  return decode_primitive_tlv<double>(in, expected, decode_real_content);
}

namespace {

void encode_implicit_oid(ByteWriter& out, std::uint64_t context_number,
                         Span<const std::uint64_t> arcs) {
  ByteWriter content;
  encode_object_identifier_content(content, arcs);
  encode_tlv(out, context(context_number, /*constructed=*/false), content.buffer());
}

void encode_implicit_integer(ByteWriter& out, std::uint64_t context_number, std::int64_t value) {
  ByteWriter content;
  encode_integer_content(content, value);
  encode_tlv(out, context(context_number, /*constructed=*/false), content.buffer());
}

void encode_implicit_null(ByteWriter& out, std::uint64_t context_number) {
  encode_tlv(out, context(context_number, /*constructed=*/false), Span<const std::uint8_t>());
}

void encode_identification_choice(ByteWriter& out, const Identification& id) {
  switch (id.kind) {
    case Identification::Kind::Syntaxes: {
      ByteWriter seq;
      encode_implicit_oid(seq, 0, id.abstract_syntax);
      encode_implicit_oid(seq, 1, id.transfer_syntax);
      ByteWriter inner;
      encode_tlv(inner, universal(kTagSequence, /*constructed=*/true), seq.buffer());
      encode_tlv(out, context(0, /*constructed=*/true), inner.buffer());
      break;
    }
    case Identification::Kind::Syntax: {
      const auto& arcs =
          !id.transfer_syntax.empty() ? id.transfer_syntax : id.abstract_syntax;
      encode_implicit_oid(out, 1, Span<const std::uint64_t>(arcs.data(), arcs.size()));
      break;
    }
    case Identification::Kind::PresentationContextId:
      encode_implicit_integer(out, 2, id.presentation_context_id);
      break;
    case Identification::Kind::ContextNegotiation: {
      ByteWriter seq;
      encode_implicit_integer(seq, 0, id.presentation_context_id);
      encode_implicit_oid(seq, 1, id.transfer_syntax);
      ByteWriter inner;
      encode_tlv(inner, universal(kTagSequence, /*constructed=*/true), seq.buffer());
      encode_tlv(out, context(3, /*constructed=*/true), inner.buffer());
      break;
    }
    case Identification::Kind::TransferSyntax:
      encode_implicit_oid(out, 4, id.transfer_syntax);
      break;
    case Identification::Kind::Fixed:
      encode_implicit_null(out, 5);
      break;
  }
}

Result<Identification> decode_identification_choice(ByteReader& in) {
  auto hdr = decode_tlv_header(in);
  if (!hdr) {
    return hdr.error();
  }
  const Tag& t = hdr.value().tag;
  auto len = require_definite(in, hdr.value().length);
  if (!len) {
    return len.error();
  }
  if (t.cls != TagClass::Context) {
    return make_error(Error::Code::InvalidArgument, in.offset(),
                      "identification CHOICE expects context tag");
  }
  auto bytes = in.read(len.value());
  if (!bytes) {
    return bytes.error();
  }
  ByteReader r(bytes.value());
  Identification id;
  switch (t.number) {
    case 0: {
      // EXPLICIT SEQUENCE { [0] OID, [1] OID }
      id.kind = Identification::Kind::Syntaxes;
      auto seq_hdr = decode_tlv_header(r);
      if (!seq_hdr) {
        return seq_hdr.error();
      }
      if (seq_hdr.value().tag != universal(kTagSequence, true)) {
        return make_error(Error::Code::TagMismatch, r.offset(),
                          "syntaxes expects SEQUENCE");
      }
      auto seq_len = require_definite(r, seq_hdr.value().length);
      if (!seq_len) {
        return seq_len.error();
      }
      auto seq_bytes = r.read(seq_len.value());
      if (!seq_bytes) {
        return seq_bytes.error();
      }
      ByteReader sr(seq_bytes.value());
      auto a_hdr = decode_tlv_header(sr);
      if (!a_hdr) {
        return a_hdr.error();
      }
      auto a_len = require_definite(sr, a_hdr.value().length);
      if (!a_len) {
        return a_len.error();
      }
      auto a = decode_object_identifier_content(sr, a_len.value());
      if (!a) {
        return a.error();
      }
      auto b_hdr = decode_tlv_header(sr);
      if (!b_hdr) {
        return b_hdr.error();
      }
      auto b_len = require_definite(sr, b_hdr.value().length);
      if (!b_len) {
        return b_len.error();
      }
      auto b = decode_object_identifier_content(sr, b_len.value());
      if (!b) {
        return b.error();
      }
      id.abstract_syntax = std::move(a.value());
      id.transfer_syntax = std::move(b.value());
      break;
    }
    case 1: {
      id.kind = Identification::Kind::Syntax;
      auto v = decode_object_identifier_content(r, bytes.value().size());
      if (!v) {
        return v.error();
      }
      id.transfer_syntax = std::move(v.value());
      break;
    }
    case 2: {
      id.kind = Identification::Kind::PresentationContextId;
      auto v = decode_integer_content(r, bytes.value().size());
      if (!v) {
        return v.error();
      }
      id.presentation_context_id = v.value();
      break;
    }
    case 3: {
      id.kind = Identification::Kind::ContextNegotiation;
      auto seq_hdr = decode_tlv_header(r);
      if (!seq_hdr) {
        return seq_hdr.error();
      }
      if (seq_hdr.value().tag != universal(kTagSequence, true)) {
        return make_error(Error::Code::TagMismatch, r.offset(),
                          "context-negotiation expects SEQUENCE");
      }
      auto seq_len = require_definite(r, seq_hdr.value().length);
      if (!seq_len) {
        return seq_len.error();
      }
      auto seq_bytes = r.read(seq_len.value());
      if (!seq_bytes) {
        return seq_bytes.error();
      }
      ByteReader sr(seq_bytes.value());
      auto i_hdr = decode_tlv_header(sr);
      if (!i_hdr) {
        return i_hdr.error();
      }
      auto i_len = require_definite(sr, i_hdr.value().length);
      if (!i_len) {
        return i_len.error();
      }
      auto iv = decode_integer_content(sr, i_len.value());
      if (!iv) {
        return iv.error();
      }
      auto o_hdr = decode_tlv_header(sr);
      if (!o_hdr) {
        return o_hdr.error();
      }
      auto o_len = require_definite(sr, o_hdr.value().length);
      if (!o_len) {
        return o_len.error();
      }
      auto ov = decode_object_identifier_content(sr, o_len.value());
      if (!ov) {
        return ov.error();
      }
      id.presentation_context_id = iv.value();
      id.transfer_syntax = std::move(ov.value());
      break;
    }
    case 4: {
      id.kind = Identification::Kind::TransferSyntax;
      auto v = decode_object_identifier_content(r, bytes.value().size());
      if (!v) {
        return v.error();
      }
      id.transfer_syntax = std::move(v.value());
      break;
    }
    case 5: {
      id.kind = Identification::Kind::Fixed;
      if (!bytes.value().empty()) {
        return make_error(Error::Code::InvalidArgument, in.offset(),
                          "fixed identification must be NULL");
      }
      break;
    }
    default:
      return make_error(Error::Code::InvalidArgument, in.offset(),
                        "unknown identification CHOICE alternative");
  }
  return id;
}

void encode_associated_pdv_body(ByteWriter& body, const Identification& identification,
                                const std::optional<std::string>& descriptor,
                                Span<const std::uint8_t> value_octets) {
  ByteWriter ident_content;
  encode_identification_choice(ident_content, identification);
  encode_tlv(body, context(0, /*constructed=*/true), ident_content.buffer());
  if (descriptor) {
    ByteWriter desc;
    encode_utf8_string_content(desc, *descriptor);
    encode_tlv(body, context(1, /*constructed=*/false), desc.buffer());
  }
  encode_tlv(body, context(2, /*constructed=*/false), value_octets);
}

struct AssociatedPdvBody {
  Identification identification;
  std::optional<std::string> data_value_descriptor;
  std::vector<std::uint8_t> value;
};

/// Concatenate BER constructed OCTET STRING / restricted character string fragments.
/// Nested encodings use the base universal tag (`kTagOctetString` or `kTagObjectDescriptor`).
Result<std::vector<std::uint8_t>> decode_constructed_string_fragments(
    ByteReader& in, std::size_t length, std::uint64_t fragment_univ_tag) {
  auto bytes = in.read(length);
  if (!bytes) {
    return bytes.error();
  }
  ByteReader r(bytes.value());
  std::vector<std::uint8_t> out;
  while (!r.eof()) {
    auto hdr = decode_tlv_header(r);
    if (!hdr) {
      return hdr.error();
    }
    if (hdr.value().tag.cls != TagClass::Universal ||
        hdr.value().tag.number != fragment_univ_tag || hdr.value().tag.constructed) {
      return make_error(Error::Code::InvalidArgument, r.offset(),
                        "constructed string fragment must be primitive universal");
    }
    auto len = require_definite(r, hdr.value().length);
    if (!len) {
      return len.error();
    }
    auto part = r.read(len.value());
    if (!part) {
      return part.error();
    }
    out.insert(out.end(), part.value().begin(), part.value().end());
  }
  return out;
}

Result<std::vector<std::uint8_t>> decode_implicit_string_component(
    ByteReader& in, const TlvHeader& hdr, std::uint64_t fragment_univ_tag) {
  if (hdr.length.indefinite) {
    return make_error(Error::Code::Unsupported, in.offset(),
                      "indefinite length not supported for implicit string component");
  }
  if (!hdr.tag.constructed) {
    auto bytes = in.read(hdr.length.value);
    if (!bytes) {
      return bytes.error();
    }
    return std::vector<std::uint8_t>(bytes.value().begin(), bytes.value().end());
  }
  return decode_constructed_string_fragments(in, hdr.length.value, fragment_univ_tag);
}

Result<AssociatedPdvBody> decode_associated_pdv_body(ByteReader& r) {
  AssociatedPdvBody out;
  bool saw_ident = false;
  bool saw_value = false;
  while (!r.eof()) {
    auto hdr = decode_tlv_header(r);
    if (!hdr) {
      return hdr.error();
    }
    const Tag& t = hdr.value().tag;
    if (t.cls != TagClass::Context) {
      return make_error(Error::Code::InvalidArgument, r.offset(),
                        "unexpected component in EMBEDDED PDV / CHARACTER STRING");
    }
    if (t.number == 0) {
      auto len = require_definite(r, hdr.value().length);
      if (!len) {
        return len.error();
      }
      auto bytes = r.read(len.value());
      if (!bytes) {
        return bytes.error();
      }
      ByteReader ir(bytes.value());
      auto id = decode_identification_choice(ir);
      if (!id) {
        return id.error();
      }
      out.identification = std::move(id.value());
      saw_ident = true;
      continue;
    }
    if (t.number == 1) {
      auto bytes = decode_implicit_string_component(r, hdr.value(), kTagObjectDescriptor);
      if (!bytes) {
        return bytes.error();
      }
      out.data_value_descriptor =
          std::string(reinterpret_cast<const char*>(bytes.value().data()), bytes.value().size());
      continue;
    }
    if (t.number == 2) {
      auto bytes = decode_implicit_string_component(r, hdr.value(), kTagOctetString);
      if (!bytes) {
        return bytes.error();
      }
      out.value = std::move(bytes.value());
      saw_value = true;
      continue;
    }
    return make_error(Error::Code::InvalidArgument, r.offset(),
                      "unexpected context tag in associated PDV SEQUENCE");
  }
  if (!saw_ident || !saw_value) {
    return make_error(Error::Code::InvalidArgument, r.offset(),
                      "associated PDV missing identification or value");
  }
  return out;
}

}  // namespace

void encode_external(ByteWriter& out, const ExternalValue& value, Tag tag) {
  ByteWriter body;
  if (value.direct_reference) {
    encode_object_identifier(body, *value.direct_reference);
  }
  if (value.indirect_reference) {
    encode_integer(body, *value.indirect_reference);
  }
  if (value.data_value_descriptor) {
    // ObjectDescriptor ::= [UNIVERSAL 7] IMPLICIT GraphicString
    encode_utf8_string(body, *value.data_value_descriptor, universal(kTagObjectDescriptor));
  }
  switch (value.encoding) {
    case ExternalValue::Encoding::SingleAsn1Type: {
      // [0] EXPLICIT — wrap open-type octets in a constructed context tag.
      Tag t = context(0, /*constructed=*/true);
      encode_tlv(body, t, value.encoding_value);
      break;
    }
    case ExternalValue::Encoding::OctetAligned: {
      Tag t = context(1, /*constructed=*/false);
      encode_tlv(body, t, value.encoding_value);
      break;
    }
    case ExternalValue::Encoding::Arbitrary: {
      Tag t = context(2, /*constructed=*/false);
      ByteWriter bits;
      const std::uint8_t unused =
          value.encoding_value.empty()
              ? 0
              : static_cast<std::uint8_t>((8 - (value.arbitrary_bit_length % 8)) % 8);
      encode_bit_string_content(bits, value.encoding_value, unused);
      encode_tlv(body, t, bits.buffer());
      break;
    }
  }
  Tag outer = tag;
  outer.constructed = true;
  encode_tlv(out, outer, body.buffer());
}

Result<ExternalValue> decode_external(ByteReader& in, Tag expected) {
  Tag want = expected;
  want.constructed = true;
  auto content = decode_constructed(in, want);
  if (!content) {
    return content.error();
  }
  ByteReader r(content.value());
  ExternalValue out;
  while (!r.eof()) {
    auto hdr = decode_tlv_header(r);
    if (!hdr) {
      return hdr.error();
    }
    const Tag& t = hdr.value().tag;
    auto len = require_definite(r, hdr.value().length);
    if (!len) {
      return len.error();
    }
    if (t.cls == TagClass::Universal && t.number == kTagOid) {
      auto v = decode_object_identifier_content(r, len.value());
      if (!v) {
        return v.error();
      }
      out.direct_reference = std::move(v.value());
      continue;
    }
    if (t.cls == TagClass::Universal && t.number == kTagInteger) {
      auto v = decode_integer_content(r, len.value());
      if (!v) {
        return v.error();
      }
      out.indirect_reference = v.value();
      continue;
    }
    if (t.cls == TagClass::Universal && t.number == kTagObjectDescriptor) {
      auto v = decode_utf8_string_content(r, len.value());
      if (!v) {
        return v.error();
      }
      out.data_value_descriptor = std::move(v.value());
      continue;
    }
    if (t.cls == TagClass::Context && t.number == 0) {
      auto bytes = r.read(len.value());
      if (!bytes) {
        return bytes.error();
      }
      out.encoding = ExternalValue::Encoding::SingleAsn1Type;
      out.encoding_value.assign(bytes.value().begin(), bytes.value().end());
      continue;
    }
    if (t.cls == TagClass::Context && t.number == 1) {
      auto bytes = r.read(len.value());
      if (!bytes) {
        return bytes.error();
      }
      out.encoding = ExternalValue::Encoding::OctetAligned;
      out.encoding_value.assign(bytes.value().begin(), bytes.value().end());
      continue;
    }
    if (t.cls == TagClass::Context && t.number == 2) {
      auto bs = decode_bit_string_content(r, len.value());
      if (!bs) {
        return bs.error();
      }
      out.encoding = ExternalValue::Encoding::Arbitrary;
      out.encoding_value = std::move(bs.value().bits);
      out.arbitrary_bit_length = bs.value().bit_length;
      continue;
    }
    return make_error(Error::Code::InvalidArgument, r.offset(),
                      "unexpected component in EXTERNAL");
  }
  return out;
}

void encode_embedded_pdv(ByteWriter& out, const EmbeddedPdvValue& value, Tag tag) {
  ByteWriter body;
  encode_associated_pdv_body(body, value.identification, value.data_value_descriptor,
                             Span<const std::uint8_t>(value.data_value.data(),
                                                      value.data_value.size()));
  Tag outer = tag;
  outer.constructed = true;
  encode_tlv(out, outer, body.buffer());
}

Result<EmbeddedPdvValue> decode_embedded_pdv(ByteReader& in, Tag expected) {
  Tag want = expected;
  want.constructed = true;
  auto content = decode_constructed(in, want);
  if (!content) {
    return content.error();
  }
  ByteReader r(content.value());
  auto body = decode_associated_pdv_body(r);
  if (!body) {
    return body.error();
  }
  EmbeddedPdvValue out;
  out.identification = std::move(body.value().identification);
  out.data_value_descriptor = std::move(body.value().data_value_descriptor);
  out.data_value = std::move(body.value().value);
  return out;
}

void encode_character_string(ByteWriter& out, const CharacterStringValue& value, Tag tag) {
  ByteWriter body;
  encode_associated_pdv_body(body, value.identification, value.data_value_descriptor,
                             Span<const std::uint8_t>(value.string_value.data(),
                                                      value.string_value.size()));
  Tag outer = tag;
  outer.constructed = true;
  encode_tlv(out, outer, body.buffer());
}

Result<CharacterStringValue> decode_character_string(ByteReader& in, Tag expected) {
  Tag want = expected;
  want.constructed = true;
  auto content = decode_constructed(in, want);
  if (!content) {
    return content.error();
  }
  ByteReader r(content.value());
  auto body = decode_associated_pdv_body(r);
  if (!body) {
    return body.error();
  }
  CharacterStringValue out;
  out.identification = std::move(body.value().identification);
  out.data_value_descriptor = std::move(body.value().data_value_descriptor);
  out.string_value = std::move(body.value().value);
  return out;
}

void encode_external_modern(ByteWriter& out, const ModernExternalValue& value, Tag tag) {
  ByteWriter body;
  encode_associated_pdv_body(body, value.identification, value.data_value_descriptor,
                             Span<const std::uint8_t>(value.data_value.data(),
                                                      value.data_value.size()));
  Tag outer = tag;
  outer.constructed = true;
  encode_tlv(out, outer, body.buffer());
}

Result<ModernExternalValue> decode_external_modern(ByteReader& in, Tag expected) {
  Tag want = expected;
  want.constructed = true;
  auto content = decode_constructed(in, want);
  if (!content) {
    return content.error();
  }
  ByteReader r(content.value());
  auto body = decode_associated_pdv_body(r);
  if (!body) {
    return body.error();
  }
  ModernExternalValue out;
  out.identification = std::move(body.value().identification);
  out.data_value_descriptor = std::move(body.value().data_value_descriptor);
  out.data_value = std::move(body.value().value);
  return out;
}

}  // namespace ber
}  // namespace asn1
