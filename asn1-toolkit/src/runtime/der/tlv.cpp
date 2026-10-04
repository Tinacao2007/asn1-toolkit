/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/src/runtime/der/tlv.cpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   DER TLV canonicalization utilities.
**
** Specification: ITU-T X.690 — Distinguished Encoding Rules (DER),
**                 canonical BER subset.
** Design Spec:   asn1-toolkit/docs/ARCHITECTURE.md
**                 asn1-toolkit/README.md
***************************************************************************/
#include <asn1/runtime/der/tlv.hpp>

#include <climits>
#include <cstddef>

namespace asn1 {
namespace der {

/**
 *  Function    : decode_length
 *  Description : Returns success or an error from decode length.
 *  Parameters  : in — ByteReader& in
 *  Returns     : Result<ber::Length>
 */
Result<ber::Length> decode_length(ByteReader& in) {
  auto first_r = in.get();
  if (!first_r) {
    return first_r.error();
  }
  const std::uint8_t first = first_r.value();
  if (first == 0x80u) {
    return make_error(Error::Code::NonCanonical, in.offset() - 1,
                      "DER forbids indefinite length");
  }
  ber::Length len;
  if ((first & 0x80u) == 0) {
    len.value = first;
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
    const std::uint8_t b = b_r.value();
    if (i == 0 && b == 0x00u) {
      return make_error(Error::Code::NonCanonical, in.offset() - 1,
                        "DER length long form must not have leading 0x00");
    }
    if (value > (SIZE_MAX >> 8)) {
      return make_error(Error::Code::LengthOverflow, in.offset() - 1,
                        "length too large");
    }
    value = (value << 8) | b;
  }
  // Short form is required whenever the value fits in 7 bits.
  if (value <= 0x7Fu) {
    return make_error(Error::Code::NonCanonical, in.offset() - nbytes - 1,
                      "DER requires short-form length for values <= 127");
  }
  len.value = value;
  return len;
}

/**
 *  Function    : decode_tlv_header
 *  Description : Returns success or an error from decode tlv header.
 *  Parameters  : in — ByteReader& in
 *  Returns     : Result<ber::TlvHeader>
 */
Result<ber::TlvHeader> decode_tlv_header(ByteReader& in) {
  auto tag_r = ber::decode_tag(in);
  if (!tag_r) {
    return tag_r.error();
  }
  auto len_r = decode_length(in);
  if (!len_r) {
    return len_r.error();
  }
  return ber::TlvHeader{tag_r.value(), len_r.value()};
}

}  // namespace der
}  // namespace asn1
