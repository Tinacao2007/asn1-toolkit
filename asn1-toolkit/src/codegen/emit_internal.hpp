/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/src/codegen/emit_internal.hpp
**   $Version: 0.1
**   $Date:   2026-10-14
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   Internal (src-local) declarations shared by the
   emit_*.cpp family files: shared free helpers plus
   the full Emitter class interface. NOT installed.
**
** Specification: Generated code layout follows ITU type rules; emission
**                 logic is toolchain-internal.
** Design Spec:   asn1-toolkit/docs/architecture.md
**                 asn1-toolkit/README.md
***************************************************************************/
#pragma once
#include <asn1/codegen/emit.hpp>
#include <asn1/ir/type.hpp>
#include <asn1/support/diagnostics.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "emit_cache.hpp"

namespace asn1 {
namespace codegen {

// --- shared free helpers (defined in emit_core.cpp) ---

/**
 *  Function    : is_cpp_keyword
 *  Description : Returns whether cpp keyword holds for the given inputs.
 *  Parameters  : id — std::string_view id
 *  Returns     : bool
 */
bool is_cpp_keyword(std::string_view id);

/**
 *  Function    : cpp_ident
 *  Description : Builds and returns a string for cpp ident.
 *  Parameters  : name — std::string_view name
 *  Returns     : std::string
 */
std::string cpp_ident(std::string_view name);

/**
 *  Function    : resolve
 *  Description : Computes resolve from (model, id).
 *  Parameters  : model — const ir::Model& model; id — ir::TypeId id
 *  Returns     : const ir::Type&
 */
const ir::Type& resolve(const ir::Model& model, ir::TypeId id);

/**
 *  Function    : is_named
 *  Description : Returns whether the given type carries an ASN.1 name.
 *  Parameters  : t — const ir::Type& t
 *  Returns     : bool
 */
bool is_named(const ir::Type& t);

/**
 *  Function    : named_type_cpp
 *  Description : Builds the C++ name of a named type.
 *  Parameters  : t — const ir::Type& t
 *  Returns     : std::string
 */
std::string named_type_cpp(const ir::Type& t);

/**
 *  Function    : integer_is_bigint
 *  Description : Returns whether the integer needs the BigInt host type.
 *  Parameters  : d — const ir::IntegerDesc& d
 *  Returns     : bool
 */
bool integer_is_bigint(const ir::IntegerDesc& d);

/**
 *  Function    : integer_host_type
 *  Description : Builds and returns a string for integer host type.
 *  Parameters  : d — const ir::IntegerDesc& d
 *  Returns     : std::string
 */
std::string integer_host_type(const ir::IntegerDesc& d);

/**
 *  Function    : opt_i64_expr
 *  Description : Builds and returns a string for opt i64 expr.
 *  Parameters  : b — const std::optional<ir::BigInt>& b
 *  Returns     : std::string
 */
std::string opt_i64_expr(const std::optional<ir::BigInt>& b);

/**
 *  Function    : opt_size_expr
 *  Description : Builds and returns a string for opt size expr.
 *  Parameters  : b — const std::optional<ir::BigInt>& b
 *  Returns     : std::string
 */
std::string opt_size_expr(const std::optional<ir::BigInt>& b);

class Emitter {
 public:
  Emitter(const ir::Model& model, EmitOptions options, Diagnostics& diag)
      : model_(model), options_(std::move(options)), diag_(diag) {}

  // A `using` alias is not a distinct C++ type, so two ASN.1 names that share a
  // host (both INTEGER -> uint32_t, both OCTET STRING -> vector, ...) produce
  // identical encode/decode signatures. Emit a unique struct instead.
  enum class AliasShape { Scalar, Class };

  // --- core: driver, naming, type declarations (emit_core.cpp) ---

    /**
     *  Function    : run
     *  Description : Performs run (definition).
     *  Parameters  : header — std::ostream& header; source — std::ostream* source
     *  Returns     : void
     */
    void run(std::ostream& header, std::ostream* source);

