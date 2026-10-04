/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/include/asn1/ast/encoding.hpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   AST for encoding instructions (XER/EXTENDED-XER and related).
**
** Specification: ITU-T X.680 — abstract syntax;
**                 ITU-T X.693 — encoding instruction syntax (XER /
**                 EXTENDED-XER references).
** Design Spec:   asn1-toolkit/docs/ARCHITECTURE.md
**                 asn1-toolkit/README.md
***************************************************************************/
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
