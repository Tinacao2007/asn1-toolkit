/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/src/codegen/emit_xer.cpp
**   $Version: 0.1
**   $Date:   2026-10-14
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   XER family emitter (XER/E-XER/CXER variants): XML element
   encode/decode body emission.
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
   *  Function    : xer_use_exer
   *  Description : Returns a boolean result from none.
   *  Parameters  : none
   *  Returns     : bool
   */
bool Emitter::xer_use_exer() const {
    return (options_.codec & CodecKind::Exer) != CodecKind::None;
  }

  /**
   *  Function    : xer_use_cxer
   *  Description : Returns a boolean result from none.
   *  Parameters  : none
   *  Returns     : bool
   */
bool Emitter::xer_use_cxer() const {
    return (options_.codec & CodecKind::Cxer) != CodecKind::None;
  }

  /**
   *  Function    : xml_ns
   *  Description : Selects the XML runtime namespace (cxer variant vs xer).
   *  Parameters  : none
   *  Returns     : const char*
   */
const char* Emitter::xml_ns() const { return xer_use_cxer() ? "asn1::cxer" : "asn1::xer"; }


  /**
   *  Function    : xml_fn_suffix
   *  Description : Selects the XML codec function suffix (cxer vs xer).
   *  Parameters  : none
   *  Returns     : const char*
   */
const char* Emitter::xml_fn_suffix() const { return xer_use_cxer() ? "cxer" : "xer"; }


std::string Emitter::transform_xml_name(const std::string& name, ir::Field::JerNameForm form,
                                        const std::string& literal) {
    switch (form) {
      case ir::Field::JerNameForm::Literal:
        return literal;
      case ir::Field::JerNameForm::Capitalized: {
        if (name.empty()) {
          return name;
        }
        std::string out = name;
        if (out[0] >= 'a' && out[0] <= 'z') {
          out[0] = static_cast<char>(out[0] - 'a' + 'A');
        }
        return out;
      }
      case ir::Field::JerNameForm::Uppercased: {
        std::string out = name;
        for (char& c : out) {
          if (c >= 'a' && c <= 'z') {
            c = static_cast<char>(c - 'a' + 'A');
          }
        }
        return out;
      }
      case ir::Field::JerNameForm::Lowercased: {
        std::string out = name;
        for (char& c : out) {
          if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
          }
        }
        return out;
      }
      case ir::Field::JerNameForm::AsIs:
        break;
    }
    return name;
  }

  /**
   *  Function    : xer_field_name
   *  Description : Builds and returns a string for xer field name.
   *  Parameters  : f — const ir::Field& f
   *  Returns     : std::string
   */
std::string Emitter::xer_field_name(const ir::Field& f) const {
    return transform_xml_name(f.name, f.jer_name_form, f.jer_name_literal);
  }

void Emitter::emit_xer_enum_id_switch(std::ostream& out, const ir::Type& rt, const std::string& expr,
                               const std::string& ind) {
    out << ind << "std::string xer_enum_id;\n";
    out << ind << "switch (static_cast<std::int64_t>(" << expr << ")) {\n";
    for (const auto& nn : rt.enumerated.root) {
      out << ind << "  case " << (nn.value.as_i64 ? std::to_string(*nn.value.as_i64) : "0")
          << ": xer_enum_id = \"" << nn.name << "\"; break;\n";
    }
    for (const auto& nn : rt.enumerated.extensions) {
      out << ind << "  case " << (nn.value.as_i64 ? std::to_string(*nn.value.as_i64) : "0")
          << ": xer_enum_id = \"" << nn.name << "\"; break;\n";
    }
    out << ind << "  default: xer_enum_id = \""
        << (rt.enumerated.root.empty() ? "?" : rt.enumerated.root.front().name)
        << "\"; break;\n";
    out << ind << "}\n";
  }

void Emitter::emit_xer_field_encode_expr(std::ostream& out, const ir::Type& ft,
                                  const std::string& expr, const std::string& ind) {
    if (is_named(ft)) {
      out << "encode_" << xml_fn_suffix() << "(" << expr << ");\n";
    } else {
      out << "([&]() -> asn1::Result<" << xml_ns() << "::Element> {\n";
      emit_xer_encode_body(out, ft, expr, ind);
      out << ind << "})();\n";
    }
  }

void Emitter::emit_xer_field_decode_stmt(std::ostream& out, const ir::Type& ft,
                                 const std::string& lhs_expr, const std::string& el_expr,
                                 const std::string& ind) {
    if (is_named(ft)) {
      out << ind << "{\n";
      out << ind << "  if (auto dr = decode_" << xml_fn_suffix() << "(" << el_expr << ", "
          << lhs_expr << "); !dr) return dr.error();\n";
      out << ind << "}\n";
      return;
    }
    emit_xer_decode_body(out, ft, lhs_expr, el_expr, ind);
  }

  /**
   *  Function    : emit_xer_codec_fns
   *  Description : Performs emit xer codec fns (definition).
   *  Parameters  : out — std::ostream& out; id — ir::TypeId id
   *  Returns     : void
   */
void Emitter::emit_xer_codec_fns(std::ostream& out, ir::TypeId id) {
    const ir::Type& t = model_.arena.get(id);
    const std::string name = named_type_cpp(t);
    const char* suf = xml_fn_suffix();
    std::ostream* body = begin_codec_fn(
        out, std::string("asn1::Result<") + xml_ns() + "::Element> encode_" + suf +
                "(const " + name + "& value)");
    emit_xer_encode_body(*body, t, "value", "  ");
    *body << "}\n\n";

    body = begin_codec_fn(out, std::string("asn1::Result<void> decode_") + suf +
                                   "(const " + xml_ns() + "::Element& el, " + name +
                                   "& value)");
    emit_xer_decode_body(*body, t, "value", "el", "  ");
    *body << "  return asn1::Result<void>::success();\n}\n\n";
  }

