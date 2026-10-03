#include <asn1/ast/print.hpp>
#include <asn1/ast/encoding.hpp>
#include <asn1/ast/ioc.hpp>

#include <sstream>

namespace asn1 {
namespace ast {
namespace {

class Printer : public Visitor {
 public:
  explicit Printer(std::ostream& out) : out_(out) {}

  void visit(const Module& n) override {
    indent() << "Module " << n.name()
             << " tagDefault=" << tag_default_name(n.tag_default())
             << " extensibilityImplied=" << (n.extensibility_implied() ? "true" : "false");
    if (n.jer_instructions()) {
      out_ << " jerInstructions=true";
    }
    if (n.xer_instructions()) {
      out_ << " xerInstructions=true";
    }
    if (!n.jer_encoding_control().empty()) {
      out_ << " jerEncodingControl=" << n.jer_encoding_control().size();
    }
    if (!n.xer_encoding_control().empty()) {
      out_ << " xerEncodingControl=" << n.xer_encoding_control().size();
    }
    out_ << '\n';
    ++depth_;
    if (n.exports_all()) {
      indent() << "Exports ALL\n";
    } else if (!n.exports().empty()) {
      indent() << "Exports";
      for (const auto& e : n.exports()) {
        out_ << ' ' << e;
      }
      out_ << '\n';
    }
    for (const auto& imp : n.imports()) {
      indent() << "ImportFrom " << imp.module << " {";
      for (std::size_t i = 0; i < imp.symbols.size(); ++i) {
        if (i) {
          out_ << ", ";
        }
        out_ << imp.symbols[i].name;
        if (imp.symbols[i].parameterized) {
          out_ << "{}";
        }
      }
      out_ << "}\n";
    }
    for (const auto& a : n.assignments()) {
      a->accept(*this);
    }
    --depth_;
  }

  void visit(const TypeAssignment& n) override {
    indent() << "TypeAssignment " << n.name();
    if (!n.parameters().empty()) {
      out_ << " {";
      for (std::size_t i = 0; i < n.parameters().size(); ++i) {
        if (i) {
          out_ << ", ";
        }
        const auto& p = n.parameters()[i];
        if (!p.governor.empty()) {
          out_ << p.governor << " : ";
        }
        out_ << p.name;
      }
      out_ << '}';
    }
    out_ << '\n';
    ++depth_;
    n.type().accept(*this);
    --depth_;
  }

  void visit(const ValueAssignment& n) override {
    indent() << "ValueAssignment " << n.name() << '\n';
    ++depth_;
    n.type().accept(*this);
    n.value().accept(*this);
    --depth_;
  }

  void visit(const ObjectClassAssignment& n) override {
    indent() << "ObjectClassAssignment " << n.name() << '\n';
    ++depth_;
    n.defn().accept(*this);
    --depth_;
  }
  void visit(const ObjectAssignment& n) override {
    indent() << "ObjectAssignment " << n.name() << " class=" << n.class_name() << '\n';
    ++depth_;
    n.defn().accept(*this);
    --depth_;
  }
  void visit(const ObjectSetAssignment& n) override {
    indent() << "ObjectSetAssignment " << n.name() << " class=" << n.class_name() << '\n';
    ++depth_;
    n.defn().accept(*this);
    --depth_;
  }
  void visit(const FieldSpec& n) override {
    indent() << "FieldSpec &" << n.name()
             << (n.kind() == FieldSpecKind::TypeField ? " TypeField" : " ValueField");
    if (n.unique()) {
      out_ << " UNIQUE";
    }
    out_ << ' ' << presence_name(n.presence()) << '\n';
    ++depth_;
    if (n.field_type()) {
      n.field_type()->accept(*this);
    }
    if (n.default_value()) {
      n.default_value()->accept(*this);
    }
    --depth_;
  }
  void visit(const ObjectClassDefn& n) override {
    indent() << "ObjectClassDefn\n";
    ++depth_;
    for (const auto& f : n.fields()) {
      f->accept(*this);
    }
    if (!n.with_syntax().empty()) {
      indent() << "WithSyntax";
      for (const auto& t : n.with_syntax()) {
        out_ << ' ' << (t.is_field ? ("&" + t.text) : t.text);
      }
      out_ << '\n';
    }
    --depth_;
  }
  void visit(const ObjectDefn& n) override {
    indent() << "ObjectDefn\n";
    ++depth_;
    for (const auto& s : n.settings()) {
      indent() << "FieldSetting &" << s.field_name << '\n';
      ++depth_;
      if (s.type_setting) {
        s.type_setting->accept(*this);
      }
      if (s.value_setting) {
        s.value_setting->accept(*this);
      }
      --depth_;
    }
    --depth_;
  }
  void visit(const ObjectSetDefn& n) override {
    indent() << "ObjectSetDefn" << (n.extensible() ? " extensible" : "") << '\n';
    ++depth_;
    for (const auto& e : n.elements()) {
      if (e.object_ref) {
        indent() << "ObjectRef " << *e.object_ref << '\n';
      } else if (e.inline_object) {
        e.inline_object->accept(*this);
      }
    }
    --depth_;
  }