    /**
     *  Function    : run_to_dir
     *  Description : Flat emit into a directory, consulting the content-hash
     *                cache when `options.emit_cache` is true. Writes
     *                `<dir>/<basename>.hpp` (and `.cpp` when out-of-line);
     *                a cache hit (key + output hashes match) skips the emit
     *                and reuses the previously written files verbatim.
     *  Parameters  : dir — const std::filesystem::path& dir
     *  Returns     : void
     */
    void run_to_dir(const std::filesystem::path& dir);

    /**
     *  Function    : begin_codec_fn
     *  Description : Emits a codec function header. Inline mode writes
     *                "inline <sig> {" to the declaration stream; out-of-line
     *                mode writes "<sig>;" to the header and "<sig> {" to the
     *                definition stream.
     *  Parameters  : decl_out — header stream; sig — full signature text
     *  Returns     : std::ostream* — stream the body must be written to
     */
    std::ostream* begin_codec_fn(std::ostream& decl_out, const std::string& sig);

    /**
     *  Function    : collect_named
     *  Description : Performs collect named (definition).
     *  Parameters  : none
     *  Returns     : void
     */
    void collect_named();

    /**
     *  Function    : type_cpp
     *  Description : Builds and returns a string for type cpp.
     *  Parameters  : id — ir::TypeId id; for_optional — bool for_optional
     *  Returns     : std::string
     */
    std::string type_cpp(ir::TypeId id, bool for_optional = false);

    /**
     *  Function    : inline_type_cpp
     *  Description : Builds and returns a string for inline type cpp.
     *  Parameters  : t — const ir::Type& t; for_optional — bool for_optional
     *  Returns     : std::string
     */
    std::string inline_type_cpp(const ir::Type& t, bool /*for_optional*/);

    /**
     *  Function    : emit_header
     *  Description : Performs emit header (definition).
     *  Parameters  : out — std::ostream& out
     *  Returns     : void
     */
    void emit_header(std::ostream& out);

    /**
     *  Function    : emit_includes
     *  Description : Emits the runtime includes selected by options_.codec
     *                (shared by the flat header and every split module
     *                header).
     *  Parameters  : out — std::ostream& out
     *  Returns     : void
     */
    void emit_includes(std::ostream& out);

    /**
     *  Function    : emit_types_and_codecs
     *  Description : Emits forward declarations, type declarations and codec
     *                functions for the given named types.
     *  Parameters  : out — std::ostream& out; ids — const std::vector<ir::TypeId>& ids
     *  Returns     : void
     */
    void emit_types_and_codecs(std::ostream& out, const std::vector<ir::TypeId>& ids);

    /**
     *  Function    : emit_type_codecs
     *  Description : Emits the codec functions of one named type (the
     *                per-type slice of emit_types_and_codecs).
     *  Parameters  : out — std::ostream& out; id — ir::TypeId id
     *  Returns     : void
     */
    void emit_type_codecs(std::ostream& out, ir::TypeId id);

    /**
     *  Function    : named_ref
     *  Description : In split mode, the cross-module qualified name
     *                (`<Module>::Name`) of a named type that lives outside
     *                the module currently being emitted; the plain C++ name
     *                otherwise (same module, or flat single-namespace mode).
     *  Parameters  : t — const ir::Type& t
     *  Returns     : std::string
     */
    std::string named_ref(const ir::Type& t) const;

    /**
     *  Function    : collect_module_groups
     *  Description : Split mode: groups the named types by ASN.1 module,
     *                assigns a unique C++ namespace ident per module, and
     *                computes the cross-module include dependencies.
     *  Parameters  : none
     *  Returns     : void
     */
    void collect_module_groups();

    /**
     *  Function    : run_split
     *  Description : Split mode driver: writes one `<Module>.hpp/.cpp` pair
     *                per module plus the aggregate header into `dir`.
     *  Parameters  : dir — const std::filesystem::path& dir
     *  Returns     : void
     */
    void run_split(const std::filesystem::path& dir);

    static std::string table_entry_alt_name(const ir::TableConstraintEntry& entry, std::size_t idx);

