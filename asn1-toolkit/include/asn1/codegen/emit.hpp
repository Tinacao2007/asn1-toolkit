/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/include/asn1/codegen/emit.hpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   Public API for C++ code generation (EmitOptions, CodecKind,
**   CppGenerator).
**
** Specification: Generated code layout follows ITU type rules; emission
**                 logic is toolchain-internal.
** Design Spec:   asn1-toolkit/docs/ARCHITECTURE.md
**                 asn1-toolkit/README.md
***************************************************************************/
#pragma once

#include <asn1/ir/type.hpp>
#include <asn1/support/diagnostics.hpp>

#include <ostream>
#include <string>

namespace asn1 {
namespace codegen {

enum class CodecKind { Uper, Aper, Both, Jer, Xer, Exer, Ber, Der, Oer, Coer, Cxer };

struct EmitOptions {
  std::string namespace_name = "asn1_gen";
  CodecKind codec = CodecKind::Uper;
  /// Header guard / include basename without extension (default: generated).
  std::string basename = "generated";
};

class CppGenerator {
 public:
  /// Write a self-contained header (types + inline encode/decode) to `header`,
  /// and an optional companion `.cpp` (currently empty preamble) to `source`
  /// when non-null. Does not link or include the codec runtime implementation
  /// libraries — only prints calls into the selected runtime codec namespace.
  /**
   *  Function    : emit
   *  Description : Generates C++ header (and optional stub source) for the IR model.
   *  Parameters  : model — resolved type IR; options — namespace/codec/basename;
   *                diag — diagnostic sink; header — output stream; source — optional .cpp
   *  Returns     : void
   */
  void emit(const ir::Model& model, const EmitOptions& options, Diagnostics& diag,
            std::ostream& header, std::ostream* source = nullptr);

  /// Convenience: emit header text only.
  /**
   *  Function    : emit_header_string
   *  Description : Generates header text into an in-memory string.
   *  Parameters  : model — type IR; options — emit options; diag — diagnostics
   *  Returns     : std::string — generated header contents
   */
  std::string emit_header_string(const ir::Model& model, const EmitOptions& options,
                                 Diagnostics& diag);
};

}  // namespace codegen
}  // namespace asn1