  void visit(const BooleanType& n) override {
    type_line("BooleanType", n);
    print_constraint(n);
  }
  void visit(const IntegerType& n) override {
    type_line("IntegerType", n);
    ++depth_;
    for (const auto& nn : n.named_numbers()) {
      indent() << "NamedNumber " << nn.name << '('
               << (nn.negative ? "-" : "") << nn.number_text << ")\n";
    }
    if (n.constraint()) {
      n.constraint()->accept(*this);
    }
    --depth_;
  }
  void visit(const BitStringType& n) override {
    type_line("BitStringType", n);
    ++depth_;
    for (const auto& nn : n.named_bits()) {
      indent() << "NamedBit " << nn.name << '(' << nn.number_text << ")\n";
    }
    if (n.constraint()) {
      n.constraint()->accept(*this);
    }
    --depth_;
  }
  void visit(const OctetStringType& n) override {
    type_line("OctetStringType", n);
    print_constraint(n);
  }
  void visit(const NullType& n) override { type_line("NullType", n); }
  void visit(const StringType& n) override {
    indent() << "StringType " << string_kind_name(n.kind());
    print_tag(n);
    out_ << '\n';
    print_constraint(n);
  }
  void visit(const SequenceType& n) override {
    type_line("SequenceType", n);
    ++depth_;
    for (const auto& item : n.items()) {
      item->accept(*this);
    }
    if (n.constraint()) {
      n.constraint()->accept(*this);
    }
    --depth_;
  }
  void visit(const ChoiceType& n) override {
    type_line("ChoiceType", n);
    ++depth_;
    for (const auto& alt : n.alternatives()) {
      alt->accept(*this);
    }
    --depth_;
  }
  void visit(const SequenceOfType& n) override {
    type_line("SequenceOfType", n);
    ++depth_;
    n.element().accept(*this);
    if (n.constraint()) {
      n.constraint()->accept(*this);
    }
    --depth_;
  }
  void visit(const SetType& n) override {
    type_line("SetType", n);
    ++depth_;
    for (const auto& item : n.items()) {
      item->accept(*this);
    }
    if (n.constraint()) {
      n.constraint()->accept(*this);
    }
    --depth_;
  }
  void visit(const SetOfType& n) override {
    type_line("SetOfType", n);
    ++depth_;
    n.element().accept(*this);
    if (n.constraint()) {
      n.constraint()->accept(*this);
    }
    --depth_;
  }
  void visit(const EnumeratedType& n) override {
    type_line("EnumeratedType", n);
    ++depth_;
    for (const auto& nn : n.root()) {
      indent() << "EnumItem " << nn.name;
      if (!nn.number_text.empty()) {
        out_ << '(' << (nn.negative ? "-" : "") << nn.number_text << ')';
      }
      out_ << '\n';
    }
    if (n.extensible()) {
      indent() << "ExtensionMarker\n";
      for (const auto& nn : n.extensions()) {
        indent() << "EnumItem " << nn.name;
        if (!nn.number_text.empty()) {
          out_ << '(' << (nn.negative ? "-" : "") << nn.number_text << ')';
        }
        out_ << '\n';
      }
    }
    if (n.constraint()) {
      n.constraint()->accept(*this);
    }
    --depth_;
  }
  void visit(const ObjectIdentifierType& n) override {
    type_line("ObjectIdentifierType", n);
    print_constraint(n);
  }
  void visit(const RelativeOidType& n) override {
    type_line("RelativeOidType", n);
    print_constraint(n);
  }
  void visit(const RealType& n) override {
    type_line("RealType", n);
    print_constraint(n);
  }
  void visit(const ExternalType& n) override {
    type_line("ExternalType", n);
    print_constraint(n);
  }
  void visit(const EmbeddedPdvType& n) override {
    type_line("EmbeddedPdvType", n);
    print_constraint(n);
  }
  void visit(const CharacterStringType& n) override {
    type_line("CharacterStringType", n);
    print_constraint(n);
  }
  void visit(const InstanceOfType& n) override {
    type_line("InstanceOfType", n);
    indent() << "className " << n.class_name() << '\n';
    print_constraint(n);
  }
  void visit(const ReferencedType& n) override {
    indent() << "ReferencedType ";
    if (n.module()) {
      out_ << *n.module() << '.';
    }
    out_ << n.name();
    if (!n.actuals().empty()) {
      out_ << " {";
      for (std::size_t i = 0; i < n.actuals().size(); ++i) {
        if (i) {
          out_ << ", ";
        }
        const auto& a = n.actuals()[i];
        if (a.type) {
          out_ << "type";
        } else if (a.value) {
          out_ << "value";
        } else if (a.object_set_name) {
          out_ << "{" << *a.object_set_name << "}";
        } else if (a.inline_object_set) {
          out_ << "objectSet";
        } else {
          out_ << "?";
        }
      }
      out_ << '}';
    }
    print_tag(n);
    out_ << '\n';
    ++depth_;
    for (const auto& a : n.actuals()) {
      if (a.type) {
        a.type->accept(*this);
      }
      if (a.value) {
        a.value->accept(*this);
      }
    }
    --depth_;
    print_constraint(n);
  }
  void visit(const ObjectClassFieldType& n) override {
    indent() << "ObjectClassFieldType " << n.class_name() << ".&" << n.field_name();
    print_tag(n);
    out_ << '\n';
    print_constraint(n);
  }

