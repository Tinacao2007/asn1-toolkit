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
  void emit(const ir::Model& model, const EmitOptions& options, Diagnostics& diag,
            std::ostream& header, std::ostream* source = nullptr);

  /// Convenience: emit header text only.
  std::string emit_header_string(const ir::Model& model, const EmitOptions& options,
                                 Diagnostics& diag);
};

}  // namespace codegen
}  // namespace asn1
