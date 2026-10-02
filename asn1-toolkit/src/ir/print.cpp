#include <asn1/ir/print.hpp>

#include <sstream>

namespace asn1 {
namespace ir {
namespace {

const char* tag_class_name(TagClass c) {
  switch (c) {
    case TagClass::Universal:
      return "UNIVERSAL";
    case TagClass::Application:
      return "APPLICATION";
    case TagClass::Context:
      return "CONTEXT";
    case TagClass::Private:
      return "PRIVATE";
  }
  return "?";
}

const char* kind_name(TypeKind k) {
  switch (k) {
    case TypeKind::Boolean:
      return "Boolean";
    case TypeKind::Integer:
      return "Integer";
    case TypeKind::BitString:
      return "BitString";
    case TypeKind::OctetString:
      return "OctetString";
    case TypeKind::Null:
      return "Null";
    case TypeKind::String:
      return "String";
    case TypeKind::Sequence:
      return "Sequence";
    case TypeKind::Choice:
      return "Choice";
    case TypeKind::SequenceOf:
      return "SequenceOf";
    case TypeKind::Set:
      return "Set";
    case TypeKind::SetOf:
      return "SetOf";
    case TypeKind::Enumerated:
      return "Enumerated";
    case TypeKind::ObjectIdentifier:
      return "ObjectIdentifier";
    case TypeKind::RelativeOid:
      return "RelativeOid";
    case TypeKind::Real:
      return "Real";
    case TypeKind::Referenced:
      return "Referenced";
  }
  return "?";
}

const char* presence_name(Presence p) {
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

void print_tag(std::ostream& out, const Tag& t) {
  out << '[' << tag_class_name(t.cls) << ' ' << t.number << ']'
      << (t.is_explicit ? " EXPLICIT" : " IMPLICIT");
}

void print_constraint(std::ostream& out, const ConstraintDesc& c, int depth) {
  if (c.root_ranges.empty() && !c.lower && !c.upper && !c.extensible && !c.is_size &&
      !c.empty) {
    return;
  }
  for (int i = 0; i < depth; ++i) {
    out << "  ";
  }
  out << (c.is_size ? "Size" : "Constraint");
  if (c.empty) {
    out << "(EMPTY)";
  } else if (c.root_ranges.empty()) {
    out << '(';
    if (c.lower) {
      out << c.lower->to_string();
    } else {
      out << "MIN";
    }
    out << "..";
    if (c.upper) {
      out << c.upper->to_string();
    } else {
      out << "MAX";
    }
    out << ')';
  } else {
    out << '{';
    for (std::size_t i = 0; i < c.root_ranges.size(); ++i) {
      if (i) {
        out << " | ";
      }
      const auto& iv = c.root_ranges[i];
      if (iv.lower) {
        out << iv.lower->to_string();
      } else {
        out << "MIN";
      }
      if (iv.lower && iv.upper && *iv.lower == *iv.upper) {
        // single value
      } else {
        out << "..";
        if (iv.upper) {
          out << iv.upper->to_string();
        } else {
          out << "MAX";
        }
      }
    }
    out << '}';
  }
  switch (c.per_kind()) {
    case PerBoundKind::Unconstrained:
      out << " per=unconstrained";
      break;
    case PerBoundKind::SemiConstrained:
      out << " per=semi-constrained";
      break;
    case PerBoundKind::Constrained:
      out << " per=constrained";
      if (auto span = c.constrained_span()) {
        out << " span=" << *span;
      }
      break;
  }
  if (c.extensible) {
    out << " extensible";
  }
  if (!c.statically_foldable) {
    out << " !static";
  }
  out << '\n';
}

void print_field(std::ostream& out, const Field& f, int depth) {
  for (int i = 0; i < depth; ++i) {
    out << "  ";
  }
  out << "Field " << f.name << ' ' << presence_name(f.presence) << " type#" << f.type
      << ' ';
  print_tag(out, f.tag);
  out << '\n';
}

void print_type(std::ostream& out, const TypeArena& arena, TypeId id, int depth) {
  const Type& t = arena.get(id);
  for (int i = 0; i < depth; ++i) {
    out << "  ";
  }
  out << "Type#" << id << ' ' << kind_name(t.kind);
  if (!t.name.empty()) {
    out << ' ' << t.module << "::" << t.name;
  }
  if (t.recursive) {
    out << " recursive";
  }
  out << ' ';
  print_tag(out, t.tag);
  out << '\n';

  switch (t.kind) {
    case TypeKind::Integer:
      print_constraint(out, t.integer.constraint, depth + 1);
      if (t.integer.host_bits) {
        for (int i = 0; i < depth + 1; ++i) {
          out << "  ";
        }
        out << "host_bits=" << *t.integer.host_bits
            << (t.integer.is_signed ? " signed" : " unsigned") << '\n';
      }
      for (const auto& nn : t.integer.named_numbers) {
        for (int i = 0; i < depth + 1; ++i) {
          out << "  ";
        }
        out << "NamedNumber " << nn.name << '=' << nn.value.to_string() << '\n';
      }
      break;
    case TypeKind::BitString:
      print_constraint(out, t.bit_string.size, depth + 1);
      break;
    case TypeKind::OctetString:
      print_constraint(out, t.octet_string.size, depth + 1);
      break;
    case TypeKind::String:
      print_constraint(out, t.string.size, depth + 1);
      break;
    case TypeKind::Sequence:
      for (const auto& f : t.sequence.root) {
        print_field(out, f, depth + 1);
      }
      if (t.sequence.extensible) {
        for (int i = 0; i < depth + 1; ++i) {
          out << "  ";
        }
        out << "...\n";
      }
      break;
    case TypeKind::Choice:
      for (const auto& f : t.choice.alternatives) {
        print_field(out, f, depth + 1);
      }
      break;
    case TypeKind::SequenceOf:
      for (int i = 0; i < depth + 1; ++i) {
        out << "  ";
      }
      out << "element type#" << t.sequence_of.element << '\n';
      print_constraint(out, t.sequence_of.size, depth + 1);
      break;
    case TypeKind::Set:
      for (const auto& f : t.set.root) {
        print_field(out, f, depth + 1);
      }
      if (t.set.extensible) {
        for (int i = 0; i < depth + 1; ++i) {
          out << "  ";
        }
        out << "...\n";
      }
      break;
    case TypeKind::SetOf:
      for (int i = 0; i < depth + 1; ++i) {
        out << "  ";
      }
      out << "element type#" << t.set_of.element << '\n';
      print_constraint(out, t.set_of.size, depth + 1);
      break;
    case TypeKind::Enumerated:
      for (const auto& nn : t.enumerated.root) {
        for (int i = 0; i < depth + 1; ++i) {
          out << "  ";
        }
        out << "Enum " << nn.name << '=' << nn.value.to_string() << '\n';
      }
      if (t.enumerated.extensible) {
        for (int i = 0; i < depth + 1; ++i) {
          out << "  ";
        }
        out << "...\n";
        for (const auto& nn : t.enumerated.extensions) {
          for (int i = 0; i < depth + 1; ++i) {
            out << "  ";
          }
          out << "Enum " << nn.name << '=' << nn.value.to_string() << '\n';
        }
      }
      break;
    case TypeKind::ObjectIdentifier:
    case TypeKind::RelativeOid:
      break;
    case TypeKind::Real:
      break;
    case TypeKind::Referenced:
      for (int i = 0; i < depth + 1; ++i) {
        out << "  ";
      }
      out << "ref " << t.referenced.module << "::" << t.referenced.name << " -> #"
          << t.referenced.resolved << '\n';
      break;
    default:
      break;
  }
}

}  // namespace

void print(std::ostream& out, const Model& model) {
  for (const auto& mod : model.modules) {
    out << "Module " << mod.name << " tagDefault=";
    switch (mod.tag_default) {
      case ModuleInfo::TagDefault::Explicit:
        out << "EXPLICIT";
        break;
      case ModuleInfo::TagDefault::Implicit:
        out << "IMPLICIT";
        break;
      case ModuleInfo::TagDefault::Automatic:
        out << "AUTOMATIC";
        break;
    }
    out << '\n';
    for (TypeId id : mod.types) {
      print_type(out, model.arena, id, 1);
    }
  }
}

std::string to_string(const Model& model) {
  std::ostringstream oss;
  print(oss, model);
  return oss.str();
}

}  // namespace ir
}  // namespace asn1