    static std::vector<std::string> compute_table_entry_alt_names(
      const std::vector<ir::TableConstraintEntry>& entries);

    void emit_table_constraint_structs(std::ostream& out, const std::vector<ir::Field>& fields);

    AliasShape alias_shape(const ir::Type& t) const;

    void emit_distinct_alias(std::ostream& out, const std::string& name, const std::string& host,
                           AliasShape shape);

  /**
   *  Function    : emit_type_decl
   *  Description : Performs emit type decl (definition).
   *  Parameters  : out — std::ostream& out; id — ir::TypeId id
   *  Returns     : void
   */
  void emit_type_decl(std::ostream& out, ir::TypeId id);

  // --- P2-3 content-hash cache helpers ---

  /// Build the deterministic key inputs (codec mask + basename + namespace
  /// + emit_split + embed flags + cli_version) so the same cache directory
  /// can hold entries for distinct emits without colliding.
  std::vector<std::string> cache_extra_inputs() const;

  /// Compute the sha256 of `options_.source_paths` content (sorted) and
  /// return it as a single extra-input string. Empty source_paths ->
  /// "src:empty" so cache entries are still well-formed.
  std::string cache_source_fingerprint() const;

  /// Precomputed sha256 of every `options_.source_paths` entry, keyed by
  /// absolute path string. Computed once per run (memoised in
  /// `cache_source_hashes_cached_`) and passed to
  /// `ContentCache::compute_key` so each .asn file is read+SHA-256'd at
  /// most once across the whole run — split mode calls this N times.
  const std::unordered_map<std::string, std::string>& cache_source_hashes() const;

  /// Lazily construct the in-memory `cache_` if `options_.emit_cache` is
  /// true and the caller provided `options_.emit_dir`. Returns nullptr
  /// when caching is disabled or no dir is available.
  ContentCache* cache_init();

  /// Try to reuse a cached unit. `unit_name` is the stable identifier
  /// ("default" for flat, the module ns ident for split). `expected_outputs`
  /// lists the on-disk files (relative paths under options_.emit_dir)
  /// that the previous successful emit produced. Returns true on a
  /// verifiable cache hit (key matches AND every expected file's
  /// current hash equals the recorded one). False on miss or any
  /// unverifiable state (we re-emit then).
  bool cache_check_unit(const std::string& unit_name,
                        const std::vector<std::filesystem::path>& expected_outputs);

  /// Hash each on-disk file under options_.emit_dir (relative paths),
  /// build a CacheEntry, and stash it in `cache_`. Called immediately
  /// after a successful emit.
  void cache_record_unit(const std::string& unit_name,
                         const std::vector<std::filesystem::path>& outputs);

  /// Persist all staged cache entries to disk (no-op if caching disabled).
  /// Also emits the diagnostic note describing hit/miss counts.
  void cache_finalise();

  /// Reset per-run counters (called at the top of run/run_split so
  /// multiple invocations on the same Emitter are independent).
  void cache_reset_stats();

    /**
     *  Function    : choice_all_fields
     *  Description : Computes choice all fields from (c).
     *  Parameters  : c — const ir::ChoiceDesc& c
     *  Returns     : static std::vector<const ir::Field*>
     */
    static std::vector<const ir::Field*> choice_all_fields(const ir::ChoiceDesc& c);

    /**
     *  Function    : field_has_presence_bit
     *  Description : Returns a boolean result from f.
     *  Parameters  : f — const ir::Field& f
     *  Returns     : static bool
     */
    static bool field_has_presence_bit(const ir::Field& f);

    /**
     *  Function    : field_wraps_optional
     *  Description : Returns a boolean result from f, force_optional.
     *  Parameters  : f — const ir::Field& f; force_optional — bool force_optional
     *  Returns     : static bool
     */
    static bool field_wraps_optional(const ir::Field& f, bool force_optional);

    static void emit_ext_group_field_present(std::ostream& out, const ir::Field& f,
                                             const std::string& expr);

