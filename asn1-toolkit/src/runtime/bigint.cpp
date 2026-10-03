#include <asn1/runtime/bigint.hpp>
#include <asn1/runtime/byte_io.hpp>

#include <algorithm>
#include <cctype>
#include <limits>

namespace asn1 {
namespace {

Error bad_arg(std::string message) {
  return make_error(Error::Code::InvalidArgument, 0, std::move(message));
}

}  // namespace

void BigInteger::normalize() {
  while (!limbs_.empty() && limbs_.back() == 0) {
    limbs_.pop_back();
  }
  if (limbs_.empty()) {
    negative_ = false;
  }
}

BigInteger BigInteger::zero() { return BigInteger{}; }

BigInteger BigInteger::from_i64(std::int64_t value) {
  if (value == 0) {
    return zero();
  }
  BigInteger b;
  if (value < 0) {
    b.negative_ = true;
    const std::uint64_t mag =
        value == std::numeric_limits<std::int64_t>::min()
            ? (static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) + 1ull)
            : static_cast<std::uint64_t>(-value);
    b.limbs_.push_back(static_cast<std::uint32_t>(mag & 0xFFFFFFFFu));
    const std::uint32_t hi = static_cast<std::uint32_t>(mag >> 32);
    if (hi != 0) {
      b.limbs_.push_back(hi);
    }
  } else {
    const std::uint64_t mag = static_cast<std::uint64_t>(value);
    b.limbs_.push_back(static_cast<std::uint32_t>(mag & 0xFFFFFFFFu));
    const std::uint32_t hi = static_cast<std::uint32_t>(mag >> 32);
    if (hi != 0) {
      b.limbs_.push_back(hi);
    }
  }
  return b;
}

BigInteger BigInteger::from_u64(std::uint64_t value) {
  if (value == 0) {
    return zero();
  }
  BigInteger b;
  b.limbs_.push_back(static_cast<std::uint32_t>(value & 0xFFFFFFFFu));
  const std::uint32_t hi = static_cast<std::uint32_t>(value >> 32);
  if (hi != 0) {
    b.limbs_.push_back(hi);
  }
  return b;
}

void BigInteger::mul_add_u32(std::uint32_t mul, std::uint32_t add) {
  std::uint64_t carry = add;
  for (std::size_t i = 0; i < limbs_.size(); ++i) {
    const std::uint64_t prod =
        static_cast<std::uint64_t>(limbs_[i]) * static_cast<std::uint64_t>(mul) + carry;
    limbs_[i] = static_cast<std::uint32_t>(prod & 0xFFFFFFFFu);
    carry = prod >> 32;
  }
  if (carry != 0) {
    limbs_.push_back(static_cast<std::uint32_t>(carry));
  }
}

std::uint32_t BigInteger::div_mod_u32(std::uint32_t divisor) {
  std::uint64_t rem = 0;
  for (std::size_t i = limbs_.size(); i-- > 0;) {
    const std::uint64_t cur = (rem << 32) | limbs_[i];
    limbs_[i] = static_cast<std::uint32_t>(cur / divisor);
    rem = cur % divisor;
  }
  normalize();
  return static_cast<std::uint32_t>(rem);
}

Result<BigInteger> BigInteger::from_decimal(std::string_view text) {
  if (text.empty()) {
    return bad_arg("empty INTEGER decimal");
  }
  std::size_t i = 0;
  bool neg = false;
  if (text[0] == '-') {
    neg = true;
    i = 1;
  } else if (text[0] == '+') {
    i = 1;
  }
  if (i >= text.size()) {
    return bad_arg("invalid INTEGER decimal");
  }
  while (i < text.size() && text[i] == '0') {
    ++i;
  }
  if (i == text.size()) {
    return zero();
  }
  BigInteger b;
  for (; i < text.size(); ++i) {
    if (!std::isdigit(static_cast<unsigned char>(text[i]))) {
      return bad_arg("invalid INTEGER decimal digit");
    }
    b.mul_add_u32(10, static_cast<std::uint32_t>(text[i] - '0'));
  }
  b.negative_ = neg && !b.is_zero();
  return b;
}

std::vector<std::uint8_t> BigInteger::magnitude_be() const {
  if (limbs_.empty()) {
    return {0};
  }
  std::vector<std::uint8_t> out(limbs_.size() * 4);
  for (std::size_t i = 0; i < limbs_.size(); ++i) {
    const std::uint32_t w = limbs_[limbs_.size() - 1 - i];
    const std::size_t o = i * 4;
    out[o] = static_cast<std::uint8_t>((w >> 24) & 0xFFu);
    out[o + 1] = static_cast<std::uint8_t>((w >> 16) & 0xFFu);
    out[o + 2] = static_cast<std::uint8_t>((w >> 8) & 0xFFu);
    out[o + 3] = static_cast<std::uint8_t>(w & 0xFFu);
  }
  std::size_t start = 0;
  while (start + 1 < out.size() && out[start] == 0) {
    ++start;
  }
  return std::vector<std::uint8_t>(out.begin() + static_cast<std::ptrdiff_t>(start),
                                   out.end());
}

