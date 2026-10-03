#pragma once

#include <asn1/support/source_location.hpp>

#include <string>
#include <vector>

namespace asn1 {
namespace ast {

/// Encoding instruction kinds for JER (X.697) and EXTENDED-XER (X.693).
enum class EncodingInstructionKind {
  // JER
  Array,
  Object,
  Unwrapped,
  // Shared
  Base64,
  Name,
  Text,
  // EXTENDED-XER
  Attribute,
  UseNumber,
  List,
  Untagged,
  UseNil,
};

enum class NameTransform {
  AsIs,
  Capitalized,
  Uppercased,
  Lowercased,
  Literal,
};

struct EncodingInstruction {
  EncodingInstructionKind kind = EncodingInstructionKind::Array;
  NameTransform transform = NameTransform::AsIs;
  std::string literal;       // NAME/TEXT AS "..."
  bool text_all = false;     // TEXT ALL AS ...
  std::string text_item;     // TEXT item-name AS ...
  SourceRange range;
};

/// ENCODING-CONTROL JER/XER clause target type.
enum class EncodingControlTarget {
  OctetString,
  BitString,
  Sequence,
  Choice,
  SetOf,
  SequenceOf,
  Enumerated,
  Boolean,
  Unknown,
};

enum class EncodingControlFamily { Jer, Xer };

struct EncodingControlClause {
  EncodingInstruction instruction;
  EncodingControlTarget target = EncodingControlTarget::Unknown;
  EncodingControlFamily family = EncodingControlFamily::Jer;
  SourceRange range;
};

}  // namespace ast
}  // namespace asn1