    /**
     *  Function    : default_val_cpp
     *  Description : Builds the C++ default-value expression for a field.
     *                BigInteger members get from_i64(...) wrapping.
     *  Parameters  : f — const ir::Field& f
     *  Returns     : std::string
     */
    std::string default_val_cpp(const ir::Field& f);

    void emit_struct_fields(std::ostream& out, const std::vector<ir::Field>& fields,
                          bool force_optional);

    /**
     *  Function    : type_is_constructed
     *  Description : Returns a boolean result from t.
     *  Parameters  : t — const ir::Type& t
     *  Returns     : static bool
     */
    static bool type_is_constructed(const ir::Type& t);

  // --- PER family (emit_per.cpp) ---

    /**
     *  Function    : ns_codec
     *  Description : Computes ns codec from (aper).
     *  Parameters  : aper — bool aper
     *  Returns     : const char*
     */
    const char* ns_codec(bool aper) const;

    /**
     *  Function    : fn_suffix
     *  Description : Computes fn suffix from (aper).
     *  Parameters  : aper — bool aper
     *  Returns     : const char*
     */
    const char* fn_suffix(bool aper) const;

    /**
     *  Function    : emit_codec_fns
     *  Description : Performs emit codec fns (definition).
     *  Parameters  : out — std::ostream& out; id — ir::TypeId id; aper — bool aper
     *  Returns     : void
     */
    void emit_codec_fns(std::ostream& out, ir::TypeId id, bool aper);

    /**
     *  Function    : enum_root_sorted_values
     *  Description : Computes enum root sorted values from (e).
     *  Parameters  : e — const ir::EnumeratedDesc& e
     *  Returns     : static std::vector<std::int64_t>
     */
    static std::vector<std::int64_t> enum_root_sorted_values(const ir::EnumeratedDesc& e);

    /**
     *  Function    : enum_extension_values
     *  Description : Computes enum extension values from (e).
     *  Parameters  : e — const ir::EnumeratedDesc& e
     *  Returns     : static std::vector<std::int64_t>
     */
    static std::vector<std::int64_t> enum_extension_values(const ir::EnumeratedDesc& e);

    void emit_extension_group_encode(std::ostream& out, const ir::ExtensionGroup& group,
                                   const std::string& expr, bool aper,
                                   const std::string& ind);

    void emit_extension_group_decode(std::ostream& out, const ir::ExtensionGroup& group,
                                   const std::string& expr, bool aper,
                                   const std::string& ind);

    void emit_root_fields_encode(std::ostream& out, const std::vector<ir::Field>& fields,
                               const std::string& expr, bool aper, const std::string& ind);

    void emit_sequence_like_encode(std::ostream& out, const ir::SequenceDesc& seq,
                                 const std::string& expr, bool aper,
                                 const std::string& ind);

    void emit_root_fields_decode(std::ostream& out, const std::vector<ir::Field>& fields,
                               const std::string& expr, bool aper, const std::string& ind,
                               std::size_t& opt_i);

    void emit_sequence_like_decode(std::ostream& out, const ir::SequenceDesc& seq,
                                 const std::string& expr, bool aper,
                                 const std::string& ind);

    void emit_encode_body(std::ostream& out, const ir::Type& t, const std::string& expr,
                        bool aper, const std::string& ind);

    void emit_field_encode(std::ostream& out, const ir::Type& ft, const std::string& expr,
                         bool aper, const std::string& ind);

    void emit_decode_body(std::ostream& out, const ir::Type& t, const std::string& expr,
                        bool aper, const std::string& ind);

    void emit_field_decode(std::ostream& out, const ir::Type& ft, const std::string& expr,
                         bool aper, const std::string& ind);

    static const char* per_char_string_kind_expr(ir::StringKind kind);

    std::string per_string_encode_call(const char* C, ir::StringKind kind,
                                     const std::string& expr,
                                     const std::string& size_expr);

    std::string per_string_decode_call(const char* C, ir::StringKind kind,
                                     const std::string& expr,
                                     const std::string& size_expr);