Result<BigInteger> BigInteger::from_unsigned_bytes(Span<const std::uint8_t> bytes) {
  if (bytes.empty()) {
    return bad_arg("unsigned INTEGER content empty");
  }
  std::size_t start = 0;
  while (start + 1 < bytes.size() && bytes[start] == 0) {
    ++start;
  }
  BigInteger b;
  for (std::size_t i = start; i < bytes.size(); ++i) {
    b.mul_add_u32(256, bytes[i]);
  }
  return b;
}

Result<BigInteger> BigInteger::from_twos_complement(Span<const std::uint8_t> bytes) {
  if (bytes.empty()) {
    return bad_arg("INTEGER content must not be empty");
  }
  if ((bytes[0] & 0x80u) == 0) {
    return from_unsigned_bytes(bytes);
  }
  // Negative: magnitude = ~bytes + 1.
  std::vector<std::uint8_t> mag(bytes.begin(), bytes.end());
  for (auto& x : mag) {
    x = static_cast<std::uint8_t>(~x);
  }
  for (std::size_t i = mag.size(); i-- > 0;) {
    if (++mag[i] != 0) {
      break;
    }
  }
  auto abs = from_unsigned_bytes(Span<const std::uint8_t>(mag.data(), mag.size()));
  if (!abs.ok()) {
    return abs.error();
  }
  abs.value().negative_ = !abs.value().is_zero();
  return abs;
}

std::vector<std::uint8_t> BigInteger::to_twos_complement() const {
  if (is_zero()) {
    return {0};
  }
  if (!negative_) {
    auto mag = magnitude_be();
    if ((mag[0] & 0x80u) != 0) {
      mag.insert(mag.begin(), 0x00);
    }
    return mag;
  }
  auto mag = magnitude_be();
  mag.insert(mag.begin(), 0x00);
  for (auto& x : mag) {
    x = static_cast<std::uint8_t>(~x);
  }
  for (std::size_t i = mag.size(); i-- > 0;) {
    if (++mag[i] != 0) {
      break;
    }
  }
  std::size_t start = 0;
  while (start + 1 < mag.size() && mag[start] == 0xFFu && (mag[start + 1] & 0x80u) != 0) {
    ++start;
  }
  return std::vector<std::uint8_t>(mag.begin() + static_cast<std::ptrdiff_t>(start), mag.end());
}

Result<std::vector<std::uint8_t>> BigInteger::to_unsigned_bytes() const {
  if (negative_) {
    return bad_arg("unsigned INTEGER encoding of negative value");
  }
  return magnitude_be();
}

bool BigInteger::is_zero() const noexcept { return limbs_.empty(); }

std::optional<std::int64_t> BigInteger::as_i64() const {
  if (limbs_.empty()) {
    return 0;
  }
  if (limbs_.size() > 2) {
    return std::nullopt;
  }
  std::uint64_t mag = limbs_[0];
  if (limbs_.size() == 2) {
    mag |= static_cast<std::uint64_t>(limbs_[1]) << 32;
  }
  if (!negative_) {
    if (mag > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
      return std::nullopt;
    }
    return static_cast<std::int64_t>(mag);
  }
  const std::uint64_t min_mag =
      static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) + 1ull;
  if (mag > min_mag) {
    return std::nullopt;
  }
  if (mag == min_mag) {
    return std::numeric_limits<std::int64_t>::min();
  }
  return -static_cast<std::int64_t>(mag);
}

std::optional<std::uint64_t> BigInteger::as_u64() const {
  if (negative_) {
    return std::nullopt;
  }
  if (limbs_.empty()) {
    return 0;
  }
  if (limbs_.size() > 2) {
    return std::nullopt;
  }
  std::uint64_t mag = limbs_[0];
  if (limbs_.size() == 2) {
    mag |= static_cast<std::uint64_t>(limbs_[1]) << 32;
  }
  return mag;
}

std::string BigInteger::to_decimal() const {
  if (is_zero()) {
    return "0";
  }
  BigInteger tmp = *this;
  tmp.negative_ = false;
  std::string digits;
  while (!tmp.is_zero()) {
    const std::uint32_t d = tmp.div_mod_u32(10);
    digits.push_back(static_cast<char>('0' + d));
  }
  if (negative_) {
    digits.push_back('-');
  }
  std::reverse(digits.begin(), digits.end());
  return digits;
}

int BigInteger::compare_mag(const BigInteger& a, const BigInteger& b) {
  if (a.limbs_.size() != b.limbs_.size()) {
    return a.limbs_.size() < b.limbs_.size() ? -1 : 1;
  }
  for (std::size_t i = a.limbs_.size(); i-- > 0;) {
    if (a.limbs_[i] != b.limbs_[i]) {
      return a.limbs_[i] < b.limbs_[i] ? -1 : 1;
    }
  }
  return 0;
}

int BigInteger::compare(const BigInteger& other) const {
  if (is_zero() && other.is_zero()) {
    return 0;
  }
  if (negative_ != other.negative_) {
    return negative_ ? -1 : 1;
  }
  const int mag = compare_mag(*this, other);
  return negative_ ? -mag : mag;
}

}  // namespace asn1
