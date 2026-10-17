/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/src/codegen/emit_tlv.cpp
**   $Version: 0.1
**   $Date:   2026-10-14
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   TLV family emitter (BER/DER share one implementation via the der
   flag): tag-length-value encode/decode body emission.
**
** Specification: Generated code layout follows ITU type rules; emission
**                 logic is toolchain-internal.
** Design Spec:   asn1-toolkit/docs/architecture.md
**                 asn1-toolkit/README.md
***************************************************************************/
#include "emit_internal.hpp"

namespace asn1 {
namespace codegen {

  /**
   *  Function    : tlv_ns
   *  Description : Computes tlv ns from (tlv).
   *  Parameters  : tlv — TlvKind tlv
   *  Returns     : const char*
   */
const char* Emitter::tlv_ns(TlvKind tlv) const {
  return tlv == TlvKind::Ber ? "asn1::ber" : (tlv == TlvKind::Cer ? "asn1::cer" : "asn1::der");
}

  /**
   *  Function    : tlv_fn
   *  Description : Computes tlv fn from (tlv).
   *  Parameters  : tlv — TlvKind tlv
   *  Returns     : const char*
   */
const char* Emitter::tlv_fn(TlvKind tlv) const {
  return tlv == TlvKind::Ber ? "ber" : (tlv == TlvKind::Cer ? "cer" : "der");
}

  /**
   *  Function    : ber_tag_expr
   *  Description : Builds and returns a string for ber tag expr.
   *  Parameters  : tag — const ir::Tag& tag; constructed — bool constructed
   *  Returns     : static std::string
   */
std::string Emitter::ber_tag_expr(const ir::Tag& tag, bool constructed) {
    std::ostringstream os;
    const char* fn = "asn1::ber::universal";
    switch (tag.cls) {
      case ir::TagClass::Application:
        fn = "asn1::ber::application";
        break;
      case ir::TagClass::Context:
        fn = "asn1::ber::context";
        break;
      case ir::TagClass::Private:
        fn = "asn1::ber::private_tag";
        break;
      case ir::TagClass::Universal:
      default:
        fn = "asn1::ber::universal";
        break;
    }
    os << fn << "(" << tag.number << "u, " << (constructed ? "true" : "false") << ")";
    return os.str();
  }

  /**
   *  Function    : universal_number_for
   *  Description : Computes universal number for from (t).
   *  Parameters  : t — const ir::Type& t
   *  Returns     : static std::uint64_t
   */
std::uint64_t Emitter::universal_number_for(const ir::Type& t) {
    switch (t.kind) {
      case ir::TypeKind::Boolean:
        return 1;
      case ir::TypeKind::Integer:
        return 2;
      case ir::TypeKind::BitString:
        return 3;
      case ir::TypeKind::OctetString:
        return 4;
      case ir::TypeKind::Null:
        return 5;
      case ir::TypeKind::ObjectIdentifier:
        return 6;
      case ir::TypeKind::InstanceOf:
        return 8;
      case ir::TypeKind::Real:
        return 9;
      case ir::TypeKind::Enumerated:
        return 10;
      case ir::TypeKind::RelativeOid:
        return 13;
      case ir::TypeKind::Sequence:
      case ir::TypeKind::SequenceOf:
      case ir::TypeKind::Choice:
        return 16;
      case ir::TypeKind::Set:
      case ir::TypeKind::SetOf:
        return 17;
      case ir::TypeKind::String:
        return 12;  // UTF8String
      default:
        return 16;
    }
  }

namespace {

unsigned tag_class_rank(ir::TagClass c) {
  switch (c) {
    case ir::TagClass::Application:
      return 1;
    case ir::TagClass::Context:
      return 2;
    case ir::TagClass::Private:
      return 3;
    case ir::TagClass::Universal:
    default:
      return 0;
  }
}

}  // namespace

Emitter::SetComponentKey Emitter::set_component_key(const ir::Field& f) const {
  const ir::Type& ft = resolve(model_, f.type);
  const ir::Type& frt = ft.kind == ir::TypeKind::Referenced && ft.referenced.resolved != ir::kInvalidType
                           ? resolve(model_, ft.referenced.resolved)
                           : ft;
  const bool untagged =
      f.tag.cls == ir::TagClass::Universal && f.tag.number == 0 && !f.tag.is_explicit;
  if (untagged && frt.kind == ir::TypeKind::Choice) {
    // X.690 9.3: ordered as though the choice carried its smallest tag,
    // including tags of untagged choice types nested within.
    SetComponentKey best;
    best.cls = 4;  // larger than any real class so the first alternative wins
    for (const ir::Field* alt : choice_all_fields(frt.choice)) {
      const SetComponentKey k = set_component_key(*alt);
      if (k < best) {
        best = k;
      }
    }
    return best;
  }
  SetComponentKey k;
  if (untagged) {
    k.cls = 0;
    k.number = universal_number_for(frt);
    k.constructed = type_is_constructed(frt);
  } else {
    k.cls = tag_class_rank(f.tag.cls);
    k.number = f.tag.number;
    k.constructed = type_is_constructed(frt) || f.tag.is_explicit;
  }
  return k;
}

std::vector<const ir::Field*> Emitter::set_fields_ordered(const ir::SequenceDesc& set) const {
  std::vector<const ir::Field*> fields;
  for (const ir::Field& f : set.root) {
    fields.push_back(&f);
  }
  for (const ir::Field& f : set.trailing_root) {
    fields.push_back(&f);
  }
  std::stable_sort(fields.begin(), fields.end(),
                   [this](const ir::Field* a, const ir::Field* b) {
                     return set_component_key(*a) < set_component_key(*b);
                   });
  return fields;
}

std::vector<std::string> Emitter::choice_dispatch_tags(const ir::ChoiceDesc& choice) const {
  std::vector<std::string> tags;
  for (const ir::Field* alt : choice_all_fields(choice)) {
    const ir::Type& at = resolve(model_, alt->type);
    const ir::Type& art =
        at.kind == ir::TypeKind::Referenced && at.referenced.resolved != ir::kInvalidType
            ? resolve(model_, at.referenced.resolved)
            : at;
    const bool alt_untagged =
        alt->tag.cls == ir::TagClass::Universal && alt->tag.number == 0 && !alt->tag.is_explicit;
    if (alt_untagged && art.kind == ir::TypeKind::Choice) {
      auto nested = choice_dispatch_tags(art.choice);
      tags.insert(tags.end(), nested.begin(), nested.end());
      continue;
    }
    ir::Tag t = alt->tag;
    if (alt_untagged) {
      // A natural-tagged alternative carries its universal type tag.
      t.cls = ir::TagClass::Universal;
      t.number = universal_number_for(art);
      t.is_explicit = false;
    }
    tags.push_back(ber_tag_expr(t, type_is_constructed(art) || t.is_explicit));
  }
  return tags;
}