  // --- JER family (emit_jer.cpp) ---

    /**
     *  Function    : jer_name_form_expr
     *  Description : Computes jer name form expr from (f).
     *  Parameters  : f — ir::Field::JerNameForm f
     *  Returns     : static const char*
     */
    static const char* jer_name_form_expr(ir::Field::JerNameForm f);

    /**
     *  Function    : jer_text_form_expr
     *  Description : Computes jer text form expr from (f).
     *  Parameters  : f — ir::JerEncoding::TextForm f
     *  Returns     : static const char*
     */
    static const char* jer_text_form_expr(ir::JerEncoding::TextForm f);

    /**
     *  Function    : emit_jer_codec_fns
     *  Description : Performs emit jer codec fns (definition).
     *  Parameters  : out — std::ostream& out; id — ir::TypeId id
     *  Returns     : void
     */
    void emit_jer_codec_fns(std::ostream& out, ir::TypeId id);

    void emit_jer_encode_call(std::ostream& out, const ir::Type& ft, const std::string& expr,
                            const std::string& ind);

    void emit_jer_encode_body(std::ostream& out, const ir::Type& t, const std::string& expr,
                            const std::string& ind);

    void emit_jer_field_encode_expr(std::ostream& out, const ir::Type& ft,
                                  const std::string& expr, const std::string& ind);

    void emit_jer_decode_body(std::ostream& out, const ir::Type& t, const std::string& expr,
                            const std::string& v_expr, const std::string& ind);

    void emit_jer_field_decode_stmt(std::ostream& out, const ir::Type& ft,
                                  const std::string& lhs_expr, const std::string& v_expr,
                                  const std::string& ind);

  // --- XER family (emit_xer.cpp) ---

    /**
     *  Function    : xer_use_exer
     *  Description : Returns a boolean result from none.
     *  Parameters  : none
     *  Returns     : bool
     */
    bool xer_use_exer() const;

    /**
     *  Function    : xer_use_cxer
     *  Description : Returns a boolean result from none.
     *  Parameters  : none
     *  Returns     : bool
     */
    bool xer_use_cxer() const;

    /**
     *  Function    : xml_ns
     *  Description : Selects the XML runtime namespace (cxer variant vs xer).
     *  Parameters  : none
     *  Returns     : const char*
     */
    const char* xml_ns() const;

    /**
     *  Function    : xml_fn_suffix
     *  Description : Selects the XML codec function suffix (cxer vs xer).
     *  Parameters  : none
     *  Returns     : const char*
     */
    const char* xml_fn_suffix() const;

    static std::string transform_xml_name(const std::string& name, ir::Field::JerNameForm form,
                                        const std::string& literal);

    /**
     *  Function    : xer_field_name
     *  Description : Builds and returns a string for xer field name.
     *  Parameters  : f — const ir::Field& f
     *  Returns     : std::string
     */
    std::string xer_field_name(const ir::Field& f) const;

    void emit_xer_enum_id_switch(std::ostream& out, const ir::Type& rt, const std::string& expr,
                               const std::string& ind);

    void emit_xer_field_encode_expr(std::ostream& out, const ir::Type& ft,
                                  const std::string& expr, const std::string& ind);

    void emit_xer_field_decode_stmt(std::ostream& out, const ir::Type& ft,
                                 const std::string& lhs_expr, const std::string& el_expr,
                                 const std::string& ind);

    /**
     *  Function    : emit_xer_codec_fns
     *  Description : Performs emit xer codec fns (definition).
     *  Parameters  : out — std::ostream& out; id — ir::TypeId id
     *  Returns     : void
     */
    void emit_xer_codec_fns(std::ostream& out, ir::TypeId id);

    void emit_xer_encode_body(std::ostream& out, const ir::Type& t, const std::string& expr,
                            const std::string& ind);

    void emit_xer_decode_body(std::ostream& out, const ir::Type& t, const std::string& expr,
                            const std::string& el_expr, const std::string& ind);

  // --- OER family (emit_oer.cpp) ---

