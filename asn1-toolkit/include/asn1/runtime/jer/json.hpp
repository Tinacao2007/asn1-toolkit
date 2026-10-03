#pragma once

#include <asn1/common/result.hpp>
#include <asn1/runtime/bigint.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace asn1 {
namespace jer {

enum class ValueKind { Null, Bool, Number, String, Array, Object };

/// Minimal JSON value tree for JER (X.697). Numbers are host int64, BigInteger, or double.
struct Value {
  ValueKind kind = ValueKind::Null;
  bool bool_value = false;
  std::int64_t number = 0;
  double real = 0.0;
  bool number_is_real = false;     // meaningful when kind == Number
  bool number_is_bigint = false;   // decimal digits in string_value
  std::string string_value;
  std::vector<Value> array;
  std::vector<std::pair<std::string, Value>> object;

  static Value null_value() { return Value{}; }
  static Value boolean(bool v) {
    Value x;
    x.kind = ValueKind::Bool;
    x.bool_value = v;
    return x;
  }
  static Value integer(std::int64_t v) {
    Value x;
    x.kind = ValueKind::Number;
    x.number = v;
    x.number_is_real = false;
    x.number_is_bigint = false;
    return x;
  }
  static Value big_integer(BigInteger v) {
    Value x;
    x.kind = ValueKind::Number;
    x.number_is_real = false;
    x.number_is_bigint = true;
    x.string_value = v.to_decimal();
    if (auto narrow = v.as_i64()) {
      x.number = *narrow;
      x.number_is_bigint = false;
    }
    return x;
  }
  static Value real_number(double v) {
    Value x;
    x.kind = ValueKind::Number;
    x.real = v;
    x.number_is_real = true;
    return x;
  }
  static Value string(std::string v) {
    Value x;
    x.kind = ValueKind::String;
    x.string_value = std::move(v);
    return x;
  }
  static Value make_array(std::vector<Value> items = {}) {
    Value x;
    x.kind = ValueKind::Array;
    x.array = std::move(items);
    return x;
  }
  static Value make_object(std::vector<std::pair<std::string, Value>> members = {}) {
    Value x;
    x.kind = ValueKind::Object;
    x.object = std::move(members);
    return x;
  }
};

/// Compact JSON (no insignificant whitespace).
void write_value(std::string& out, const Value& v);
std::string to_string(const Value& v);

Result<Value> parse_document(const std::string& json);

}  // namespace jer
}  // namespace asn1
