/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/include/asn1/runtime/bit_io.hpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   BitReader/BitWriter used by PER-family codecs.
**
** Specification: Internal I/O and safety limits serving ITU encoding
**                 implementations in this codebase.
** Design Spec:   asn1-toolkit/docs/ARCHITECTURE.md
**                 asn1-toolkit/README.md
***************************************************************************/
#pragma once

#include <asn1/common/result.hpp>
#include <asn1/common/span.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace asn1 {

/// MSB-first bit stream writer. Final incomplete octet is zero-padded on take().
class BitWriter {
 public:
  /**
   *  Function    : put_bit
   *  Description : Performs put bit (declaration).
   *  Parameters  : bit — bool bit
   *  Returns     : void
   */
  void put_bit(bool bit);
  /// Write the low `nbits` of `value` as an MSB-first field (nbits <= 64).
  /**
   *  Function    : put_bits
   *  Description : Performs put bits (declaration).
   *  Parameters  : value — std::uint64_t value; nbits — std::size_t nbits
   *  Returns     : void
   */
  void put_bits(std::uint64_t value, std::size_t nbits);
  /// Write `nbits` from `bytes` (MSB of bytes[0] first).
  /**
   *  Function    : put_bits
   *  Description : Performs put bits (declaration).
   *  Parameters  : bytes — Span<const std::uint8_t> bytes; nbits — std::size_t nbits
   *  Returns     : void
   */
  void put_bits(Span<const std::uint8_t> bytes, std::size_t nbits);
  /**
   *  Function    : put_octet
   *  Description : Performs put octet (declaration).
   *  Parameters  : octet — std::uint8_t octet
   *  Returns     : void
   */
  void put_octet(std::uint8_t octet);
  /**
   *  Function    : put_octets
   *  Description : Performs put octets (declaration).
   *  Parameters  : bytes — Span<const std::uint8_t> bytes
   *  Returns     : void
   */
  void put_octets(Span<const std::uint8_t> bytes);

  /// Pad with zero bits to the next octet boundary (no-op if already aligned).
  /**
   *  Function    : align_to_octet
   *  Description : Performs align to octet (declaration).
   *  Parameters  : none
   *  Returns     : void
   */
  void align_to_octet();

  /**
   *  Function    : bit_size
   *  Description : Computes bit size from (none).
   *  Parameters  : none
   *  Returns     : std::size_t
   */
  std::size_t bit_size() const noexcept { return bit_count_; }
  /**
   *  Function    : return
   *  Description : Computes return from (7).
   *  Parameters  : 7 — ) const noexcept { return (bit_count_ + 7
   *  Returns     : std::size_t byte_size() const noexcept {
   */
  std::size_t byte_size() const noexcept { return (bit_count_ + 7) / 8; }
  /**
   *  Function    : return
   *  Description : Returns a boolean result from 8.
   *  Parameters  : 8 — ) const noexcept { return (bit_count_ % 8
   *  Returns     : bool octet_aligned() const noexcept {
   */
  bool octet_aligned() const noexcept { return (bit_count_ % 8) == 0; }

  /// Returns bytes with trailing zero-bit padding in the last octet if needed.
  /**
   *  Function    : take
   *  Description : Computes take from (none).
   *  Parameters  : none
   *  Returns     : std::vector<std::uint8_t>
   */
  std::vector<std::uint8_t> take();
  /**
   *  Function    : buffer
   *  Description : Computes buffer from (none).
   *  Parameters  : none
   *  Returns     : const std::vector<std::uint8_t>&
   */
  const std::vector<std::uint8_t>& buffer() const noexcept { return buf_; }
  /**
   *  Function    : clear
   *  Description : Performs clear (declaration).
   *  Parameters  : none
   *  Returns     : void
   */
  void clear();

 private:
  /**
   *  Function    : ensure_byte
   *  Description : Performs ensure byte (declaration).
   *  Parameters  : none
   *  Returns     : void
   */
  void ensure_byte();

  std::vector<std::uint8_t> buf_;
  std::size_t bit_count_ = 0;  // total bits written
};

/// MSB-first bit stream reader over a byte buffer.
class BitReader {
 public:
  /**
   *  Function    : BitReader
   *  Description : Computes BitReader from (data).
   *  Parameters  : data — Span<const std::uint8_t> data
   *  Returns     : explicit
   */
  explicit BitReader(Span<const std::uint8_t> data);

  /**
   *  Function    : get_bit
   *  Description : Returns a boolean result from none.
   *  Parameters  : none
   *  Returns     : Result<bool>
   */
  Result<bool> get_bit();
  /**
   *  Function    : get_bits
   *  Description : Returns success or an error from get bits.
   *  Parameters  : nbits — std::size_t nbits
   *  Returns     : Result<std::uint64_t>
   */
  Result<std::uint64_t> get_bits(std::size_t nbits);
  /**
   *  Function    : get_bits_as_bytes
   *  Description : Returns success or an error from get bits as bytes.
   *  Parameters  : nbits — std::size_t nbits
   *  Returns     : Result<std::vector<std::uint8_t>>
   */
  Result<std::vector<std::uint8_t>> get_bits_as_bytes(std::size_t nbits);
  /**
   *  Function    : get_octet
   *  Description : Returns success or an error from get octet.
   *  Parameters  : none
   *  Returns     : Result<std::uint8_t>
   */
  Result<std::uint8_t> get_octet();
  /**
   *  Function    : get_octets
   *  Description : Returns success or an error from get octets.
   *  Parameters  : n — std::size_t n
   *  Returns     : Result<std::vector<std::uint8_t>>
   */
  Result<std::vector<std::uint8_t>> get_octets(std::size_t n);

  /// Skip zero padding to next octet boundary. Does not require padding bits to be zero.
  /**
   *  Function    : align_to_octet
   *  Description : Returns success or an error from align to octet.
   *  Parameters  : none
   *  Returns     : Result<void>
   */
  Result<void> align_to_octet();

  /**
   *  Function    : bit_offset
   *  Description : Computes bit offset from (none).
   *  Parameters  : none
   *  Returns     : std::size_t
   */
  std::size_t bit_offset() const noexcept { return bit_pos_; }
  std::size_t bit_remaining() const noexcept;
  /**
   *  Function    : return
   *  Description : Returns a boolean result from 8.
   *  Parameters  : 8 — ) const noexcept { return (bit_pos_ % 8
   *  Returns     : bool octet_aligned() const noexcept {
   */
  bool octet_aligned() const noexcept { return (bit_pos_ % 8) == 0; }
  /**
   *  Function    : bit_remaining
   *  Description : Returns a boolean result from bit_remaining(.
   *  Parameters  : bit_remaining( — ) const noexcept { return bit_remaining(
   *  Returns     : bool eof() const noexcept { return
   */
  bool eof() const noexcept { return bit_remaining() == 0; }

 private:
  Span<const std::uint8_t> data_;
  std::size_t bit_pos_ = 0;
};

}  // namespace asn1