    /**
     *  Function    : oer_use_coer
     *  Description : Returns a boolean result from none.
     *  Parameters  : none
     *  Returns     : bool
     */
    bool oer_use_coer() const;

    /**
     *  Function    : oer_ns
     *  Description : Selects the OER runtime namespace (coer variant vs oer).
     *  Parameters  : none
     *  Returns     : const char*
     */
    const char* oer_ns() const;

    /**
     *  Function    : oer_fn_suffix
     *  Description : Selects the OER codec function suffix (coer vs oer).
     *  Parameters  : none
     *  Returns     : const char*
     */
    const char* oer_fn_suffix() const;

    /**
     *  Function    : oer_integer_constraint_expr
     *  Description : Builds and returns a string for oer integer constraint expr.
     *  Parameters  : d — const ir::IntegerDesc& d
     *  Returns     : std::string
     */
    std::string oer_integer_constraint_expr(const ir::IntegerDesc& d);

    /**
     *  Function    : oer_size_constraint_expr
     *  Description : Builds and returns a string for oer size constraint expr.
     *  Parameters  : d — const ir::ConstraintDesc& d
     *  Returns     : std::string
     */
    std::string oer_size_constraint_expr(const ir::ConstraintDesc& d);

    /**
     *  Function    : oer_real_form_expr
     *  Description : Builds and returns a string for oer real form expr.
     *  Parameters  : f — ir::RealIeeeForm f
     *  Returns     : std::string
     */
    std::string oer_real_form_expr(ir::RealIeeeForm f);

    /**
     *  Function    : emit_oer_codec_fns
     *  Description : Performs emit oer codec fns (definition).
     *  Parameters  : out — std::ostream& out; id — ir::TypeId id
     *  Returns     : void
     */
    void emit_oer_codec_fns(std::ostream& out, ir::TypeId id);

    void emit_oer_field_encode(std::ostream& out, const ir::Type& ft, const std::string& expr,
                             const std::string& ind);

    void emit_oer_field_decode(std::ostream& out, const ir::Type& ft, const std::string& expr,
                             const std::string& ind);

    void emit_oer_extension_group_encode(std::ostream& out, const ir::ExtensionGroup& group,
                                       const std::string& expr, const std::string& ind);

    void emit_oer_extension_group_decode(std::ostream& out, const ir::ExtensionGroup& group,
                                       const std::string& expr, const std::string& ind);

    void emit_oer_sequence_encode(std::ostream& out, const ir::SequenceDesc& seq,
                                const std::string& expr, const std::string& ind,
                                bool is_set);

    void emit_oer_sequence_decode(std::ostream& out, const ir::SequenceDesc& seq,
                                const std::string& expr, const std::string& ind,
                                bool is_set);

    void emit_oer_encode_body(std::ostream& out, const ir::Type& t, const std::string& expr,
                            const std::string& ind);

    void emit_oer_decode_body(std::ostream& out, const ir::Type& t, const std::string& expr,
                            const std::string& ind);

  // --- TLV family (emit_tlv.cpp) ---

  /// TLV variant threaded through the TLV emitter: BER (basic), DER
  /// (distinguished) and CER (canonical) share one emission shape and differ
  /// only in the runtime namespace and the canonicalization rules applied.
  enum class TlvKind { Ber, Der, Cer };

  /// X.690 9.3 / X.680 8.6: the canonical position key of a SET component —
  /// tag class first (universal < application < context < private), then the
  /// tag number; the constructed bit only breaks ties. An untagged CHOICE
  /// component is keyed by the SMALLEST tag in the choice, recursively
  /// through nested untagged choices, so the key is static.
  struct SetComponentKey {
    unsigned cls = 0;
    std::uint64_t number = 0;
    bool constructed = false;

    bool operator<(const SetComponentKey& o) const {
      if (cls != o.cls) {
        return cls < o.cls;
      }
      if (number != o.number) {
        return number < o.number;
      }
      return constructed < o.constructed;
    }
  };

