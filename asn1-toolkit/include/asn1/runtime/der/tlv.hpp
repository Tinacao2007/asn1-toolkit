/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/include/asn1/runtime/der/tlv.hpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   DER canonical length and ordering checks on BER TLV.
**
** Specification: ITU-T X.690 — Distinguished Encoding Rules (DER),
**                 canonical BER subset.
** Design Spec:   asn1-toolkit/docs/ARCHITECTURE.md
**                 asn1-toolkit/README.md
***************************************************************************/
#pragma once

#include <asn1/runtime/ber/tlv.hpp>

namespace asn1 {
namespace der {

/// Decode length under DER rules: definite only, shortest form, no leading 0x00
/// in long-form length octets.
/**
 *  Function    : decode_length
 *  Description : Returns success or an error from decode length.
 *  Parameters  : in — ByteReader& in
 *  Returns     : Result<ber::Length>
 */
Result<ber::Length> decode_length(ByteReader& in);

/**
 *  Function    : decode_tlv_header
 *  Description : Returns success or an error from decode tlv header.
 *  Parameters  : in — ByteReader& in
 *  Returns     : Result<ber::TlvHeader>
 */
Result<ber::TlvHeader> decode_tlv_header(ByteReader& in);

/// Encode tag using BER rules (already DER-canonical in our encoder).
using ber::encode_tag;

/// Encode definite length (shortest form). Same as BER encoder.
using ber::encode_length;

using ber::encode_tlv;

}  // namespace der
}  // namespace asn1
