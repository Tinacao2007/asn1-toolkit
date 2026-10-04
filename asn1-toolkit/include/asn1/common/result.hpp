/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/include/asn1/common/result.hpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   Result<T> and Error for non-throwing compiler/codec control flow.
**
** Specification: No external protocol; C++ infrastructure shared by
**                 compiler and runtime.
** Design Spec:   asn1-toolkit/docs/ARCHITECTURE.md
**                 asn1-toolkit/README.md
***************************************************************************/
#pragma once

#include <cstddef>
#include <string>
#include <utility>
#include <variant>

namespace asn1 {

/// Lightweight error payload used by the codec runtime and (later) helpers.
struct Error {
  enum class Code {
    Ok = 0,
    Truncated,
    TagMismatch,
    LengthOverflow,
    ConstraintViolation,
    NonCanonical,
    InvalidArgument,
    Unsupported,
  };

  Code code = Code::InvalidArgument;
  std::size_t offset = 0;  // byte or bit offset when known
  std::string message;
};

/// C++17 stand-in for std::expected<T, Error>.
/// Decode / constraint failures return Error; they do not throw.
template <typename T>
class Result {
 public:
  Result(T value) : storage_(std::move(value)) {}
  Result(Error error) : storage_(std::move(error)) {}

  /**
   *  Function    : move
   *  Description : Returns success or an error from move.
   *  Parameters  : Result(std::move(value) — T value) { return Result(std::move(value)
   *  Returns     : static Result success(T value) { return Result(std::
   */
  static Result success(T value) { return Result(std::move(value)); }
  /**
   *  Function    : move
   *  Description : Returns success or an error from move.
   *  Parameters  : Result(std::move(error) — Error error) { return Result(std::move(error)
   *  Returns     : static Result failure(Error error) { return Result(std::
   */
  static Result failure(Error error) { return Result(std::move(error)); }

  /**
   *  Function    : holds_alternative<T>
   *  Description : Returns a boolean result from std::holds_alternative<T>(storage_.
   *  Parameters  : std::holds_alternative<T>(storage_ — ) const noexcept { return std::holds_alternative<T>(storage_
   *  Returns     : bool ok() const noexcept { return std::
   */
  bool ok() const noexcept { return std::holds_alternative<T>(storage_); }
  /**
   *  Function    : ok
   *  Description : Performs ok (definition).
   *  Parameters  : ok( — ) const noexcept { return ok(
   *  Returns     : —
   */
  explicit operator bool() const noexcept { return ok(); }

  /// Precondition: ok(). Callers must check ok() first (codec paths never throw).
  T& value() & { return std::get<T>(storage_); }
  const T& value() const& { return std::get<T>(storage_); }
  T&& value() && { return std::get<T>(std::move(storage_)); }

  /// Precondition: !ok().
  Error& error() & { return std::get<Error>(storage_); }
  const Error& error() const& { return std::get<Error>(storage_); }

 private:
  std::variant<T, Error> storage_;
};

/// Specialization for operations that only report success or Error.
template <>
class Result<void> {
 public:
  Result() : ok_(true) {}
  Result(Error error) : ok_(false), error_(std::move(error)) {}

  /**
   *  Function    : Result
   *  Description : Returns success or an error from Result.
   *  Parameters  : Result( — ) { return Result(
   *  Returns     : static Result success() { return
   */
  static Result success() { return Result(); }
  /**
   *  Function    : move
   *  Description : Returns success or an error from move.
   *  Parameters  : Result(std::move(error) — Error error) { return Result(std::move(error)
   *  Returns     : static Result failure(Error error) { return Result(std::
   */
  static Result failure(Error error) { return Result(std::move(error)); }

  /**
   *  Function    : ok
   *  Description : Returns a boolean result from none.
   *  Parameters  : none
   *  Returns     : bool
   */
  bool ok() const noexcept { return ok_; }
  /**
   *  Function    : ok
   *  Description : Performs ok (definition).
   *  Parameters  : ok( — ) const noexcept { return ok(
   *  Returns     : —
   */
  explicit operator bool() const noexcept { return ok(); }

  const Error& error() const& { return error_; }
  Error& error() & { return error_; }

 private:
  bool ok_;
  Error error_{};
};

}  // namespace asn1
