/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/src/runtime/cer/codec.cpp
**   $Version: 0.1
**   $Date:   2026-10-14
**   $Author: tina.cao
**
**  File Description:
**
**   CER codec implementation.
**
** Specification: ITU-T X.690 — Canonical Encoding Rules (CER).
**                 Clause 9 (CER restrictions) + clause 11 (restrictions
**                 common to CER and DER).
** Design Spec:   asn1-toolkit/docs/architecture.md
**                 asn1-toolkit/README.md
**************************************************************************/
#include <asn1/runtime/cer/codec.hpp>

#include <algorithm>
#include <utility>

namespace asn1 {
namespace cer {
namespace {

/// Maximum contents octets of a primitive CER string (X.690 9.2).
constexpr std::size_t kFragmentContents = 1000;
/// Data octets per non-final BIT STRING fragment: a fragment's 1000 contents
/// octets include its own initial (unused-bits) octet (X.690 9.2 + 8.6.4).
constexpr std::size_t kBitStringFragmentData = kFragmentContents - 1;

Result<void> tag_ok(ByteReader& in, const ber::Tag& got, const ber::Tag& expected,
                    const char* what) {
  if (got.cls != expected.cls || got.number != expected.number) {
    return make_error(Error::Code::TagMismatch, in.offset(), what);
  }
  return Result<void>::success();
}

Result<void> require_indefinite(ByteReader& in, const ber::TlvHeader& hdr) {
  if (!hdr.length.indefinite) {
    return make_error(Error::Code::NonCanonical, in.offset(),
                      "CER requires the indefinite length form for constructed encodings");
  }
  return Result<void>::success();
}

bool at_end_of_contents(ByteReader& in) {
  auto p0 = in.peek(0);
  auto p1 = in.peek(1);
  return p0 && p1 && p0.value() == 0x00 && p1.value() == 0x00;
}

/// Copy one complete TLV (raw bytes, any length form, nested indefinite
/// included) from `in` into `out`. The BER indefinite reader refuses nested
/// indefinite lengths; CER nests them everywhere, so CER needs its own.
Result<void> copy_one_tlv(ByteReader& in, ByteWriter& out) {
  auto b0 = in.peek(0);
  if (!b0) {
    return make_error(Error::Code::InvalidArgument, in.offset(), "truncated TLV");
  }
  std::size_t hdr = 1;
  if ((b0.value() & 0x1F) == 0x1F) {  // high-tag-number form
    for (;;) {
      auto b = in.peek(hdr);
      if (!b) {
        return make_error(Error::Code::InvalidArgument, in.offset(), "truncated tag octets");
      }
      ++hdr;
      if ((b.value() & 0x80) == 0) {
        break;
      }
    }
  }
  auto lb = in.peek(hdr);
  if (!lb) {
    return make_error(Error::Code::InvalidArgument, in.offset(), "truncated length octets");
  }
  ++hdr;
  const bool indefinite = lb.value() == 0x80;
  std::size_t content = 0;
  if (indefinite) {
    // content length determined by the end-of-contents marker below
  } else if (lb.value() > 0x7F) {  // long form
    const std::size_t n = lb.value() & 0x7F;
    for (std::size_t i = 0; i < n; ++i) {
      auto b = in.peek(hdr + i);
      if (!b) {
        return make_error(Error::Code::InvalidArgument, in.offset(), "truncated length octets");
      }
      content = (content << 8) | b.value();
    }
    hdr += n;
  } else {
    content = lb.value();  // short form: the length IS the contents octets
  }
  auto hb = in.read(hdr);
  if (!hb) {
    return hb.error();
  }
  out.write(hb.value());
  if (indefinite) {
    for (;;) {
      if (at_end_of_contents(in)) {
        auto eoc = in.read(2);
        if (!eoc) {
          return eoc.error();
        }
        out.write(eoc.value());
        break;
      }
      if (auto r = copy_one_tlv(in, out); !r) {
        return r.error();
      }
    }
    return Result<void>::success();
  }
  auto cb = in.read(content);
  if (!cb) {
    return cb.error();
  }
  out.write(cb.value());
  return Result<void>::success();
}

/// Length of the leading identifier octets of a complete TLV.
std::size_t tag_octet_count(Span<const std::uint8_t> tlv) {
  if (tlv.empty()) {
    return 0;
  }
  std::size_t n = 1;
  if ((tlv[0] & 0x1F) == 0x1F) {
    while (n < tlv.size() && (tlv[n] & 0x80) != 0) {
      ++n;
    }
    if (n < tlv.size()) {
      ++n;
    }
  }
  return n;
}

void sort_components(std::vector<std::vector<std::uint8_t>>& components) {
  std::sort(components.begin(), components.end(),
            [](const std::vector<std::uint8_t>& a, const std::vector<std::uint8_t>& b) {
              const std::size_t na = tag_octet_count(a);
              const std::size_t nb = tag_octet_count(b);
              const std::size_t n = std::min(na, nb);
              for (std::size_t i = 0; i < n; ++i) {
                if (a[i] != b[i]) {
                  return a[i] < b[i];
                }
              }
              if (na != nb) {
                return na < nb;
              }
              return a < b;  // X.690 11.6: equal tags order by full encoding
            });
}

void write_indefinite(ByteWriter& out, ber::Tag tag, Span<const std::uint8_t> components) {
  ber::Tag t = tag;
  t.constructed = true;
  ber::encode_tag(out, t);
  out.put(0x80);
  out.write(components);
  out.put(0x00);
  out.put(0x00);  // end-of-contents (X.690 8.1.5)
}

/// Concatenate primitive fragments of an indefinite constructed string.
/// Fragments carry `expected` (constructed bit ignored) and each keeps at
/// most 1000 contents octets (X.690 9.2).
Result<std::vector<std::uint8_t>> read_string_fragments(ByteReader& in, ber::Tag expected) {
  std::vector<std::uint8_t> out;
  for (;;) {
    if (at_end_of_contents(in)) {
      if (auto eoc = ber::decode_end_of_contents(in); !eoc) {
        return eoc.error();
      }
      break;
    }
    auto fh = ber::decode_tlv_header(in);
    if (!fh) {
      return fh.error();
    }
    if (auto r = tag_ok(in, fh.value().tag, expected, "CER string fragment tag mismatch"); !r) {
      return r.error();
    }
    if (fh.value().tag.constructed) {
      return make_error(Error::Code::NonCanonical, in.offset(),
                        "CER string fragments shall be primitive");
    }
    if (fh.value().length.indefinite) {
      return make_error(Error::Code::NonCanonical, in.offset(),
                        "CER string fragments shall be definite");
    }
    if (fh.value().length.value > kFragmentContents) {
      return make_error(Error::Code::NonCanonical, in.offset(),
                        "CER string fragment exceeds 1000 contents octets");
    }
    auto part = in.read(fh.value().length.value);
    if (!part) {
      return part.error();
    }
    out.insert(out.end(), part.value().begin(), part.value().end());
  }
  return out;
}

}  // namespace

// ---- Primitives: BER encodings are already primitive / minimal ----

void encode_boolean(ByteWriter& out, bool value, ber::Tag tag) {
  ber::encode_boolean(out, value, tag);
}

Result<bool> decode_boolean(ByteReader& in, ber::Tag expected) {
  return ber::decode_boolean(in, expected);
}

void encode_integer(ByteWriter& out, std::int64_t value, ber::Tag tag) {
  ber::encode_integer(out, value, tag);
}

void encode_integer(ByteWriter& out, const BigInteger& value, ber::Tag tag) {
  ber::encode_integer(out, value, tag);
}

Result<std::int64_t> decode_integer(ByteReader& in, ber::Tag expected) {
  return ber::decode_integer(in, expected);
}

Result<BigInteger> decode_big_integer(ByteReader& in, ber::Tag expected) {
  return ber::decode_big_integer(in, expected);
}

void encode_null(ByteWriter& out, ber::Tag tag) { ber::encode_null(out, tag); }

Result<void> decode_null(ByteReader& in, ber::Tag expected) {
  return ber::decode_null(in, expected);
}

void encode_enumerated(ByteWriter& out, std::int64_t value, ber::Tag tag) {
  ber::encode_enumerated(out, value, tag);
}

Result<std::int64_t> decode_enumerated(ByteReader& in, ber::Tag expected) {
  return ber::decode_enumerated(in, expected);
}

void encode_object_identifier(ByteWriter& out, Span<const std::uint64_t> arcs, ber::Tag tag) {
  ber::encode_object_identifier(out, arcs, tag);
}

Result<std::vector<std::uint64_t>> decode_object_identifier(ByteReader& in, ber::Tag expected) {
  return ber::decode_object_identifier(in, expected);
}

void encode_relative_oid(ByteWriter& out, Span<const std::uint64_t> arcs, ber::Tag tag) {
  ber::encode_relative_oid(out, arcs, tag);
}

Result<std::vector<std::uint64_t>> decode_relative_oid(ByteReader& in, ber::Tag expected) {
  return ber::decode_relative_oid(in, expected);
}

void encode_real(ByteWriter& out, double value, ber::Tag tag) {
  ber::encode_real(out, value, tag);
}

Result<double> decode_real(ByteReader& in, ber::Tag expected) {
  return ber::decode_real(in, expected);
}

// ---- Strings (X.690 9.2) ----

void encode_octet_string(ByteWriter& out, Span<const std::uint8_t> value, ber::Tag tag) {
  if (value.size() <= kFragmentContents) {
    ber::Tag t = tag;
    t.constructed = false;
    ber::encode_octet_string(out, value, t);
    return;
  }
  ByteWriter body;
  std::size_t off = 0;
  while (off < value.size()) {
    const std::size_t n = std::min<std::size_t>(kFragmentContents, value.size() - off);
    ber::Tag ft = tag;
    ft.constructed = false;
    ber::encode_octet_string(body, Span<const std::uint8_t>(value.data() + off, n), ft);
    off += n;
  }
  write_indefinite(out, tag, body.buffer());
}

Result<std::vector<std::uint8_t>> decode_octet_string(ByteReader& in, ber::Tag expected) {
  auto hdr = ber::decode_tlv_header(in);
  if (!hdr) {
    return hdr.error();
  }
  if (auto r = tag_ok(in, hdr.value().tag, expected, "CER octet string tag mismatch"); !r) {
    return r.error();
  }
  if (!hdr.value().tag.constructed) {
    if (hdr.value().length.indefinite) {
      return make_error(Error::Code::NonCanonical, in.offset(),
                        "CER primitive string with indefinite length");
    }
    if (hdr.value().length.value > kFragmentContents) {
      return make_error(Error::Code::NonCanonical, in.offset(),
                        "CER octet strings over 1000 contents octets shall be constructed");
    }
    return ber::decode_octet_string_content(in, hdr.value().length.value);
  }
  if (auto r = require_indefinite(in, hdr.value()); !r) {
    return r.error();
  }
  auto out = read_string_fragments(in, expected);
  if (!out) {
    return out.error();
  }
  if (out.value().size() <= kFragmentContents) {
    return make_error(Error::Code::NonCanonical, in.offset(),
                      "CER octet strings within 1000 contents octets shall be primitive");
  }
  return out;
}

void encode_bit_string(ByteWriter& out, Span<const std::uint8_t> bits, std::uint8_t unused_bits,
                       ber::Tag tag) {
  // X.690 11.2: the unused bits of the last octet shall be zero. The BER
  // primitive encoder writes the octets verbatim, so CER encodes a masked
  // copy (DER shares the gap and is left untouched).
  std::vector<std::uint8_t> masked;
  if (unused_bits > 0 && !bits.empty()) {
    masked.assign(bits.begin(), bits.end());
    masked.back() &= static_cast<std::uint8_t>(0xFFu << unused_bits);
    bits = Span<const std::uint8_t>(masked.data(), masked.size());
  }
  const std::size_t data = bits.size();
  if (data + 1 <= kFragmentContents) {
    ber::encode_bit_string(out, bits, unused_bits, tag);
    return;
  }
  ByteWriter body;
  std::size_t off = 0;
  while (off < data) {
    const std::size_t n = std::min<std::size_t>(kBitStringFragmentData, data - off);
    const std::uint8_t seg_unused = (n == data - off) ? unused_bits : 0;
    ber::Tag ft = tag;
    ft.constructed = false;
    ber::encode_bit_string(body, Span<const std::uint8_t>(bits.data() + off, n), seg_unused, ft);
    off += n;
  }
  write_indefinite(out, tag, body.buffer());
}

Result<ber::BitStringValue> decode_bit_string(ByteReader& in, ber::Tag expected) {
  auto hdr = ber::decode_tlv_header(in);
  if (!hdr) {
    return hdr.error();
  }
  if (auto r = tag_ok(in, hdr.value().tag, expected, "CER bit string tag mismatch"); !r) {
    return r.error();
  }
  if (!hdr.value().tag.constructed) {
    if (hdr.value().length.indefinite) {
      return make_error(Error::Code::NonCanonical, in.offset(),
                        "CER primitive string with indefinite length");
    }
    if (hdr.value().length.value > kFragmentContents) {
      return make_error(Error::Code::NonCanonical, in.offset(),
                        "CER bit strings over 1000 contents octets shall be constructed");
    }
    return ber::decode_bit_string_content(in, hdr.value().length.value);
  }
  if (auto r = require_indefinite(in, hdr.value()); !r) {
    return r.error();
  }
  // Each fragment is a complete BIT STRING TLV (X.690 8.6.4): its own
  // initial octet counts toward its 1000 contents octets; only the last
  // fragment may carry unused bits.
  std::vector<std::uint8_t> bits;
  std::size_t total_bits = 0;
  for (;;) {
    if (at_end_of_contents(in)) {
      if (auto eoc = ber::decode_end_of_contents(in); !eoc) {
        return eoc.error();
      }
      break;
    }
    auto fh = ber::decode_tlv_header(in);
    if (!fh) {
      return fh.error();
    }
    if (auto r = tag_ok(in, fh.value().tag, expected, "CER bit string fragment tag mismatch"); !r) {
      return r.error();
    }
    if (fh.value().tag.constructed || fh.value().length.indefinite) {
      return make_error(Error::Code::NonCanonical, in.offset(),
                        "CER bit string fragments shall be primitive and definite");
    }
    if (fh.value().length.value > kFragmentContents) {
      return make_error(Error::Code::NonCanonical, in.offset(),
                        "CER bit string fragment exceeds 1000 contents octets");
    }
    auto seg = ber::decode_bit_string_content(in, fh.value().length.value);
    if (!seg) {
      return seg.error();
    }
    bits.insert(bits.end(), seg.value().bits.begin(), seg.value().bits.end());
    total_bits += seg.value().bit_length;
  }
  if (bits.size() + 1 <= kFragmentContents) {
    return make_error(Error::Code::NonCanonical, in.offset(),
                      "CER bit strings within 1000 contents octets shall be primitive");
  }
  ber::BitStringValue v;
  v.bits = std::move(bits);
  v.bit_length = total_bits;
  return v;
}

void encode_utf8_string(ByteWriter& out, const std::string& value, ber::Tag tag) {
  if (value.size() <= kFragmentContents) {
    ber::Tag t = tag;
    t.constructed = false;
    ber::encode_utf8_string(out, value, t);
    return;
  }
  ByteWriter body;
  std::size_t off = 0;
  while (off < value.size()) {
    const std::size_t n = std::min<std::size_t>(kFragmentContents, value.size() - off);
    ber::Tag ft = tag;
    ft.constructed = false;
    ber::encode_utf8_string(body, value.substr(off, n), ft);
    off += n;
  }
  write_indefinite(out, tag, body.buffer());
}

Result<std::string> decode_utf8_string(ByteReader& in, ber::Tag expected) {
  auto hdr = ber::decode_tlv_header(in);
  if (!hdr) {
    return hdr.error();
  }
  if (auto r = tag_ok(in, hdr.value().tag, expected, "CER utf8 string tag mismatch"); !r) {
    return r.error();
  }
  if (!hdr.value().tag.constructed) {
    if (hdr.value().length.indefinite) {
      return make_error(Error::Code::NonCanonical, in.offset(),
                        "CER primitive string with indefinite length");
    }
    if (hdr.value().length.value > kFragmentContents) {
      return make_error(Error::Code::NonCanonical, in.offset(),
                        "CER utf8 strings over 1000 contents octets shall be constructed");
    }
    return ber::decode_utf8_string_content(in, hdr.value().length.value);
  }
  if (auto r = require_indefinite(in, hdr.value()); !r) {
    return r.error();
  }
  auto out = read_string_fragments(in, expected);
  if (!out) {
    return out.error();
  }
  if (out.value().size() <= kFragmentContents) {
    return make_error(Error::Code::NonCanonical, in.offset(),
                      "CER utf8 strings within 1000 contents octets shall be primitive");
  }
  return std::string(out.value().begin(), out.value().end());
}

// ---- Constructed wrappers (X.690 9.1 indefinite) ----

void encode_constructed(ByteWriter& out, ber::Tag tag, Span<const std::uint8_t> components) {
  write_indefinite(out, tag, components);
}

void encode_sequence(ByteWriter& out, Span<const std::uint8_t> components, ber::Tag tag) {
  write_indefinite(out, tag, components);
}

Result<std::vector<std::uint8_t>> decode_constructed(ByteReader& in, ber::Tag expected) {
  auto hdr = ber::decode_tlv_header(in);
  if (!hdr) {
    return hdr.error();
  }
  if (auto r = tag_ok(in, hdr.value().tag, expected, "CER constructed tag mismatch"); !r) {
    return r.error();
  }
  if (!hdr.value().tag.constructed) {
    return make_error(Error::Code::TagMismatch, in.offset(), "CER constructed encoding expected");
  }
  if (auto r = require_indefinite(in, hdr.value()); !r) {
    return r.error();
  }
  ByteWriter acc;
  for (;;) {
    if (at_end_of_contents(in)) {
      if (auto eoc = ber::decode_end_of_contents(in); !eoc) {
        return eoc.error();
      }
      break;
    }
    if (auto r = copy_one_tlv(in, acc); !r) {
      return r.error();
    }
  }
  return acc.take();
}

Result<std::vector<std::uint8_t>> decode_sequence(ByteReader& in, ber::Tag expected) {
  // Qualified: ADL would also bring asn1::ber::decode_constructed into the
  // candidate set (ber::Tag is an argument type) and the call would be
  // ambiguous.
  return cer::decode_constructed(in, expected);
}

void encode_set(ByteWriter& out, std::vector<std::vector<std::uint8_t>> components, ber::Tag tag) {
  sort_components(components);
  ByteWriter body;
  for (const auto& c : components) {
    body.write(c);
  }
  write_indefinite(out, tag, body.buffer());
}

void encode_set_ordered(ByteWriter& out, std::vector<std::vector<std::uint8_t>> components,
                        ber::Tag tag) {
  ByteWriter body;
  for (const auto& c : components) {
    body.write(c);
  }
  write_indefinite(out, tag, body.buffer());
}

Result<std::vector<std::uint8_t>> decode_set(ByteReader& in, ber::Tag expected) {
  // Component order is intentionally NOT validated on decode: X.690 9.3
  // orders an untagged CHOICE component by the SMALLEST tag in the choice,
  // which the runtime cannot reconstruct from the encoded bytes — a valid
  // CER SET may therefore not be ascending in its encoded tag prefixes.
  // Qualified: ADL would also bring asn1::ber::decode_constructed into the
  // candidate set (ber::Tag is an argument type) and the call would be
  // ambiguous.
  return cer::decode_constructed(in, expected);
}

}  // namespace cer
}  // namespace asn1