void Emitter::emit_xer_encode_body(std::ostream& out, const ir::Type& t, const std::string& expr,
                            const std::string& ind) {
    const ir::Type& rt = t.kind == ir::TypeKind::Referenced && t.referenced.resolved != ir::kInvalidType
                             ? resolve(model_, t.referenced.resolved)
                             : t;
    const bool exer_mode = xer_use_exer();
    const std::string el_name = rt.name.empty() ? "Type" : rt.name;
    switch (rt.kind) {
      case ir::TypeKind::Boolean:
        if (exer_mode && rt.exer.text) {
          out << ind << "return asn1::Result<" << xml_ns() << "::Element>::success("
              << "asn1::exer::encode_boolean_text(\"" << el_name << "\", " << expr << "));\n";
        } else {
          out << ind << "return asn1::Result<" << xml_ns() << "::Element>::success("
              << "" << xml_ns() << "::encode_boolean(\"" << el_name << "\", " << expr << "));\n";
        }
        break;
      case ir::TypeKind::Null:
        out << ind << "return asn1::Result<" << xml_ns() << "::Element>::success("
            << "" << xml_ns() << "::encode_null(\"" << el_name << "\"));\n";
        break;
      case ir::TypeKind::Integer:
        if (integer_is_bigint(rt.integer)) {
          out << ind << "return asn1::Result<" << xml_ns() << "::Element>::success("
              << "" << xml_ns() << "::encode_integer(\"" << el_name << "\", " << expr << "));\n";
        } else {
          out << ind << "return asn1::Result<" << xml_ns() << "::Element>::success("
              << "" << xml_ns() << "::encode_integer(\"" << el_name << "\", static_cast<std::int64_t>("
              << expr << ")));\n";
        }
        break;
      case ir::TypeKind::OctetString:
        if (exer_mode && rt.exer.base64) {
          out << ind << "return asn1::Result<" << xml_ns() << "::Element>::success("
              << "asn1::exer::encode_octet_string_base64(\"" << el_name
              << "\", asn1::Span<const std::uint8_t>(" << expr << ".data(), " << expr
              << ".size())));\n";
        } else {
          out << ind << "return asn1::Result<" << xml_ns() << "::Element>::success("
              << "" << xml_ns() << "::encode_octet_string(\"" << el_name
              << "\", asn1::Span<const std::uint8_t>(" << expr << ".data(), " << expr
              << ".size())));\n";
        }
        break;
      case ir::TypeKind::BitString:
        out << ind << "return asn1::Result<" << xml_ns() << "::Element>::success("
            << "" << xml_ns() << "::encode_bit_string(\"" << el_name
            << "\", asn1::Span<const std::uint8_t>(" << expr << ".bits.data(), " << expr
            << ".bits.size()), " << expr << ".bit_length));\n";
        break;
      case ir::TypeKind::String:
        out << ind << "return asn1::Result<" << xml_ns() << "::Element>::success("
            << "" << xml_ns() << "::encode_utf8_string(\"" << el_name << "\", " << expr << "));\n";
        break;
      case ir::TypeKind::ObjectIdentifier:
      case ir::TypeKind::RelativeOid:
        out << ind << "return asn1::Result<" << xml_ns() << "::Element>::success("
            << "" << xml_ns() << "::encode_object_identifier(\"" << el_name
            << "\", asn1::Span<const std::uint64_t>(" << expr << ".data(), " << expr
            << ".size())));\n";
        break;
      case ir::TypeKind::Real:
        out << ind << "return asn1::Result<" << xml_ns() << "::Element>::success("
            << "" << xml_ns() << "::encode_real(\"" << el_name << "\", " << expr << "));\n";
        break;
      case ir::TypeKind::Enumerated: {
        if (exer_mode && rt.exer.use_number) {
          out << ind << "return asn1::Result<" << xml_ns() << "::Element>::success("
              << "asn1::exer::encode_enumerated_number(\"" << el_name
              << "\", static_cast<std::int64_t>(" << expr << ")));\n";
        } else {
          emit_xer_enum_id_switch(out, rt, expr, ind);
          if (exer_mode && rt.exer.text) {
            out << ind << "return asn1::Result<" << xml_ns() << "::Element>::success("
                << "asn1::exer::encode_enumerated_text(\"" << el_name
                << "\", xer_enum_id));\n";
          } else {
            out << ind << "return asn1::Result<" << xml_ns() << "::Element>::success("
                << "" << xml_ns() << "::encode_enumerated(\"" << el_name << "\", xer_enum_id));\n";
          }
        }
        break;
      }
      case ir::TypeKind::SequenceOf:
      case ir::TypeKind::SetOf: {
        const ir::TypeId elem_id =
            rt.kind == ir::TypeKind::SequenceOf ? rt.sequence_of.element : rt.set_of.element;
        const ir::Type& elem = resolve(model_, elem_id);
        // A named SEQUENCE OF / SET OF lowers to an alias struct with a
        // `.value` member; an inline (anonymous) one is a bare vector.
        const std::string vec = is_named(rt) ? expr + ".value" : expr;
        if (exer_mode && rt.exer.list) {
          if (elem.kind == ir::TypeKind::String) {
            out << ind << "return asn1::Result<" << xml_ns() << "::Element>::success("
                << "asn1::exer::encode_list_of_strings(\"" << el_name << "\", " << vec
                << "));\n";
          } else if (elem.kind == ir::TypeKind::Integer && integer_is_bigint(elem.integer)) {
            out << ind << "return asn1::Result<" << xml_ns() << "::Element>::success("
                << "asn1::exer::encode_list_of_integers(\"" << el_name << "\", " << vec
                << "));\n";
          } else if (elem.kind == ir::TypeKind::Integer) {
            out << ind << "{\n";
            out << ind << "  std::vector<std::int64_t> xer_list;\n";
            out << ind << "  xer_list.reserve(" << vec << ".size());\n";
            out << ind << "  for (const auto& xer_item : " << vec << ") {\n";
            out << ind << "    xer_list.push_back(static_cast<std::int64_t>(xer_item));\n";
            out << ind << "  }\n";
            out << ind << "  return asn1::Result<" << xml_ns() << "::Element>::success("
                << "asn1::exer::encode_list_of_integers(\"" << el_name
                << "\", xer_list));\n";
            out << ind << "}\n";
          } else if (elem.kind == ir::TypeKind::Enumerated && elem.exer.use_number) {
            out << ind << "{\n";
            out << ind << "  std::vector<std::int64_t> xer_list;\n";
            out << ind << "  xer_list.reserve(" << vec << ".size());\n";
            out << ind << "  for (const auto& xer_item : " << vec << ") {\n";
            out << ind << "    xer_list.push_back(static_cast<std::int64_t>(xer_item));\n";
            out << ind << "  }\n";
            out << ind << "  return asn1::Result<" << xml_ns() << "::Element>::success("
                << "asn1::exer::encode_list_of_integers(\"" << el_name
                << "\", xer_list));\n";
            out << ind << "}\n";
          } else if (elem.kind == ir::TypeKind::Enumerated) {
            out << ind << "{\n";
            out << ind << "  std::vector<std::string> xer_list;\n";
            out << ind << "  xer_list.reserve(" << vec << ".size());\n";
            out << ind << "  for (const auto& xer_item : " << vec << ") {\n";
            out << ind << "    std::string xer_enum_id;\n";
            out << ind << "    switch (static_cast<std::int64_t>(xer_item)) {\n";
            for (const auto& nn : elem.enumerated.root) {
              out << ind << "      case "
                  << (nn.value.as_i64 ? std::to_string(*nn.value.as_i64) : "0")
                  << ": xer_enum_id = \"" << nn.name << "\"; break;\n";
            }
            for (const auto& nn : elem.enumerated.extensions) {
              out << ind << "      case "
                  << (nn.value.as_i64 ? std::to_string(*nn.value.as_i64) : "0")
                  << ": xer_enum_id = \"" << nn.name << "\"; break;\n";
            }
            out << ind << "      default: xer_enum_id = \""
                << (elem.enumerated.root.empty() ? "?" : elem.enumerated.root.front().name)
                << "\"; break;\n";
            out << ind << "    }\n";
            out << ind << "    xer_list.push_back(std::move(xer_enum_id));\n";
            out << ind << "  }\n";
            out << ind << "  return asn1::Result<" << xml_ns() << "::Element>::success("
                << "asn1::exer::encode_list_of_strings(\"" << el_name
                << "\", xer_list));\n";
            out << ind << "}\n";
          } else if (elem.kind == ir::TypeKind::Boolean) {
            out << ind << "{\n";
            out << ind << "  std::vector<std::uint8_t> xer_list;\n";
            out << ind << "  xer_list.reserve(" << vec << ".size());\n";
            out << ind << "  for (const auto& xer_item : " << vec << ") {\n";
            out << ind << "    xer_list.push_back(xer_item ? 1 : 0);\n";
            out << ind << "  }\n";
            out << ind << "  return asn1::Result<" << xml_ns() << "::Element>::success("
                << "asn1::exer::encode_list_of_booleans(\"" << el_name
                << "\", xer_list));\n";
            out << ind << "}\n";
          } else if (elem.kind == ir::TypeKind::ObjectIdentifier ||
                     elem.kind == ir::TypeKind::RelativeOid) {
            out << ind << "return asn1::Result<" << xml_ns() << "::Element>::success("
                << "asn1::exer::encode_list_of_object_identifiers(\"" << el_name
                << "\", " << vec << "));\n";
          } else if (elem.kind == ir::TypeKind::Real) {
            out << ind << "return asn1::Result<" << xml_ns() << "::Element>::success("
                << "asn1::exer::encode_list_of_reals(\"" << el_name << "\", " << vec << "));\n";
          } else if (elem.kind == ir::TypeKind::OctetString) {
            out << ind << "return asn1::Result<" << xml_ns() << "::Element>::success("
                << "asn1::exer::encode_list_of_octet_strings(\"" << el_name << "\", "
                << vec << "));\n";
          } else if (elem.kind == ir::TypeKind::BitString) {
            out << ind << "{\n";
            out << ind << "  std::vector<asn1::xer::BitStringValue> xer_list;\n";
            out << ind << "  xer_list.reserve(" << vec << ".size());\n";
            out << ind << "  for (const auto& xer_item : " << vec << ") {\n";
            out << ind << "    asn1::xer::BitStringValue bs;\n";
            out << ind << "    bs.bits = xer_item.bits;\n";
            out << ind << "    bs.bit_length = xer_item.bit_length;\n";
            out << ind << "    xer_list.push_back(std::move(bs));\n";
            out << ind << "  }\n";
            out << ind << "  return asn1::Result<" << xml_ns() << "::Element>::success("
                << "asn1::exer::encode_list_of_bit_strings(\"" << el_name
                << "\", xer_list));\n";
            out << ind << "}\n";
          } else {
            out << ind << "return asn1::make_error(asn1::Error::Code::Unsupported, 0, "
                   "\"LIST encoding requires a character-encodable element type\");\n";
          }
        } else {
          out << ind << "{\n";
          out << ind << "  std::vector<" << xml_ns() << "::Element> xer_items;\n";
          out << ind << "  xer_items.reserve(" << vec << ".size());\n";
          out << ind << "  for (const auto& xer_item : " << vec << ") {\n";
          out << ind << "    auto enc = ";
          emit_xer_field_encode_expr(out, elem, "xer_item", ind + "    ");
          out << ind << "    if (!enc) return enc.error();\n";
          out << ind << "    xer_items.push_back(std::move(enc.value()));\n";
          out << ind << "  }\n";
          out << ind << "  return asn1::Result<" << xml_ns() << "::Element>::success("
              << "" << xml_ns() << "::"
              << (xer_use_cxer() && rt.kind == ir::TypeKind::SetOf ? "encode_set_of"
                                                                  : "encode_sequence_of")
              << "(\"" << el_name << "\", std::move(xer_items)));\n";
          out << ind << "}\n";
        }
        break;
      }
      case ir::TypeKind::InstanceOf:
        out << ind << "{\n";
        out << ind << "  " << xml_ns() << "::Element root;\n";
        out << ind << "  root.name = \"" << el_name << "\";\n";
        out << ind << "  root.children.push_back(" << xml_ns() << "::encode_object_identifier(\"type-id\", "
            << "asn1::Span<const std::uint64_t>(" << expr
            << ".type_id.data(), " << expr << ".type_id.size())));\n";
        out << ind << "  root.children.push_back(" << xml_ns() << "::encode_octet_string(\"value\", "
            << "asn1::Span<const std::uint8_t>(" << expr << ".value.data(), " << expr
            << ".value.size())));\n";
        out << ind << "  return asn1::Result<" << xml_ns() << "::Element>::success(std::move(root));\n";
        out << ind << "}\n";
        break;
      case ir::TypeKind::Choice: {
        const auto all = choice_all_fields(rt.choice);
        out << ind << "{\n";
        out << ind << "  const std::size_t xer_idx = " << expr << ".alt.index();\n";
        out << ind << "  switch (xer_idx) {\n";
        for (std::size_t i = 0; i < all.size(); ++i) {
          const ir::Field& f = *all[i];
          const ir::Type& ft = resolve(model_, f.type);
          const std::string xml_name = xer_field_name(f);
          out << ind << "    case " << i << ": {\n";
          // Unique local: a nested CHOICE alternative re-enters this block
          // with expr = "<outer alt local>.value", and a duplicate `alt`
          // shadows the initializer it is deduced from.
          const std::string alt_local = "alt" + std::to_string(temp_counter_++);
          out << ind << "      const auto& " << alt_local << " = std::get<" << i << ">(" << expr
              << ".alt);\n";
          out << ind << "      auto enc = ";
          emit_xer_field_encode_expr(out, ft, alt_local + ".value", ind + "      ");
          out << ind << "      if (!enc) return enc.error();\n";
          out << ind << "      enc.value().name = \"" << xml_name << "\";\n";
          out << ind << "      return asn1::Result<" << xml_ns() << "::Element>::success("
              << "" << xml_ns() << "::encode_choice(\"" << el_name
              << "\", std::move(enc.value())));\n";
          out << ind << "    }\n";
        }
        out << ind << "    default:\n";
        out << ind << "      return asn1::make_error(asn1::Error::Code::InvalidArgument, 0, "
               "\"CHOICE index out of range\");\n";
        out << ind << "  }\n";
        out << ind << "}\n";
        break;
      }
      case ir::TypeKind::Sequence:
      case ir::TypeKind::Set: {
        const auto& fields =
            rt.kind == ir::TypeKind::Sequence ? rt.sequence.root : rt.set.root;
        out << ind << "{\n";
        out << ind << "  " << xml_ns() << "::Element seq;\n";
        out << ind << "  seq.name = \"" << el_name << "\";\n";
        for (const ir::Field& f : fields) {
          const std::string fname = cpp_ident(f.name);
          const std::string xml_name = xer_field_name(f);
          const ir::Type& ft = resolve(model_, f.type);
          const bool optional = f.presence == ir::Presence::Optional;
          if (exer_mode && f.exer.attribute) {
            const ir::Type& rft = ft.kind == ir::TypeKind::Referenced &&
                                          ft.referenced.resolved != ir::kInvalidType
                                      ? resolve(model_, ft.referenced.resolved)
                                      : ft;
            auto emit_attr = [&](const char* api, const std::string& val_expr) {
              if (optional) {
                out << ind << "  if (" << expr << "." << fname << ") {\n";
                out << ind << "    asn1::exer::" << api << "(seq, \"" << xml_name << "\", "
                    << val_expr << ");\n";
                out << ind << "  }\n";
              } else {
                out << ind << "  asn1::exer::" << api << "(seq, \"" << xml_name << "\", "
                    << val_expr << ");\n";
              }
            };
            if (rft.kind == ir::TypeKind::Boolean) {
              emit_attr("encode_attribute_boolean",
                        optional ? ("*" + expr + "." + fname) : (expr + "." + fname));
              continue;
            }
            if (rft.kind == ir::TypeKind::Integer || rft.kind == ir::TypeKind::Enumerated) {
              if (rft.kind == ir::TypeKind::Integer && integer_is_bigint(rft.integer)) {
                emit_attr("encode_attribute_integer",
                          optional ? ("*" + expr + "." + fname) : (expr + "." + fname));
              } else {
                emit_attr("encode_attribute_integer",
                          "static_cast<std::int64_t>(" +
                              (optional ? ("*" + expr + "." + fname) : (expr + "." + fname)) +
                              ")");
              }
              continue;
            }
            if (rft.kind == ir::TypeKind::String) {
              emit_attr("encode_attribute_string",
                        optional ? ("*" + expr + "." + fname) : (expr + "." + fname));
              continue;
            }
          }
          if (optional) {
            out << ind << "  if (" << expr << "." << fname << ") {\n";
            out << ind << "    auto enc = ";
            // Parenthesise the deref: member accesses are appended downstream
            // (`(*value.f).bits`), and `*value.f.bits` would parse the member
            // access onto the optional before the deref.
            emit_xer_field_encode_expr(out, ft, "(*" + expr + "." + fname + ")", ind + "    ");
            out << ind << "    if (!enc) return enc.error();\n";
            if (exer_mode && f.exer.untagged) {
              out << ind << "    asn1::exer::append_untagged(seq, std::move(enc.value()));\n";
            } else {
              out << ind << "    enc.value().name = \"" << xml_name << "\";\n";
              out << ind << "    seq.children.push_back(std::move(enc.value()));\n";
            }
            out << ind << "  }";
            if (exer_mode && f.exer.use_nil) {
              out << " else {\n";
              out << ind << "    " << xml_ns() << "::Element nil;\n";
              out << ind << "    nil.name = \"" << xml_name << "\";\n";
              out << ind << "    asn1::exer::set_nil(nil, true);\n";
              out << ind << "    seq.children.push_back(std::move(nil));\n";
              out << ind << "  }\n";
            } else {
              out << "\n";
            }
          } else if (f.presence == ir::Presence::Default) {
            if (f.default_value) {
              std::string def_val = default_val_cpp(f);
              if (!def_val.empty()) {
                out << ind << "  if (" << expr << "." << fname << " != " << def_val << ") {\n";
                out << ind << "    auto enc = ";
                emit_xer_field_encode_expr(out, ft, expr + "." + fname, ind + "    ");
                out << ind << "    if (!enc) return enc.error();\n";
                if (exer_mode && f.exer.untagged) {
                  out << ind << "    asn1::exer::append_untagged(seq, std::move(enc.value()));\n";
                } else {
                  out << ind << "    enc.value().name = \"" << xml_name << "\";\n";
                  out << ind << "    seq.children.push_back(std::move(enc.value()));\n";
                }
                out << ind << "  }\n";
                continue;
              }
            }
            out << ind << "  {\n";
            out << ind << "    auto enc = ";
            emit_xer_field_encode_expr(out, ft, expr + "." + fname, ind + "    ");
            out << ind << "    if (!enc) return enc.error();\n";
            if (exer_mode && f.exer.untagged) {
              out << ind << "    asn1::exer::append_untagged(seq, std::move(enc.value()));\n";
            } else {
              out << ind << "    enc.value().name = \"" << xml_name << "\";\n";
              out << ind << "    seq.children.push_back(std::move(enc.value()));\n";
            }
            out << ind << "  }\n";
          } else {
            out << ind << "  {\n";
            out << ind << "    auto enc = ";
            emit_xer_field_encode_expr(out, ft, expr + "." + fname, ind + "    ");
            out << ind << "    if (!enc) return enc.error();\n";
            if (exer_mode && f.exer.untagged) {
              out << ind << "    asn1::exer::append_untagged(seq, std::move(enc.value()));\n";
            } else {
              out << ind << "    enc.value().name = \"" << xml_name << "\";\n";
              out << ind << "    seq.children.push_back(std::move(enc.value()));\n";
            }
            out << ind << "  }\n";
          }
        }
        out << ind << "  return asn1::Result<" << xml_ns() << "::Element>::success(std::move(seq));\n";
        out << ind << "}\n";
        break;
      }
      case ir::TypeKind::Referenced:
        if (rt.referenced.resolved != ir::kInvalidType) {
          emit_xer_encode_body(out, resolve(model_, rt.referenced.resolved), expr, ind);
        } else {
          out << ind << "return asn1::make_error(asn1::Error::Code::InvalidArgument, 0, "
                 "\"unresolved type reference\");\n";
        }
        break;
      default:
        out << ind << "return asn1::make_error(asn1::Error::Code::Unsupported, 0, "
               "\"XER encoding not generated for this type\");\n";
        break;
    }
  }

