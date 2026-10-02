#include <asn1/ir/type.hpp>

#include <algorithm>
#include <climits>
#include <stdexcept>

namespace asn1 {
namespace ir {
namespace {

bool all_digits(const std::string& s) {
  return !s.empty() && std::all_of(s.begin(), s.end(), [](char c) {
    return c >= '0' && c <= '9';
  });
}

std::optional<std::int64_t> try_parse_i64(bool negative, const std::string& digits) {
  if (digits.size() > 18) {
    return std::nullopt;
  }
  std::uint64_t mag = 0;
  for (char c : digits) {
    mag = mag * 10u + static_cast<std::uint64_t>(c - '0');
  }
  if (!negative) {
    if (mag > static_cast<std::uint64_t>(INT64_MAX)) {
      return std::nullopt;
    }
    return static_cast<std::int64_t>(mag);
  }
  // magnitude for INT64_MIN is 9223372036854775808
  if (mag > static_cast<std::uint64_t>(INT64_MAX) + 1u) {
    return std::nullopt;
  }
  if (mag == static_cast<std::uint64_t>(INT64_MAX) + 1u) {
    return INT64_MIN;
  }
  return -static_cast<std::int64_t>(mag);
}

}  // namespace

BigInt BigInt::from_decimal(std::string text, bool negative) {
  BigInt b;
  b.negative = negative && text != "0";
  // strip leading zeros
  std::size_t i = 0;
  while (i + 1 < text.size() && text[i] == '0') {
    ++i;
  }
  b.digits = text.substr(i);
  if (b.digits.empty()) {
    b.digits = "0";
    b.negative = false;
  }
  if (!all_digits(b.digits)) {
    throw std::invalid_argument("BigInt::from_decimal: non-digit content");
  }
  b.as_i64 = try_parse_i64(b.negative, b.digits);
  return b;
}

BigInt BigInt::from_i64(std::int64_t v) {
  BigInt b;
  b.as_i64 = v;
  if (v < 0) {
    b.negative = true;
    if (v == INT64_MIN) {
      b.digits = "9223372036854775808";
      return b;
    }
    v = -v;
  }
  b.digits = std::to_string(v);
  return b;
}

std::string BigInt::to_string() const {
  return (negative ? "-" : "") + digits;
}

bool BigInt::operator==(const BigInt& o) const {
  return negative == o.negative && digits == o.digits;
}

int BigInt::compare(const BigInt& o) const {
  if (negative != o.negative) {
    return negative ? -1 : 1;
  }
  const int mag = [&]() {
    if (digits.size() != o.digits.size()) {
      return digits.size() < o.digits.size() ? -1 : 1;
    }
    if (digits == o.digits) {
      return 0;
    }
    return digits < o.digits ? -1 : 1;
  }();
  return negative ? -mag : mag;
}

bool IntegerInterval::contains(const BigInt& v) const {
  if (lower && v < *lower) {
    return false;
  }
  if (upper && v > *upper) {
    return false;
  }
  return true;
}

bool IntegerInterval::overlaps_or_adjacent(const IntegerInterval& o) const {
  // Empty if this.upper < o.lower - 1 or o.upper < this.lower - 1
  auto strictly_less_with_gap = [](const std::optional<BigInt>& a,
                                   const std::optional<BigInt>& b) {
    // a < b - 1  (gap between a and b)
    if (!a || !b || !a->as_i64 || !b->as_i64) {
      // Fall back: treat only when both finite and a < b (merge overlapping only,
      // adjacency without i64 is conservative merge if a < b is false for gap).
      if (!a) {
        return false;  // a is +inf? lower missing means -inf for lower side...
      }
      if (!b) {
        return false;
      }
      return *a < *b;
    }
    // Check a <= b - 1 i.e. a + 1 <= b for adjacency as overlap for merging.
    // We want: intervals [L1,U1] [L2,U2] merge if U1+1 >= L2 (assuming L1<=L2).
    return false;  // unused
  };
  (void)strictly_less_with_gap;

  // Normalize order: assume we compare generally.
  // Disjoint if upper < other.lower - 1 OR other.upper < lower - 1
  auto less_with_gap = [](const std::optional<BigInt>& hi, const std::optional<BigInt>& lo) {
    // hi < lo - 1 ?
    if (!hi) {
      return false;  // +inf is not < anything
    }
    if (!lo) {
      return false;  // -inf: hi < -inf - 1 is false
    }
    if (hi->as_i64 && lo->as_i64) {
      // hi + 1 < lo  => gap
      if (*hi->as_i64 == INT64_MAX) {
        return false;
      }
      return (*hi->as_i64 + 1) < *lo->as_i64;
    }
    // Without i64: consider adjacent only if equal or overlapping via <=
    return *hi < *lo;
  };

  if (less_with_gap(upper, o.lower)) {
    return false;
  }
  if (less_with_gap(o.upper, lower)) {
    return false;
  }
  return true;
}

IntegerInterval IntegerInterval::merged_with(const IntegerInterval& o) const {
  IntegerInterval r;
  if (!lower) {
    r.lower = std::nullopt;
  } else if (!o.lower) {
    r.lower = std::nullopt;
  } else {
    r.lower = (*lower <= *o.lower) ? lower : o.lower;
  }
  if (!upper) {
    r.upper = std::nullopt;
  } else if (!o.upper) {
    r.upper = std::nullopt;
  } else {
    r.upper = (*upper >= *o.upper) ? upper : o.upper;
  }
  return r;
}

std::optional<IntegerInterval> IntegerInterval::intersected_with(
    const IntegerInterval& o) const {
  IntegerInterval r;
  if (!lower) {
    r.lower = o.lower;
  } else if (!o.lower) {
    r.lower = lower;
  } else {
    r.lower = (*lower >= *o.lower) ? lower : o.lower;
  }
  if (!upper) {
    r.upper = o.upper;
  } else if (!o.upper) {
    r.upper = upper;
  } else {
    r.upper = (*upper <= *o.upper) ? upper : o.upper;
  }
  if (r.lower && r.upper && *r.lower > *r.upper) {
    return std::nullopt;
  }
  return r;
}

void ConstraintDesc::recompute_envelope() {
  lower.reset();
  upper.reset();
  if (root_ranges.empty()) {
    return;
  }
  lower = root_ranges.front().lower;
  upper = root_ranges.front().upper;
  for (std::size_t i = 1; i < root_ranges.size(); ++i) {
    const auto& iv = root_ranges[i];
    if (!iv.lower) {
      lower = std::nullopt;
    } else if (lower && *iv.lower < *lower) {
      lower = iv.lower;
    }
    if (!iv.upper) {
      upper = std::nullopt;
    } else if (upper && *iv.upper > *upper) {
      upper = iv.upper;
    }
  }
}

PerBoundKind ConstraintDesc::per_kind() const {
  if (empty) {
    return PerBoundKind::Constrained;  // empty set is degenerate constrained
  }
  if (!lower && !upper) {
    return PerBoundKind::Unconstrained;
  }
  if (lower && upper) {
    return PerBoundKind::Constrained;
  }
  if (lower && !upper) {
    return PerBoundKind::SemiConstrained;
  }
  // upper only (MAX-bounded from above without lower) - treat as unconstrained
  // for PER whole-number purposes (rare in practice).
  return PerBoundKind::Unconstrained;
}

std::optional<std::uint64_t> ConstraintDesc::constrained_span() const {
  if (per_kind() != PerBoundKind::Constrained || !lower || !upper) {
    return std::nullopt;
  }
  if (!lower->as_i64 || !upper->as_i64) {
    return std::nullopt;
  }
  const std::int64_t lo = *lower->as_i64;
  const std::int64_t hi = *upper->as_i64;
  if (lo > hi) {
    return std::nullopt;
  }
  return static_cast<std::uint64_t>(hi - lo);
}

TypeId TypeArena::add(Type type) {
  const TypeId id = static_cast<TypeId>(types_.size());
  types_.push_back(std::move(type));
  return id;
}

ValueId TypeArena::add_value(Value value) {
  const ValueId id = static_cast<ValueId>(values_.size());
  values_.push_back(std::move(value));
  return id;
}

Type& TypeArena::get(TypeId id) { return types_.at(id); }
const Type& TypeArena::get(TypeId id) const { return types_.at(id); }
Value& TypeArena::get_value(ValueId id) { return values_.at(id); }
const Value& TypeArena::get_value(ValueId id) const { return values_.at(id); }

}  // namespace ir
}  // namespace asn1
