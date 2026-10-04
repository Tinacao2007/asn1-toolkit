/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/src/runtime/der/codec.cpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   DER codec implementation.
**
** Specification: ITU-T X.690 — Distinguished Encoding Rules (DER),
**                 canonical BER subset.
** Design Spec:   asn1-toolkit/docs/ARCHITECTURE.md
**                 asn1-toolkit/README.md
***************************************************************************/
#include <asn1/runtime/der/codec.hpp>

#include <algorithm>
#include <utility>

namespace asn1 {
namespace der {
namespace {

/**
 *  Function    : require_tag_number
 *  Description : Returns success or an error from require tag number.
 *  Parameters  : in — ByteReader& in; expected — ber::Tag expected; got — ber::Tag got
 *  Returns     : Result<void>
 */
Result<void> require_tag_number(ByteReader& in, ber::Tag expected, ber::Tag got) {
  if (got.cls != expected.cls || got.number != expected.number) {
    return make_error(Error::Code::TagMismatch, in.offset(), "DER tag mismatch");
  }
  return Result<void>::success();
}

/**
 *  Function    : require_primitive
 *  Description : Returns success or an error from require primitive.
 *  Parameters  : in — ByteReader& in; tag — const ber::Tag& tag
 *  Returns     : Result<void>
 */
Result<void> require_primitive(ByteReader& in, const ber::Tag& tag) {
  if (tag.constructed) {
    return make_error(Error::Code::NonCanonical, in.offset(),
                      "DER forbids constructed encoding for this type");
  }
  return Result<void>::success();
}

/**
 *  Function    : require_constructed
 *  Description : Returns success or an error from require constructed.
 *  Parameters  : in — ByteReader& in; tag — const ber::Tag& tag
 *  Returns     : Result<void>
 */
Result<void> require_constructed(ByteReader& in, const ber::Tag& tag) {
  if (!tag.constructed) {
    return make_error(Error::Code::NonCanonical, in.offset(),
                      "DER requires constructed encoding for SEQUENCE/SET");
  }
  return Result<void>::success();
}

/**
 *  Function    : ensure_remaining
 *  Description : Returns success or an error from ensure remaining.
 *  Parameters  : in — ByteReader& in; length — std::size_t length
 *  Returns     : Result<void>
 */
Result<void> ensure_remaining(ByteReader& in, std::size_t length) {
  if (in.remaining() < length) {
    return make_error(Error::Code::Truncated, in.offset(), "truncated DER value");
  }
  return Result<void>::success();
}

/**
 *  Function    : check_integer_minimal
 *  Description : Returns success or an error from check integer minimal.
 *  Parameters  : bytes — Span<const std::uint8_t> bytes; offset — std::size_t offset
 *  Returns     : Result<void>
 */
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
  const std::size_t unused =
      v.bits.empty() ? 0u : (v.bits.size() * 8u - v.bit_length);
  if (unused == 0 || v.bits.empty()) {
    return Result<void>::success();
  }
  const std::uint8_t mask =
      static_cast<std::uint8_t>((1u << unused) - 1u);
  if ((v.bits.back() & mask) != 0) {
    return make_error(Error::Code::NonCanonical, offset,
                      "DER BIT STRING unused bits must be zero");
  }
  return Result<void>::success();
}

/**
 *  Function    : tag_encoding
 *  Description : Computes tag encoding from (tag).
 *  Parameters  : tag — ber::Tag tag
 *  Returns     : std::vector<std::uint8_t>
 */
std::vector<std::uint8_t> tag_encoding(ber::Tag tag) {
  ByteWriter w;
  ber::encode_tag(w, tag);
  return w.take();
}

/**
 *  Function    : read_tag_encoding_prefix
 *  Description : Returns success or an error from read tag encoding prefix.
 *  Parameters  : tlv — Span<const std::uint8_t> tlv
 *  Returns     : Result<std::vector<std::uint8_t>>
 */
Result<std::vector<std::uint8_t>> read_tag_encoding_prefix(Span<const std::uint8_t> tlv) {
  ByteReader r(tlv);
  auto tag = ber::decode_tag(r);
  if (!tag) {
    return tag.error();
  }
  return tag_encoding(tag.value());
}

/**
 *  Function    : tag_encoding_less
 *  Description : Returns a boolean result from a, b.
 *  Parameters  : a — Span<const std::uint8_t> a; b — Span<const std::uint8_t> b
 *  Returns     : bool
 */
bool tag_encoding_less(Span<const std::uint8_t> a, Span<const std::uint8_t> b) {
  const std::size_t n = std::min(a.size(), b.size());
  for (std::size_t i = 0; i < n; ++i) {
    if (a[i] != b[i]) {
      return a[i] < b[i];
    }
  }
  return a.size() < b.size();
}

/**
 *  Function    : validate_set_order
 *  Description : Returns success or an error from validate set order.
 *  Parameters  : content — Span<const std::uint8_t> content
 *  Returns     : Result<void>
 */
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
                           /**
                            *  Function    : (*decode_content)
                            *  Description : Returns success or an error from (*decode content).
                            *  Parameters  : decode_content)(ByteReader — *decode_content)(ByteReader&
                            *  Returns     : Result<T>
                            */
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

/**
 *  Function    : decode_boolean_content_der
 *  Description : Returns a boolean result from in, length.
 *  Parameters  : in — ByteReader& in; length — std::size_t length
 *  Returns     : Result<bool>
 */
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

/**
 *  Function    : decode_big_integer_content_der
 *  Description : Returns success or an error from decode big integer content der.
 *  Parameters  : in — ByteReader& in; length — std::size_t length
 *  Returns     : Result<BigInteger>
 */
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

/**
 *  Function    : decode_integer_content_der
 *  Description : Returns success or an error from decode integer content der.
 *  Parameters  : in — ByteReader& in; length — std::size_t length
 *  Returns     : Result<std::int64_t>
 */
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

/**
 *  Function    : encode_boolean
 *  Description : Performs encode boolean (definition).
 *  Parameters  : out — ByteWriter& out; value — bool value; tag — ber::Tag tag
 *  Returns     : void
 */
void encode_boolean(ByteWriter& out, bool value, ber::Tag tag) {
  ber::encode_boolean(out, value, tag);
}

/**
 *  Function    : decode_boolean
 *  Description : Returns a boolean result from in, expected.
 *  Parameters  : in — ByteReader& in; expected — ber::Tag expected
 *  Returns     : Result<bool>
 */
Result<bool> decode_boolean(ByteReader& in, ber::Tag expected) {
  return decode_primitive<bool>(in, expected, decode_boolean_content_der);
}

/**
 *  Function    : encode_integer
 *  Description : Performs encode integer (definition).
 *  Parameters  : out — ByteWriter& out; value — std::int64_t value; tag — ber::Tag tag
 *  Returns     : void
 */
void encode_integer(ByteWriter& out, std::int64_t value, ber::Tag tag) {
  ber::encode_integer(out, value, tag);
}

/**
 *  Function    : encode_integer
 *  Description : Performs encode integer (definition).
 *  Parameters  : out — ByteWriter& out; value — const BigInteger& value; tag — ber::Tag tag
 *  Returns     : void
 */
void encode_integer(ByteWriter& out, const BigInteger& value, ber::Tag tag) {
  ber::encode_integer(out, value, tag);
}

/**
 *  Function    : decode_integer
 *  Description : Returns success or an error from decode integer.
 *  Parameters  : in — ByteReader& in; expected — ber::Tag expected
 *  Returns     : Result<std::int64_t>
 */
Result<std::int64_t> decode_integer(ByteReader& in, ber::Tag expected) {
  return decode_primitive<std::int64_t>(in, expected, decode_integer_content_der);
}

/**
 *  Function    : decode_big_integer
 *  Description : Returns success or an error from decode big integer.
 *  Parameters  : in — ByteReader& in; expected — ber::Tag expected
 *  Returns     : Result<BigInteger>
 */
Result<BigInteger> decode_big_integer(ByteReader& in, ber::Tag expected) {
  return decode_primitive<BigInteger>(in, expected, decode_big_integer_content_der);
}

/**
 *  Function    : encode_null
 *  Description : Performs encode null (definition).
 *  Parameters  : out — ByteWriter& out; tag — ber::Tag tag
 *  Returns     : void
 */
void encode_null(ByteWriter& out, ber::Tag tag) {
  ber::encode_null(out, tag);
}

/**
 *  Function    : decode_null
 *  Description : Returns success or an error from decode null.
 *  Parameters  : in — ByteReader& in; expected — ber::Tag expected
 *  Returns     : Result<void>
 */
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

/**
 *  Function    : encode_octet_string
 *  Description : Performs encode octet string (definition).
 *  Parameters  : out — ByteWriter& out; value — Span<const std::uint8_t> value; tag — ber::Tag tag
 *  Returns     : void
 */
void encode_octet_string(ByteWriter& out, Span<const std::uint8_t> value, ber::Tag tag) {
  ber::Tag t = tag;
  t.constructed = false;
  ber::encode_octet_string(out, value, t);
}

/**
 *  Function    : decode_octet_string
 *  Description : Returns success or an error from decode octet string.
 *  Parameters  : in — ByteReader& in; expected — ber::Tag expected
 *  Returns     : Result<std::vector<std::uint8_t>>
 */
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

/**
 *  Function    : decode_bit_string
 *  Description : Returns success or an error from decode bit string.
 *  Parameters  : in — ByteReader& in; expected — ber::Tag expected
 *  Returns     : Result<ber::BitStringValue>
 */
Result<ber::BitStringValue> decode_bit_string(ByteReader& in, ber::Tag expected) {
  ber::Tag want = expected;
  want.constructed = false;
  return decode_primitive<ber::BitStringValue>(in, want, decode_bit_string_content_der);
}

/**
 *  Function    : encode_utf8_string
 *  Description : Performs encode utf8 string (definition).
 *  Parameters  : out — ByteWriter& out; value — const std::string& value; tag — ber::Tag tag
 *  Returns     : void
 */
void encode_utf8_string(ByteWriter& out, const std::string& value, ber::Tag tag) {
  ber::Tag t = tag;
  t.constructed = false;
  ber::encode_utf8_string(out, value, t);
}

/**
 *  Function    : decode_utf8_string
 *  Description : Builds and returns a string for decode utf8 string.
 *  Parameters  : in — ByteReader& in; expected — ber::Tag expected
 *  Returns     : Result<std::string>
 */
Result<std::string> decode_utf8_string(ByteReader& in, ber::Tag expected) {
  ber::Tag want = expected;
  want.constructed = false;
  return decode_primitive<std::string>(in, want, ber::decode_utf8_string_content);
}

/**
 *  Function    : encode_enumerated
 *  Description : Performs encode enumerated (definition).
 *  Parameters  : out — ByteWriter& out; value — std::int64_t value; tag — ber::Tag tag
 *  Returns     : void
 */
void encode_enumerated(ByteWriter& out, std::int64_t value, ber::Tag tag) {
  ber::Tag t = tag;
  t.constructed = false;
  ber::encode_enumerated(out, value, t);
}

/**
 *  Function    : decode_enumerated
 *  Description : Returns success or an error from decode enumerated.
 *  Parameters  : in — ByteReader& in; expected — ber::Tag expected
 *  Returns     : Result<std::int64_t>
 */
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

/**
 *  Function    : encode_relative_oid
 *  Description : Performs encode relative oid (definition).
 *  Parameters  : out — ByteWriter& out; arcs — Span<const std::uint64_t> arcs; tag — ber::Tag tag
 *  Returns     : void
 */
void encode_relative_oid(ByteWriter& out, Span<const std::uint64_t> arcs, ber::Tag tag) {
  ber::Tag t = tag;
  t.constructed = false;
  ber::encode_relative_oid(out, arcs, t);
}

/**
 *  Function    : decode_relative_oid
 *  Description : Returns success or an error from decode relative oid.
 *  Parameters  : in — ByteReader& in; expected — ber::Tag expected
 *  Returns     : Result<std::vector<std::uint64_t>>
 */
Result<std::vector<std::uint64_t>> decode_relative_oid(ByteReader& in, ber::Tag expected) {
  ber::Tag want = expected;
  want.constructed = false;
  return decode_primitive<std::vector<std::uint64_t>>(in, want,
                                                      ber::decode_relative_oid_content);
}

/**
 *  Function    : encode_real
 *  Description : Performs encode real (definition).
 *  Parameters  : out — ByteWriter& out; value — double value; tag — ber::Tag tag
 *  Returns     : void
 */
void encode_real(ByteWriter& out, double value, ber::Tag tag) {
  ber::Tag t = tag;
  t.constructed = false;
  ber::encode_real(out, value, t);
}

/**
 *  Function    : decode_real
 *  Description : Returns success or an error from decode real.
 *  Parameters  : in — ByteReader& in; expected — ber::Tag expected
 *  Returns     : Result<double>
 */
Result<double> decode_real(ByteReader& in, ber::Tag expected) {
  ber::Tag want = expected;
  want.constructed = false;
  return decode_primitive<double>(in, want, ber::decode_real_content);
}

/**
 *  Function    : encode_sequence
 *  Description : Performs encode sequence (definition).
 *  Parameters  : out — ByteWriter& out; components — Span<const std::uint8_t> components; tag — ber::Tag tag
 *  Returns     : void
 */
void encode_sequence(ByteWriter& out, Span<const std::uint8_t> components, ber::Tag tag) {
  ber::encode_constructed(out, tag, components);
}

/**
 *  Function    : decode_sequence
 *  Description : Returns success or an error from decode sequence.
 *  Parameters  : in — ByteReader& in; expected — ber::Tag expected
 *  Returns     : Result<std::vector<std::uint8_t>>
 */
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

/**
 *  Function    : decode_set
 *  Description : Returns success or an error from decode set.
 *  Parameters  : in — ByteReader& in; expected — ber::Tag expected
 *  Returns     : Result<std::vector<std::uint8_t>>
 */
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

/**
 *  Function    : validate_associated_pdv_der_content
 *  Description : Returns success or an error from validate associated pdv der content.
 *  Parameters  : content — Span<const std::uint8_t> content
 *  Returns     : Result<void>
 */
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
        /**
         *  Function    : value
         *  Description : Computes value from (none).
         *  Parameters  : none
         *  Returns     : hdr.
         */
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
    /**
     *  Function    : (*ber_decode)
     *  Description : Returns success or an error from (*ber decode).
     *  Parameters  : ber_decode)(ByteReader — *ber_decode)(ByteReader&
     *  Returns     : Result<T>
     */
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

/**
 *  Function    : encode_embedded_pdv
 *  Description : Performs encode embedded pdv (definition).
 *  Parameters  : out — ByteWriter& out; value — const ber::EmbeddedPdvValue& value; tag — ber::Tag tag
 *  Returns     : void
 */
void encode_embedded_pdv(ByteWriter& out, const ber::EmbeddedPdvValue& value, ber::Tag tag) {
  ber::encode_embedded_pdv(out, value, tag);
}

/**
 *  Function    : decode_embedded_pdv
 *  Description : Returns success or an error from decode embedded pdv.
 *  Parameters  : in — ByteReader& in; expected — ber::Tag expected
 *  Returns     : Result<ber::EmbeddedPdvValue>
 */
Result<ber::EmbeddedPdvValue> decode_embedded_pdv(ByteReader& in, ber::Tag expected) {
  return decode_associated_pdv_der<ber::EmbeddedPdvValue>(in, expected, ber::decode_embedded_pdv);
}

void encode_character_string(ByteWriter& out, const ber::CharacterStringValue& value,
                             ber::Tag tag) {
  ber::encode_character_string(out, value, tag);
}

/**
 *  Function    : decode_character_string
 *  Description : Returns success or an error from decode character string.
 *  Parameters  : in — ByteReader& in; expected — ber::Tag expected
 *  Returns     : Result<ber::CharacterStringValue>
 */
Result<ber::CharacterStringValue> decode_character_string(ByteReader& in, ber::Tag expected) {
  return decode_associated_pdv_der<ber::CharacterStringValue>(in, expected,
                                                              ber::decode_character_string);
}

void encode_external_modern(ByteWriter& out, const ber::ModernExternalValue& value,
                            ber::Tag tag) {
  ber::encode_external_modern(out, value, tag);
}

/**
 *  Function    : decode_external_modern
 *  Description : Returns success or an error from decode external modern.
 *  Parameters  : in — ByteReader& in; expected — ber::Tag expected
 *  Returns     : Result<ber::ModernExternalValue>
 */
Result<ber::ModernExternalValue> decode_external_modern(ByteReader& in, ber::Tag expected) {
  return decode_associated_pdv_der<ber::ModernExternalValue>(in, expected,
                                                             ber::decode_external_modern);
}

/**
 *  Function    : encode_external
 *  Description : Performs encode external (definition).
 *  Parameters  : out — ByteWriter& out; value — const ber::ExternalValue& value; tag — ber::Tag tag
 *  Returns     : void
 */
void encode_external(ByteWriter& out, const ber::ExternalValue& value, ber::Tag tag) {
  ber::encode_external(out, value, tag);
}

/**
 *  Function    : decode_external
 *  Description : Returns success or an error from decode external.
 *  Parameters  : in — ByteReader& in; expected — ber::Tag expected
 *  Returns     : Result<ber::ExternalValue>
 */
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