void Emitter::emit_xer_decode_body(std::ostream& out, const ir::Type& t, const std::string& expr,
                            const std::string& el_expr, const std::string& ind) {
    const ir::Type& rt = t.kind == ir::TypeKind::Referenced && t.referenced.resolved != ir::kInvalidType
                             ? resolve(model_, t.referenced.resolved)
                             : t;
    const bool exer_mode = xer_use_exer();
    switch (rt.kind) {
      case ir::TypeKind::Boolean:
        out << ind << "{\n";
        if (exer_mode && rt.exer.text) {
          out << ind << "  auto tmp = asn1::exer::decode_boolean_text(" << el_expr << ");\n";
        } else {
          out << ind << "  auto tmp = " << xml_ns() << "::decode_boolean(" << el_expr << ");\n";
        }
        out << ind << "  if (!tmp) return tmp.error();\n";
        out << ind << "  " << expr << " = tmp.value();\n";
        out << ind << "}\n";
        break;
      case ir::TypeKind::Null:
        out << ind << "{\n";
        out << ind << "  auto tmp = " << xml_ns() << "::decode_null(" << el_expr << ");\n";
        out << ind << "  if (!tmp) return tmp.error();\n";
        out << ind << "}\n";
        break;
      case ir::TypeKind::Integer:
        out << ind << "{\n";
        if (integer_is_bigint(rt.integer)) {
          out << ind << "  auto tmp = " << xml_ns() << "::decode_big_integer(" << el_expr << ");\n";
          out << ind << "  if (!tmp) return tmp.error();\n";
          out << ind << "  " << expr << " = std::move(tmp.value());\n";
        } else {
          out << ind << "  auto tmp = " << xml_ns() << "::decode_integer(" << el_expr << ");\n";
          out << ind << "  if (!tmp) return tmp.error();\n";
          out << ind << "  " << expr << " = static_cast<" << integer_host_type(rt.integer)
              << ">(tmp.value());\n";
        }
        out << ind << "}\n";
        break;
      case ir::TypeKind::OctetString:
        out << ind << "{\n";
        if (exer_mode && rt.exer.base64) {
          out << ind << "  auto tmp = asn1::exer::decode_octet_string_base64(" << el_expr
              << ");\n";
        } else {
          out << ind << "  auto tmp = " << xml_ns() << "::decode_octet_string(" << el_expr << ");\n";
        }
        out << ind << "  if (!tmp) return tmp.error();\n";
        out << ind << "  " << expr << " = std::move(tmp.value());\n";
        out << ind << "}\n";
        break;
      case ir::TypeKind::BitString:
        out << ind << "{\n";
        out << ind << "  auto tmp = " << xml_ns() << "::decode_bit_string(" << el_expr << ");\n";
        out << ind << "  if (!tmp) return tmp.error();\n";
        out << ind << "  " << expr << ".bits = std::move(tmp.value().bits);\n";
        out << ind << "  " << expr << ".bit_length = tmp.value().bit_length;\n";
        out << ind << "}\n";
        break;
      case ir::TypeKind::String:
        out << ind << "{\n";
        out << ind << "  auto tmp = " << xml_ns() << "::decode_utf8_string(" << el_expr << ");\n";
        out << ind << "  if (!tmp) return tmp.error();\n";
        out << ind << "  " << expr << " = std::move(tmp.value());\n";
        out << ind << "}\n";
        break;
      case ir::TypeKind::Real:
        out << ind << "{\n";
        out << ind << "  auto tmp = " << xml_ns() << "::decode_real(" << el_expr << ");\n";
        out << ind << "  if (!tmp) return tmp.error();\n";
        out << ind << "  " << expr << " = tmp.value();\n";
        out << ind << "}\n";
        break;
      case ir::TypeKind::ObjectIdentifier:
      case ir::TypeKind::RelativeOid:
        out << ind << "{\n";
        out << ind << "  auto tmp = " << xml_ns() << "::decode_object_identifier(" << el_expr << ");\n";
        out << ind << "  if (!tmp) return tmp.error();\n";
        out << ind << "  " << expr << " = std::move(tmp.value());\n";
        out << ind << "}\n";
        break;
      case ir::TypeKind::Enumerated: {
        out << ind << "{\n";
        if (exer_mode && rt.exer.use_number) {
          out << ind << "  auto tmp = asn1::exer::decode_enumerated_number(" << el_expr << ");\n";
          out << ind << "  if (!tmp) return tmp.error();\n";
          out << ind << "  " << expr << " = static_cast<"
              << (is_named(rt) ? named_ref(rt) : "std::int64_t") << ">(tmp.value());\n";
        } else {
          if (exer_mode && rt.exer.text) {
            out << ind << "  auto tmp = asn1::exer::decode_enumerated_text(" << el_expr << ");\n";
          } else {
            out << ind << "  auto tmp = " << xml_ns() << "::decode_enumerated(" << el_expr << ");\n";
          }
          out << ind << "  if (!tmp) return tmp.error();\n";
          out << ind << "  bool xer_enum_ok = false;\n";
          auto emit_match = [&](const ir::NamedNumber& nn) {
            out << ind << "  if (!xer_enum_ok && tmp.value() == \"" << nn.name << "\") { "
                << expr << " = static_cast<" << (is_named(rt) ? named_ref(rt) : "std::int64_t")
                << ">(" << (nn.value.as_i64 ? *nn.value.as_i64 : 0)
                << "); xer_enum_ok = true; }\n";
          };
          for (const auto& nn : rt.enumerated.root) {
            emit_match(nn);
          }
          for (const auto& nn : rt.enumerated.extensions) {
            emit_match(nn);
          }
          out << ind << "  if (!xer_enum_ok) return asn1::make_error("
                 "asn1::Error::Code::InvalidArgument, 0, \"unknown ENUMERATED identifier\");\n";
        }
        out << ind << "}\n";
        break;
      }
      case ir::TypeKind::SequenceOf:
      case ir::TypeKind::SetOf: {
        const ir::TypeId elem_id =
            rt.kind == ir::TypeKind::SequenceOf ? rt.sequence_of.element : rt.set_of.element;
        const ir::Type& elem = resolve(model_, elem_id);
        // A named SEQUENCE OF / SET OF lowers to an alias struct with a
        // `.value` member; an inline (anonymous) one is a bare vector.
        const std::string vec = is_named(rt) ? expr + ".value" : expr;
        out << ind << "{\n";
        if (exer_mode && rt.exer.list) {
          if (elem.kind == ir::TypeKind::String) {
            out << ind << "  auto tmp = asn1::exer::decode_list_of_strings(" << el_expr << ");\n";
            out << ind << "  if (!tmp) return tmp.error();\n";
            out << ind << "  " << vec << " = std::move(tmp.value());\n";
          } else if (elem.kind == ir::TypeKind::Integer && integer_is_bigint(elem.integer)) {
            out << ind << "  auto tmp = asn1::exer::decode_list_of_big_integers(" << el_expr
                << ");\n";
            out << ind << "  if (!tmp) return tmp.error();\n";
            out << ind << "  " << vec << " = std::move(tmp.value());\n";
          } else if (elem.kind == ir::TypeKind::Integer) {
            out << ind << "  auto tmp = asn1::exer::decode_list_of_integers(" << el_expr << ");\n";
            out << ind << "  if (!tmp) return tmp.error();\n";
            out << ind << "  " << vec << ".clear();\n";
            out << ind << "  " << vec << ".reserve(tmp.value().size());\n";
            out << ind << "  for (std::int64_t xer_n : tmp.value()) {\n";
            out << ind << "    " << vec << ".push_back(static_cast<"
                << type_cpp(elem_id) << ">(xer_n));\n";
            out << ind << "  }\n";
          } else if (elem.kind == ir::TypeKind::Enumerated && elem.exer.use_number) {
            out << ind << "  auto tmp = asn1::exer::decode_list_of_integers(" << el_expr << ");\n";
            out << ind << "  if (!tmp) return tmp.error();\n";
            out << ind << "  " << vec << ".clear();\n";
            out << ind << "  " << vec << ".reserve(tmp.value().size());\n";
            out << ind << "  for (std::int64_t xer_n : tmp.value()) {\n";
            out << ind << "    " << vec << ".push_back(static_cast<"
                << type_cpp(elem_id) << ">(xer_n));\n";
            out << ind << "  }\n";
          } else if (elem.kind == ir::TypeKind::Enumerated) {
            out << ind << "  auto tmp = asn1::exer::decode_list_of_strings(" << el_expr << ");\n";
            out << ind << "  if (!tmp) return tmp.error();\n";
            out << ind << "  " << vec << ".clear();\n";
            out << ind << "  " << vec << ".reserve(tmp.value().size());\n";
            out << ind << "  for (const auto& xer_tok : tmp.value()) {\n";
            out << ind << "    bool xer_enum_ok = false;\n";
            auto emit_match = [&](const ir::NamedNumber& nn) {
              out << ind << "    if (!xer_enum_ok && xer_tok == \"" << nn.name << "\") { "
                  << vec << ".push_back(static_cast<" << type_cpp(elem_id) << ">("
                  << (nn.value.as_i64 ? *nn.value.as_i64 : 0)
                  << ")); xer_enum_ok = true; }\n";
            };
            for (const auto& nn : elem.enumerated.root) {
              emit_match(nn);
            }
            for (const auto& nn : elem.enumerated.extensions) {
              emit_match(nn);
            }
            out << ind << "    if (!xer_enum_ok) return asn1::make_error("
                   "asn1::Error::Code::InvalidArgument, 0, "
                   "\"unknown ENUMERATED identifier in LIST\");\n";
            out << ind << "  }\n";
          } else if (elem.kind == ir::TypeKind::Boolean) {
            out << ind << "  auto tmp = asn1::exer::decode_list_of_booleans(" << el_expr << ");\n";
            out << ind << "  if (!tmp) return tmp.error();\n";
            out << ind << "  " << vec << ".clear();\n";
            out << ind << "  " << vec << ".reserve(tmp.value().size());\n";
            out << ind << "  for (std::uint8_t xer_b : tmp.value()) {\n";
            out << ind << "    " << vec << ".push_back(xer_b != 0);\n";
            out << ind << "  }\n";
          } else if (elem.kind == ir::TypeKind::ObjectIdentifier ||
                     elem.kind == ir::TypeKind::RelativeOid) {
            out << ind << "  auto tmp = asn1::exer::decode_list_of_object_identifiers("
                << el_expr << ");\n";
            out << ind << "  if (!tmp) return tmp.error();\n";
            out << ind << "  " << vec << " = std::move(tmp.value());\n";
          } else if (elem.kind == ir::TypeKind::Real) {
            out << ind << "  auto tmp = asn1::exer::decode_list_of_reals(" << el_expr << ");\n";
            out << ind << "  if (!tmp) return tmp.error();\n";
            out << ind << "  " << vec << " = std::move(tmp.value());\n";
          } else if (elem.kind == ir::TypeKind::OctetString) {
            out << ind << "  auto tmp = asn1::exer::decode_list_of_octet_strings(" << el_expr
                << ");\n";
            out << ind << "  if (!tmp) return tmp.error();\n";
            out << ind << "  " << vec << " = std::move(tmp.value());\n";
          } else if (elem.kind == ir::TypeKind::BitString) {
            out << ind << "  auto tmp = asn1::exer::decode_list_of_bit_strings(" << el_expr
                << ");\n";
            out << ind << "  if (!tmp) return tmp.error();\n";
            out << ind << "  " << vec << ".clear();\n";
            out << ind << "  " << vec << ".reserve(tmp.value().size());\n";
            out << ind << "  for (auto& xer_bs : tmp.value()) {\n";
            out << ind << "    " << type_cpp(elem_id) << " xer_item{};\n";
            out << ind << "    xer_item.bits = std::move(xer_bs.bits);\n";
            out << ind << "    xer_item.bit_length = xer_bs.bit_length;\n";
            out << ind << "    " << vec << ".push_back(std::move(xer_item));\n";
            out << ind << "  }\n";
          } else {
            out << ind << "  return asn1::make_error(asn1::Error::Code::Unsupported, 0, "
                   "\"LIST decode requires a character-encodable element type\");\n";
          }
        } else {
          out << ind << "  " << vec << ".clear();\n";
          out << ind << "  for (const auto& xer_child : " << el_expr << ".children) {\n";
          out << ind << "    " << type_cpp(elem_id) << " xer_item{};\n";
          emit_xer_field_decode_stmt(out, elem, "xer_item", "xer_child", ind + "    ");
          out << ind << "    " << vec << ".push_back(std::move(xer_item));\n";
          out << ind << "  }\n";
        }
        out << ind << "}\n";
        break;
      }
      case ir::TypeKind::InstanceOf:
        out << ind << "{\n";
        out << ind << "  auto tid = " << xml_ns() << "::find_child(" << el_expr << ", \"type-id\");\n";
        out << ind << "  if (!tid) return asn1::make_error(asn1::Error::Code::InvalidArgument, 0, "
               "\"missing type-id\");\n";
        out << ind << "  auto oid = " << xml_ns() << "::decode_object_identifier(*tid.value());\n";
        out << ind << "  if (!oid) return oid.error();\n";
        out << ind << "  " << expr << ".type_id = std::move(oid.value());\n";
        out << ind << "  auto val = " << xml_ns() << "::find_child(" << el_expr << ", \"value\");\n";
        out << ind << "  if (!val) return asn1::make_error(asn1::Error::Code::InvalidArgument, 0, "
               "\"missing value\");\n";
        out << ind << "  auto oct = " << xml_ns() << "::decode_octet_string(*val.value());\n";
        out << ind << "  if (!oct) return oct.error();\n";
        out << ind << "  " << expr << ".value = std::move(oct.value());\n";
        out << ind << "}\n";
        break;
      case ir::TypeKind::Choice: {
        out << ind << "{\n";
        out << ind << "  if (" << el_expr << ".children.size() != 1) {\n";
        out << ind << "    return asn1::make_error(asn1::Error::Code::InvalidArgument, 0, "
               "\"CHOICE expects one child\");\n";
        out << ind << "  }\n";
        out << ind << "  const auto& xer_alt = " << el_expr << ".children[0];\n";
        // A named CHOICE nests its alternative structs; qualify the name so
        // nested choices (Nested_rec_q::m_) resolve from a free function.
        const std::string choice_ty = is_named(rt) ? named_ref(rt) : std::string{};
        for (const ir::Field* fp : choice_all_fields(rt.choice)) {
          const ir::Field& f = *fp;
          const ir::Type& ft = resolve(model_, f.type);
          const std::string xml_name = xer_field_name(f);
          out << ind << "  if (xer_alt.name == \"" << xml_name << "\") {\n";
          if (!choice_ty.empty()) {
            out << ind << "    " << choice_ty << "::" << cpp_ident(f.name) << "_ alt{};\n";
          } else {
            out << ind << "    " << cpp_ident(f.name) << "_ alt{};\n";
          }
          emit_xer_field_decode_stmt(out, ft, "alt.value", "xer_alt", ind + "    ");
          out << ind << "    " << expr << ".alt = std::move(alt);\n";
          out << ind << "  } else";
        }
        out << " {\n";
        out << ind << "    return asn1::make_error(asn1::Error::Code::InvalidArgument, 0, "
               "\"unknown CHOICE alternative\");\n";
        out << ind << "  }\n";
        out << ind << "}\n";
        break;
      }
      case ir::TypeKind::Sequence:
      case ir::TypeKind::Set: {
        const auto& fields =
            rt.kind == ir::TypeKind::Sequence ? rt.sequence.root : rt.set.root;
        out << ind << "{\n";
        for (const ir::Field& f : fields) {
          const std::string fname = cpp_ident(f.name);
          const std::string xml_name = xer_field_name(f);
          const ir::Type& ft = resolve(model_, f.type);
          const bool optional = f.presence == ir::Presence::Optional;
          if (exer_mode && f.exer.attribute) {
            const ir::Type& rft = ft.kind == ir::TypeKind::Referenced &&
                                          ft.referenced.resolved != ir::kInvalidType
                                      ? resolve(model_, ft.referenced.resolved)
                                      : ft;
            out << ind << "  {\n";
            if (rft.kind == ir::TypeKind::Boolean) {
              out << ind << "    auto tmp = asn1::exer::decode_attribute_boolean(" << el_expr
                  << ", \"" << xml_name << "\");\n";
              out << ind << "    if (tmp) " << expr << "." << fname << " = tmp.value();\n";
            } else if (rft.kind == ir::TypeKind::Integer && integer_is_bigint(rft.integer)) {
              out << ind << "    auto tmp = asn1::exer::get_attribute(" << el_expr << ", \""
                  << xml_name << "\");\n";
              out << ind << "    if (tmp) {\n";
              out << ind << "      auto bi = asn1::BigInteger::from_decimal(tmp.value());\n";
              out << ind << "      if (bi) " << expr << "." << fname
                  << " = std::move(bi.value());\n";
              out << ind << "    }\n";
            } else if (rft.kind == ir::TypeKind::Integer || rft.kind == ir::TypeKind::Enumerated) {
              out << ind << "    auto tmp = asn1::exer::get_attribute(" << el_expr << ", \""
                  << xml_name << "\");\n";
              out << ind << "    if (tmp) " << expr << "." << fname
                  << " = static_cast<" << type_cpp(f.type)
                  << ">(std::stoll(tmp.value()));\n";
            } else if (rft.kind == ir::TypeKind::String) {
              out << ind << "    auto tmp = asn1::exer::get_attribute(" << el_expr << ", \""
                  << xml_name << "\");\n";
              out << ind << "    if (tmp) " << expr << "." << fname << " = tmp.value();\n";
            }
            out << ind << "  }\n";
            continue;
          }
          if (exer_mode && f.exer.untagged) {
            // UNTAGGED: component content is merged into the parent element.
            out << ind << "  {\n";
            if (optional) {
              const std::string tmp = "xer_field" + std::to_string(temp_counter_++);
              out << ind << "    " << type_cpp(f.type) << " " << tmp << "{};\n";
              emit_xer_field_decode_stmt(out, ft, tmp, el_expr, ind + "    ");
              out << ind << "    " << expr << "." << fname << " = std::move(" << tmp << ");\n";
            } else {
              emit_xer_field_decode_stmt(out, ft, expr + "." + fname, el_expr, ind + "    ");
            }
            out << ind << "  }\n";
            continue;
          }
          out << ind << "  {\n";
          out << ind << "    auto child = " << xml_ns() << "::find_child(" << el_expr << ", \"" << xml_name
              << "\");\n";
          out << ind << "    if (child) {\n";
          if (exer_mode && f.exer.use_nil) {
            out << ind << "      auto xer_nil = asn1::exer::is_nil(*child.value());\n";
            out << ind << "      if (xer_nil && xer_nil.value()) {\n";
            if (optional) {
              out << ind << "        " << expr << "." << fname << " = std::nullopt;\n";
            }
            out << ind << "      } else {\n";
            if (optional) {
              const std::string tmp = "xer_field" + std::to_string(temp_counter_++);
              out << ind << "        " << type_cpp(f.type) << " " << tmp << "{};\n";
              emit_xer_field_decode_stmt(out, ft, tmp, "(*child.value())",
                                         ind + "        ");
              out << ind << "        " << expr << "." << fname << " = std::move(" << tmp << ");\n";
            } else {
              emit_xer_field_decode_stmt(out, ft, expr + "." + fname, "(*child.value())",
                                         ind + "        ");
            }
            out << ind << "      }\n";
          } else if (optional) {
            const std::string tmp = "xer_field" + std::to_string(temp_counter_++);
            out << ind << "      " << type_cpp(f.type) << " " << tmp << "{};\n";
            emit_xer_field_decode_stmt(out, ft, tmp, "(*child.value())", ind + "      ");
            out << ind << "      " << expr << "." << fname << " = std::move(" << tmp << ");\n";
          } else {
            emit_xer_field_decode_stmt(out, ft, expr + "." + fname, "(*child.value())",
                                       ind + "      ");
          }
          out << ind << "    } else if (" << (f.presence == ir::Presence::Default ? "true" : "false") << ") {\n";
          if (f.presence == ir::Presence::Default && f.default_value) {
            const std::string def_expr = default_val_cpp(f);
            if (!def_expr.empty()) {
              out << ind << "      " << expr << "." << fname << " = " << def_expr << ";\n";
            }
          }
          out << ind << "    }\n";
          out << ind << "  }\n";
        }
        out << ind << "}\n";
        break;
      }
      case ir::TypeKind::Referenced:
        if (rt.referenced.resolved != ir::kInvalidType) {
          emit_xer_decode_body(out, resolve(model_, rt.referenced.resolved), expr, el_expr, ind);
        } else {
          out << ind << "return asn1::make_error(asn1::Error::Code::InvalidArgument, 0, "
                 "\"unresolved type reference\");\n";
        }
        break;
      default:
        out << ind << "return asn1::make_error(asn1::Error::Code::Unsupported, 0, "
               "\"XER decode not generated for this type\");\n";
        break;
    }
  }

}  // namespace codegen
}  // namespace asn1
