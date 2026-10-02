#pragma once

#include <asn1/runtime/byte_io.hpp>

#include <cstdint>

namespace asn1 {
namespace ber {

enum class TagClass : std::uint8_t {
  Universal = 0,
  Application = 1,
  Context = 2,
  Private = 3,
};

struct Tag {
  TagClass cls = TagClass::Universal;
  bool constructed = false;
  std::uint64_t number = 0;

  bool operator==(const Tag& o) const noexcept {
    return cls == o.cls && constructed == o.constructed && number == o.number;
  }
  bool operator!=(const Tag& o) const noexcept { return !(*this == o); }
};

inline Tag universal(std::uint64_t number, bool constructed = false) {
  return Tag{TagClass::Universal, constructed, number};
}
inline Tag application(std::uint64_t number, bool constructed = false) {
  return Tag{TagClass::Application, constructed, number};
}
inline Tag context(std::uint64_t number, bool constructed = false) {
  return Tag{TagClass::Context, constructed, number};
}
inline Tag private_tag(std::uint64_t number, bool constructed = false) {
  return Tag{TagClass::Private, constructed, number};
}

// Universal tag numbers (X.680)
constexpr std::uint64_t kTagBoolean = 1;
constexpr std::uint64_t kTagInteger = 2;
constexpr std::uint64_t kTagBitString = 3;
constexpr std::uint64_t kTagOctetString = 4;
constexpr std::uint64_t kTagNull = 5;
constexpr std::uint64_t kTagOid = 6;
constexpr std::uint64_t kTagObjectDescriptor = 7;
constexpr std::uint64_t kTagExternal = 8;
constexpr std::uint64_t kTagReal = 9;
constexpr std::uint64_t kTagEnumerated = 10;
constexpr std::uint64_t kTagUtf8String = 12;
constexpr std::uint64_t kTagRelativeOid = 13;
constexpr std::uint64_t kTagSequence = 16;
constexpr std::uint64_t kTagSet = 17;
constexpr std::uint64_t kTagIa5String = 22;

struct Length {
  bool indefinite = false;
  std::size_t value = 0;  // meaningful when !indefinite
};

void encode_tag(ByteWriter& out, Tag tag);
Result<Tag> decode_tag(ByteReader& in);

/// Encode a definite length (short or long form). Never encodes indefinite.
void encode_length(ByteWriter& out, std::size_t length);

/// Decode short, long, or indefinite length. Indefinite => Length::indefinite.
Result<Length> decode_length(ByteReader& in);

/// Encode TLV: tag + definite length + content bytes.
void encode_tlv(ByteWriter& out, Tag tag, Span<const std::uint8_t> content);

/// Decode next TLV header; on success reader is positioned at the start of content.
/// For definite length, content has exactly `length.value` bytes remaining for that value.
/// For indefinite, caller must parse until end-of-contents (00 00).
struct TlvHeader {
  Tag tag;
  Length length;
};
Result<TlvHeader> decode_tlv_header(ByteReader& in);

/// Require end-of-contents octets 0x00 0x00 (indefinite constructed values).
Result<void> decode_end_of_contents(ByteReader& in);

}  // namespace ber
}  // namespace asn1