  void visit(const Component& n) override {
    indent() << "Component " << n.name() << ' ' << presence_name(n.presence()) << '\n';
    ++depth_;
    n.type().accept(*this);
    if (n.default_value()) {
      n.default_value()->accept(*this);
    }
    --depth_;
  }

  void visit(const ExtensionMarker&) override { indent() << "ExtensionMarker\n"; }

  void visit(const VersionAdditionGroup& n) override {
    indent() << "VersionAdditionGroup\n";
    ++depth_;
    for (const auto& item : n.items()) {
      item->accept(*this);
    }
    --depth_;
  }

  void visit(const ValueRangeConstraint& n) override {
    indent() << "ValueRangeConstraint\n";
    ++depth_;
    if (n.lower()) {
      n.lower()->accept(*this);
    } else {
      indent() << "MIN\n";
    }
    if (n.upper()) {
      n.upper()->accept(*this);
    } else {
      indent() << "MAX\n";
    }
    --depth_;
  }
  void visit(const SingleValueConstraint& n) override {
    indent() << "SingleValueConstraint\n";
    ++depth_;
    n.value().accept(*this);
    --depth_;
  }
  void visit(const SizeConstraint& n) override {
    indent() << "SizeConstraint\n";
    ++depth_;
    n.inner().accept(*this);
    --depth_;
  }
  void visit(const UnionConstraint& n) override {
    indent() << "UnionConstraint\n";
    ++depth_;
    for (const auto& a : n.alternatives()) {
      a->accept(*this);
    }
    --depth_;
  }
  void visit(const IntersectionConstraint& n) override {
    indent() << "IntersectionConstraint\n";
    ++depth_;
    for (const auto& p : n.parts()) {
      p->accept(*this);
    }
    --depth_;
  }
  void visit(const ExtensibleConstraint& n) override {
    indent() << "ExtensibleConstraint\n";
    ++depth_;
    if (n.root()) {
      n.root()->accept(*this);
    }
    --depth_;
  }
  void visit(const ContentsConstraint& n) override {
    indent() << "ContentsConstraint\n";
    ++depth_;
    if (n.contained()) {
      indent() << "CONTAINING\n";
      ++depth_;
      n.contained()->accept(*this);
      --depth_;
    }
    if (n.encoded_by()) {
      indent() << "ENCODED BY\n";
      ++depth_;
      n.encoded_by()->accept(*this);
      --depth_;
    }
    --depth_;
  }
  void visit(const WithComponentsConstraint& n) override {
    indent() << "WithComponentsConstraint\n";
    ++depth_;
    for (const auto& c : n.components()) {
      indent() << "Component " << c.name;
      if (c.presence == ComponentPresence::Present) {
        out_ << " PRESENT";
      } else if (c.presence == ComponentPresence::Absent) {
        out_ << " ABSENT";
      } else if (c.presence == ComponentPresence::Optional) {
        out_ << " OPTIONAL";
      }
      out_ << '\n';
      if (c.value_constraint) {
        ++depth_;
        c.value_constraint->accept(*this);
        --depth_;
      }
    }
    --depth_;
  }
  void visit(const TableConstraint& n) override {
    indent() << "TableConstraint";
    if (!n.object_set_name().empty()) {
      out_ << ' ' << n.object_set_name();
    }
    out_ << '\n';
    ++depth_;
    if (n.inline_set()) {
      n.inline_set()->accept(*this);
    }
    --depth_;
  }
  void visit(const ComponentRelationConstraint& n) override {
    indent() << "ComponentRelationConstraint";
    if (!n.object_set_name().empty()) {
      out_ << ' ' << n.object_set_name();
    }
    for (const auto& c : n.at_components()) {
      out_ << " @" << c;
    }
    out_ << '\n';
    ++depth_;
    if (n.inline_set()) {
      n.inline_set()->accept(*this);
    }
    --depth_;
  }

