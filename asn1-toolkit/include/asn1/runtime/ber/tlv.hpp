/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/include/asn1/runtime/ber/tlv.hpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   BER TLV tag/length/value helpers and type tags.
**
** Specification: ITU-T X.690 — ASN.1 encoding rules: Basic Encoding
**                 Rules (BER).
** Design Spec:   asn1-toolkit/docs/ARCHITECTURE.md
**                 asn1-toolkit/README.md
***************************************************************************/
#pragma once

#include <asn1/runtime/byte_io.hpp>

#include <cstdint>

namespace asn1 {
namespace ber {

enum class TagClass : std::uint8_t {
  Universal = 0,
  Application = 1,
  Context = 2,
  Private = 3,
};

struct Tag {
  TagClass cls = TagClass::Universal;
  bool constructed = false;
  std::uint64_t number = 0;

  /**
   *  Function    : operator==
   *  Description : Implements operator== for this type.
   *  Parameters  : o — const Tag& o
   *  Returns     : —
   */
  bool operator==(const Tag& o) const noexcept {
    return cls == o.cls && constructed == o.constructed && number == o.number;
  }
  /**
   *  Function    : !
   *  Description : Performs ! (definition).
   *  Parameters  : this — const Tag& o) const noexcept { return !(*this
   *  Returns     : —
   */
  bool operator!=(const Tag& o) const noexcept { return !(*this == o); }
};

/**
 *  Function    : universal
 *  Description : Computes universal from (number, constructed).
 *  Parameters  : number — std::uint64_t number; constructed — bool constructed
 *  Returns     : inline Tag
 */
inline Tag universal(std::uint64_t number, bool constructed = false) {
  return Tag{TagClass::Universal, constructed, number};
}
/**
 *  Function    : application
 *  Description : Computes application from (number, constructed).
 *  Parameters  : number — std::uint64_t number; constructed — bool constructed
 *  Returns     : inline Tag
 */
inline Tag application(std::uint64_t number, bool constructed = false) {
  return Tag{TagClass::Application, constructed, number};
}
/**
 *  Function    : context
 *  Description : Computes context from (number, constructed).
 *  Parameters  : number — std::uint64_t number; constructed — bool constructed
 *  Returns     : inline Tag
 */
inline Tag context(std::uint64_t number, bool constructed = false) {
  return Tag{TagClass::Context, constructed, number};
}
/**
 *  Function    : private_tag
 *  Description : Computes private tag from (number, constructed).
 *  Parameters  : number — std::uint64_t number; constructed — bool constructed
 *  Returns     : inline Tag
 */
inline Tag private_tag(std::uint64_t number, bool constructed = false) {
  return Tag{TagClass::Private, constructed, number};
}

// Universal tag numbers (X.680)
constexpr std::uint64_t kTagBoolean = 1;
constexpr std::uint64_t kTagInteger = 2;
constexpr std::uint64_t kTagBitString = 3;
constexpr std::uint64_t kTagOctetString = 4;
constexpr std::uint64_t kTagNull = 5;
constexpr std::uint64_t kTagOid = 6;
constexpr std::uint64_t kTagObjectDescriptor = 7;
constexpr std::uint64_t kTagExternal = 8;
constexpr std::uint64_t kTagReal = 9;
constexpr std::uint64_t kTagEnumerated = 10;
constexpr std::uint64_t kTagEmbeddedPdv = 11;
constexpr std::uint64_t kTagUtf8String = 12;
constexpr std::uint64_t kTagRelativeOid = 13;
constexpr std::uint64_t kTagSequence = 16;
constexpr std::uint64_t kTagSet = 17;
constexpr std::uint64_t kTagIa5String = 22;
constexpr std::uint64_t kTagCharacterString = 29;

struct Length {
  bool indefinite = false;
  std::size_t value = 0;  // meaningful when !indefinite
};

/**
 *  Function    : encode_tag
 *  Description : Performs encode tag (declaration).
 *  Parameters  : out — ByteWriter& out; tag — Tag tag
 *  Returns     : void
 */
void encode_tag(ByteWriter& out, Tag tag);
/**
 *  Function    : decode_tag
 *  Description : Returns success or an error from decode tag.
 *  Parameters  : in — ByteReader& in
 *  Returns     : Result<Tag>
 */
Result<Tag> decode_tag(ByteReader& in);

/// Encode a definite length (short or long form). Never encodes indefinite.
/**
 *  Function    : encode_length
 *  Description : Performs encode length (declaration).
 *  Parameters  : out — ByteWriter& out; length — std::size_t length
 *  Returns     : void
 */
void encode_length(ByteWriter& out, std::size_t length);

/// Decode short, long, or indefinite length. Indefinite => Length::indefinite.
/**
 *  Function    : decode_length
 *  Description : Returns success or an error from decode length.
 *  Parameters  : in — ByteReader& in
 *  Returns     : Result<Length>
 */
Result<Length> decode_length(ByteReader& in);

/// Encode TLV: tag + definite length + content bytes.
/**
 *  Function    : encode_tlv
 *  Description : Performs encode tlv (declaration).
 *  Parameters  : out — ByteWriter& out; tag — Tag tag; content — Span<const std::uint8_t> content
 *  Returns     : void
 */
void encode_tlv(ByteWriter& out, Tag tag, Span<const std::uint8_t> content);

/// Decode next TLV header; on success reader is positioned at the start of content.
/// For definite length, content has exactly `length.value` bytes remaining for that value.
/// For indefinite, caller must parse until end-of-contents (00 00).
struct TlvHeader {
  Tag tag;
  Length length;
};
/**
 *  Function    : decode_tlv_header
 *  Description : Returns success or an error from decode tlv header.
 *  Parameters  : in — ByteReader& in
 *  Returns     : Result<TlvHeader>
 */
Result<TlvHeader> decode_tlv_header(ByteReader& in);

/// Require end-of-contents octets 0x00 0x00 (indefinite constructed values).
/**
 *  Function    : decode_end_of_contents
 *  Description : Returns success or an error from decode end of contents.
 *  Parameters  : in — ByteReader& in
 *  Returns     : Result<void>
 */
Result<void> decode_end_of_contents(ByteReader& in);

}  // namespace ber
}  // namespace asn1