    /**
     *  Function    : set_component_key
     *  Description : Computes the X.690 9.3 position key of a SET component.
     *  Parameters  : f — const ir::Field& f
     *  Returns     : SetComponentKey
     */
    SetComponentKey set_component_key(const ir::Field& f) const;

    /**
     *  Function    : set_fields_ordered
     *  Description : Returns the SET components in X.690 9.3 canonical order.
     *  Parameters  : set — const ir::SequenceDesc& set
     *  Returns     : std::vector<const ir::Field*>
     */
    std::vector<const ir::Field*> set_fields_ordered(const ir::SequenceDesc& set) const;

    /**
     *  Function    : choice_dispatch_tags
     *  Description : Returns the wire tag expressions identifying a SET
     *                component that is an untagged CHOICE: the expected tag
     *                of every alternative, recursing through alternatives
     *                that are themselves untagged CHOICE types.
     *  Parameters  : choice — const ir::ChoiceDesc& choice
     *  Returns     : std::vector<std::string>
     */
    std::vector<std::string> choice_dispatch_tags(const ir::ChoiceDesc& choice) const;

    /**
     *  Function    : tlv_ns
     *  Description : Computes tlv ns from (tlv).
     *  Parameters  : tlv — TlvKind tlv
     *  Returns     : const char*
     */
    const char* tlv_ns(TlvKind tlv) const;

    /**
     *  Function    : tlv_fn
     *  Description : Computes tlv fn from (tlv).
     *  Parameters  : tlv — TlvKind tlv
     *  Returns     : const char*
     */
    const char* tlv_fn(TlvKind tlv) const;

    /**
     *  Function    : ber_tag_expr
     *  Description : Builds and returns a string for ber tag expr.
     *  Parameters  : tag — const ir::Tag& tag; constructed — bool constructed
     *  Returns     : static std::string
     */
    static std::string ber_tag_expr(const ir::Tag& tag, bool constructed);

    /**
     *  Function    : universal_number_for
     *  Description : Computes universal number for from (t).
     *  Parameters  : t — const ir::Type& t
     *  Returns     : static std::uint64_t
     */
    static std::uint64_t universal_number_for(const ir::Type& t);

    /**
     *  Function    : natural_tag_expr
     *  Description : Builds and returns a string for natural tag expr.
     *  Parameters  : t — const ir::Type& t
     *  Returns     : static std::string
     */
    static std::string natural_tag_expr(const ir::Type& t);

    /**
     *  Function    : emit_tlv_codec_fns
     *  Description : Performs emit tlv codec fns (definition).
     *  Parameters  : out — std::ostream& out; id — ir::TypeId id; tlv — TlvKind tlv
     *  Returns     : void
     */
    void emit_tlv_codec_fns(std::ostream& out, ir::TypeId id, TlvKind tlv);

    void emit_tlv_field_encode(std::ostream& out, const ir::Type& ft, const ir::Field* field,
                             const std::string& expr, TlvKind tlv, const std::string& ind);

    void emit_tlv_encode_content(std::ostream& out, const ir::Type& t, const std::string& expr,
                               TlvKind tlv, const std::string& ind);

    void emit_tlv_sequence_field_encode(std::ostream& out, const ir::Field& f,
                                      const std::string& expr, TlvKind tlv,
                                      const std::string& ind);

    void emit_tlv_sequence_components(std::ostream& out, const ir::SequenceDesc& seq,
                                    const std::string& expr, TlvKind tlv,
                                    const std::string& ind);

    void emit_tlv_encode_body(std::ostream& out, const ir::Type& t, const std::string& expr,
                            TlvKind tlv, const ir::Tag* override_tag, const std::string& ind);

    void emit_tlv_decode_body(std::ostream& out, const ir::Type& t, const std::string& expr,
                            TlvKind tlv, const ir::Tag* override_tag, const std::string& ind);

    void emit_tlv_field_decode_stmt(std::ostream& out, const ir::Field& f, const std::string& expr,
                                  TlvKind tlv, const std::string& ind);

