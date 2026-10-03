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

  static Result success(T value) { return Result(std::move(value)); }
  static Result failure(Error error) { return Result(std::move(error)); }

  bool ok() const noexcept { return std::holds_alternative<T>(storage_); }
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

  static Result success() { return Result(); }
  static Result failure(Error error) { return Result(std::move(error)); }

  bool ok() const noexcept { return ok_; }
  explicit operator bool() const noexcept { return ok(); }

  const Error& error() const& { return error_; }
  Error& error() & { return error_; }

 private:
  bool ok_;
  Error error_{};
};

}  // namespace asn1
