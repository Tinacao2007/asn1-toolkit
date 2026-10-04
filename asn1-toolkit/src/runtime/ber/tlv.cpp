/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/src/runtime/ber/tlv.cpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   BER TLV parsing and serialization.
**
** Specification: ITU-T X.690 — ASN.1 encoding rules: Basic Encoding
**                 Rules (BER).
** Design Spec:   asn1-toolkit/docs/ARCHITECTURE.md
**                 asn1-toolkit/README.md
***************************************************************************/
#include <asn1/runtime/ber/tlv.hpp>
#include <asn1/runtime/limits.hpp>

#include <climits>
#include <cstdint>

namespace asn1 {
namespace ber {
namespace {

/**
 *  Function    : fail
 *  Description : Returns success or an error from fail.
 *  Parameters  : code — Error::Code code; offset — std::size_t offset; message — std::string message
 *  Returns     : Result<void>
 */
Result<void> fail(Error::Code code, std::size_t offset, std::string message) {
  return make_error(code, offset, std::move(message));
}

}  // namespace

/**
 *  Function    : encode_tag
 *  Description : Performs encode tag (definition).
 *  Parameters  : out — ByteWriter& out; tag — Tag tag
 *  Returns     : void
 */
void encode_tag(ByteWriter& out, Tag tag) {
  std::uint8_t first = static_cast<std::uint8_t>(
      (static_cast<std::uint8_t>(tag.cls) << 6) | (tag.constructed ? 0x20u : 0u));
  if (tag.number < 31) {
    out.put(static_cast<std::uint8_t>(first | static_cast<std::uint8_t>(tag.number)));
    return;
  }
  out.put(static_cast<std::uint8_t>(first | 0x1Fu));
  // Base-128 high-tag-number form, most significant first, without leading 0x80 bytes.
  std::uint64_t n = tag.number;
  std::uint8_t stack[10];
  int sp = 0;
  stack[sp++] = static_cast<std::uint8_t>(n & 0x7Fu);
  n >>= 7;
  while (n > 0) {
    stack[sp++] = static_cast<std::uint8_t>(0x80u | (n & 0x7Fu));
    n >>= 7;
  }
  while (sp > 0) {
    out.put(stack[--sp]);
  }
}

/**
 *  Function    : decode_tag
 *  Description : Returns success or an error from decode tag.
 *  Parameters  : in — ByteReader& in
 *  Returns     : Result<Tag>
 */
Result<Tag> decode_tag(ByteReader& in) {
  auto first_r = in.get();
  if (!first_r) {
    return first_r.error();
  }
  const std::uint8_t first = first_r.value();
  Tag tag;
  tag.cls = static_cast<TagClass>((first >> 6) & 0x03u);
  tag.constructed = (first & 0x20u) != 0;
  const std::uint8_t low = static_cast<std::uint8_t>(first & 0x1Fu);
  if (low != 0x1Fu) {
    tag.number = low;
    return tag;
  }

  // High-tag-number form.
  tag.number = 0;
  bool first_octet = true;
  for (;;) {
    auto b_r = in.get();
    if (!b_r) {
      return b_r.error();
    }
    const std::uint8_t b = b_r.value();
    if (first_octet && b == 0x80u) {
      return make_error(Error::Code::InvalidArgument, in.offset() - 1,
                        "invalid high-tag-number encoding (leading 0x80)");
    }
    first_octet = false;
    if (tag.number > (UINT64_MAX >> 7)) {
      return make_error(Error::Code::LengthOverflow, in.offset() - 1,
                        "tag number too large");
    }
    tag.number = (tag.number << 7) | static_cast<std::uint64_t>(b & 0x7Fu);
    if ((b & 0x80u) == 0) {
      break;
    }
  }
  return tag;
}

/**
 *  Function    : encode_length
 *  Description : Performs encode length (definition).
 *  Parameters  : out — ByteWriter& out; length — std::size_t length
 *  Returns     : void
 */
void encode_length(ByteWriter& out, std::size_t length) {
  if (length <= 0x7Fu) {
    out.put(static_cast<std::uint8_t>(length));
    return;
  }
  std::uint8_t bytes[sizeof(std::size_t)];
  int n = 0;
  std::size_t v = length;
  while (v > 0) {
    bytes[n++] = static_cast<std::uint8_t>(v & 0xFFu);
    v >>= 8;
  }
  out.put(static_cast<std::uint8_t>(0x80u | static_cast<std::uint8_t>(n)));
  while (n > 0) {
    out.put(bytes[--n]);
  }
}

/**
 *  Function    : decode_length
 *  Description : Returns success or an error from decode length.
 *  Parameters  : in — ByteReader& in
 *  Returns     : Result<Length>
 */
Result<Length> decode_length(ByteReader& in) {
  auto first_r = in.get();
  if (!first_r) {
    return first_r.error();
  }
  const std::uint8_t first = first_r.value();
  Length len;
  if (first == 0x80u) {
    len.indefinite = true;
    len.value = 0;
    return len;
  }
  if ((first & 0x80u) == 0) {
    len.value = first;
    if (len.value > kMaxCodecBytes) {
      return make_error(Error::Code::LengthOverflow, in.offset() - 1,
                        "TLV length exceeds codec limit");
    }
    return len;
  }
  const std::uint8_t nbytes = static_cast<std::uint8_t>(first & 0x7Fu);
  if (nbytes == 0 || nbytes > sizeof(std::size_t)) {
    return make_error(Error::Code::LengthOverflow, in.offset() - 1,
                      "unsupported long-form length size");
  }
  std::size_t value = 0;
  for (std::uint8_t i = 0; i < nbytes; ++i) {
    auto b_r = in.get();
    if (!b_r) {
      return b_r.error();
    }
    value = (value << 8) | b_r.value();
  }
  len.value = value;
  if (len.value > kMaxCodecBytes) {
    return make_error(Error::Code::LengthOverflow, in.offset() - 1,
                      "TLV length exceeds codec limit");
  }
  return len;
}

/**
 *  Function    : encode_tlv
 *  Description : Performs encode tlv (definition).
 *  Parameters  : out — ByteWriter& out; tag — Tag tag; content — Span<const std::uint8_t> content
 *  Returns     : void
 */
void encode_tlv(ByteWriter& out, Tag tag, Span<const std::uint8_t> content) {
  encode_tag(out, tag);
  encode_length(out, content.size());
  out.write(content);
}

/**
 *  Function    : decode_tlv_header
 *  Description : Returns success or an error from decode tlv header.
 *  Parameters  : in — ByteReader& in
 *  Returns     : Result<TlvHeader>
 */
Result<TlvHeader> decode_tlv_header(ByteReader& in) {
  auto tag_r = decode_tag(in);
  if (!tag_r) {
    return tag_r.error();
  }
  auto len_r = decode_length(in);
  if (!len_r) {
    return len_r.error();
  }
  return TlvHeader{tag_r.value(), len_r.value()};
}

/**
 *  Function    : decode_end_of_contents
 *  Description : Returns success or an error from decode end of contents.
 *  Parameters  : in — ByteReader& in
 *  Returns     : Result<void>
 */
Result<void> decode_end_of_contents(ByteReader& in) {
  auto a = in.get();
  if (!a) {
    return a.error();
  }
  auto b = in.get();
  if (!b) {
    return b.error();
  }
  if (a.value() != 0 || b.value() != 0) {
    return make_error(Error::Code::InvalidArgument, in.offset() - 2,
                      "expected end-of-contents octets 0x00 0x00");
  }
  return Result<void>::success();
}

}  // namespace ber
}  // namespace asn1