  void visit(const IntegerValue& n) override {
    indent() << "IntegerValue " << (n.negative() ? "-" : "") << n.text() << '\n';
  }
  void visit(const BooleanValue& n) override {
    indent() << "BooleanValue " << (n.value() ? "TRUE" : "FALSE") << '\n';
  }
  void visit(const StringValue& n) override {
    indent() << "StringValue " << n.text() << '\n';
  }
  void visit(const BitOrOctetValue& n) override {
    indent() << (n.kind() == BitOrOctetKind::Binary ? "BinaryValue " : "HexValue ")
             << n.text() << '\n';
  }
  void visit(const ValueReference& n) override {
    indent() << "ValueReference " << n.name() << '\n';
  }
  void visit(const NamedValue& n) override {
    indent() << "NamedValue " << n.name() << '\n';
    ++depth_;
    n.value().accept(*this);
    --depth_;
  }
  void visit(const NamedValueList& n) override {
    indent() << "NamedValueList\n";
    ++depth_;
    for (const auto& v : n.values()) {
      v->accept(*this);
    }
    --depth_;
  }
  void visit(const ObjectIdentifierValue& n) override {
    indent() << "ObjectIdentifierValue";
    for (const auto& a : n.arcs()) {
      out_ << ' ';
      if (a.name) {
        out_ << *a.name;
      }
      if (a.number) {
        if (a.name) {
          out_ << '(' << *a.number << ')';
        } else {
          out_ << *a.number;
        }
      }
    }
    out_ << '\n';
  }

 private:
  std::ostream& indent() {
    for (int i = 0; i < depth_; ++i) {
      out_ << "  ";
    }
    return out_;
  }

