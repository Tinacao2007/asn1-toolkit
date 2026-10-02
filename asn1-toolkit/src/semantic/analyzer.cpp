#include <asn1/semantic/analyzer.hpp>

#include <asn1/ast/type.hpp>
#include <asn1/constraints/normalize.hpp>

#include <cstdint>
#include <unordered_map>
#include <unordered_set>

namespace asn1 {
namespace {

ir::Tag universal(std::uint64_t number) {
  return ir::Tag{ir::TagClass::Universal, number, false};
}

ir::Tag builtin_tag_for(const ast::Type& type) {
  if (dynamic_cast<const ast::BooleanType*>(&type)) {
    return universal(1);
  }
  if (dynamic_cast<const ast::IntegerType*>(&type)) {
    return universal(2);
  }
  if (dynamic_cast<const ast::BitStringType*>(&type)) {
    return universal(3);
  }
  if (dynamic_cast<const ast::OctetStringType*>(&type)) {
    return universal(4);
  }
  if (dynamic_cast<const ast::NullType*>(&type)) {
    return universal(5);
  }
  if (dynamic_cast<const ast::ObjectIdentifierType*>(&type)) {
    return universal(6);
  }
  if (dynamic_cast<const ast::RealType*>(&type)) {
    return universal(9);
  }
  if (dynamic_cast<const ast::EnumeratedType*>(&type)) {
    return universal(10);
  }
  if (dynamic_cast<const ast::RelativeOidType*>(&type)) {
    return universal(13);
  }
  if (dynamic_cast<const ast::SequenceOfType*>(&type) ||
      dynamic_cast<const ast::SequenceType*>(&type)) {
    return universal(16);
  }
  if (dynamic_cast<const ast::SetOfType*>(&type) ||
      dynamic_cast<const ast::SetType*>(&type)) {
    return universal(17);
  }
  if (const auto* s = dynamic_cast<const ast::StringType*>(&type)) {
    switch (s->kind()) {
      case ast::StringKind::UTF8String:
        return universal(12);
      case ast::StringKind::NumericString:
        return universal(18);
      case ast::StringKind::PrintableString:
        return universal(19);
      case ast::StringKind::TeletexString:
        return universal(20);
      case ast::StringKind::VideotexString:
        return universal(21);
      case ast::StringKind::IA5String:
        return universal(22);
      case ast::StringKind::GraphicString:
        return universal(25);
      case ast::StringKind::VisibleString:
        return universal(26);
      case ast::StringKind::GeneralString:
        return universal(27);
      case ast::StringKind::UniversalString:
        return universal(28);
      case ast::StringKind::BMPString:
        return universal(30);
    }
  }
  return universal(0);
}

bool parse_u64(const std::string& text, std::uint64_t& out) {
  if (text.empty()) {
    return false;
  }
  out = 0;
  for (char c : text) {
    if (c < '0' || c > '9') {
      return false;
    }
    const std::uint64_t digit = static_cast<std::uint64_t>(c - '0');
    if (out > (UINT64_MAX - digit) / 10u) {
      return false;
    }
    out = out * 10u + digit;
  }
  return true;
}

ir::StringKind map_string_kind(ast::StringKind k) {
  switch (k) {
    case ast::StringKind::UTF8String:
      return ir::StringKind::UTF8String;
    case ast::StringKind::IA5String:
      return ir::StringKind::IA5String;
    case ast::StringKind::PrintableString:
      return ir::StringKind::PrintableString;
    case ast::StringKind::VisibleString:
      return ir::StringKind::VisibleString;
    case ast::StringKind::NumericString:
      return ir::StringKind::NumericString;
    case ast::StringKind::TeletexString:
      return ir::StringKind::TeletexString;
    case ast::StringKind::VideotexString:
      return ir::StringKind::VideotexString;
    case ast::StringKind::GraphicString:
      return ir::StringKind::GraphicString;
    case ast::StringKind::GeneralString:
      return ir::StringKind::GeneralString;
    case ast::StringKind::BMPString:
      return ir::StringKind::BMPString;
    case ast::StringKind::UniversalString:
      return ir::StringKind::UniversalString;
  }
  return ir::StringKind::UTF8String;
}

ir::Tag make_ast_tag(const ast::Tag& t, ast::TagDefault module_default, bool force_explicit) {
  ir::Tag out;
  switch (t.cls) {
    case ast::TagClass::Universal:
      out.cls = ir::TagClass::Universal;
      break;
    case ast::TagClass::Application:
      out.cls = ir::TagClass::Application;
      break;
    case ast::TagClass::Context:
      out.cls = ir::TagClass::Context;
      break;
    case ast::TagClass::Private:
      out.cls = ir::TagClass::Private;
      break;
  }
  parse_u64(t.number_text, out.number);
  if (force_explicit) {
    out.is_explicit = true;
  } else if (t.mode) {
    out.is_explicit = (*t.mode == ast::TagMode::Explicit);
  } else {
    out.is_explicit = (module_default == ast::TagDefault::Explicit);
  }
  return out;
}

ir::Tag component_tag(const ast::Type& field_type, ast::TagDefault module_default,
                      std::uint64_t auto_index) {
  const bool is_choice = dynamic_cast<const ast::ChoiceType*>(&field_type) != nullptr;
  if (field_type.tag()) {
    return make_ast_tag(*field_type.tag(), module_default, is_choice);
  }
  if (module_default == ast::TagDefault::Automatic) {
    return ir::Tag{ir::TagClass::Context, auto_index, is_choice};
  }
  return builtin_tag_for(field_type);
}

struct LowerCtx {
  ir::Model& model;
  SymbolTable& symbols;
  Diagnostics& diagnostics;
  const ast::Module& mod;
  std::unordered_set<std::string> resolving;
  std::unordered_map<std::string, ir::TypeId> named_ids;
};

ir::TypeId lower_type(LowerCtx& ctx, const ast::Type& type, const std::string& assigned_name);

ir::Presence map_presence(ast::Presence p) {
  switch (p) {
    case ast::Presence::Mandatory:
      return ir::Presence::Mandatory;
    case ast::Presence::Optional:
      return ir::Presence::Optional;
    case ast::Presence::Default:
      return ir::Presence::Default;
  }
  return ir::Presence::Mandatory;
}

void apply_outer_tag(ir::Type& out, const ast::Type& type, ast::TagDefault tag_default) {
  out.tag = builtin_tag_for(type);
  if (type.tag()) {
    const bool is_choice = dynamic_cast<const ast::ChoiceType*>(&type) != nullptr;
    out.tag = make_ast_tag(*type.tag(), tag_default, is_choice);
  }
}

ir::TypeId lower_type(LowerCtx& ctx, const ast::Type& type, const std::string& assigned_name) {
  if (const auto* ref = dynamic_cast<const ast::ReferencedType*>(&type)) {
    std::string owner = ref->module() ? *ref->module() : ctx.mod.name();
    std::string name = ref->name();
    Symbol* sym = ctx.symbols.lookup(owner, name);
    if (sym && sym->is_import) {
      owner = sym->import_from_module;
      sym = ctx.symbols.find(owner, name);
    }
    const std::string key = owner + "::" + name;

    if (auto it = ctx.named_ids.find(key); it != ctx.named_ids.end()) {
      return it->second;
    }

    if (sym && sym->kind == SymbolKind::Type && sym->type_id != ir::kInvalidType) {
      return sym->type_id;
    }

    if (sym && sym->kind == SymbolKind::Type && !sym->is_import) {
      if (ctx.resolving.count(key)) {
        ir::Type placeholder;
        placeholder.kind = ir::TypeKind::Referenced;
        placeholder.module = owner;
        placeholder.name = name;
        placeholder.recursive = true;
        placeholder.referenced.module = owner;
        placeholder.referenced.name = name;
        const ir::TypeId id = ctx.model.arena.add(std::move(placeholder));
        ctx.named_ids[key] = id;
        return id;
      }
      const auto* ta = dynamic_cast<const ast::TypeAssignment*>(sym->ast);
      if (ta) {
        ctx.resolving.insert(key);
        ir::TypeId id;
        if (owner == ctx.mod.name()) {
          id = lower_type(ctx, ta->type(), ta->name());
        } else {
          const Symbol* mod_sym = ctx.symbols.find(owner, owner);
          const ast::Module* other =
              mod_sym ? dynamic_cast<const ast::Module*>(mod_sym->ast) : nullptr;
          if (!other) {
            ctx.diagnostics.error(ref->range(),
                                  "cannot lower imported type '" + name + "'");
            ctx.resolving.erase(key);
            return ctx.model.arena.add(ir::Type{});
          }
          LowerCtx other_ctx{ctx.model, ctx.symbols, ctx.diagnostics, *other,
                             ctx.resolving, ctx.named_ids};
          id = lower_type(other_ctx, ta->type(), ta->name());
          ctx.resolving = std::move(other_ctx.resolving);
          ctx.named_ids = std::move(other_ctx.named_ids);
        }
        ctx.resolving.erase(key);
        ctx.model.arena.get(id).module = owner;
        ctx.model.arena.get(id).name = ta->name();
        sym->type_id = id;
        ctx.named_ids[key] = id;
        for (std::size_t i = 0; i < ctx.model.arena.type_count(); ++i) {
          ir::Type& t = ctx.model.arena.get(static_cast<ir::TypeId>(i));
          if (t.kind == ir::TypeKind::Referenced && t.referenced.module == owner &&
              t.referenced.name == name) {
            t.referenced.resolved = id;
          }
        }
        return id;
      }
    }

    ir::Type t;
    t.kind = ir::TypeKind::Referenced;
    t.module = owner;
    t.name = assigned_name;
    t.referenced.module = owner;
    t.referenced.name = name;
    return ctx.model.arena.add(std::move(t));
  }

  ir::Type out;
  out.module = ctx.mod.name();
  out.name = assigned_name;
  apply_outer_tag(out, type, ctx.mod.tag_default());

  if (dynamic_cast<const ast::BooleanType*>(&type)) {
    out.kind = ir::TypeKind::Boolean;
  } else if (const auto* integer = dynamic_cast<const ast::IntegerType*>(&type)) {
    out.kind = ir::TypeKind::Integer;
    out.integer.constraint =
        constraints::normalize(integer->constraint(), ctx.diagnostics);
    for (const auto& nn : integer->named_numbers()) {
      out.integer.named_numbers.push_back(
          ir::NamedNumber{nn.name, ir::BigInt::from_decimal(nn.number_text, nn.negative)});
    }
    constraints::suggest_host_integer(out.integer);
  } else if (const auto* bits = dynamic_cast<const ast::BitStringType*>(&type)) {
    out.kind = ir::TypeKind::BitString;
    out.bit_string.size =
        constraints::normalize_size(bits->constraint(), ctx.diagnostics);
    for (const auto& nn : bits->named_bits()) {
      out.bit_string.named_bits.push_back(
          ir::NamedNumber{nn.name, ir::BigInt::from_decimal(nn.number_text, false)});
    }
  } else if (const auto* octets = dynamic_cast<const ast::OctetStringType*>(&type)) {
    out.kind = ir::TypeKind::OctetString;
    out.octet_string.size =
        constraints::normalize_size(octets->constraint(), ctx.diagnostics);
  } else if (dynamic_cast<const ast::NullType*>(&type)) {
    out.kind = ir::TypeKind::Null;
  } else if (const auto* str = dynamic_cast<const ast::StringType*>(&type)) {
    out.kind = ir::TypeKind::String;
    out.string.kind = map_string_kind(str->kind());
    out.string.size = constraints::normalize_size(str->constraint(), ctx.diagnostics);
  } else if (const auto* seq = dynamic_cast<const ast::SequenceType*>(&type)) {
    out.kind = ir::TypeKind::Sequence;
    std::uint64_t auto_index = 0;
    std::vector<ir::Field>* dest = &out.sequence.root;
    for (const auto& item : seq->items()) {
      if (dynamic_cast<const ast::ExtensionMarker*>(item.get())) {
        out.sequence.extensible = true;
        out.sequence.extension_groups.emplace_back();
        dest = &out.sequence.extension_groups.back();
        continue;
      }
      const auto* comp = dynamic_cast<const ast::Component*>(item.get());
      if (!comp) {
        continue;
      }
      ir::Field field;
      field.name = comp->name();
      field.presence = map_presence(comp->presence());
      field.tag = component_tag(comp->type(), ctx.mod.tag_default(), auto_index);
      ++auto_index;
      field.type = lower_type(ctx, comp->type(), "");
      dest->push_back(std::move(field));
    }
  } else if (const auto* ch = dynamic_cast<const ast::ChoiceType*>(&type)) {
    out.kind = ir::TypeKind::Choice;
    std::uint64_t auto_index = 0;
    for (const auto& item : ch->alternatives()) {
      if (dynamic_cast<const ast::ExtensionMarker*>(item.get())) {
        out.choice.extensible = true;
        continue;
      }
      const auto* comp = dynamic_cast<const ast::Component*>(item.get());
      if (!comp) {
        continue;
      }
      ir::Field field;
      field.name = comp->name();
      field.presence = ir::Presence::Mandatory;
      field.tag = component_tag(comp->type(), ctx.mod.tag_default(), auto_index);
      ++auto_index;
      field.type = lower_type(ctx, comp->type(), "");
      out.choice.alternatives.push_back(std::move(field));
    }
  } else if (const auto* of = dynamic_cast<const ast::SequenceOfType*>(&type)) {
    out.kind = ir::TypeKind::SequenceOf;
    out.sequence_of.element = lower_type(ctx, of->element(), "");
    out.sequence_of.size =
        constraints::normalize_size(of->constraint(), ctx.diagnostics);
  } else if (const auto* set = dynamic_cast<const ast::SetType*>(&type)) {
    out.kind = ir::TypeKind::Set;
    std::uint64_t auto_index = 0;
    std::vector<ir::Field>* dest = &out.set.root;
    for (const auto& item : set->items()) {
      if (dynamic_cast<const ast::ExtensionMarker*>(item.get())) {
        out.set.extensible = true;
        out.set.extension_groups.emplace_back();
        dest = &out.set.extension_groups.back();
        continue;
      }
      const auto* comp = dynamic_cast<const ast::Component*>(item.get());
      if (!comp) {
        continue;
      }
      ir::Field field;
      field.name = comp->name();
      field.presence = map_presence(comp->presence());
      field.tag = component_tag(comp->type(), ctx.mod.tag_default(), auto_index);
      ++auto_index;
      field.type = lower_type(ctx, comp->type(), "");
      dest->push_back(std::move(field));
    }
  } else if (const auto* setof = dynamic_cast<const ast::SetOfType*>(&type)) {
    out.kind = ir::TypeKind::SetOf;
    out.set_of.element = lower_type(ctx, setof->element(), "");
    out.set_of.size =
        constraints::normalize_size(setof->constraint(), ctx.diagnostics);
  } else if (const auto* en = dynamic_cast<const ast::EnumeratedType*>(&type)) {
    out.kind = ir::TypeKind::Enumerated;
    auto assign_enum = [](const std::vector<ast::NamedNumber>& in,
                          std::vector<ir::NamedNumber>& dest) {
      std::int64_t next = 0;
      bool have_next = true;
      for (const auto& nn : in) {
        ir::NamedNumber out_nn;
        out_nn.name = nn.name;
        if (!nn.number_text.empty()) {
          out_nn.value = ir::BigInt::from_decimal(nn.number_text, nn.negative);
          if (out_nn.value.as_i64) {
            next = *out_nn.value.as_i64 + 1;
            have_next = true;
          } else {
            have_next = false;
          }
        } else if (have_next) {
          out_nn.value = ir::BigInt::from_i64(next);
          ++next;
        } else {
          out_nn.value = ir::BigInt::from_i64(0);
        }
        dest.push_back(std::move(out_nn));
      }
    };
    assign_enum(en->root(), out.enumerated.root);
    out.enumerated.extensible = en->extensible();
    assign_enum(en->extensions(), out.enumerated.extensions);
  } else if (dynamic_cast<const ast::ObjectIdentifierType*>(&type)) {
    out.kind = ir::TypeKind::ObjectIdentifier;
    out.object_identifier.size =
        constraints::normalize_size(type.constraint(), ctx.diagnostics);
  } else if (dynamic_cast<const ast::RelativeOidType*>(&type)) {
    out.kind = ir::TypeKind::RelativeOid;
    out.relative_oid.size =
        constraints::normalize_size(type.constraint(), ctx.diagnostics);
  } else if (dynamic_cast<const ast::RealType*>(&type)) {
    out.kind = ir::TypeKind::Real;
  } else {
    ctx.diagnostics.error(type.range(), "internal error: unsupported AST type in lower");
    out.kind = ir::TypeKind::Null;
  }

  return ctx.model.arena.add(std::move(out));
}

void check_type_refs(const ast::Type& type, const std::string& module, SymbolTable& symbols,
                     Diagnostics& diagnostics) {
  if (const auto* ref = dynamic_cast<const ast::ReferencedType*>(&type)) {
    const std::string owner = ref->module() ? *ref->module() : module;
    const Symbol* sym = symbols.lookup(owner, ref->name());
    if (!sym || sym->kind != SymbolKind::Type) {
      diagnostics.error(ref->range(), "undefined ASN.1 type '" + ref->name() + "'");
    }
  } else if (const auto* seq = dynamic_cast<const ast::SequenceType*>(&type)) {
    for (const auto& item : seq->items()) {
      if (const auto* comp = dynamic_cast<const ast::Component*>(item.get())) {
        check_type_refs(comp->type(), module, symbols, diagnostics);
      }
    }
  } else if (const auto* ch = dynamic_cast<const ast::ChoiceType*>(&type)) {
    for (const auto& item : ch->alternatives()) {
      if (const auto* comp = dynamic_cast<const ast::Component*>(item.get())) {
        check_type_refs(comp->type(), module, symbols, diagnostics);
      }
    }
  } else if (const auto* of = dynamic_cast<const ast::SequenceOfType*>(&type)) {
    check_type_refs(of->element(), module, symbols, diagnostics);
  } else if (const auto* set = dynamic_cast<const ast::SetType*>(&type)) {
    for (const auto& item : set->items()) {
      if (const auto* comp = dynamic_cast<const ast::Component*>(item.get())) {
        check_type_refs(comp->type(), module, symbols, diagnostics);
      }
    }
  } else if (const auto* setof = dynamic_cast<const ast::SetOfType*>(&type)) {
    check_type_refs(setof->element(), module, symbols, diagnostics);
  }
}

}  // namespace

Analyzer::Analyzer(Diagnostics& diagnostics) : diagnostics_(diagnostics) {}

ir::Model Analyzer::analyze(std::vector<std::unique_ptr<ast::Module>> modules) {
  modules_ = std::move(modules);
  model_ = ir::Model{};
  symbols_ = SymbolTable{};

  declare_pass();
  resolve_pass();
  lower_pass();
  tag_pass();

  return std::move(model_);
}

void Analyzer::declare_pass() {
  for (auto& mod_ptr : modules_) {
    if (!mod_ptr) {
      continue;
    }
    ast::Module& mod = *mod_ptr;

    Symbol mod_sym;
    mod_sym.kind = SymbolKind::Module;
    mod_sym.module = mod.name();
    mod_sym.name = mod.name();
    mod_sym.range = mod.range();
    mod_sym.ast = &mod;
    symbols_.declare(std::move(mod_sym), diagnostics_);

    for (const auto& imp : mod.imports()) {
      for (const auto& sym : imp.symbols) {
        Symbol s;
        const bool is_type = !sym.name.empty() && sym.name[0] >= 'A' && sym.name[0] <= 'Z';
        s.kind = is_type ? SymbolKind::Type : SymbolKind::Value;
        s.module = mod.name();
        s.name = sym.name;
        s.range = sym.range;
        s.is_import = true;
        s.import_from_module = imp.module;
        symbols_.declare(std::move(s), diagnostics_);
      }
    }

    for (const auto& assignment : mod.assignments()) {
      if (const auto* ta = dynamic_cast<const ast::TypeAssignment*>(assignment.get())) {
        Symbol s;
        s.kind = SymbolKind::Type;
        s.module = mod.name();
        s.name = ta->name();
        s.range = ta->range();
        s.ast = ta;
        symbols_.declare(std::move(s), diagnostics_);
      } else if (const auto* va =
                     dynamic_cast<const ast::ValueAssignment*>(assignment.get())) {
        Symbol s;
        s.kind = SymbolKind::Value;
        s.module = mod.name();
        s.name = va->name();
        s.range = va->range();
        s.ast = va;
        symbols_.declare(std::move(s), diagnostics_);
      }
    }
  }
}

void Analyzer::resolve_pass() {
  for (const auto& mod_ptr : modules_) {
    if (!mod_ptr) {
      continue;
    }
    const ast::Module& mod = *mod_ptr;
    for (const auto& imp : mod.imports()) {
      bool found_module = false;
      for (const auto& other : modules_) {
        if (other && other->name() == imp.module) {
          found_module = true;
          for (const auto& sym : imp.symbols) {
            const Symbol* exported = symbols_.find(imp.module, sym.name);
            if (!exported || exported->is_import) {
              diagnostics_.error(sym.range, "undefined ASN.1 type '" + sym.name +
                                                "' imported from module '" + imp.module + "'");
            }
          }
          break;
        }
      }
      if (!found_module) {
        diagnostics_.warning(imp.range, "imported module '" + imp.module +
                                            "' is not among the analyzed modules");
      }
    }

    for (const auto& assignment : mod.assignments()) {
      if (const auto* ta = dynamic_cast<const ast::TypeAssignment*>(assignment.get())) {
        check_type_refs(ta->type(), mod.name(), symbols_, diagnostics_);
      } else if (const auto* va =
                     dynamic_cast<const ast::ValueAssignment*>(assignment.get())) {
        check_type_refs(va->type(), mod.name(), symbols_, diagnostics_);
      }
    }
  }
}

void Analyzer::lower_pass() {
  for (auto& mod_ptr : modules_) {
    if (!mod_ptr) {
      continue;
    }
    ast::Module& mod = *mod_ptr;
    ir::ModuleInfo info;
    info.name = mod.name();
    switch (mod.tag_default()) {
      case ast::TagDefault::Explicit:
        info.tag_default = ir::ModuleInfo::TagDefault::Explicit;
        break;
      case ast::TagDefault::Implicit:
        info.tag_default = ir::ModuleInfo::TagDefault::Implicit;
        break;
      case ast::TagDefault::Automatic:
        info.tag_default = ir::ModuleInfo::TagDefault::Automatic;
        break;
    }
    info.extensibility_implied = mod.extensibility_implied();

    LowerCtx ctx{model_, symbols_, diagnostics_, mod, {}, {}};

    // Reserve stable TypeIds for all named types so recursion can point at them.
    for (const auto& assignment : mod.assignments()) {
      const auto* ta = dynamic_cast<const ast::TypeAssignment*>(assignment.get());
      if (!ta) {
        continue;
      }
      const std::string key = mod.name() + "::" + ta->name();
      ir::Type stub;
      stub.module = mod.name();
      stub.name = ta->name();
      stub.kind = ir::TypeKind::Null;
      const ir::TypeId id = model_.arena.add(std::move(stub));
      ctx.named_ids[key] = id;
      if (Symbol* sym = symbols_.find(mod.name(), ta->name())) {
        sym->type_id = id;
      }
    }

    for (const auto& assignment : mod.assignments()) {
      const auto* ta = dynamic_cast<const ast::TypeAssignment*>(assignment.get());
      if (!ta) {
        continue;
      }
      const std::string key = mod.name() + "::" + ta->name();
      const ir::TypeId reserved = ctx.named_ids[key];
      ctx.resolving.insert(key);
      const ir::TypeId built = lower_type(ctx, ta->type(), ta->name());
      ctx.resolving.erase(key);

      // Move built content into the reserved slot (keep stable id for recursion).
      if (built != reserved) {
        model_.arena.get(reserved) = model_.arena.get(built);
        model_.arena.get(reserved).module = mod.name();
        model_.arena.get(reserved).name = ta->name();
        // Redirect any field still pointing at `built` toward `reserved`.
        for (std::size_t i = 0; i < model_.arena.type_count(); ++i) {
          ir::Type& t = model_.arena.get(static_cast<ir::TypeId>(i));
          auto rew = [&](ir::TypeId& id) {
            if (id == built) {
              id = reserved;
            }
          };
          if (t.kind == ir::TypeKind::Sequence) {
            for (auto& f : t.sequence.root) {
              rew(f.type);
            }
            for (auto& g : t.sequence.extension_groups) {
              for (auto& f : g) {
                rew(f.type);
              }
            }
          } else if (t.kind == ir::TypeKind::Set) {
            for (auto& f : t.set.root) {
              rew(f.type);
            }
            for (auto& g : t.set.extension_groups) {
              for (auto& f : g) {
                rew(f.type);
              }
            }
          } else if (t.kind == ir::TypeKind::Choice) {
            for (auto& f : t.choice.alternatives) {
              rew(f.type);
            }
          } else if (t.kind == ir::TypeKind::SequenceOf) {
            rew(t.sequence_of.element);
          } else if (t.kind == ir::TypeKind::SetOf) {
            rew(t.set_of.element);
          } else if (t.kind == ir::TypeKind::Referenced) {
            if (t.referenced.resolved == built) {
              t.referenced.resolved = reserved;
            }
          }
        }
      }

      info.types.push_back(reserved);
      model_.exported_types.push_back(reserved);
    }
    model_.modules.push_back(std::move(info));
  }
}

void Analyzer::tag_pass() {
  // Resolve Referenced placeholders and rewrite field type ids to the real type.
  for (std::size_t i = 0; i < model_.arena.type_count(); ++i) {
    ir::Type& t = model_.arena.get(static_cast<ir::TypeId>(i));
    if (t.kind == ir::TypeKind::Referenced && t.referenced.resolved == ir::kInvalidType) {
      if (Symbol* sym = symbols_.find(t.referenced.module, t.referenced.name)) {
        t.referenced.resolved = sym->type_id;
      }
    }
  }

  auto rewrite = [&](ir::TypeId& id) {
    if (id == ir::kInvalidType) {
      return;
    }
    const ir::Type& t = model_.arena.get(id);
    if (t.kind == ir::TypeKind::Referenced && t.referenced.resolved != ir::kInvalidType) {
      id = t.referenced.resolved;
    }
  };

  for (std::size_t i = 0; i < model_.arena.type_count(); ++i) {
    ir::Type& t = model_.arena.get(static_cast<ir::TypeId>(i));
    if (t.kind == ir::TypeKind::Sequence) {
      for (auto& f : t.sequence.root) {
        rewrite(f.type);
      }
      for (auto& group : t.sequence.extension_groups) {
        for (auto& f : group) {
          rewrite(f.type);
        }
      }
      for (auto& f : t.sequence.trailing_root) {
        rewrite(f.type);
      }
    } else if (t.kind == ir::TypeKind::Set) {
      for (auto& f : t.set.root) {
        rewrite(f.type);
      }
      for (auto& group : t.set.extension_groups) {
        for (auto& f : group) {
          rewrite(f.type);
        }
      }
      for (auto& f : t.set.trailing_root) {
        rewrite(f.type);
      }
    } else if (t.kind == ir::TypeKind::Choice) {
      for (auto& f : t.choice.alternatives) {
        rewrite(f.type);
      }
    } else if (t.kind == ir::TypeKind::SequenceOf) {
      rewrite(t.sequence_of.element);
    } else if (t.kind == ir::TypeKind::SetOf) {
      rewrite(t.set_of.element);
    }
  }
}

}  // namespace asn1
