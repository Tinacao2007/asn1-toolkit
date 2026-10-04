/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/src/ir/print.cpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   Pretty-prints lowered type IR (arena types, constraints, tags) for
**   --dump-ir.
**
** Specification: Internal type IR (lowering target for X.680/X.681
**                 constructs; not a wire standard).
** Design Spec:   asn1-toolkit/docs/ARCHITECTURE.md
**                 asn1-toolkit/README.md
***************************************************************************/
#include <asn1/ir/print.hpp>

#include <sstream>

namespace asn1 {
namespace ir {
namespace {

/**
 *  Function    : tag_class_name
 *  Description : Computes tag class name from (c).
 *  Parameters  : c — TagClass c
 *  Returns     : const char*
 */
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

/**
 *  Function    : kind_name
 *  Description : Computes kind name from (k).
 *  Parameters  : k — TypeKind k
 *  Returns     : const char*
 */
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
    case TypeKind::ObjectClassField:
      return "ObjectClassField";
    case TypeKind::InstanceOf:
      return "InstanceOf";
    case TypeKind::Referenced:
      return "Referenced";
  }
  return "?";
}

/**
 *  Function    : presence_name
 *  Description : Computes presence name from (p).
 *  Parameters  : p — Presence p
 *  Returns     : const char*
 */
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

/**
 *  Function    : print_tag
 *  Description : Performs print tag (definition).
 *  Parameters  : out — std::ostream& out; t — const Tag& t
 *  Returns     : void
 */
void print_tag(std::ostream& out, const Tag& t) {
  out << '[' << tag_class_name(t.cls) << ' ' << t.number << ']'
      << (t.is_explicit ? " EXPLICIT" : " IMPLICIT");
}

/**
 *  Function    : print_constraint
 *  Description : Performs print constraint (definition).
 *  Parameters  : out — std::ostream& out; c — const ConstraintDesc& c; depth — int depth
 *  Returns     : void
 */
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

/**
 *  Function    : print_exer_flags
 *  Description : Performs print exer flags (definition).
 *  Parameters  : out — std::ostream& out; exer — const ExerEncoding& exer; depth — int depth
 *  Returns     : void
 */
void print_exer_flags(std::ostream& out, const ExerEncoding& exer, int depth) {
  if (!exer.attribute && !exer.base64 && !exer.text && !exer.use_number && !exer.list &&
      !exer.untagged && !exer.use_nil) {
    return;
  }
  for (int i = 0; i < depth; ++i) {
    out << "  ";
  }
  out << "exer";
  if (exer.attribute) {
    out << " attribute";
  }
  if (exer.base64) {
    out << " base64";
  }
  if (exer.text) {
    out << " text";
  }
  if (exer.use_number) {
    out << " use-number";
  }
  if (exer.list) {
    out << " list";
  }
  if (exer.untagged) {
    out << " untagged";
  }
  if (exer.use_nil) {
    out << " use-nil";
  }
  out << '\n';
}

/**
 *  Function    : print_jer_flags
 *  Description : Performs print jer flags (definition).
 *  Parameters  : out — std::ostream& out; jer — const JerEncoding& jer; depth — int depth
 *  Returns     : void
 */
void print_jer_flags(std::ostream& out, const JerEncoding& jer, int depth) {
  if (!jer.array && !jer.base64 && !jer.object && !jer.unwrapped &&
      jer.text_form == JerEncoding::TextForm::AsIs) {
    return;
  }
  for (int i = 0; i < depth; ++i) {
    out << "  ";
  }
  out << "jer";
  if (jer.array) {
    out << " array";
  }
  if (jer.base64) {
    out << " base64";
  }
  if (jer.object) {
    out << " object";
  }
  if (jer.unwrapped) {
    out << " unwrapped";
  }
  if (jer.text_form != JerEncoding::TextForm::AsIs) {
    out << " text";
  }
  out << '\n';
}

/**
 *  Function    : print_field
 *  Description : Performs print field (definition).
 *  Parameters  : out — std::ostream& out; f — const Field& f; depth — int depth
 *  Returns     : void
 */
void print_field(std::ostream& out, const Field& f, int depth) {
  for (int i = 0; i < depth; ++i) {
    out << "  ";
  }
  out << "Field " << f.name << ' ' << presence_name(f.presence) << " type#" << f.type
      << ' ';
  print_tag(out, f.tag);
  if (f.jer_name_form != Field::JerNameForm::AsIs) {
    out << " jerName";
  }
  if (f.exer.attribute) {
    out << " exerAttribute";
  }
  if (f.exer.untagged) {
    out << " exerUntagged";
  }
  if (f.exer.use_nil) {
    out << " exerUseNil";
  }
  out << '\n';
}

/**
 *  Function    : print_type
 *  Description : Performs print type (definition).
 *  Parameters  : out — std::ostream& out; arena — const TypeArena& arena; id — TypeId id; depth — int depth
 *  Returns     : void
 */
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
  print_jer_flags(out, t.jer, depth + 1);
  print_exer_flags(out, t.exer, depth + 1);

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
      if (t.bit_string.containing != kInvalidType) {
        for (int i = 0; i < depth + 1; ++i) {
          out << "  ";
        }
        out << "CONTAINING type#" << t.bit_string.containing << '\n';
      }
      break;
    case TypeKind::OctetString:
      print_constraint(out, t.octet_string.size, depth + 1);
      if (t.octet_string.containing != kInvalidType) {
        for (int i = 0; i < depth + 1; ++i) {
          out << "  ";
        }
        out << "CONTAINING type#" << t.octet_string.containing << '\n';
      }
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
        for (const auto& group : t.sequence.extension_groups) {
          for (const auto& f : group) {
            print_field(out, f, depth + 1);
          }
        }
        for (const auto& f : t.sequence.trailing_root) {
          print_field(out, f, depth + 1);
        }
      }
      break;
    case TypeKind::Choice:
      for (const auto& f : t.choice.alternatives) {
        print_field(out, f, depth + 1);
      }
      if (t.choice.extensible) {
        for (int i = 0; i < depth + 1; ++i) {
          out << "  ";
        }
        out << "...\n";
        for (const auto& f : t.choice.extensions) {
          print_field(out, f, depth + 1);
        }
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
        for (const auto& group : t.set.extension_groups) {
          for (const auto& f : group) {
            print_field(out, f, depth + 1);
          }
        }
        for (const auto& f : t.set.trailing_root) {
          print_field(out, f, depth + 1);
        }
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
      for (int i = 0; i < depth + 1; ++i) {
        out << "  ";
      }
      out << "ieeeForm=";
      switch (t.real.ieee_form) {
        case RealIeeeForm::Unconstrained:
          out << "unconstrained";
          break;
        case RealIeeeForm::Binary32:
          out << "binary32";
          break;
        case RealIeeeForm::Binary64:
          out << "binary64";
          break;
      }
      out << '\n';
      break;
    case TypeKind::ObjectClassField:
      for (int i = 0; i < depth + 1; ++i) {
        out << "  ";
      }
      out << t.object_class_field.class_name << ".&" << t.object_class_field.field_name;
      if (t.object_class_field.open_type) {
        out << " openType";
      } else {
        out << " fixed type#" << t.object_class_field.fixed_type;
      }
      out << '\n';
      break;
    case TypeKind::InstanceOf:
      for (int i = 0; i < depth + 1; ++i) {
        out << "  ";
      }
      out << "INSTANCE OF " << t.instance_of.class_name << '\n';
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

/**
 *  Function    : print
 *  Description : Performs print (definition).
 *  Parameters  : out — std::ostream& out; model — const Model& model
 *  Returns     : void
 */
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
    if (mod.jer_instructions) {
      out << "  jerInstructions=true\n";
    }
    if (mod.xer_instructions) {
      out << "  xerInstructions=true\n";
    }
    out << '\n';
    for (TypeId id : mod.types) {
      print_type(out, model.arena, id, 1);
    }
  }
}

/**
 *  Function    : to_string
 *  Description : Builds and returns a string for to string.
 *  Parameters  : model — const Model& model
 *  Returns     : std::string
 */
std::string to_string(const Model& model) {
  std::ostringstream oss;
  print(oss, model);
  return oss.str();
}

}  // namespace ir
}  // namespace asn1
