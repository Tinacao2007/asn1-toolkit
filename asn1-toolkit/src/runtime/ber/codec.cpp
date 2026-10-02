#include <asn1/runtime/ber/codec.hpp>

#include <climits>

namespace asn1 {
namespace ber {
namespace {

Result<void> require_tag(ByteReader& in, Tag expected, Tag got) {
  if (got != expected) {
    return make_error(Error::Code::TagMismatch, in.offset(), "BER tag mismatch");
  }
  return Result<void>::success();
}

Result<std::size_t> require_definite(ByteReader& in, const Length& len) {
  if (len.indefinite) {
    return make_error(Error::Code::Unsupported, in.offset(),
                      "indefinite length not valid for this primitive");
  }
  return len.value;
}

template <typename T>
Result<T> decode_primitive_tlv(ByteReader& in, Tag expected,
                               Result<T> (*decode_content)(ByteReader&, std::size_t)) {
  auto hdr = decode_tlv_header(in);
  if (!hdr) {
    return hdr.error();
  }
  if (auto r = require_tag(in, expected, hdr.value().tag); !r) {
    return r.error();
  }
  auto len = require_definite(in, hdr.value().length);
  if (!len) {
    return len.error();
  }
  if (in.remaining() < len.value()) {
    return make_error(Error::Code::Truncated, in.offset(), "truncated BER value");
  }
  return decode_content(in, len.value());
}

}  // namespace

void encode_boolean_content(ByteWriter& out, bool value) {
  out.put(value ? 0xFFu : 0x00u);
}

Result<bool> decode_boolean_content(ByteReader& in, std::size_t length) {
  if (length != 1) {
    return make_error(Error::Code::InvalidArgument, in.offset(),
                      "BOOLEAN content must be 1 octet");
  }
  auto b = in.get();
  if (!b) {
    return b.error();
  }
  return b.value() != 0;
}

void encode_integer_content(ByteWriter& out, std::int64_t value) {
  // Minimal two's-complement encoding.
  std::uint8_t bytes[8];
  int n = 0;
  std::uint64_t u = static_cast<std::uint64_t>(value);
  for (int i = 0; i < 8; ++i) {
    bytes[7 - i] = static_cast<std::uint8_t>(u & 0xFFu);
    u >>= 8;
  }
  // Find first needed byte (keep sign bit correct).
  int start = 0;
  if (value >= 0) {
    while (start < 7 && bytes[start] == 0x00 && (bytes[start + 1] & 0x80u) == 0) {
      ++start;
    }
  } else {
    while (start < 7 && bytes[start] == 0xFFu && (bytes[start + 1] & 0x80u) != 0) {
      ++start;
    }
  }
  out.write(bytes + start, static_cast<std::size_t>(8 - start));
}

Result<std::int64_t> decode_integer_content(ByteReader& in, std::size_t length) {
  if (length == 0) {
    return make_error(Error::Code::InvalidArgument, in.offset(),
                      "INTEGER content must not be empty");
  }
  if (length > 8) {
    return make_error(Error::Code::Unsupported, in.offset(),
                      "INTEGER wider than 64 bits not supported in Phase 6");
  }
  auto bytes_r = in.read(length);
  if (!bytes_r) {
    return bytes_r.error();
  }
  const auto bytes = bytes_r.value();
  // Reject non-minimal encodings? BER allows non-minimal; DER forbids.
  // Phase 6 BER: accept non-minimal.
  std::int64_t value = 0;
  if ((bytes[0] & 0x80u) != 0) {
    value = -1;  // sign-extend
  }
  for (std::size_t i = 0; i < bytes.size(); ++i) {
    value = (value << 8) | bytes[i];
  }
  return value;
}

void encode_null_content(ByteWriter&) {}

Result<void> decode_null_content(ByteReader& in, std::size_t length) {
  if (length != 0) {
    return make_error(Error::Code::InvalidArgument, in.offset(),
                      "NULL content must be empty");
  }
  return Result<void>::success();
}

void encode_octet_string_content(ByteWriter& out, Span<const std::uint8_t> value) {
  out.write(value);
}

Result<std::vector<std::uint8_t>> decode_octet_string_content(ByteReader& in,
                                                              std::size_t length) {
  auto bytes_r = in.read(length);
  if (!bytes_r) {
    return bytes_r.error();
  }
  auto view = bytes_r.value();
  return std::vector<std::uint8_t>(view.begin(), view.end());
}

void encode_bit_string_content(ByteWriter& out, Span<const std::uint8_t> bits,
                               std::uint8_t unused_bits) {
  if (unused_bits > 7) {
    unused_bits = 7;
  }
  if (bits.empty() && unused_bits != 0) {
    unused_bits = 0;
  }
  out.put(unused_bits);
  out.write(bits);
}

Result<BitStringValue> decode_bit_string_content(ByteReader& in, std::size_t length) {
  if (length < 1) {
    return make_error(Error::Code::InvalidArgument, in.offset(),
                      "BIT STRING content must include unused-bits octet");
  }
  auto unused_r = in.get();
  if (!unused_r) {
    return unused_r.error();
  }
  const std::uint8_t unused = unused_r.value();
  if (unused > 7) {
    return make_error(Error::Code::InvalidArgument, in.offset() - 1,
                      "BIT STRING unused bits must be 0..7");
  }
  if (length == 1) {
    if (unused != 0) {
      return make_error(Error::Code::InvalidArgument, in.offset() - 1,
                        "empty BIT STRING must have 0 unused bits");
    }
    return BitStringValue{};
  }
  auto data_r = in.read(length - 1);
  if (!data_r) {
    return data_r.error();
  }
  BitStringValue v;
  v.unused_bits = unused;
  auto view = data_r.value();
  v.bits.assign(view.begin(), view.end());
  return v;
}

void encode_utf8_string_content(ByteWriter& out, const std::string& value) {
  out.write(reinterpret_cast<const std::uint8_t*>(value.data()), value.size());
}

Result<std::string> decode_utf8_string_content(ByteReader& in, std::size_t length) {
  auto bytes_r = in.read(length);
  if (!bytes_r) {
    return bytes_r.error();
  }
  auto view = bytes_r.value();
  return std::string(reinterpret_cast<const char*>(view.data()), view.size());
}

void encode_boolean(ByteWriter& out, bool value, Tag tag) {
  ByteWriter content;
  encode_boolean_content(content, value);
  encode_tlv(out, tag, content.buffer());
}

Result<bool> decode_boolean(ByteReader& in, Tag expected) {
  return decode_primitive_tlv<bool>(in, expected, decode_boolean_content);
}

void encode_integer(ByteWriter& out, std::int64_t value, Tag tag) {
  ByteWriter content;
  encode_integer_content(content, value);
  encode_tlv(out, tag, content.buffer());
}

Result<std::int64_t> decode_integer(ByteReader& in, Tag expected) {
  return decode_primitive_tlv<std::int64_t>(in, expected, decode_integer_content);
}

void encode_null(ByteWriter& out, Tag tag) {
  encode_tlv(out, tag, Span<const std::uint8_t>());
}

Result<void> decode_null(ByteReader& in, Tag expected) {
  auto hdr = decode_tlv_header(in);
  if (!hdr) {
    return hdr.error();
  }
  if (auto r = require_tag(in, expected, hdr.value().tag); !r) {
    return r;
  }
  auto len = require_definite(in, hdr.value().length);
  if (!len) {
    return len.error();
  }
  return decode_null_content(in, len.value());
}

void encode_octet_string(ByteWriter& out, Span<const std::uint8_t> value, Tag tag) {
  encode_tlv(out, tag, value);
}

Result<std::vector<std::uint8_t>> decode_octet_string(ByteReader& in, Tag expected) {
  return decode_primitive_tlv<std::vector<std::uint8_t>>(in, expected,
                                                         decode_octet_string_content);
}

void encode_bit_string(ByteWriter& out, Span<const std::uint8_t> bits, std::uint8_t unused_bits,
                       Tag tag) {
  ByteWriter content;
  encode_bit_string_content(content, bits, unused_bits);
  encode_tlv(out, tag, content.buffer());
}

Result<BitStringValue> decode_bit_string(ByteReader& in, Tag expected) {
  return decode_primitive_tlv<BitStringValue>(in, expected, decode_bit_string_content);
}

void encode_utf8_string(ByteWriter& out, const std::string& value, Tag tag) {
  ByteWriter content;
  encode_utf8_string_content(content, value);
  Tag t = tag;
  // UTF8String is primitive by default.
  encode_tlv(out, t, content.buffer());
}

Result<std::string> decode_utf8_string(ByteReader& in, Tag expected) {
  return decode_primitive_tlv<std::string>(in, expected, decode_utf8_string_content);
}

void encode_constructed(ByteWriter& out, Tag tag, Span<const std::uint8_t> components) {
  Tag t = tag;
  t.constructed = true;
  encode_tlv(out, t, components);
}

Result<std::vector<std::uint8_t>> decode_constructed(ByteReader& in, Tag expected) {
  auto hdr = decode_tlv_header(in);
  if (!hdr) {
    return hdr.error();
  }
  Tag want = expected;
  want.constructed = true;
  if (hdr.value().tag != want) {
    return make_error(Error::Code::TagMismatch, in.offset(),
                      "BER constructed tag mismatch");
  }
  if (!hdr.value().length.indefinite) {
    return decode_octet_string_content(in, hdr.value().length.value);
  }

  // Indefinite: collect TLVs until EOC.
  ByteWriter acc;
  for (;;) {
    auto peek0 = in.peek(0);
    auto peek1 = in.peek(1);
    if (peek0 && peek1 && peek0.value() == 0x00 && peek1.value() == 0x00) {
      auto eoc = decode_end_of_contents(in);
      if (!eoc) {
        return eoc.error();
      }
      break;
    }
    // Read one complete TLV (definite only for nested in this Phase 6 helper).
    const std::size_t start = in.offset();
    auto nested = decode_tlv_header(in);
    if (!nested) {
      return nested.error();
    }
    if (nested.value().length.indefinite) {
      return make_error(Error::Code::Unsupported, in.offset(),
                        "nested indefinite length not supported in Phase 6 helper");
    }
    auto content = in.read(nested.value().length.value);
    if (!content) {
      return content.error();
    }
    // Copy from start to current.
    // Re-read is awkward; rebuild from header+content.
    ByteWriter one;
    encode_tag(one, nested.value().tag);
    encode_length(one, nested.value().length.value);
    one.write(content.value());
    acc.write(one.buffer());
    (void)start;
  }
  return acc.take();
}

void encode_enumerated_content(ByteWriter& out, std::int64_t value) {
  encode_integer_content(out, value);
}

Result<std::int64_t> decode_enumerated_content(ByteReader& in, std::size_t length) {
  return decode_integer_content(in, length);
}

void encode_enumerated(ByteWriter& out, std::int64_t value, Tag tag) {
  ByteWriter content;
  encode_enumerated_content(content, value);
  encode_tlv(out, tag, content.buffer());
}

Result<std::int64_t> decode_enumerated(ByteReader& in, Tag expected) {
  return decode_primitive_tlv<std::int64_t>(in, expected, decode_enumerated_content);
}

namespace {

void encode_oid_subidentifier(ByteWriter& out, std::uint64_t value) {
  std::uint8_t stack[10];
  int n = 0;
  stack[n++] = static_cast<std::uint8_t>(value & 0x7Fu);
  value >>= 7;
  while (value > 0) {
    stack[n++] = static_cast<std::uint8_t>(0x80u | (value & 0x7Fu));
    value >>= 7;
  }
  while (n > 0) {
    out.put(stack[--n]);
  }
}

Result<std::uint64_t> decode_oid_subidentifier(ByteReader& in, std::size_t& remaining) {
  std::uint64_t value = 0;
  for (;;) {
    if (remaining == 0) {
      return make_error(Error::Code::Truncated, in.offset(), "truncated OID subidentifier");
    }
    auto b = in.get();
    if (!b) {
      return b.error();
    }
    --remaining;
    if (value > (UINT64_MAX >> 7)) {
      return make_error(Error::Code::Unsupported, in.offset(), "OID arc too large");
    }
    value = (value << 7) | (b.value() & 0x7Fu);
    if ((b.value() & 0x80u) == 0) {
      return value;
    }
  }
}

}  // namespace

void encode_object_identifier_content(ByteWriter& out, Span<const std::uint64_t> arcs) {
  if (arcs.size() < 2) {
    return;
  }
  const std::uint64_t first = 40ull * arcs[0] + arcs[1];
  encode_oid_subidentifier(out, first);
  for (std::size_t i = 2; i < arcs.size(); ++i) {
    encode_oid_subidentifier(out, arcs[i]);
  }
}

Result<std::vector<std::uint64_t>> decode_object_identifier_content(ByteReader& in,
                                                                   std::size_t length) {
  if (length == 0) {
    return make_error(Error::Code::InvalidArgument, in.offset(),
                      "OBJECT IDENTIFIER content must not be empty");
  }
  std::size_t remaining = length;
  auto first = decode_oid_subidentifier(in, remaining);
  if (!first) {
    return first.error();
  }
  std::vector<std::uint64_t> arcs;
  arcs.push_back(first.value() / 40);
  arcs.push_back(first.value() % 40);
  while (remaining > 0) {
    auto arc = decode_oid_subidentifier(in, remaining);
    if (!arc) {
      return arc.error();
    }
    arcs.push_back(arc.value());
  }
  return arcs;
}

void encode_object_identifier(ByteWriter& out, Span<const std::uint64_t> arcs, Tag tag) {
  ByteWriter content;
  encode_object_identifier_content(content, arcs);
  encode_tlv(out, tag, content.buffer());
}

Result<std::vector<std::uint64_t>> decode_object_identifier(ByteReader& in, Tag expected) {
  return decode_primitive_tlv<std::vector<std::uint64_t>>(in, expected,
                                                          decode_object_identifier_content);
}

void encode_relative_oid_content(ByteWriter& out, Span<const std::uint64_t> arcs) {
  for (std::uint64_t a : arcs) {
    encode_oid_subidentifier(out, a);
  }
}

Result<std::vector<std::uint64_t>> decode_relative_oid_content(ByteReader& in,
                                                              std::size_t length) {
  std::size_t remaining = length;
  std::vector<std::uint64_t> arcs;
  while (remaining > 0) {
    auto arc = decode_oid_subidentifier(in, remaining);
    if (!arc) {
      return arc.error();
    }
    arcs.push_back(arc.value());
  }
  return arcs;
}

void encode_relative_oid(ByteWriter& out, Span<const std::uint64_t> arcs, Tag tag) {
  ByteWriter content;
  encode_relative_oid_content(content, arcs);
  encode_tlv(out, tag, content.buffer());
}

Result<std::vector<std::uint64_t>> decode_relative_oid(ByteReader& in, Tag expected) {
  return decode_primitive_tlv<std::vector<std::uint64_t>>(in, expected,
                                                          decode_relative_oid_content);
}

}  // namespace ber
}  // namespace asn1