  /**
   *  Function    : natural_tag_expr
   *  Description : Builds and returns a string for natural tag expr.
   *  Parameters  : t — const ir::Type& t
   *  Returns     : static std::string
   */
std::string Emitter::natural_tag_expr(const ir::Type& t) {
    ir::Tag tag;
    tag.cls = ir::TagClass::Universal;
    tag.number = universal_number_for(t);
    tag.is_explicit = false;
    return ber_tag_expr(tag, type_is_constructed(t));
  }

  /**
   *  Function    : emit_tlv_codec_fns
   *  Description : Performs emit tlv codec fns (definition).
   *  Parameters  : out — std::ostream& out; id — ir::TypeId id; tlv — TlvKind tlv
   *  Returns     : void
   */
void Emitter::emit_tlv_codec_fns(std::ostream& out, ir::TypeId id, TlvKind tlv) {
    // Canonical TLV: DER and CER share every canonicalization branch; CER
    // additionally selects the indefinite-length wrappers and string
    // fragmentation of the asn1::cer runtime.
    const bool der = tlv != TlvKind::Ber;
    const bool cer = tlv == TlvKind::Cer;
    const ir::Type& t = model_.arena.get(id);
    const std::string name = named_type_cpp(t);
    const char* suf = tlv_fn(tlv);
    std::ostream* body = begin_codec_fn(
        out, std::string("asn1::Result<void> encode_") + suf +
                "(asn1::ByteWriter& w, const " + name + "& value)");
    emit_tlv_encode_body(*body, t, "value", tlv, /*override_tag=*/nullptr, "  ");
    *body << "  return asn1::Result<void>::success();\n}\n\n";

    body = begin_codec_fn(out, std::string("asn1::Result<void> decode_") + suf +
                                   "(asn1::ByteReader& r, " + name + "& value)");
    emit_tlv_decode_body(*body, t, "value", tlv, /*override_tag=*/nullptr, "  ");
    *body << "  return asn1::Result<void>::success();\n}\n\n";
  }

void Emitter::emit_tlv_field_encode(std::ostream& out, const ir::Type& ft, const ir::Field* field,
                             const std::string& expr, TlvKind tlv, const std::string& ind) {
    // Canonical TLV: DER and CER share every canonicalization branch; CER
    // additionally selects the indefinite-length wrappers and string
    // fragmentation of the asn1::cer runtime.
    const bool der = tlv != TlvKind::Ber;
    const bool cer = tlv == TlvKind::Cer;
    const std::string NS = tlv_ns(tlv);
    if (field && (field->tag.cls != ir::TagClass::Universal || field->tag.number != 0 ||
                  field->tag.is_explicit)) {
      if (field->tag.is_explicit) {
        out << ind << "{\n";
        out << ind << "  asn1::ByteWriter inner;\n";
        out << ind << "  {\n";
        out << ind << "    asn1::ByteWriter& w = inner;\n";
        if (is_named(ft)) {
          out << ind << "    if (auto r = encode_" << tlv_fn(tlv) << "(w, " << expr
              << "); !r) return r;\n";
        } else {
          // EXPLICIT wraps the type's NATURAL encoding. Lowering stamps the
          // explicit component tag onto the inline type as well; when the
          // two coincide the inner value must use the natural universal tag
          // (X.690 8.5), not the explicit wrapper tag a second time.
          const bool same_tag = ft.tag.cls == field->tag.cls &&
                                ft.tag.number == field->tag.number &&
                                ft.tag.is_explicit;
          ir::Tag natural;
          natural.cls = ir::TagClass::Universal;
          natural.number = universal_number_for(ft);
          natural.is_explicit = false;
          emit_tlv_encode_body(out, ft, expr, tlv, same_tag ? &natural : nullptr,
                               ind + "    ");
        }
        out << ind << "  }\n";
        out << ind << "  " << NS << "::encode_constructed(w, "
            << ber_tag_expr(field->tag, true) << ", asn1::Span<const std::uint8_t>(inner.buffer().data(), "
            << "inner.buffer().size()));\n";
        out << ind << "}\n";
        return;
      }
      // IMPLICIT: replace natural tag
      if (is_named(ft) && type_is_constructed(ft)) {
        out << ind << "{\n";
        out << ind << "  asn1::ByteWriter inner;\n";
        out << ind << "  {\n";
        out << ind << "    asn1::ByteWriter& w = inner;\n";
        emit_tlv_encode_content(out, ft, expr, tlv, ind + "    ");
        out << ind << "  }\n";
        out << ind << "  " << NS << "::encode_constructed(w, "
            << ber_tag_expr(field->tag, true)
            << ", asn1::Span<const std::uint8_t>(inner.buffer().data(), inner.buffer().size()));\n";
        out << ind << "}\n";
        return;
      }
      emit_tlv_encode_body(out, ft, expr, tlv, &field->tag, ind);
      return;
    }
    if (is_named(ft)) {
      out << ind << "if (auto r = encode_" << tlv_fn(tlv) << "(w, " << expr
          << "); !r) return r;\n";
    } else {
      emit_tlv_encode_body(out, ft, expr, tlv, nullptr, ind);
    }
  }

void Emitter::emit_tlv_encode_content(std::ostream& out, const ir::Type& t, const std::string& expr,
                               TlvKind tlv, const std::string& ind) {
    // Canonical TLV: DER and CER share every canonicalization branch; CER
    // additionally selects the indefinite-length wrappers and string
    // fragmentation of the asn1::cer runtime.
    const bool der = tlv != TlvKind::Ber;
    const bool cer = tlv == TlvKind::Cer;
    // Encode without outermost constructed wrapper (for IMPLICIT SEQUENCE/SET).
    const ir::Type& rt = t.kind == ir::TypeKind::Referenced && t.referenced.resolved != ir::kInvalidType
                             ? resolve(model_, t.referenced.resolved)
                             : t;
    if (rt.kind == ir::TypeKind::Sequence) {
      emit_tlv_sequence_components(out, rt.sequence, expr, tlv, ind);
    } else if (rt.kind == ir::TypeKind::Set) {
      emit_tlv_sequence_components(out, rt.set, expr, tlv, ind);
    } else {
      emit_tlv_encode_body(out, rt, expr, tlv, nullptr, ind);
    }
  }

void Emitter::emit_tlv_sequence_field_encode(std::ostream& out, const ir::Field& f,
                                      const std::string& expr, TlvKind tlv,
                                      const std::string& ind) {
    // Canonical TLV: DER and CER share every canonicalization branch; CER
    // additionally selects the indefinite-length wrappers and string
    // fragmentation of the asn1::cer runtime.
    const bool der = tlv != TlvKind::Ber;
    const bool cer = tlv == TlvKind::Cer;
    const std::string fname = cpp_ident(f.name);
    const ir::Type& ft = resolve(model_, f.type);
    if (f.presence == ir::Presence::Optional) {
      out << ind << "if (" << expr << "." << fname << ") {\n";
      emit_tlv_field_encode(out, ft, &f, expr + "." + fname + ".value()", tlv, ind + "  ");
      out << ind << "}\n";
    } else if (f.presence == ir::Presence::Default && f.default_value) {
      const std::string def_val = default_val_cpp(f);
      if (!def_val.empty()) {
        out << ind << "if (" << expr << "." << fname << " != " << def_val << ") {\n";
        emit_tlv_field_encode(out, ft, &f, expr + "." + fname, tlv, ind + "  ");
        out << ind << "}\n";
      } else {
        emit_tlv_field_encode(out, ft, &f, expr + "." + fname, tlv, ind);
      }
    } else {
      emit_tlv_field_encode(out, ft, &f, expr + "." + fname, tlv, ind);
    }
  }

void Emitter::emit_tlv_sequence_components(std::ostream& out, const ir::SequenceDesc& seq,
                                    const std::string& expr, TlvKind tlv,
                                    const std::string& ind) {
    // Canonical TLV: DER and CER share every canonicalization branch; CER
    // additionally selects the indefinite-length wrappers and string
    // fragmentation of the asn1::cer runtime.
    const bool der = tlv != TlvKind::Ber;
    const bool cer = tlv == TlvKind::Cer;
    for (const ir::Field& f : seq.root) {
      emit_tlv_sequence_field_encode(out, f, expr, tlv, ind);
    }
    for (const ir::Field& f : seq.trailing_root) {
      emit_tlv_sequence_field_encode(out, f, expr, tlv, ind);
    }
    for (const auto& group : seq.extension_groups) {
      for (const ir::Field& f : group.fields) {
        emit_tlv_sequence_field_encode(out, f, expr, tlv, ind);
      }
    }
  }

void Emitter::emit_tlv_encode_body(std::ostream& out, const ir::Type& t, const std::string& expr,
                            TlvKind tlv, const ir::Tag* override_tag, const std::string& ind) {
    // Canonical TLV: DER and CER share every canonicalization branch; CER
    // additionally selects the indefinite-length wrappers and string
    // fragmentation of the asn1::cer runtime.
    const bool der = tlv != TlvKind::Ber;
    const bool cer = tlv == TlvKind::Cer;
    const ir::Type& rt = t.kind == ir::TypeKind::Referenced && t.referenced.resolved != ir::kInvalidType
                             ? resolve(model_, t.referenced.resolved)
                             : t;
    const std::string NS = tlv_ns(tlv);
    const std::string tag =
        override_tag ? ber_tag_expr(*override_tag, type_is_constructed(rt))
                     : (rt.tag.cls != ir::TagClass::Universal || rt.tag.number != 0
                            ? ber_tag_expr(rt.tag, type_is_constructed(rt))
                            : natural_tag_expr(rt));

    switch (rt.kind) {
      case ir::TypeKind::Boolean:
        out << ind << NS << "::encode_boolean(w, " << expr << ", " << tag << ");\n";
        break;
      case ir::TypeKind::Null:
        out << ind << NS << "::encode_null(w, " << tag << ");\n";
        break;
      case ir::TypeKind::Integer:
        if (integer_is_bigint(rt.integer)) {
          out << ind << NS << "::encode_integer(w, " << expr << ", " << tag << ");\n";
        } else {
          out << ind << NS << "::encode_integer(w, static_cast<std::int64_t>(" << expr
              << "), " << tag << ");\n";
        }
        break;
      case ir::TypeKind::OctetString:
        out << ind << NS << "::encode_octet_string(w, asn1::Span<const std::uint8_t>("
            << expr << ".data(), " << expr << ".size()), " << tag << ");\n";
        break;
      case ir::TypeKind::BitString:
        out << ind << "{\n";
        out << ind << "  const std::uint8_t unused = static_cast<std::uint8_t>(("
            << expr << ".bit_length % 8u == 0u) ? 0u : (8u - (" << expr
            << ".bit_length % 8u)));\n";
        out << ind << "  " << NS << "::encode_bit_string(w, asn1::Span<const std::uint8_t>("
            << expr << ".bits.data(), " << expr << ".bits.size()), unused, " << tag << ");\n";
        out << ind << "}\n";
        break;
      case ir::TypeKind::String:
        out << ind << NS << "::encode_utf8_string(w, " << expr << ", " << tag << ");\n";
        break;
      case ir::TypeKind::Enumerated:
        out << ind << NS << "::encode_enumerated(w, static_cast<std::int64_t>(" << expr
            << "), " << tag << ");\n";
        break;
      case ir::TypeKind::ObjectIdentifier:
        out << ind << NS << "::encode_object_identifier(w, asn1::Span<const std::uint64_t>("
            << expr << ".data(), " << expr << ".size()), " << tag << ");\n";
        break;
      case ir::TypeKind::RelativeOid:
        out << ind << NS << "::encode_relative_oid(w, asn1::Span<const std::uint64_t>("
            << expr << ".data(), " << expr << ".size()), " << tag << ");\n";
        break;
      case ir::TypeKind::Real:
        out << ind << NS << "::encode_real(w, " << expr << ", " << tag << ");\n";
        break;
      case ir::TypeKind::Sequence:
      case ir::TypeKind::Set: {
        out << ind << "{\n";
        if (der && rt.kind == ir::TypeKind::Set) {
          // X.690 9.3: components in canonical tag order. The order is
          // computed statically — an untagged CHOICE component is keyed by
          // the smallest tag in the choice (recursively), which the runtime
          // cannot see inside an encoded value — so the parts vector comes
          // out pre-ordered and the runtime must not re-sort.
          out << ind << "  std::vector<std::vector<std::uint8_t>> parts;\n";
          const auto ordered = set_fields_ordered(rt.set);
          auto emit_set_parts = [&](const std::vector<const ir::Field*>& fields) {
            for (const ir::Field* fp : fields) {
              const ir::Field& f = *fp;
              const std::string fname = cpp_ident(f.name);
              const ir::Type& ft = resolve(model_, f.type);
              if (f.presence == ir::Presence::Optional) {
                out << ind << "  if (" << expr << "." << fname << ") {\n";
                out << ind << "    asn1::ByteWriter ew;\n";
                out << ind << "    {\n";
                out << ind << "      asn1::ByteWriter& w = ew;\n";
                emit_tlv_field_encode(out, ft, &f, expr + "." + fname + ".value()", tlv,
                                      ind + "      ");
                out << ind << "    }\n";
                out << ind << "    parts.push_back(ew.take());\n";
                out << ind << "  }\n";
              } else if (f.presence == ir::Presence::Default) {
                if (f.default_value) {
                  std::string def_val = default_val_cpp(f);
                  if (!def_val.empty()) {
                    out << ind << "  if (" << expr << "." << fname << " != " << def_val << ") {\n";
                    out << ind << "    asn1::ByteWriter ew;\n";
                    out << ind << "    {\n";
                    out << ind << "      asn1::ByteWriter& w = ew;\n";
                    emit_tlv_field_encode(out, ft, &f, expr + "." + fname, tlv, ind + "      ");
                    out << ind << "    }\n";
                    out << ind << "    parts.push_back(ew.take());\n";
                    out << ind << "  }\n";
                    continue;
                  }
                }
                out << ind << "  {\n";
                out << ind << "    asn1::ByteWriter ew;\n";
                out << ind << "    {\n";
                out << ind << "      asn1::ByteWriter& w = ew;\n";
                emit_tlv_field_encode(out, ft, &f, expr + "." + fname, tlv, ind + "      ");
                out << ind << "    }\n";
                out << ind << "    parts.push_back(ew.take());\n";
                out << ind << "  }\n";
              } else {
                out << ind << "  {\n";
                out << ind << "    asn1::ByteWriter ew;\n";
                out << ind << "    {\n";
                out << ind << "      asn1::ByteWriter& w = ew;\n";
                emit_tlv_field_encode(out, ft, &f, expr + "." + fname, tlv, ind + "      ");
                out << ind << "    }\n";
                out << ind << "    parts.push_back(ew.take());\n";
                out << ind << "  }\n";
              }
            }
          };
          emit_set_parts(ordered);
          if (cer) {
            out << ind << "  " << NS << "::encode_set_ordered(w, std::move(parts), " << tag
                << ");\n";
          } else {
            out << ind << "  " << NS << "::encode_set(w, std::move(parts), " << tag << ");\n";
          }
        } else {
          out << ind << "  asn1::ByteWriter comps;\n";
          out << ind << "  {\n";
          out << ind << "    asn1::ByteWriter& w = comps;\n";
          if (rt.kind == ir::TypeKind::Sequence) {
            emit_tlv_sequence_components(out, rt.sequence, expr, tlv, ind + "    ");
          } else {
            emit_tlv_sequence_components(out, rt.set, expr, tlv, ind + "    ");
          }
          out << ind << "  }\n";
          if (der) {
            out << ind << "  " << NS << "::encode_sequence(w, asn1::Span<const std::uint8_t>("
                << "comps.buffer().data(), comps.buffer().size()), " << tag << ");\n";
          } else {
            out << ind << "  " << NS << "::encode_constructed(w, " << tag
                << ", asn1::Span<const std::uint8_t>(comps.buffer().data(), comps.buffer().size()));\n";
          }
        }
        out << ind << "}\n";
        break;
      }
      case ir::TypeKind::SequenceOf:
      case ir::TypeKind::SetOf: {
        const ir::TypeId elem_id =
            rt.kind == ir::TypeKind::SequenceOf ? rt.sequence_of.element : rt.set_of.element;
        const ir::Type& elem = resolve(model_, elem_id);
        // A named SEQUENCE OF lowers to an alias struct with a `.value`
        // member; an inline (anonymous) one is a bare vector.
        const std::string vec = is_named(rt) ? expr + ".value" : expr;
        out << ind << "{\n";
        if (der && rt.kind == ir::TypeKind::SetOf) {
          out << ind << "  std::vector<std::vector<std::uint8_t>> parts;\n";
          out << ind << "  parts.reserve(" << vec << ".size());\n";
          out << ind << "  for (const auto& elem : " << vec << ") {\n";
          out << ind << "    asn1::ByteWriter ew;\n";
          out << ind << "    {\n";
          out << ind << "      asn1::ByteWriter& w = ew;\n";
          if (is_named(elem)) {
            out << ind << "      if (auto r = encode_" << tlv_fn(tlv)
                << "(w, elem); !r) return r;\n";
          } else {
            emit_tlv_encode_body(out, elem, "elem", tlv, nullptr, ind + "      ");
          }
          out << ind << "    }\n";
          out << ind << "    parts.push_back(ew.take());\n";
          out << ind << "  }\n";
          out << ind << "  " << NS << "::encode_set(w, std::move(parts), " << tag << ");\n";
        } else {
          out << ind << "  asn1::ByteWriter comps;\n";
          out << ind << "  for (const auto& elem : " << vec << ") {\n";
          out << ind << "    asn1::ByteWriter& w = comps;\n";
          if (is_named(elem)) {
            out << ind << "    if (auto r = encode_" << tlv_fn(tlv)
                << "(w, elem); !r) return r;\n";
          } else {
            emit_tlv_encode_body(out, elem, "elem", tlv, nullptr, ind + "    ");
          }
          out << ind << "  }\n";
          if (der) {
            out << ind << "  " << NS << "::encode_sequence(w, asn1::Span<const std::uint8_t>("
                << "comps.buffer().data(), comps.buffer().size()), " << tag << ");\n";
          } else {
            out << ind << "  " << NS << "::encode_constructed(w, " << tag
                << ", asn1::Span<const std::uint8_t>(comps.buffer().data(), comps.buffer().size()));\n";
          }
        }
        out << ind << "}\n";
        break;
      }
      case ir::TypeKind::Choice: {
        const auto all = choice_all_fields(rt.choice);
        out << ind << "{\n";
        out << ind << "  const std::size_t index = " << expr << ".alt.index();\n";
        out << ind << "  switch (index) {\n";
        for (std::size_t i = 0; i < all.size(); ++i) {
          const ir::Field& f = *all[i];
          const ir::Type& ft = resolve(model_, f.type);
          out << ind << "    case " << i << ": {\n";
          // Unique local: a nested CHOICE alternative re-enters this block
          // with expr = "<outer alt local>.value", and a duplicate `alt`
          // shadows the initializer it is deduced from.
          const std::string alt_local = "alt" + std::to_string(temp_counter_++);
          out << ind << "      const auto& " << alt_local << " = std::get<" << i << ">(" << expr
              << ".alt);\n";
          emit_tlv_field_encode(out, ft, &f, alt_local + ".value", tlv, ind + "      ");
          out << ind << "      break;\n";
          out << ind << "    }\n";
        }
        out << ind << "    default:\n";
        out << ind << "      return asn1::make_error(asn1::Error::Code::InvalidArgument, "
               "w.size(), \"CHOICE index out of range\");\n";
        out << ind << "  }\n";
        out << ind << "}\n";
        break;
      }
      case ir::TypeKind::InstanceOf:
        out << ind << "{\n";
        out << ind << "  asn1::ByteWriter comps;\n";
        out << ind << "  " << NS << "::encode_object_identifier(comps, asn1::Span<const std::uint64_t>("
            << expr << ".type_id.data(), " << expr << ".type_id.size()));\n";
        out << ind << "  " << NS << "::encode_octet_string(comps, asn1::Span<const std::uint8_t>("
            << expr << ".value.data(), " << expr << ".value.size()));\n";
        out << ind << "  " << NS << "::encode_constructed(w, " << tag
            << ", asn1::Span<const std::uint8_t>(comps.buffer().data(), comps.buffer().size()));\n";
        out << ind << "}\n";
        break;
      case ir::TypeKind::Referenced:
        if (rt.referenced.resolved != ir::kInvalidType) {
          emit_tlv_encode_body(out, resolve(model_, rt.referenced.resolved), expr, tlv,
                               override_tag, ind);
        }
        break;
      default:
        out << ind << "return asn1::make_error(asn1::Error::Code::Unsupported, w.size(), "
               "\"TLV encode not generated for this type\");\n";
        break;
    }
  }

void Emitter::emit_tlv_decode_body(std::ostream& out, const ir::Type& t, const std::string& expr,
                            TlvKind tlv, const ir::Tag* override_tag, const std::string& ind) {
    // Canonical TLV: DER and CER share every canonicalization branch; CER
    // additionally selects the indefinite-length wrappers and string
    // fragmentation of the asn1::cer runtime.
    const bool der = tlv != TlvKind::Ber;
    const bool cer = tlv == TlvKind::Cer;
    const ir::Type& rt = t.kind == ir::TypeKind::Referenced && t.referenced.resolved != ir::kInvalidType
                             ? resolve(model_, t.referenced.resolved)
                             : t;
    const std::string NS = tlv_ns(tlv);
    const std::string tag =
        override_tag ? ber_tag_expr(*override_tag, type_is_constructed(rt))
                     : (rt.tag.cls != ir::TagClass::Universal || rt.tag.number != 0
                            ? ber_tag_expr(rt.tag, type_is_constructed(rt))
                            : natural_tag_expr(rt));

    switch (rt.kind) {
      case ir::TypeKind::Boolean:
        out << ind << "{\n"
            << ind << "  auto tmp = " << NS << "::decode_boolean(r, " << tag << ");\n"
            << ind << "  if (!tmp) return tmp.error();\n"
            << ind << "  " << expr << " = tmp.value();\n"
            << ind << "}\n";
        break;
      case ir::TypeKind::Null:
        out << ind << "{\n"
            << ind << "  auto tmp = " << NS << "::decode_null(r, " << tag << ");\n"
            << ind << "  if (!tmp) return tmp.error();\n"
            << ind << "}\n";
        break;
      case ir::TypeKind::Integer:
        out << ind << "{\n";
        if (integer_is_bigint(rt.integer)) {
          out << ind << "  auto tmp = " << NS << "::decode_big_integer(r, " << tag << ");\n"
              << ind << "  if (!tmp) return tmp.error();\n"
              << ind << "  " << expr << " = std::move(tmp.value());\n";
        } else {
          out << ind << "  auto tmp = " << NS << "::decode_integer(r, " << tag << ");\n"
              << ind << "  if (!tmp) return tmp.error();\n"
              << ind << "  " << expr << " = static_cast<" << integer_host_type(rt.integer)
              << ">(tmp.value());\n";
        }
        out << ind << "}\n";
        break;
      case ir::TypeKind::OctetString:
        out << ind << "{\n"
            << ind << "  auto tmp = " << NS << "::decode_octet_string(r, " << tag << ");\n"
            << ind << "  if (!tmp) return tmp.error();\n"
            << ind << "  " << expr << " = std::move(tmp.value());\n"
            << ind << "}\n";
        break;
      case ir::TypeKind::BitString:
        out << ind << "{\n"
            << ind << "  auto tmp = " << NS << "::decode_bit_string(r, " << tag << ");\n"
            << ind << "  if (!tmp) return tmp.error();\n"
            << ind << "  " << expr << " = std::move(tmp.value());\n"
            << ind << "}\n";
        break;
      case ir::TypeKind::String:
        out << ind << "{\n"
            << ind << "  auto tmp = " << NS << "::decode_utf8_string(r, " << tag << ");\n"
            << ind << "  if (!tmp) return tmp.error();\n"
            << ind << "  " << expr << " = std::move(tmp.value());\n"
            << ind << "}\n";
        break;
      case ir::TypeKind::Enumerated:
        out << ind << "{\n"
            << ind << "  auto tmp = " << NS << "::decode_enumerated(r, " << tag << ");\n"
            << ind << "  if (!tmp) return tmp.error();\n"
            << ind << "  " << expr << " = static_cast<std::remove_cv_t<std::remove_reference_t<decltype(" << expr << ")>>>(tmp.value());\n"
            << ind << "}\n";
        break;
      case ir::TypeKind::ObjectIdentifier:
        out << ind << "{\n"
            << ind << "  auto tmp = " << NS << "::decode_object_identifier(r, " << tag << ");\n"
            << ind << "  if (!tmp) return tmp.error();\n"
            << ind << "  " << expr << " = std::move(tmp.value());\n"
            << ind << "}\n";
        break;
      case ir::TypeKind::RelativeOid:
        out << ind << "{\n"
            << ind << "  auto tmp = " << NS << "::decode_relative_oid(r, " << tag << ");\n"
            << ind << "  if (!tmp) return tmp.error();\n"
            << ind << "  " << expr << " = std::move(tmp.value());\n"
            << ind << "}\n";
        break;
      case ir::TypeKind::Real:
        out << ind << "{\n"
            << ind << "  auto tmp = " << NS << "::decode_real(r, " << tag << ");\n"
            << ind << "  if (!tmp) return tmp.error();\n"
            << ind << "  " << expr << " = tmp.value();\n"
            << ind << "}\n";
        break;
      case ir::TypeKind::Sequence:
      case ir::TypeKind::Set: {
        out << ind << "{\n";
        if (der && rt.kind == ir::TypeKind::Set) {
          out << ind << "  auto content = " << NS << "::decode_set(r, " << tag << ");\n";
        } else if (der) {
          out << ind << "  auto content = " << NS << "::decode_sequence(r, " << tag << ");\n";
        } else {
          out << ind << "  auto content = " << NS << "::decode_constructed(r, " << tag << ");\n";
        }
        out << ind << "  if (!content) return content.error();\n";
        out << ind << "  asn1::ByteReader cr(content.value());\n";
        out << ind << "  {\n";
        out << ind << "    asn1::ByteReader& r = cr;\n";
        if (rt.kind == ir::TypeKind::Set) {
          // SET: components arrive in canonical tag order (DER/CER) or in
          // any order (BER); dispatch on the peeked tag instead of assuming
          // declaration order (X.690 9.3/10.3 define the wire order, not
          // the declaration order).
          const auto ordered = set_fields_ordered(rt.set);
          // DEFAULT components absent from the encoding take their default
          // (X.690 11.5).
          for (const ir::Field* fp : ordered) {
            const ir::Field& f = *fp;
            if (f.presence == ir::Presence::Default && f.default_value) {
              const std::string def_expr = default_val_cpp(f);
              if (!def_expr.empty()) {
                out << ind << "    " << expr << "." << cpp_ident(f.name) << " = " << def_expr
                    << ";\n";
              }
            }
          }
          // Mandatory components must be present.
          for (const ir::Field* fp : ordered) {
            const ir::Field& f = *fp;
            if (f.presence == ir::Presence::Mandatory) {
              out << ind << "    bool seen_" << cpp_ident(f.name) << " = false;\n";
            }
          }
          out << ind << "    while (r.remaining() > 0) {\n";
          out << ind << "      asn1::ByteReader peek_r(r.remaining_span());\n";
          out << ind << "      auto hdr = asn1::ber::decode_tlv_header(peek_r);\n";
          out << ind << "      if (!hdr) return hdr.error();\n";
          for (const ir::Field* fp : ordered) {
            const ir::Field& f = *fp;
            const ir::Type& ft = resolve(model_, f.type);
            const ir::Type& frt =
                ft.kind == ir::TypeKind::Referenced && ft.referenced.resolved != ir::kInvalidType
                    ? resolve(model_, ft.referenced.resolved)
                    : ft;
            const bool untagged_choice =
                f.tag.cls == ir::TagClass::Universal && f.tag.number == 0 &&
                !f.tag.is_explicit && frt.kind == ir::TypeKind::Choice;
            if (untagged_choice) {
              // An untagged CHOICE SET component carries no single tag:
              // any of its alternatives' tags identifies it on the wire
              // (X.680 8.6 keeps SET component tags distinct). Emitting
              // the field placeholder tag (universal 0) here would never
              // match a real encoding.
              const auto dtags = choice_dispatch_tags(frt.choice);
              out << ind << "      if (";
              if (dtags.empty()) {
                out << "false";  // empty CHOICE: matches nothing
              } else {
                for (std::size_t i = 0; i < dtags.size(); ++i) {
                  if (i != 0) {
                    out << " || ";
                  }
                  out << "hdr.value().tag == " << dtags[i];
                }
              }
              out << ") {\n";
            } else {
              const std::string expect =
                  ber_tag_expr(f.tag, type_is_constructed(ft) || f.tag.is_explicit);
              out << ind << "      if (hdr.value().tag == " << expect << ") {\n";
            }
            if (f.presence == ir::Presence::Mandatory) {
              out << ind << "        seen_" << cpp_ident(f.name) << " = true;\n";
            }
            emit_tlv_field_decode_value(out, ft, &f, expr + "." + cpp_ident(f.name), tlv,
                                        ind + "        ");
            out << ind << "      } else ";
          }
          out << ind << "      {\n";
          out << ind << "        return asn1::make_error(asn1::Error::Code::InvalidArgument, "
                 "r.offset(), \"unknown SET component tag\");\n";
          out << ind << "      }\n";
          out << ind << "    }\n";
          for (const ir::Field* fp : ordered) {
            const ir::Field& f = *fp;
            if (f.presence == ir::Presence::Mandatory) {
              out << ind << "    if (!seen_" << cpp_ident(f.name)
                  << ") return asn1::make_error(asn1::Error::Code::InvalidArgument, r.offset(), "
                     "\"missing mandatory SET component\");\n";
            }
          }
        } else {
          const auto& fields = rt.sequence.root;
          const auto& trailing = rt.sequence.trailing_root;
          for (const ir::Field& f : fields) {
            emit_tlv_field_decode_stmt(out, f, expr, tlv, ind + "    ");
          }
          for (const ir::Field& f : trailing) {
            emit_tlv_field_decode_stmt(out, f, expr, tlv, ind + "    ");
          }
          for (const auto& group : rt.sequence.extension_groups) {
            for (const ir::Field& f : group.fields) {
              emit_tlv_field_decode_stmt(out, f, expr, tlv, ind + "    ");
            }
          }
        }
        out << ind << "  }\n";
        out << ind << "}\n";
        break;
      }
      case ir::TypeKind::SequenceOf:
      case ir::TypeKind::SetOf: {
        const ir::TypeId elem_id =
            rt.kind == ir::TypeKind::SequenceOf ? rt.sequence_of.element : rt.set_of.element;
        const ir::Type& elem = resolve(model_, elem_id);
        // A named SEQUENCE OF lowers to an alias struct with a `.value`
        // member; an inline (anonymous) one is a bare vector.
        const std::string vec = is_named(rt) ? expr + ".value" : expr;
        out << ind << "{\n";
        if (der && rt.kind == ir::TypeKind::SetOf) {
          out << ind << "  auto content = " << NS << "::decode_set(r, " << tag << ");\n";
        } else if (der) {
          out << ind << "  auto content = " << NS << "::decode_sequence(r, " << tag << ");\n";
        } else {
          out << ind << "  auto content = " << NS << "::decode_constructed(r, " << tag << ");\n";
        }
        out << ind << "  if (!content) return content.error();\n";
        out << ind << "  asn1::ByteReader cr(content.value());\n";
        out << ind << "  " << vec << ".clear();\n";
        out << ind << "  while (cr.remaining() > 0) {\n";
        out << ind << "    " << type_cpp(elem_id) << " elem{};\n";
        out << ind << "    {\n";
        out << ind << "      asn1::ByteReader& r = cr;\n";
        if (is_named(elem)) {
          out << ind << "      if (auto dr = decode_" << tlv_fn(tlv)
              << "(r, elem); !dr) return dr;\n";
        } else {
          emit_tlv_decode_body(out, elem, "elem", tlv, nullptr, ind + "      ");
        }
        out << ind << "    }\n";
        out << ind << "    " << vec << ".push_back(std::move(elem));\n";
        out << ind << "  }\n";
        out << ind << "}\n";
        break;
      }
      case ir::TypeKind::Choice: {
        const auto all = choice_all_fields(rt.choice);
        const std::string choice_ty = is_named(rt) ? named_ref(rt) : std::string{};
        out << ind << "{\n";
        if (cer) {
          // CER: peek the tag without consuming, then decode the matched
          // alternative's complete TLV in place (header included). A
          // constructed alternative arrives in the indefinite form
          // (X.690 9.1), so the definite-content buffer of the BER/DER
          // shape cannot be used here.
          out << ind << "  auto hdr_tag = asn1::ber::peek_tlv_tag(r);\n";
          out << ind << "  if (!hdr_tag) return hdr_tag.error();\n";
        } else {
          out << ind << "  auto hdr = asn1::ber::decode_tlv_header(r);\n";
          out << ind << "  if (!hdr) return hdr.error();\n";
          out << ind << "  if (hdr.value().length.indefinite) {\n";
          out << ind << "    return asn1::make_error(asn1::Error::Code::Unsupported, r.offset(), "
                 "\"indefinite CHOICE not generated\");\n";
          out << ind << "  }\n";
          out << ind << "  auto content = r.read(hdr.value().length.value);\n";
          out << ind << "  if (!content) return content.error();\n";
          out << ind << "  const auto hdr_tag = hdr.value().tag;\n";
        }
        for (std::size_t i = 0; i < all.size(); ++i) {
          const ir::Field& f = *all[i];
          const ir::Type& ft = resolve(model_, f.type);
          const std::string expect =
              ber_tag_expr(f.tag, type_is_constructed(ft) || f.tag.is_explicit);
          // cer: hdr_tag is the non-consuming peek result (Result<Tag>).
          out << ind << "  if (" << (cer ? "hdr_tag.value()" : "hdr_tag") << " == " << expect
              << ") {\n";
          // Unique local: a nested CHOICE alternative re-enters this
          // decode block, and a duplicate `alt` would shadow the
          // assignment target at the bottom of the block.
          const std::string alt_dec = "alt" + std::to_string(temp_counter_++);
          if (!choice_ty.empty()) {
            out << ind << "    " << choice_ty << "::" << cpp_ident(f.name) << "_ " << alt_dec << "{};\n";
          } else {
            out << ind << "    " << cpp_ident(f.name) << "_ " << alt_dec << "{};\n";
          }
          const bool tag_override =
              f.tag.cls != ir::TagClass::Universal || f.tag.number != 0;
          if (cer) {
            if (f.tag.is_explicit) {
              // EXPLICIT wrapper: the wrapper itself is indefinite under
              // CER; the inner value keeps its own encoding.
              out << ind << "    auto wcontent = " << NS << "::decode_constructed(r, "
                  << expect << ");\n";
              out << ind << "    if (!wcontent) return wcontent.error();\n";
              out << ind << "    asn1::ByteReader cr(wcontent.value());\n";
              out << ind << "    {\n";
              out << ind << "      asn1::ByteReader& r = cr;\n";
              if (is_named(ft)) {
                out << ind << "      if (auto dr = decode_" << tlv_fn(tlv)
                    << "(r, " + alt_dec + ".value); !dr) return dr;\n";
              } else {
                emit_tlv_decode_body(out, ft, alt_dec + ".value", tlv, nullptr, ind + "      ");
              }
              out << ind << "    }\n";
            } else if (tag_override || !is_named(ft)) {
              // Tagged (IMPLICIT) or inline alternative: decode in place with
              // the override tag — the encoded tag is the override, not the
              // type's natural one.
              emit_tlv_decode_body(out, ft, alt_dec + ".value", tlv, &f.tag, ind + "    ");
            } else {
              // Untagged named alternative: the encoding carries the natural
              // tag, so the out-of-line decode applies directly.
              out << ind << "    if (auto dr = decode_" << tlv_fn(tlv)
                  << "(r, " + alt_dec + ".value); !dr) return dr;\n";
            }
          } else if (f.tag.is_explicit) {
            out << ind << "    asn1::ByteReader cr(content.value());\n";
            out << ind << "    {\n";
            out << ind << "      asn1::ByteReader& r = cr;\n";
            if (is_named(ft)) {
              out << ind << "      if (auto dr = decode_" << tlv_fn(tlv)
                  << "(r, " + alt_dec + ".value); !dr) return dr;\n";
            } else {
              emit_tlv_decode_body(out, ft, alt_dec + ".value", tlv, nullptr, ind + "      ");
            }
            out << ind << "    }\n";
          } else {
            out << ind << "    asn1::ByteWriter tmpw;\n";
            out << ind << "    asn1::ber::encode_tlv(tmpw, " << natural_tag_expr(ft)
                << ", content.value());\n";
            out << ind << "    asn1::ByteReader rr(tmpw.buffer());\n";
            out << ind << "    {\n";
            out << ind << "      asn1::ByteReader& r = rr;\n";
            if (is_named(ft)) {
              out << ind << "      if (auto dr = decode_" << tlv_fn(tlv)
                  << "(r, " + alt_dec + ".value); !dr) return dr;\n";
            } else {
              emit_tlv_decode_body(out, ft, alt_dec + ".value", tlv, nullptr, ind + "      ");
            }
            out << ind << "    }\n";
          }
          out << ind << "    " << expr << ".alt = std::move(" + alt_dec + ");\n";
          out << ind << "  } else ";
        }
        out << "{\n";
        out << ind << "    return asn1::make_error(asn1::Error::Code::InvalidArgument, r.offset(), "
               "\"unknown CHOICE tag\");\n";
        out << ind << "  }\n";
        out << ind << "}\n";
        break;
      }
      case ir::TypeKind::InstanceOf:
        out << ind << "{\n";
        out << ind << "  auto content = " << NS << "::decode_constructed(r, " << tag << ");\n";
        out << ind << "  if (!content) return content.error();\n";
        out << ind << "  asn1::ByteReader cr(content.value());\n";
        out << ind << "  auto tid = " << NS << "::decode_object_identifier(cr);\n";
        out << ind << "  if (!tid) return tid.error();\n";
        out << ind << "  " << expr << ".type_id = std::move(tid.value());\n";
        out << ind << "  auto val = " << NS << "::decode_octet_string(cr);\n";
        out << ind << "  if (!val) return val.error();\n";
        out << ind << "  " << expr << ".value = std::move(val.value());\n";
        out << ind << "}\n";
        break;
      case ir::TypeKind::Referenced:
        if (rt.referenced.resolved != ir::kInvalidType) {
          emit_tlv_decode_body(out, resolve(model_, rt.referenced.resolved), expr, tlv,
                               override_tag, ind);
        }
        break;
      default:
        out << ind << "return asn1::make_error(asn1::Error::Code::Unsupported, r.offset(), "
               "\"TLV decode not generated for this type\");\n";
        break;
    }
  }

void Emitter::emit_tlv_field_decode_stmt(std::ostream& out, const ir::Field& f, const std::string& expr,
                                  TlvKind tlv, const std::string& ind) {
    // Canonical TLV: DER and CER share every canonicalization branch; CER
    // additionally selects the indefinite-length wrappers and string
    // fragmentation of the asn1::cer runtime.
    const bool der = tlv != TlvKind::Ber;
    const bool cer = tlv == TlvKind::Cer;
    const std::string fname = cpp_ident(f.name);
    const ir::Type& ft = resolve(model_, f.type);
    if (f.presence == ir::Presence::Default) {
      const std::string expect =
          ber_tag_expr(f.tag, type_is_constructed(ft) || f.tag.is_explicit);
      out << ind << "if (r.remaining() > 0) {\n";
      out << ind << "  asn1::ByteReader peek_r(r.remaining_span());\n";
      out << ind << "  auto hdr = asn1::ber::decode_tlv_header(peek_r);\n";
      out << ind << "  if (hdr && hdr.value().tag == " << expect << ") {\n";
      emit_tlv_field_decode_value(out, ft, &f, expr + "." + fname, tlv, ind + "    ");
      out << ind << "  } else {\n";
      if (f.default_value) {
        const std::string def_expr = default_val_cpp(f);
        if (!def_expr.empty()) {
          out << ind << "    " << expr << "." << fname << " = " << def_expr << ";\n";
        }
      }
      out << ind << "  }\n";
      out << ind << "} else {\n";
      if (f.default_value) {
        const std::string def_expr = default_val_cpp(f);
        if (!def_expr.empty()) {
          out << ind << "  " << expr << "." << fname << " = " << def_expr << ";\n";
        }
      }
      out << ind << "}\n";
      return;
    }
    if (f.presence == ir::Presence::Optional) {
      const std::string expect =
          ber_tag_expr(f.tag, type_is_constructed(ft) || f.tag.is_explicit);
      out << ind << "if (r.remaining() == 0) {\n";
      out << ind << "  " << expr << "." << fname << " = std::nullopt;\n";
      out << ind << "} else {\n";
      out << ind << "  asn1::ByteReader peek_r(r.remaining_span());\n";
      out << ind << "  auto hdr = asn1::ber::decode_tlv_header(peek_r);\n";
      out << ind << "  if (!hdr) return hdr.error();\n";
      out << ind << "  if (hdr.value().tag == " << expect << ") {\n";
      // Unique temp name: a nested optional SEQUENCE decodes into its own
      // `field` temp while `expr` (the assignment target) may itself be an
      // enclosing `field` temp — a duplicate name shadows it and every
      // nested optional assignment silently targets the wrong object.
      const std::string tmp = "field" + std::to_string(temp_counter_++);
      out << ind << "    " << type_cpp(f.type) << " " << tmp << "{};\n";
      emit_tlv_field_decode_value(out, ft, &f, tmp, tlv, ind + "    ");
      out << ind << "    " << expr << "." << fname << " = std::move(" << tmp << ");\n";
      out << ind << "  } else {\n";
      out << ind << "    " << expr << "." << fname << " = std::nullopt;\n";
      out << ind << "  }\n";
      out << ind << "}\n";
    } else {
      emit_tlv_field_decode_value(out, ft, &f, expr + "." + fname, tlv, ind);
    }
  }

void Emitter::emit_tlv_field_decode_value(std::ostream& out, const ir::Type& ft, const ir::Field* field,
                                   const std::string& expr, TlvKind tlv, const std::string& ind) {
    // Canonical TLV: DER and CER share every canonicalization branch; CER
    // additionally selects the indefinite-length wrappers and string
    // fragmentation of the asn1::cer runtime.
    const bool der = tlv != TlvKind::Ber;
    const bool cer = tlv == TlvKind::Cer;
    const std::string NS = tlv_ns(tlv);
    if (field && field->tag.is_explicit) {
      out << ind << "{\n";
      out << ind << "  auto content = " << NS << "::decode_constructed(r, "
          << ber_tag_expr(field->tag, true) << ");\n";
      out << ind << "  if (!content) return content.error();\n";
      out << ind << "  asn1::ByteReader cr(content.value());\n";
      out << ind << "  {\n";
      out << ind << "    asn1::ByteReader& r = cr;\n";
      if (is_named(ft)) {
        out << ind << "    if (auto dr = decode_" << tlv_fn(tlv) << "(r, " << expr
            << "); !dr) return dr;\n";
      } else {
        // Mirror of the encode side: when the inline type carries the same
        // explicit tag as the component, the inner value decodes from the
        // natural universal tag (X.690 8.5).
        const bool same_tag = ft.tag.cls == field->tag.cls &&
                              ft.tag.number == field->tag.number &&
                              ft.tag.is_explicit;
        ir::Tag natural;
        natural.cls = ir::TagClass::Universal;
        natural.number = universal_number_for(ft);
        natural.is_explicit = false;
        emit_tlv_decode_body(out, ft, expr, tlv, same_tag ? &natural : nullptr,
                             ind + "    ");
      }
      out << ind << "  }\n";
      out << ind << "}\n";
      return;
    }
    if (field && field->tag.cls != ir::TagClass::Universal) {
      emit_tlv_decode_body(out, ft, expr, tlv, &field->tag, ind);
      return;
    }
    if (is_named(ft)) {
      out << ind << "if (auto dr = decode_" << tlv_fn(tlv) << "(r, " << expr
          << "); !dr) return dr;\n";
    } else {
      emit_tlv_decode_body(out, ft, expr, tlv, nullptr, ind);
    }
  }

}  // namespace codegen
}  // namespace asn1
