#pragma once

#include <asn1/runtime/bit_string.hpp>
#include <asn1/runtime/jer/json.hpp>
#include <asn1/common/span.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace asn1 {
namespace jer {

using BitStringValue = asn1::BitStringValue;

// ---- Primitive types (BASIC-JER / X.697) ----

Value encode_boolean(bool value);
Result<bool> decode_boolean(const Value& v);

Value encode_null();
Result<void> decode_null(const Value& v);

Value encode_integer(std::int64_t value);
Value encode_integer(const BigInteger& value);
Result<std::int64_t> decode_integer(const Value& v);
Result<BigInteger> decode_big_integer(const Value& v);

Value encode_octet_string(Span<const std::uint8_t> value);
Result<std::vector<std::uint8_t>> decode_octet_string(const Value& v);

/// Unconstrained BIT STRING: `{"value":"<hex>","length":N}`.
/// Fixed-size BIT STRING: JSON hex string only (`fixed_size` bits).
Value encode_bit_string(Span<const std::uint8_t> bits, std::size_t bit_length,
                        bool fixed_size = false);
Result<BitStringValue> decode_bit_string(const Value& v, bool fixed_size = false);

Value encode_utf8_string(const std::string& value);
Result<std::string> decode_utf8_string(const Value& v);

Value encode_enumerated(const std::string& identifier);
Result<std::string> decode_enumerated(const Value& v);

Value encode_object_identifier(Span<const std::uint64_t> arcs);
Result<std::vector<std::uint64_t>> decode_object_identifier(const Value& v);

/// REAL: JSON number, or strings "INF" / "-INF" / "NaN".
Value encode_real(double value);
Result<double> decode_real(const Value& v);

// ---- Constructed scaffolding ----

Value make_sequence(std::vector<std::pair<std::string, Value>> members);
Result<const Value*> find_member(const Value& object, const std::string& name);

/// CHOICE: single-property object `{"altName": <encoding>}`.
Value encode_choice(const std::string& alternative, Value encoding);
Result<std::pair<std::string, const Value*>> decode_choice(const Value& v);

Value encode_sequence_of(std::vector<Value> items);
Result<const std::vector<Value>*> decode_sequence_of(const Value& v);

}  // namespace jer
}  // namespace asn1
