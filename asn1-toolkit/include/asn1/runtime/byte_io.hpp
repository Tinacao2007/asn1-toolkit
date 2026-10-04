/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/include/asn1/runtime/byte_io.hpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   ByteReader/ByteWriter for TLV and octet-aligned rules.
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

class ByteWriter {
 public:
  /**
   *  Function    : put
   *  Description : Performs put (declaration).
   *  Parameters  : b — std::uint8_t b
   *  Returns     : void
   */
  void put(std::uint8_t b);
  /**
   *  Function    : write
   *  Description : Performs write (declaration).
   *  Parameters  : bytes — Span<const std::uint8_t> bytes
   *  Returns     : void
   */
  void write(Span<const std::uint8_t> bytes);
  /**
   *  Function    : write
   *  Description : Performs write (declaration).
   *  Parameters  : data — const std::uint8_t* data; n — std::size_t n
   *  Returns     : void
   */
  void write(const std::uint8_t* data, std::size_t n);

  /**
   *  Function    : buffer
   *  Description : Computes buffer from (none).
   *  Parameters  : none
   *  Returns     : std::vector<std::uint8_t>&
   */
  std::vector<std::uint8_t>& buffer() noexcept { return buf_; }
  /**
   *  Function    : buffer
   *  Description : Computes buffer from (none).
   *  Parameters  : none
   *  Returns     : const std::vector<std::uint8_t>&
   */
  const std::vector<std::uint8_t>& buffer() const noexcept { return buf_; }
  /**
   *  Function    : take
   *  Description : Computes take from (none).
   *  Parameters  : none
   *  Returns     : std::vector<std::uint8_t>
   */
  std::vector<std::uint8_t> take();
  /**
   *  Function    : clear
   *  Description : Performs clear (definition).
   *  Parameters  : buf_.clear( — ) { buf_.clear(
   *  Returns     : void clear() { buf_.
   */
  void clear() { buf_.clear(); }
  /**
   *  Function    : size
   *  Description : Computes size from (buf_.size().
   *  Parameters  : buf_.size( — ) const noexcept { return buf_.size(
   *  Returns     : std::size_t size() const noexcept { return buf_.
   */
  std::size_t size() const noexcept { return buf_.size(); }

 private:
  std::vector<std::uint8_t> buf_;
};

class ByteReader {
 public:
  /**
   *  Function    : ByteReader
   *  Description : Computes ByteReader from (data).
   *  Parameters  : data — Span<const std::uint8_t> data
   *  Returns     : explicit
   */
  explicit ByteReader(Span<const std::uint8_t> data);

  /**
   *  Function    : get
   *  Description : Returns success or an error from get.
   *  Parameters  : none
   *  Returns     : Result<std::uint8_t>
   */
  Result<std::uint8_t> get();
  /**
   *  Function    : read
   *  Description : Returns success or an error from read.
   *  Parameters  : n — std::size_t n
   *  Returns     : Result<Span<const std::uint8_t>>
   */
  Result<Span<const std::uint8_t>> read(std::size_t n);
  /**
   *  Function    : expect
   *  Description : Returns success or an error from expect.
   *  Parameters  : b — std::uint8_t b
   *  Returns     : Result<void>
   */
  Result<void> expect(std::uint8_t b);

  /**
   *  Function    : offset
   *  Description : Computes offset from (none).
   *  Parameters  : none
   *  Returns     : std::size_t
   */
  std::size_t offset() const noexcept { return pos_; }
  std::size_t remaining() const noexcept;
  /**
   *  Function    : remaining
   *  Description : Returns a boolean result from remaining(.
   *  Parameters  : remaining( — ) const noexcept { return remaining(
   *  Returns     : bool eof() const noexcept { return
   */
  bool eof() const noexcept { return remaining() == 0; }

  /// View of unread bytes (does not consume).
  Span<const std::uint8_t> remaining_span() const noexcept;

  /// Peek without consuming. Fails if not enough bytes.
  /**
   *  Function    : peek
   *  Description : Returns success or an error from peek.
   *  Parameters  : ahead — std::size_t ahead
   *  Returns     : Result<std::uint8_t>
   */
  Result<std::uint8_t> peek(std::size_t ahead = 0) const;

 private:
  Span<const std::uint8_t> data_;
  std::size_t pos_ = 0;
};

/**
 *  Function    : make_error
 *  Description : Computes make error from (code, offset, message).
 *  Parameters  : code — Error::Code code; offset — std::size_t offset; message — std::string message
 *  Returns     : inline Error
 */
inline Error make_error(Error::Code code, std::size_t offset, std::string message) {
  Error e;
  e.code = code;
  e.offset = offset;
  /**
   *  Function    : move
   *  Description : Computes move from (message).
   *  Parameters  : message — message
   *  Returns     : e.message = std::
   */
  e.message = std::move(message);
  return e;
}

}  // namespace asn1
