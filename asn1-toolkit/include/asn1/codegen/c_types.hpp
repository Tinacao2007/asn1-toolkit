#pragma once
#include <asn1/ir/type.hpp>
#include <asn1/support/diagnostics.hpp>
#include <ostream>
#include <string>

namespace asn1 { namespace codegen {
// Experimental, type/lifecycle-only stream API. No codec entry is generated.
// Streams are untouched on model/option rejection. Caller owns publication.
// Unsupported value layouts are diagnosed, never emitted as placeholders.
// Prefix must start with an ASCII letter and contain only ASCII alphanumerics
// and single underscores, with no trailing underscore. header_name is a plain
// filename (no paths), and must not shadow a C11 standard header (case-folded).
// init/free allocate nothing; reset_defaults materializes owning literals with
// status and transactional replacement. copy requires initialized output.
// Metadata type IDs are schema-local, not stable wire or ABI discriminants.
// See docs/c-generated-abi.md for the experimental subset.
void emit_c_types(const ir::Model&, const std::string& prefix,
                  const std::string& header_name, Diagnostics&,
                  std::ostream& header, std::ostream& source);
}}