    void emit_tlv_field_decode_value(std::ostream& out, const ir::Type& ft, const ir::Field* field,
                                   const std::string& expr, TlvKind tlv, const std::string& ind);

 private:
  const ir::Model& model_;
  EmitOptions options_;
  Diagnostics& diag_;
  std::vector<ir::TypeId> named_order_;
  std::unordered_set<ir::TypeId> named_set_;
  /// Non-null in out-of-line mode: codec definitions stream here while the
  /// header receives declarations only.
  std::ostream* def_out_ = nullptr;
  /// Monotonic counter for generated decode-temp names. Optional-field
  /// decode temps are named `field<N>` so a nested optional SEQUENCE's temp
  /// cannot shadow an enclosing temp of the same name (the enclosing temp is
  /// the assignment target `expr`, so a shadow breaks every nested optional).
  std::size_t temp_counter_ = 0;
  /// Split mode: the ASN.1 module whose types are currently being emitted
  /// (empty in flat single-namespace mode). Cross-module references are
  /// qualified against it.
  std::string current_module_;
  /// Split mode: ASN.1 module name -> unique C++ namespace ident.
  std::unordered_map<std::string, std::string> module_ns_;
  /// Split mode: ASN.1 module name -> its named type ids in emit order.
  std::unordered_map<std::string, std::vector<ir::TypeId>> module_types_;
  /// Split mode: ASN.1 module name -> names of modules its headers must
  /// include (types referenced across module boundaries).
  std::unordered_map<std::string, std::vector<std::string>> module_deps_;
  /// Split mode: emit units in dependency-first order. A unit is one ASN.1
  /// module, or a merged strongly-connected group of modules: mutually
  /// recursive modules cannot live in separate headers (a by-value
  /// cross-module member needs the other type complete), so they share one
  /// header/.cpp pair while every type keeps its own module namespace.
  std::vector<std::vector<std::string>> unit_modules_;
  /// Split mode: file basename per unit (parallel to unit_modules_).
  std::vector<std::string> unit_files_;
  /// Split mode: external module dependencies per unit (parallel).
  std::vector<std::vector<std::string>> unit_deps_;
  /// P2-3 content-hash cache. Lazily constructed in run()/run_split() when
  /// `options_.emit_cache` is true; null when caching is disabled.
  std::unique_ptr<ContentCache> cache_;
  /// P2-3: emit-time statistics for the diagnostic note ("N/M units hit
  /// in X.Xs, K missed in Y.Ys"). Counters are bumped in run/run_split.
  std::size_t cache_units_total_ = 0;
  std::size_t cache_units_hit_ = 0;
  /// P2-3: cumulative wall time of emit work that actually ran (i.e. the
  /// sum of per-unit `run`/`run_split` miss durations). Hit units cost ~0.
  std::chrono::nanoseconds cache_miss_wall_{0};
  /// P2-3: memoised `cache_extra_inputs()` (the deterministic emit-option
  /// suffix of every cache key) and `cache_source_fingerprint()` (sorted
  /// source paths + per-source sha256). Both are pure functions of
  /// `options_` / `options_.source_paths`, so caching the result across
  /// every per-unit `cache_check_unit` / `cache_record_unit` call saves
  /// O(units * sources) file reads on miss — meaningful for the LPP/S1AP
  /// fixtures that the build gates compile. `mutable` so the const
  /// `cache_extra_inputs()` / `cache_source_fingerprint()` accessors can
  /// fill these on first call without changing the Emitter's API.
  mutable std::vector<std::string> cache_extras_cached_;
  mutable bool cache_extras_valid_ = false;
  mutable std::string cache_source_fp_cached_;
  mutable bool cache_source_fp_valid_ = false;
  /// Per-source sha256 cache (path string -> sha256(content)). Filled
  /// lazily by `cache_source_hashes()`; consumed by `compute_key`.
  mutable std::unordered_map<std::string, std::string> cache_source_hashes_cached_;
  mutable bool cache_source_hashes_valid_ = false;
};

}  // namespace codegen
}  // namespace asn1
