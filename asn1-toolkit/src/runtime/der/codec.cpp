#include <asn1/runtime/der/codec.hpp>

#include <algorithm>
#include <utility>

namespace asn1 {
namespace der {
namespace {

Result<void> require_tag_number(ByteReader& in, ber::Tag expected, ber::Tag got) {
  if (got.cls != expected.cls || got.number != expected.number) {
    return make_error(Error::Code::TagMismatch, in.offset(), "DER tag mismatch");
  }
  return Result<void>::success();
}

Result<void> require_primitive(ByteReader& in, const ber::Tag& tag) {
  if (tag.constructed) {
    return make_error(Error::Code::NonCanonical, in.offset(),
                      "DER forbids constructed encoding for this type");
  }
  return Result<void>::success();
}

Result<void> require_constructed(ByteReader& in, const ber::Tag& tag) {
  if (!tag.constructed) {
    return make_error(Error::Code::NonCanonical, in.offset(),
                      "DER requires constructed encoding for SEQUENCE/SET");
  }
  return Result<void>::success();
}

Result<void> ensure_remaining(ByteReader& in, std::size_t length) {
  if (in.remaining() < length) {
    return make_error(Error::Code::Truncated, in.offset(), "truncated DER value");
  }
  return Result<void>::success();
}

Result<void> check_integer_minimal(Span<const std::uint8_t> bytes, std::size_t offset) {
  if (bytes.empty()) {
    return make_error(Error::Code::InvalidArgument, offset, "INTEGER content empty");
  }
  if (bytes.size() > 1) {
    if (bytes[0] == 0x00u && (bytes[1] & 0x80u) == 0) {
      return make_error(Error::Code::NonCanonical, offset,
                        "DER INTEGER must use minimal encoding");
    }
    if (bytes[0] == 0xFFu && (bytes[1] & 0x80u) != 0) {
      return make_error(Error::Code::NonCanonical, offset,
                        "DER INTEGER must use minimal encoding");
    }
  }
  return Result<void>::success();
}

Result<void> check_bit_string_unused_zero(const ber::BitStringValue& v,
                                          std::size_t offset) {
  if (v.unused_bits == 0 || v.bits.empty()) {
    return Result<void>::success();
  }
  const std::uint8_t mask =
      static_cast<std::uint8_t>((1u << v.unused_bits) - 1u);
  if ((v.bits.back() & mask) != 0) {
    return make_error(Error::Code::NonCanonical, offset,
                      "DER BIT STRING unused bits must be zero");
  }
  return Result<void>::success();
}

std::vector<std::uint8_t> tag_encoding(ber::Tag tag) {
  ByteWriter w;
  ber::encode_tag(w, tag);
  return w.take();
}

Result<std::vector<std::uint8_t>> read_tag_encoding_prefix(Span<const std::uint8_t> tlv) {
  ByteReader r(tlv);
  auto tag = ber::decode_tag(r);
  if (!tag) {
    return tag.error();
  }
  return tag_encoding(tag.value());
}

bool tag_encoding_less(Span<const std::uint8_t> a, Span<const std::uint8_t> b) {
  const std::size_t n = std::min(a.size(), b.size());
  for (std::size_t i = 0; i < n; ++i) {
    if (a[i] != b[i]) {
      return a[i] < b[i];
    }
  }
  return a.size() < b.size();
}

Result<void> validate_set_order(Span<const std::uint8_t> content) {
  ByteReader r(content);
  std::vector<std::uint8_t> prev;
  bool have_prev = false;
  while (!r.eof()) {
    const std::size_t start = r.offset();
    auto hdr = decode_tlv_header(r);
    if (!hdr) {
      return hdr.error();
    }
    if (auto rem = ensure_remaining(r, hdr.value().length.value); !rem) {
      return rem;
    }
    if (auto skip = r.read(hdr.value().length.value); !skip) {
      return skip.error();
    }
    auto cur = tag_encoding(hdr.value().tag);
    if (have_prev && prev != cur && !tag_encoding_less(prev, cur)) {
      return make_error(Error::Code::NonCanonical, start,
                        "DER SET components must be in ascending tag order");
    }
    prev = std::move(cur);
    have_prev = true;
  }
  return Result<void>::success();
}

template <typename T>
Result<T> decode_primitive(ByteReader& in, ber::Tag expected,
                           Result<T> (*decode_content)(ByteReader&, std::size_t)) {
  auto hdr = decode_tlv_header(in);
  if (!hdr) {
    return hdr.error();
  }
  if (auto r = require_tag_number(in, expected, hdr.value().tag); !r) {
    return r.error();
  }
  if (auto r = require_primitive(in, hdr.value().tag); !r) {
    return r.error();
  }
  const std::size_t length = hdr.value().length.value;
  if (auto rem = ensure_remaining(in, length); !rem) {
    return rem.error();
  }
  return decode_content(in, length);
}

Result<bool> decode_boolean_content_der(ByteReader& in, std::size_t length) {
  if (length != 1) {
    return make_error(Error::Code::InvalidArgument, in.offset(),
                      "BOOLEAN content must be 1 octet");
  }
  auto b = in.get();
  if (!b) {
    return b.error();
  }
  if (b.value() != 0x00u && b.value() != 0xFFu) {
    return make_error(Error::Code::NonCanonical, in.offset() - 1,
                      "DER BOOLEAN must be 0x00 or 0xFF");
  }
  return b.value() == 0xFFu;
}

Result<BigInteger> decode_big_integer_content_der(ByteReader& in, std::size_t length) {
  if (length == 0) {
    return make_error(Error::Code::InvalidArgument, in.offset(),
                      "INTEGER content must not be empty");
  }
  const std::size_t off = in.offset();
  auto bytes_r = in.read(length);
  if (!bytes_r) {
    return bytes_r.error();
  }
  auto bytes = bytes_r.value();
  if (auto r = check_integer_minimal(bytes, off); !r) {
    return r.error();
  }
  return BigInteger::from_twos_complement(bytes);
}

Result<std::int64_t> decode_integer_content_der(ByteReader& in, std::size_t length) {
  auto big = decode_big_integer_content_der(in, length);
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

Result<ber::BitStringValue> decode_bit_string_content_der(ByteReader& in,
                                                          std::size_t length) {
  const std::size_t content_start = in.offset();
  auto v = ber::decode_bit_string_content(in, length);
  if (!v) {
    return v;
  }
  if (auto r = check_bit_string_unused_zero(v.value(), content_start); !r) {
    return r.error();
  }
  return v;
}

Result<std::vector<std::uint8_t>> decode_constructed_definite(ByteReader& in,
                                                              ber::Tag expected) {
  auto hdr = decode_tlv_header(in);
  if (!hdr) {
    return hdr.error();
  }
  ber::Tag want = expected;
  want.constructed = true;
  if (hdr.value().tag != want) {
    return make_error(Error::Code::TagMismatch, in.offset(),
                      "DER constructed tag mismatch");
  }
  if (auto r = require_constructed(in, hdr.value().tag); !r) {
    return r.error();
  }
  const std::size_t length = hdr.value().length.value;
  if (auto rem = ensure_remaining(in, length); !rem) {
    return rem.error();
  }
  return ber::decode_octet_string_content(in, length);
}

}  // namespace

void encode_boolean(ByteWriter& out, bool value, ber::Tag tag) {
  ber::encode_boolean(out, value, tag);
}

Result<bool> decode_boolean(ByteReader& in, ber::Tag expected) {
  return decode_primitive<bool>(in, expected, decode_boolean_content_der);
}

void encode_integer(ByteWriter& out, std::int64_t value, ber::Tag tag) {
  ber::encode_integer(out, value, tag);
}

void encode_integer(ByteWriter& out, const BigInteger& value, ber::Tag tag) {
  ber::encode_integer(out, value, tag);
}

Result<std::int64_t> decode_integer(ByteReader& in, ber::Tag expected) {
  return decode_primitive<std::int64_t>(in, expected, decode_integer_content_der);
}

Result<BigInteger> decode_big_integer(ByteReader& in, ber::Tag expected) {
  return decode_primitive<BigInteger>(in, expected, decode_big_integer_content_der);
}

void encode_null(ByteWriter& out, ber::Tag tag) {
  ber::encode_null(out, tag);
}

Result<void> decode_null(ByteReader& in, ber::Tag expected) {
  auto hdr = decode_tlv_header(in);
  if (!hdr) {
    return hdr.error();
  }
  if (auto r = require_tag_number(in, expected, hdr.value().tag); !r) {
    return r;
  }
  if (auto r = require_primitive(in, hdr.value().tag); !r) {
    return r;
  }
  return ber::decode_null_content(in, hdr.value().length.value);
}

void encode_octet_string(ByteWriter& out, Span<const std::uint8_t> value, ber::Tag tag) {
  ber::Tag t = tag;
  t.constructed = false;
  ber::encode_octet_string(out, value, t);
}

Result<std::vector<std::uint8_t>> decode_octet_string(ByteReader& in, ber::Tag expected) {
  ber::Tag want = expected;
  want.constructed = false;
  return decode_primitive<std::vector<std::uint8_t>>(in, want,
                                                     ber::decode_octet_string_content);
}

void encode_bit_string(ByteWriter& out, Span<const std::uint8_t> bits,
                       std::uint8_t unused_bits, ber::Tag tag) {
  std::vector<std::uint8_t> cleaned(bits.begin(), bits.end());
  if (unused_bits > 0 && !cleaned.empty()) {
    const std::uint8_t mask =
        static_cast<std::uint8_t>(0xFFu << unused_bits);
    cleaned.back() = static_cast<std::uint8_t>(cleaned.back() & mask);
  }
  ber::Tag t = tag;
  t.constructed = false;
  ber::encode_bit_string(out, cleaned, unused_bits, t);
}

Result<ber::BitStringValue> decode_bit_string(ByteReader& in, ber::Tag expected) {
  ber::Tag want = expected;
  want.constructed = false;
  return decode_primitive<ber::BitStringValue>(in, want, decode_bit_string_content_der);
}

void encode_utf8_string(ByteWriter& out, const std::string& value, ber::Tag tag) {
  ber::Tag t = tag;
  t.constructed = false;
  ber::encode_utf8_string(out, value, t);
}

Result<std::string> decode_utf8_string(ByteReader& in, ber::Tag expected) {
  ber::Tag want = expected;
  want.constructed = false;
  return decode_primitive<std::string>(in, want, ber::decode_utf8_string_content);
}

void encode_enumerated(ByteWriter& out, std::int64_t value, ber::Tag tag) {
  ber::Tag t = tag;
  t.constructed = false;
  ber::encode_enumerated(out, value, t);
}

Result<std::int64_t> decode_enumerated(ByteReader& in, ber::Tag expected) {
  ber::Tag want = expected;
  want.constructed = false;
  return decode_primitive<std::int64_t>(in, want, ber::decode_enumerated_content);
}

void encode_object_identifier(ByteWriter& out, Span<const std::uint64_t> arcs,
                              ber::Tag tag) {
  ber::Tag t = tag;
  t.constructed = false;
  ber::encode_object_identifier(out, arcs, t);
}

Result<std::vector<std::uint64_t>> decode_object_identifier(ByteReader& in,
                                                           ber::Tag expected) {
  ber::Tag want = expected;
  want.constructed = false;
  return decode_primitive<std::vector<std::uint64_t>>(
      in, want, ber::decode_object_identifier_content);
}

void encode_relative_oid(ByteWriter& out, Span<const std::uint64_t> arcs, ber::Tag tag) {
  ber::Tag t = tag;
  t.constructed = false;
  ber::encode_relative_oid(out, arcs, t);
}

Result<std::vector<std::uint64_t>> decode_relative_oid(ByteReader& in, ber::Tag expected) {
  ber::Tag want = expected;
  want.constructed = false;
  return decode_primitive<std::vector<std::uint64_t>>(in, want,
                                                      ber::decode_relative_oid_content);
}

void encode_real(ByteWriter& out, double value, ber::Tag tag) {
  ber::Tag t = tag;
  t.constructed = false;
  ber::encode_real(out, value, t);
}

Result<double> decode_real(ByteReader& in, ber::Tag expected) {
  ber::Tag want = expected;
  want.constructed = false;
  return decode_primitive<double>(in, want, ber::decode_real_content);
}

void encode_sequence(ByteWriter& out, Span<const std::uint8_t> components, ber::Tag tag) {
  ber::encode_constructed(out, tag, components);
}

Result<std::vector<std::uint8_t>> decode_sequence(ByteReader& in, ber::Tag expected) {
  return decode_constructed_definite(in, expected);
}

void encode_set(ByteWriter& out, std::vector<std::vector<std::uint8_t>> components,
                ber::Tag tag) {
  std::sort(components.begin(), components.end(),
            [](const std::vector<std::uint8_t>& a, const std::vector<std::uint8_t>& b) {
              auto ta = read_tag_encoding_prefix(a);
              auto tb = read_tag_encoding_prefix(b);
              if (!ta.ok() || !tb.ok()) {
                return a < b;
              }
              if (ta.value() == tb.value()) {
                return a < b;
              }
              return tag_encoding_less(ta.value(), tb.value());
            });
  ByteWriter body;
  for (const auto& c : components) {
    body.write(c);
  }
  ber::encode_constructed(out, tag, body.buffer());
}

Result<std::vector<std::uint8_t>> decode_set(ByteReader& in, ber::Tag expected) {
  auto content = decode_constructed_definite(in, expected);
  if (!content) {
    return content;
  }
  if (auto r = validate_set_order(content.value()); !r) {
    return r.error();
  }
  return content;
}

namespace {

Result<void> validate_associated_pdv_der_content(Span<const std::uint8_t> content) {
  ByteReader r(content);
  while (!r.eof()) {
    auto hdr = decode_tlv_header(r);
    if (!hdr) {
      return hdr.error();
    }
    if (hdr.value().length.indefinite) {
      return make_error(Error::Code::NonCanonical, r.offset(),
                        "DER forbids indefinite length");
    }
    if (hdr.value().tag.cls != ber::TagClass::Context) {
      return make_error(Error::Code::InvalidArgument, r.offset(),
                        "unexpected component in associated PDV SEQUENCE");
    }
    if ((hdr.value().tag.number == 1 || hdr.value().tag.number == 2) &&
        hdr.value().tag.constructed) {
      return make_error(Error::Code::NonCanonical, r.offset(),
                        "DER forbids constructed string components in associated PDV");
    }
    auto bytes = r.read(hdr.value().length.value);
    if (!bytes) {
      return bytes.error();
    }
  }
  return Result<void>::success();
}

template <typename T>
Result<T> decode_associated_pdv_der(
    ByteReader& in, ber::Tag expected,
    Result<T> (*ber_decode)(ByteReader&, ber::Tag)) {
  ber::Tag want = expected;
  want.constructed = true;
  auto hdr = decode_tlv_header(in);
  if (!hdr) {
    return hdr.error();
  }
  if (auto r = require_tag_number(in, want, hdr.value().tag); !r) {
    return r.error();
  }
  if (auto r = require_constructed(in, hdr.value().tag); !r) {
    return r.error();
  }
  if (hdr.value().length.indefinite) {
    return make_error(Error::Code::NonCanonical, in.offset(),
                      "DER forbids indefinite length");
  }
  const std::size_t len = hdr.value().length.value;
  if (auto rem = ensure_remaining(in, len); !rem) {
    return rem.error();
  }
  auto content = in.read(len);
  if (!content) {
    return content.error();
  }
  if (auto v = validate_associated_pdv_der_content(content.value()); !v) {
    return v.error();
  }
  ByteWriter full;
  ber::encode_tlv(full, want, content.value());
  ByteReader fr(full.buffer());
  return ber_decode(fr, expected);
}

}  // namespace

void encode_embedded_pdv(ByteWriter& out, const ber::EmbeddedPdvValue& value, ber::Tag tag) {
  ber::encode_embedded_pdv(out, value, tag);
}

Result<ber::EmbeddedPdvValue> decode_embedded_pdv(ByteReader& in, ber::Tag expected) {
  return decode_associated_pdv_der<ber::EmbeddedPdvValue>(in, expected, ber::decode_embedded_pdv);
}

void encode_character_string(ByteWriter& out, const ber::CharacterStringValue& value,
                             ber::Tag tag) {
  ber::encode_character_string(out, value, tag);
}

Result<ber::CharacterStringValue> decode_character_string(ByteReader& in, ber::Tag expected) {
  return decode_associated_pdv_der<ber::CharacterStringValue>(in, expected,
                                                              ber::decode_character_string);
}

void encode_external_modern(ByteWriter& out, const ber::ModernExternalValue& value,
                            ber::Tag tag) {
  ber::encode_external_modern(out, value, tag);
}

Result<ber::ModernExternalValue> decode_external_modern(ByteReader& in, ber::Tag expected) {
  return decode_associated_pdv_der<ber::ModernExternalValue>(in, expected,
                                                             ber::decode_external_modern);
}

void encode_external(ByteWriter& out, const ber::ExternalValue& value, ber::Tag tag) {
  ber::encode_external(out, value, tag);
}

Result<ber::ExternalValue> decode_external(ByteReader& in, ber::Tag expected) {
  ber::Tag want = expected;
  want.constructed = true;
  auto hdr = decode_tlv_header(in);
  if (!hdr) {
    return hdr.error();
  }
  if (auto r = require_tag_number(in, want, hdr.value().tag); !r) {
    return r.error();
  }
  if (auto r = require_constructed(in, hdr.value().tag); !r) {
    return r.error();
  }
  if (hdr.value().length.indefinite) {
    return make_error(Error::Code::NonCanonical, in.offset(),
                      "DER forbids indefinite length");
  }
  const std::size_t len = hdr.value().length.value;
  if (auto rem = ensure_remaining(in, len); !rem) {
    return rem.error();
  }
  auto content = in.read(len);
  if (!content) {
    return content.error();
  }
  ByteWriter full;
  ber::encode_tlv(full, want, content.value());
  ByteReader fr(full.buffer());
  return ber::decode_external(fr, expected);
}

}  // namespace der
}  // namespace asn1