  static const char* tag_default_name(TagDefault d) {
    switch (d) {
      case TagDefault::Explicit:
        return "EXPLICIT";
      case TagDefault::Implicit:
        return "IMPLICIT";
      case TagDefault::Automatic:
        return "AUTOMATIC";
    }
    return "?";
  }

  static const char* presence_name(Presence p) {
    switch (p) {
      case Presence::Mandatory:
        return "mandatory";
      case Presence::Optional:
        return "optional";
      case Presence::Default:
        return "default";
    }
    return "?";
  }

  static const char* string_kind_name(StringKind k) {
    switch (k) {
      case StringKind::UTF8String:
        return "UTF8String";
      case StringKind::IA5String:
        return "IA5String";
      case StringKind::PrintableString:
        return "PrintableString";
      case StringKind::VisibleString:
        return "VisibleString";
      case StringKind::NumericString:
        return "NumericString";
      case StringKind::TeletexString:
        return "TeletexString";
      case StringKind::VideotexString:
        return "VideotexString";
      case StringKind::GraphicString:
        return "GraphicString";
      case StringKind::GeneralString:
        return "GeneralString";
      case StringKind::BMPString:
        return "BMPString";
      case StringKind::UniversalString:
        return "UniversalString";
      case StringKind::UTCTime:
        return "UTCTime";
      case StringKind::GeneralizedTime:
        return "GeneralizedTime";
    }
    return "?";
  }

  void print_tag(const Type& n) {
    if (!n.tag()) {
      return;
    }
    const Tag& t = *n.tag();
    out_ << " tag=[";
    switch (t.cls) {
      case TagClass::Universal:
        out_ << "UNIVERSAL ";
        break;
      case TagClass::Application:
        out_ << "APPLICATION ";
        break;
      case TagClass::Private:
        out_ << "PRIVATE ";
        break;
      case TagClass::Context:
        break;
    }
    out_ << t.number_text << ']';
    if (t.mode) {
      out_ << (*t.mode == TagMode::Explicit ? " EXPLICIT" : " IMPLICIT");
    }
  }

  static const char* ei_kind_brief(EncodingInstructionKind k) {
    switch (k) {
      case EncodingInstructionKind::Array:
        return "ARRAY";
      case EncodingInstructionKind::Base64:
        return "BASE64";
      case EncodingInstructionKind::Object:
        return "OBJECT";
      case EncodingInstructionKind::Unwrapped:
        return "UNWRAPPED";
      case EncodingInstructionKind::Name:
        return "NAME";
      case EncodingInstructionKind::Text:
        return "TEXT";
      case EncodingInstructionKind::Attribute:
        return "ATTRIBUTE";
      case EncodingInstructionKind::UseNumber:
        return "USE-NUMBER";
      case EncodingInstructionKind::List:
        return "LIST";
      case EncodingInstructionKind::Untagged:
        return "UNTAGGED";
      case EncodingInstructionKind::UseNil:
        return "USE-NIL";
    }
    return "?";
  }

  void print_encoding_instructions(const Type& n) {
    if (n.encoding_instructions().empty()) {
      return;
    }
    indent() << "encodingInstructions";
    for (const auto& ei : n.encoding_instructions()) {
      out_ << ' ' << ei_kind_brief(ei.kind);
    }
    out_ << '\n';
  }

  void type_line(const char* name, const Type& n) {
    indent() << name;
    print_tag(n);
    out_ << '\n';
    print_encoding_instructions(n);
  }

  void print_constraint(const Type& n) {
    if (!n.constraint()) {
      return;
    }
    ++depth_;
    n.constraint()->accept(*this);
    --depth_;
  }

  std::ostream& out_;
  int depth_ = 0;
};

}  // namespace

void print(std::ostream& out, const Module& module) {
  Printer p(out);
  module.accept(p);
}

std::string to_string(const Module& module) {
  std::ostringstream oss;
  print(oss, module);
  return oss.str();
}

}  // namespace ast
}  // namespace asn1
