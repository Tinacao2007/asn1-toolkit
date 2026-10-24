#include <asn1/codegen/c_types.hpp>
#include <asn1/codegen/plan.hpp>
#include <functional>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>

namespace asn1 { namespace codegen {
namespace {
using namespace ir;
bool alpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
bool digit(char c) { return c >= '0' && c <= '9'; }
// Injective spelling: underscores are escaped too, so ASN.1 a-b and a_b
// cannot collapse. Every use has a letter prefix, including struct members.
std::string spell(const std::string& text) {
  const char* hex = "0123456789abcdef";
  std::string result;
  for (unsigned char c : text) {
    if (alpha(static_cast<char>(c)) || digit(static_cast<char>(c))) result += static_cast<char>(c);
    else { result += "_x"; result += hex[c >> 4]; result += hex[c & 15]; }
  }
  return result;
}
std::string literal(std::int64_t v) {
  if (v == INT64_MIN) return "(-INT64_C(9223372036854775807)-INT64_C(1))";
  if (v < 0) return "(-INT64_C(" + std::to_string(-v) + "))";
  return "INT64_C(" + std::to_string(v) + ")";
}
struct Member { const Field* field; bool extension; };
std::vector<Member> members(const Type& t) {
  std::vector<Member> result;
  for (const auto& f: planning::fields(t)) result.push_back({f.field, f.extension_group.has_value()});
  return result;
}
bool big_integer(const Type& t) {
  return t.kind == TypeKind::Integer && (!t.integer.host_bits || t.integer.constraint.extensible ||
    !t.integer.constraint.lower || !t.integer.constraint.upper ||
    !t.integer.constraint.lower->as_i64 || !t.integer.constraint.upper->as_i64);
}
bool octets(const Type& t) {
  return t.kind == TypeKind::String || t.kind == TypeKind::OctetString ||
         (t.kind == TypeKind::ObjectClassField && t.object_class_field.open_type);
}
std::vector<std::uint32_t> limbs(const BigInt& value) {
  std::vector<std::uint32_t> out;
  for (char digit: value.digits) {
    std::uint64_t carry=static_cast<unsigned>(digit-'0');
    for (auto& word: out) { const auto n=std::uint64_t(word)*10+carry; word=static_cast<std::uint32_t>(n); carry=n>>32; }
    if (carry) out.push_back(static_cast<std::uint32_t>(carry));
  }
  while (!out.empty() && out.back()==0) out.pop_back();
  return out;
}
std::string c_string(const std::string& text) {
  std::ostringstream out; out << '"';
  for (unsigned char c: text) {
    if (c>=32 && c<127 && c!='"' && c!='\\') out << c;
    else out << '\\' << std::oct << std::setw(3) << std::setfill('0') << unsigned(c) << std::dec;
  }
  return out.str()+'"';
}
class Types {
 public:
  const Model& model;
  const std::string prefix;
  Diagnostics& diag;
  std::map<TypeId, std::string> names;
  std::map<TypeId, std::vector<TypeId>> edges;
  std::set<std::pair<TypeId, TypeId>> indirect;
  std::vector<TypeId> order;
  Types(const Model& m, std::string p, Diagnostics& d) : model(m), prefix(std::move(p)), diag(d) {}
  std::string name(TypeId id) const { return names.at(id); }
  void collect(TypeId id) {
    if (id >= model.arena.type_count()) { diag.error({}, "C types: unresolved type reference"); return; }
    if (names.count(id)) return;
    const auto& t = model.arena.get(id);
    names[id] = prefix + "_m" + spell(t.module) + "_t" +
                (t.name.empty() ? "anon_" + std::to_string(id) : spell(t.name));
    // Lowering may clone a named type for a field's effective tag/constraints.
    // Keep those views distinct rather than merging non-identical descriptors.
    bool exported = false;
    for (auto public_id : model.exported_types) if (id == public_id) exported = true;
    if (!exported && !t.name.empty()) names[id] += "_view" + std::to_string(id);
    std::vector<TypeId> deps;
    for (const auto& f: planning::fields(t)) {
      deps.push_back(f.field->type);
      if (f.selector) for (const auto& entry: f.selector->entries) deps.push_back(entry.type);
    }
    if (t.kind == TypeKind::ObjectClassField && !t.object_class_field.open_type)
      deps.push_back(t.object_class_field.fixed_type);
    if (t.kind == TypeKind::SequenceOf) deps.push_back(t.sequence_of.element);
    if (t.kind == TypeKind::SetOf) deps.push_back(t.set_of.element);
    if (t.kind == TypeKind::Referenced) deps.push_back(t.referenced.resolved);
    edges[id] = deps;
    for (auto dep : deps) collect(dep);
  }
  bool reaches(TypeId from, TypeId to, std::set<TypeId>& seen) const {
    if (from == to) return true;
    if (!seen.insert(from).second) return false;
    for (auto dep : edges.at(from)) if (reaches(dep, to, seen)) return true;
    return false;
  }
  bool pointer(TypeId from, TypeId to) const { return indirect.count({from,to}) != 0; }
  // DEFAULT is a semantic value, not a presence flag. Resolve only the already
  // lowered type view; never perform name lookup or semantic analysis here.
  std::string default_assignment(const Field& field, std::string path) const {
    if (!field.default_value || *field.default_value >= model.arena.value_count()) return {};
    TypeId id = field.type;
    std::set<TypeId> seen;
    while (model.arena.get(id).kind == TypeKind::Referenced) {
      if (!seen.insert(id).second) return {};
      id = model.arena.get(id).referenced.resolved;
      path += ".value";
    }
    const auto& t = model.arena.get(id);
    const auto& value = model.arena.get_value(*field.default_value);
    if (t.kind == TypeKind::Boolean && value.kind == ValueKind::Boolean)
      return path + ".value = " + (value.boolean ? "1;\n" : "0;\n");
    if (t.kind == TypeKind::Integer && !big_integer(t) && value.kind == ValueKind::Integer && value.integer.as_i64) {
      const auto plan = planning::constraint(t.integer.constraint);
      bool member = false;
      for (const auto& r : plan.descriptor->root_ranges)
        if ((!r.lower || value.integer >= *r.lower) && (!r.upper || value.integer <= *r.upper)) member = true;
      if (member) return path + ".value = " + literal(*value.integer.as_i64) + ";\n";
    }
    if (t.kind==TypeKind::Enumerated && value.kind==ValueKind::Reference) {
      for (const auto* list:{&t.enumerated.root,&t.enumerated.extensions})
        for (const auto& item:*list)
          if(item.name==value.name && item.value.as_i64) return path+".value = "+literal(*item.value.as_i64)+";\n";
    }
    return {};
  }
  TypeId underlying(TypeId id) const {
    std::set<TypeId> seen;
    while (model.arena.get(id).kind == TypeKind::Referenced) {
      if (!seen.insert(id).second) return kInvalidType;
      id=model.arena.get(id).referenced.resolved;
    }
    return id;
  }
  bool owning_default(const Field& field) const {
    if (!field.default_value || *field.default_value >= model.arena.value_count()) return false;
    const auto id=underlying(field.type);
    if (id==kInvalidType) return false;
    const auto& t=model.arena.get(id);
    const auto& v=model.arena.get_value(*field.default_value);
    if (big_integer(t) && v.kind==ValueKind::Integer) {
      const auto& c=t.integer.constraint;
      if (c.extensible) return true;
      if (c.empty) return false;
      if (c.root_ranges.empty()) return true;
      for (const auto& r:c.root_ranges)
        if ((!r.lower || v.integer>=*r.lower) && (!r.upper || v.integer<=*r.upper)) return true;
      return false;
    }
    if (t.kind==TypeKind::String && v.kind==ValueKind::String) return true;
    const auto binary=planning::binary_default(v);
    if (!binary || (t.kind!=TypeKind::BitString && t.kind!=TypeKind::OctetString)) return false;
    if (t.kind==TypeKind::OctetString && binary->bits%8) return false;
    const auto* size=planning::storage_size(t);
    const auto count=BigInt::from_i64(static_cast<std::int64_t>(t.kind==TypeKind::BitString?binary->bits:binary->bytes.size()));
    if(size->extensible) return true;
    if(size->empty) return false;
    if(size->root_ranges.empty()) return true;
    for(const auto& r:size->root_ranges)
      if((!r.lower || count>=*r.lower) && (!r.upper || count<=*r.upper)) return true;
    return false;
  }
  void big_constant(const BigInt& value, const std::string& symbol, std::ostream& s) const {
    const auto words=limbs(value);
    if (!words.empty()) {
      s << "static uint32_t " << symbol << "_limbs[] = {";
      for (auto word: words) s << "UINT32_C(" << word << "),";
      s << "};\n";
    }
    s << "const asn1_bigint_t " << symbol << " = {" << (words.empty()?0:value.negative?-1:1)
      << "," << (words.empty()?"NULL":symbol+"_limbs") << "," << words.size() << "," << words.size() << "};\n";
  }
  void bigint_contents(const Type& t, std::ostream& s, bool copy) const {
    if (!copy) { s << "free(v->value.limbs);\n"; return; }
    s << "if (src->value.nlimbs > ASN1_MAX_BIGINT_LIMBS) { status=ASN1_ERR_RESOURCE_LIMIT; goto fail; }\n"
      << "if (src->value.nlimbs > src->value.cap || (src->value.nlimbs && (!src->value.limbs || !src->value.limbs[src->value.nlimbs-1])) || "
      << "(src->value.nlimbs ? (src->value.sign!=1 && src->value.sign!=-1) : src->value.sign!=0)) { status=ASN1_ERR_INVALID_ARGUMENT; goto fail; }\n";
    const auto plan=planning::constraint(t.integer.constraint);
    if (!plan.descriptor->extensible && (plan.descriptor->empty || plan.requires_membership)) {
      s << "{ int member=0;\n";
      for (std::size_t i=0;i<plan.descriptor->root_ranges.size();++i) {
        const auto& range=plan.descriptor->root_ranges[i];
        const auto base="bound"+std::to_string(i);
        if (range.lower) big_constant(*range.lower,base+"lo",s);
        if (range.upper) big_constant(*range.upper,base+"hi",s);
        s << "if (1";
        if (range.lower) s << " && asn1_bigint_cmp(&src->value,&" << base << "lo)>=0";
        if (range.upper) s << " && asn1_bigint_cmp(&src->value,&" << base << "hi)<=0";
        s << ") member=1;\n";
      }
      s << "if (!member) { status=ASN1_ERR_CONSTRAINT_VIOLATION; goto fail; } }\n";
    }
    s << "if (src->value.nlimbs) { tmp.value.limbs=(uint32_t*)malloc(src->value.nlimbs*sizeof(uint32_t)); "
      << "if (!tmp.value.limbs) { status=ASN1_ERR_OUT_OF_MEMORY; goto fail; } "
      << "memcpy(tmp.value.limbs,src->value.limbs,src->value.nlimbs*sizeof(uint32_t)); }\n"
      << "tmp.value.sign=src->value.sign; tmp.value.nlimbs=tmp.value.cap=src->value.nlimbs;\n";
  }
  void defaults_body(TypeId id, std::ostream& s) const {
    const auto& t=model.arena.get(id);
    const auto n=name(id);
    s << "static asn1_status_t " << n << "_defaults_at(" << n << " *v, size_t depth) {\n"
      << "asn1_status_t status=ASN1_OK; (void)v; if (depth>=ASN1_MAX_RECURSION_DEPTH) return ASN1_ERR_RESOURCE_LIMIT;\n";
    const auto recurse=[&](TypeId dep,const std::string& path) {
      s << "status=" << name(dep) << "_defaults_at(&" << path << ",depth+1); if(status!=ASN1_OK) return status;\n";
    };
    if (t.kind==TypeKind::Referenced && !pointer(id,t.referenced.resolved)) recurse(t.referenced.resolved,"v->value");
    else if (t.kind==TypeKind::ObjectClassField && !t.object_class_field.open_type && !pointer(id,t.object_class_field.fixed_type)) recurse(t.object_class_field.fixed_type,"v->value");
    else if (t.kind==TypeKind::Sequence || t.kind==TypeKind::Set) {
      for (const auto& m: members(t)) {
        if (m.extension || m.field->presence==Presence::Optional || pointer(id,m.field->type)) continue;
        std::string path="v->f"+spell(m.field->name);
        if (!owning_default(*m.field)) { recurse(m.field->type,path); continue; }
        TypeId dep=m.field->type;
        while (model.arena.get(dep).kind==TypeKind::Referenced) { path+=".value"; dep=model.arena.get(dep).referenced.resolved; }
        const auto& value=model.arena.get_value(*m.field->default_value);
        if (big_integer(model.arena.get(dep))) {
          s << "{\n"; big_constant(value.integer,"def",s);
          s << "if(def.nlimbs>ASN1_MAX_BIGINT_LIMBS) return ASN1_ERR_RESOURCE_LIMIT;\n"
            << "if(def.nlimbs) { " << path << ".value.limbs=(uint32_t*)malloc(def.nlimbs*sizeof(uint32_t)); "
            << "if(!" << path << ".value.limbs) return ASN1_ERR_OUT_OF_MEMORY; "
            << "memcpy(" << path << ".value.limbs,def.limbs,def.nlimbs*sizeof(uint32_t)); }\n"
            << path << ".value.sign=def.sign; " << path << ".value.nlimbs=" << path << ".value.cap=def.nlimbs; }\n";
        } else if (value.kind==ValueKind::BitOrOctet) {
          const auto binary=*planning::binary_default(value);
          if (!binary.bytes.empty()) {
            s << "{ static const uint8_t bytes[] = {";
            for(auto byte:binary.bytes) s << unsigned(byte) << ",";
            s << "}; size_t count=sizeof(bytes); if(count>ASN1_MAX_STRING_BYTES) return ASN1_ERR_RESOURCE_LIMIT;\n";
            const bool bits=model.arena.get(dep).kind==TypeKind::BitString;
            const auto data=path+(bits?".value.bits":".data");
            s << data << "=(uint8_t*)malloc(count); if(!" << data << ") return ASN1_ERR_OUT_OF_MEMORY; memcpy(" << data << ",bytes,count);\n";
            if(bits) s << path << ".value.bit_length=" << binary.bits << "; " << path << ".value.cap_bytes=count; }\n";
            else s << path << ".count=" << path << ".capacity=count; }\n";
          }
        } else {
          s << "{ static const uint8_t bytes[] = " << c_string(*planning::string_default(value)) << ";\n"
            << "size_t count=sizeof(bytes)-1; if(count>ASN1_MAX_STRING_BYTES) return ASN1_ERR_RESOURCE_LIMIT;\n"
            << "if(count) { " << path << ".data=(uint8_t*)malloc(count); if(!" << path << ".data) return ASN1_ERR_OUT_OF_MEMORY; "
            << "memcpy(" << path << ".data,bytes,count); } " << path << ".count=" << path << ".capacity=count; }\n";
        }
      }
    }
    s << "return status; }\n"
      << "asn1_status_t " << n << "_reset_defaults(" << n << " *out) {\n"
      << n << " tmp; asn1_status_t status; if(!out) return ASN1_ERR_INVALID_ARGUMENT; " << n << "_init(&tmp);\n"
      << "status=" << n << "_defaults_at(&tmp,0); if(status!=ASN1_OK) { " << n << "_free(&tmp); return status; }\n"
      << n << "_free(out); *out=tmp; return ASN1_OK; }\n";
  }
  void metadata(TypeId id, std::ostream& s) const {
    const auto n=name(id); const auto fs=planning::fields(model.arena.get(id));
    for (std::size_t i=0;i<fs.size();++i) if (fs[i].selector && !fs[i].selector->entries.empty()) {
      s << "static const " << prefix << "_selector_entry " << n << "_select" << i << "[] = {\n";
      for (const auto& entry: fs[i].selector->entries)
        s << "{" << entry.has_id_value << "," << c_string(entry.id_value.to_string()) << ",UINT32_C(" << entry.type << ")},\n";
      s << "};\n";
    }
    if (!fs.empty()) {
      s << "static const " << prefix << "_field_plan " << n << "_fields[] = {\n";
      for (std::size_t i=0;i<fs.size();++i) {
        const auto& f=fs[i];
        s << "{" << c_string(f.field->name) << "," << static_cast<unsigned>(f.effective_tag.cls)
          << ",UINT64_C(" << f.effective_tag.number << ")," << f.effective_tag.is_explicit << ","
          << (f.presence_index?std::to_string(*f.presence_index):"SIZE_MAX") << ","
          << (f.extension_group?std::to_string(*f.extension_group):"SIZE_MAX") << "," << f.nested_sequence
          << "," << f.ordinal << "," << (f.selector?c_string(f.selector->governor_field_name):"NULL") << ","
          << (f.selector?f.selector->entries.size():0) << ","
          << (f.selector && !f.selector->entries.empty()?n+"_select"+std::to_string(i):"NULL") << "},\n";
      }
      s << "};\n";
    }
    s << "size_t " << n << "_field_count(void) { return " << fs.size() << "; }\n"
      << "const " << prefix << "_field_plan *" << n << "_field(size_t index) { "
      << (fs.empty()?"(void)index; return NULL;":"return index < "+std::to_string(fs.size())+" ? &"+n+"_fields[index] : NULL;") << " }\n"
      << "asn1_status_t " << n << "_select(size_t index, const char *key, uint32_t *type) {\n"
      << "const " << prefix << "_field_plan *f=" << n << "_field(index); if(!f || !key || !type) return ASN1_ERR_INVALID_ARGUMENT;\n"
      << "if(!f->governor) return ASN1_ERR_UNSUPPORTED;\n"
      << "for(size_t i=0;i<f->selector_count;++i) { if(!f->entries[i].has_key) return ASN1_ERR_UNSUPPORTED; "
      << "if(strcmp(key,f->entries[i].decimal_key)==0) { *type=f->entries[i].type_id; return ASN1_OK; } }\n"
      << "return ASN1_ERR_CONSTRAINT_VIOLATION; }\n";
  }
  void visit(TypeId id, std::set<TypeId>& seen) {
    if (!seen.insert(id).second) return;
    for (auto dep : edges[id]) if (!pointer(id, dep)) visit(dep, seen);
    order.push_back(id);
  }
  bool prepare() {
    for (auto id : model.exported_types) collect(id);
    if (!diag.ok()) return false;
    std::set<std::string> unique;
    for (const auto& item : names) {
      if (!unique.insert(item.second).second) diag.error({}, "C types: duplicate qualified type name");
      const auto& t = model.arena.get(item.first);
      if (const auto* size = planning::storage_size(t)) {
        for (const auto& range : planning::constraint(*size).descriptor->root_ranges)
          if ((range.lower && (!range.lower->as_i64 || *range.lower->as_i64 < 0)) ||
              (range.upper && (!range.upper->as_i64 || *range.upper->as_i64 < 0)))
            diag.error({}, "Unsupported C type layout: SIZE bound exceeds nonnegative int64");
        if (!size->statically_foldable)
          diag.error({}, "Unsupported C type layout: unresolved SIZE constraint");
      }
      switch (t.kind) {
        case TypeKind::Boolean: case TypeKind::Null: case TypeKind::OctetString:
        case TypeKind::String: case TypeKind::Sequence: case TypeKind::Set:
        case TypeKind::Choice: case TypeKind::SequenceOf: case TypeKind::SetOf:
        case TypeKind::Referenced: break;
        case TypeKind::Integer:
          if (!t.integer.constraint.statically_foldable)
            diag.error({}, "Unsupported C type layout: unresolved INTEGER constraint");
          break;
        case TypeKind::BitString: case TypeKind::ObjectIdentifier: case TypeKind::RelativeOid:
        case TypeKind::Real: case TypeKind::ObjectClassField: break;
        case TypeKind::Enumerated:
          for (const auto* list: {&t.enumerated.root,&t.enumerated.extensions})
            for (const auto& item: *list) if (!item.value.as_i64)
              diag.error({}, "Unsupported C ENUMERATED number exceeds int64 profile");
          break;
        default: diag.error({}, "Unsupported C type layout: " + item.second); break;
      }
      for (const auto& f:planning::fields(t)) if (f.selector) {
        std::set<std::string> keys;
        for (const auto& entry:f.selector->entries)
          if (!entry.has_id_value || !keys.insert(entry.id_value.to_string()).second)
            diag.error({}, "Unsupported C selector: unresolved or duplicate key");
      }
      std::set<std::string> fields;
      for (auto m : members(t)) {
        if (!fields.insert(m.field->name).second) diag.error({}, "C types: duplicate member name");
        if (m.field->presence == Presence::Default && default_assignment(*m.field,"v->f" + spell(m.field->name)).empty() && !owning_default(*m.field))
          diag.error({}, "Unsupported C type layout: DEFAULT is not representable by the lowered literal profile");
      }
    }
    if (!diag.ok()) return false;
    for (const auto& edge : edges) for (auto dep : edge.second) {
      std::set<TypeId> seen;
      if (reaches(dep, edge.first, seen)) indirect.insert({edge.first, dep});
    }
    std::set<TypeId> seen;
    for (const auto& item : names) visit(item.first, seen);
    return true;
  }
  void declaration(TypeId id, std::ostream& h) const {
    const auto& t = model.arena.get(id);
    const auto n = name(id);
    if (t.kind == TypeKind::Choice) {
      h << "enum " << n << "_kind { " << n << "_invalid = 0";
      for (auto m : members(t)) h << ", " << n << "_arm_f" << spell(m.field->name);
      h << " };\n";
    }
    h << "struct " << n << " {\n";
    if (t.kind == TypeKind::Boolean) h << "  int value;\n";
    else if (big_integer(t)) h << "  asn1_bigint_t value;\n";
    else if (t.kind == TypeKind::Integer || t.kind == TypeKind::Enumerated) h << "  int64_t value;\n";
    else if (t.kind == TypeKind::Real) h << "  double value;\n";
    else if (t.kind == TypeKind::BitString) h << "  asn1_bitstring_t value;\n";
    else if (t.kind == TypeKind::Null) h << "  uint8_t reserved;\n";
    else if (octets(t))
      h << "  uint8_t *data;\n  size_t count, capacity;\n";
    else if (t.kind == TypeKind::ObjectIdentifier || t.kind == TypeKind::RelativeOid)
      h << "  uint64_t *data;\n  size_t count, capacity;\n";
    else if (t.kind == TypeKind::ObjectClassField)
      h << "  " << name(t.object_class_field.fixed_type) << (pointer(id,t.object_class_field.fixed_type)?" *":" ") << "value;\n";
    else if (t.kind == TypeKind::SequenceOf || t.kind == TypeKind::SetOf) {
      auto dep = t.kind == TypeKind::SequenceOf ? t.sequence_of.element : t.set_of.element;
      h << "  " << name(dep) << " *data;\n  size_t count, capacity;\n";
    } else if (t.kind == TypeKind::Referenced) {
      h << "  " << name(t.referenced.resolved) << (pointer(id,t.referenced.resolved) ? " *" : " ") << "value;\n";
    } else {
      const auto ms = members(t);
      if (t.kind == TypeKind::Choice) h << "  enum " << n << "_kind kind;\n  union {\n";
      if (ms.empty()) h << "  uint8_t reserved;\n";
      for (auto m : ms) {
        if (t.kind != TypeKind::Choice && (m.extension || m.field->presence == Presence::Optional))
          h << "  int has_f" << spell(m.field->name) << ";\n";
        h << "  " << name(m.field->type) << (pointer(id,m.field->type) ? " *" : " ")
          << "f" << spell(m.field->name) << ";\n";
      }
      if (t.kind == TypeKind::Choice) h << "  } value;\n";
    }
    h << "};\n";
  }
  void child(TypeId parent, TypeId dep, const std::string& path, std::ostream& s, bool copy) const {
    const auto n = name(dep);
    if (copy) {
      if (pointer(parent, dep)) {
        s << "if (src->" << path << ") { tmp." << path << " = (" << n << "*)calloc(1, sizeof(" << n << "));\n"
          << "if (!tmp." << path << ") { status = ASN1_ERR_OUT_OF_MEMORY; goto fail; }\n"
          << n << "_init(tmp." << path << ");\n"
          << "status = " << n << "_copy_at(src->" << path << ", tmp." << path << ", depth + 1); }\n";
      } else s << "status = " << n << "_copy_at(&src->" << path << ", &tmp." << path << ", depth + 1);\n";
      s << "if (status != ASN1_OK) goto fail;\n";
    } else {
      if (pointer(parent, dep)) s << "if (v->" << path << ") { " << n << "_free(v->" << path << "); free(v->" << path << "); }\n";
      else s << n << "_free(&v->" << path << ");\n";
    }
  }
  void body(TypeId id, std::ostream& s) const {
    const auto& t = model.arena.get(id);
    const auto n = name(id);
    s << "void " << n << "_init(" << n << " *v) { if (v) { " << n << " zero = {0}; *v = zero;\n";
    if (t.kind == TypeKind::Sequence || t.kind == TypeKind::Set) {
      for (auto m : members(t)) {
        const auto path = "v->f" + spell(m.field->name);
        if (!pointer(id,m.field->type)) s << name(m.field->type) << "_init(&" << path << ");\n";
        if (m.field->presence == Presence::Default && !owning_default(*m.field)) s << default_assignment(*m.field,path);
      }
    } else if (t.kind == TypeKind::ObjectClassField && !t.object_class_field.open_type && !pointer(id,t.object_class_field.fixed_type))
      s << name(t.object_class_field.fixed_type) << "_init(&v->value);\n";
    else if (t.kind == TypeKind::Referenced && !pointer(id,t.referenced.resolved))
      s << name(t.referenced.resolved) << "_init(&v->value);\n";
    s << "} }\nvoid " << n << "_free(" << n << " *v) { if (!v) return;\n";
    contents(id, s, false);
    s << n << "_init(v); }\n"
      << "static asn1_status_t " << n << "_copy_at(const " << n << " *src, " << n << " *out, size_t depth) {\n"
      << n << " tmp = {0}; asn1_status_t status = ASN1_OK;\n"
      << "if (!src || !out) return ASN1_ERR_INVALID_ARGUMENT;\n"
      << "if (depth >= 256) return ASN1_ERR_RESOURCE_LIMIT;\n";
    s << n << "_init(&tmp);\n";
    contents(id, s, true);
    // All generated copy paths share one transactional cleanup route.
    s << "if (status != ASN1_OK) goto fail;\n" << n << "_free(out); *out = tmp; return ASN1_OK;\n"
      << "fail: " << n << "_free(&tmp); return status; }\n"
      << "asn1_status_t " << n << "_copy(const " << n << " *src, " << n << " *out) { return "
      << n << "_copy_at(src, out, 0); }\n";
  }
  void contents(TypeId id, std::ostream& s, bool copy) const {
    const auto& t = model.arena.get(id);
    const auto n = name(id);
    if (copy) {
      if (const auto* size = planning::storage_size(t)) {
        const auto plan = planning::constraint(*size);
        const std::string count = t.kind == TypeKind::BitString ? "src->value.bit_length" : "src->count";
        if (size->empty && !size->extensible)
          s << "status = ASN1_ERR_CONSTRAINT_VIOLATION; goto fail;\n";
        else if (plan.requires_membership) {
          s << "if (!(0";
          for (const auto& range : plan.descriptor->root_ranges) {
            s << " || (1";
            if (range.lower && *range.lower->as_i64 != 0)
              s << " && " << count << " >= UINT64_C(" << *range.lower->as_i64 << ")";
            if (range.upper)
              s << " && " << count << " <= UINT64_C(" << *range.upper->as_i64 << ")";
            s << ")";
          }
          s << ")) { status = ASN1_ERR_CONSTRAINT_VIOLATION; goto fail; }\n";
        }
      }
    }
    if (big_integer(t)) {
      bigint_contents(t,s,copy);
    } else if (t.kind == TypeKind::BitString) {
      if (!copy) s << "free(v->value.bits);\n";
      else s << "{ size_t bytes = src->value.bit_length/8 + (src->value.bit_length%8 != 0);\n"
        << "if (bytes > ASN1_MAX_STRING_BYTES) { status = ASN1_ERR_RESOURCE_LIMIT; goto fail; }\n"
        << "if (bytes > src->value.cap_bytes || (bytes && !src->value.bits)) { status = ASN1_ERR_INVALID_ARGUMENT; goto fail; }\n"
        << "if (bytes && src->value.bit_length%8 && (src->value.bits[bytes-1] & ((1u << (8-src->value.bit_length%8))-1u))) { status = ASN1_ERR_CONSTRAINT_VIOLATION; goto fail; }\n"
        << "if (bytes) { tmp.value.bits = (uint8_t*)malloc(bytes); if (!tmp.value.bits) { status = ASN1_ERR_OUT_OF_MEMORY; goto fail; } memcpy(tmp.value.bits,src->value.bits,bytes); }\n"
        << "tmp.value.bit_length = src->value.bit_length; tmp.value.cap_bytes = bytes; }\n";
    } else if (octets(t) || t.kind == TypeKind::ObjectIdentifier || t.kind == TypeKind::RelativeOid) {
      if (!copy) s << "free(v->data);\n";
      else {
        const bool oid = t.kind == TypeKind::ObjectIdentifier || t.kind == TypeKind::RelativeOid;
        s << "if (src->count > src->capacity || (src->count && !src->data)) { status = ASN1_ERR_INVALID_ARGUMENT; goto fail; }\n";
        if (oid) {
          s << "if (src->count < " << (t.kind == TypeKind::ObjectIdentifier ? 2 : 1)
            << " || !src->data) { status = ASN1_ERR_CONSTRAINT_VIOLATION; goto fail; }\n";
          if (t.kind == TypeKind::ObjectIdentifier)
            s << "if (src->data[0] > 2 || (src->data[0] < 2 && src->data[1] > 39)) { status = ASN1_ERR_CONSTRAINT_VIOLATION; goto fail; }\n";
        }
        s << "if (src->count > ASN1_MAX_STRING_BYTES/sizeof(*src->data)) { status = ASN1_ERR_RESOURCE_LIMIT; goto fail; }\n";
        s << "if (src->count) { tmp.data = (" << (oid ? "uint64_t" : "uint8_t") << "*)malloc(src->count*sizeof(*src->data)); if (!tmp.data) { status = ASN1_ERR_OUT_OF_MEMORY; goto fail; }\n"
        << "memcpy(tmp.data, src->data, src->count*sizeof(*src->data)); tmp.count = tmp.capacity = src->count; }\n";
      }
    } else if (t.kind == TypeKind::SequenceOf || t.kind == TypeKind::SetOf) {
      const auto dep = t.kind == TypeKind::SequenceOf ? t.sequence_of.element : t.set_of.element;
      if (!copy) s << "if (v->data) { for (size_t i = 0; i < v->count; ++i) " << name(dep) << "_free(&v->data[i]); free(v->data); }\n";
      else s << "if (src->count > src->capacity || (src->count && !src->data)) { status = ASN1_ERR_INVALID_ARGUMENT; goto fail; }\n"
        << "if (src->count > ASN1_MAX_STRING_BYTES / sizeof(*tmp.data)) { status = ASN1_ERR_RESOURCE_LIMIT; goto fail; }\nif (src->count > SIZE_MAX / sizeof(*tmp.data)) { status = ASN1_ERR_LENGTH_OVERFLOW; goto fail; }\n"
        << "if (src->count) { tmp.data = (" << name(dep) << "*)calloc(src->count, sizeof(*tmp.data));\n"
        << "if (!tmp.data) { status = ASN1_ERR_OUT_OF_MEMORY; goto fail; } tmp.capacity = src->count;\n"
        << "for (size_t i = 0; i < src->count; ++i) { " << name(dep) << "_init(&tmp.data[i]); tmp.count = i + 1; status = " << name(dep)
        << "_copy_at(&src->data[i], &tmp.data[i], depth + 1); if (status != ASN1_OK) goto fail; } }\n";
    } else if (t.kind == TypeKind::ObjectClassField) child(id,t.object_class_field.fixed_type,"value",s,copy);
    else if (t.kind == TypeKind::Referenced) child(id,t.referenced.resolved,"value",s,copy);
    else if (t.kind == TypeKind::Choice) {
      if (copy) s << "tmp.kind = src->kind;\n";
      s << "switch (" << (copy ? "src" : "v") << "->kind) {\n";
      for (auto m : members(t)) {
        s << "case " << n << "_arm_f" << spell(m.field->name) << ":\n";
        child(id,m.field->type,"value.f" + spell(m.field->name),s,copy);
        s << "break;\n";
      }
      s << "case " << n << "_invalid: break;\ndefault: ";
      if (copy) s << "status = ASN1_ERR_INVALID_ARGUMENT; goto fail;";
      else s << "break;";
      s << "\n}\n";
    } else if (t.kind == TypeKind::Sequence || t.kind == TypeKind::Set) {
      if (copy) {
        for (const auto& m:members(t)) if (m.extension || m.field->presence==Presence::Optional) {
          const auto path="has_f"+spell(m.field->name);
          s << "if(src->" << path << "!=0 && src->" << path << "!=1) { status=ASN1_ERR_INVALID_ARGUMENT; goto fail; }\n";
        }
        const auto plan=planning::sequence(t.kind==TypeKind::Set?t.set:t.sequence);
        for (const auto& group: plan.extensions) if (group.nested_sequence) {
          s << "if (0";
          for (const auto& f:group.group->fields) s << " || src->has_f" << spell(f.name);
          s << ") {\n";
          for (const auto& f:group.group->fields) if (f.presence==Presence::Mandatory)
            s << "if (!src->has_f" << spell(f.name) << ") { status=ASN1_ERR_CONSTRAINT_VIOLATION; goto fail; }\n";
          s << "}\n";
        }
      }
      for (auto m : members(t)) {
        const auto path = "f" + spell(m.field->name);
        const bool present = m.extension || m.field->presence == Presence::Optional;
        if (copy && present)
          s << "if (src->has_" << path << " != 0 && src->has_" << path
            << " != 1) { status = ASN1_ERR_INVALID_ARGUMENT; goto fail; }\n"
            << "tmp.has_" << path << " = src->has_" << path << ";\n"
            << "if (src->has_" << path << ") {\n";
        // Even absent fields can own storage during partial construction.
        child(id,m.field->type,path,s,copy);
        if (copy && present) s << "}\n";
      }
    } else if (copy) {
      if (t.kind == TypeKind::Boolean)
        s << "if (src->value != 0 && src->value != 1) { status = ASN1_ERR_CONSTRAINT_VIOLATION; goto fail; }\n";
      if (t.kind == TypeKind::Enumerated && !t.enumerated.extensible) {
        s << "if (!(0";
        for (const auto& item: t.enumerated.root) s << " || src->value == " << literal(*item.value.as_i64);
        s << ")) { status = ASN1_ERR_CONSTRAINT_VIOLATION; goto fail; }\n";
      }
      if (t.kind == TypeKind::Integer) {
        const auto plan = planning::constraint(t.integer.constraint);
        s << "if (!(0";
        for (const auto& range : plan.descriptor->root_ranges)
          s << " || (src->value >= " << literal(*range.lower->as_i64)
            << " && src->value <= " << literal(*range.upper->as_i64) << ")";
        s << ")) { status = ASN1_ERR_CONSTRAINT_VIOLATION; goto fail; }\n";
      }
      s << "tmp = *src;\n";
    }
  }
};
}
void emit_c_types(const ir::Model& model, const std::string& prefix,
                  const std::string& header_name, Diagnostics& diag,
                  std::ostream& header, std::ostream& source) try {
  bool valid = !prefix.empty() && alpha(prefix[0]);
  for (char c : prefix) valid = valid && (alpha(c) || digit(c) || c == '_');
  valid = valid && prefix.back() != '_' && prefix.find("__") == std::string::npos;
  if (!valid) { diag.error({}, "C types: invalid non-reserved symbol prefix"); return; }
  if (header_name.empty() || header_name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.") != std::string::npos ||
      header_name.find("..") != std::string::npos || header_name.back() == '.') {
    diag.error({}, "C types: header name must be a plain portable filename"); return;
  }
  // Case-fold only output filenames, never C symbols. A generated standard
  // header can shadow the very declarations needed by its implementation,
  // including on case-insensitive filesystems.
  std::string folded = header_name;
  for (auto& c : folded) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  static const std::set<std::string> standard_headers = {
    "assert.h","complex.h","ctype.h","errno.h","fenv.h","float.h","inttypes.h",
    "iso646.h","limits.h","locale.h","math.h","setjmp.h","signal.h","stdalign.h",
    "stdarg.h","stdatomic.h","stdbool.h","stddef.h","stdint.h","stdio.h","stdlib.h",
    "stdnoreturn.h","string.h","tgmath.h","threads.h","time.h","uchar.h","wchar.h","wctype.h"};
  if (standard_headers.count(folded)) {
    diag.error({}, "C types: generated header would shadow a C11 standard header"); return;
  }
  const auto stem=folded.substr(0,folded.find('.'));
  if (stem=="con" || stem=="prn" || stem=="aux" || stem=="nul" ||
      (stem.size()==4 && (stem.substr(0,3)=="com" || stem.substr(0,3)=="lpt") && stem[3]>='0' && stem[3]<='9')) {
    diag.error({}, "C types: generated header uses a Windows device name"); return;
  }
  Types types(model,prefix,diag);
  if (!types.prepare()) return;
  std::ostringstream h,s;
  h << "/* Generated C11 types only; no codec implementation. */\n#ifndef " << prefix << "_types_h\n#define " << prefix
    << "_types_h\n#define " << prefix << "_GENERATED_ABI_VERSION 1\n"
    << "#include <asn1/c_runtime/asn1_c_common.h>\n#include <asn1/c_runtime/bit_string.h>\n#include <asn1/c_runtime/limits.h>\n#ifdef __cplusplus\nextern \"C\" {\n#endif\n";
  h << "typedef struct { int has_key; const char *decimal_key; uint32_t type_id; } " << prefix << "_selector_entry;\n"
    << "typedef struct { const char *name; unsigned tag_class; uint64_t tag_number; int explicit_tag; size_t presence_index,extension_group; int nested_sequence; size_t ordinal; const char *governor; size_t selector_count; const " << prefix << "_selector_entry *entries; } " << prefix << "_field_plan;\n";
  for (const auto& item : types.names) h << "typedef struct " << item.second << ' ' << item.second << ";\n";
  for (auto id : types.order) types.declaration(id,h);
  for (const auto& item : types.names) {
    const auto& n = item.second;
    h << "void " << n << "_init(" << n << "*);\nvoid " << n << "_free(" << n << "*);\n"
      << "asn1_status_t " << n << "_copy(const " << n << "*, " << n << "*);\n"
      << "asn1_status_t " << n << "_reset_defaults(" << n << "*);\n"
      << "size_t " << n << "_field_count(void);\nconst " << prefix << "_field_plan *" << n << "_field(size_t);\n"
      << "asn1_status_t " << n << "_select(size_t,const char*,uint32_t*);\n";
  }
  h << "#ifdef __cplusplus\n}\n#endif\n#endif\n";
  s << "#include \"" << header_name << "\"\n#include <stdlib.h>\n#include <string.h>\n";
  for (const auto& item : types.names) s << "static asn1_status_t " << item.second << "_copy_at(const " << item.second
    << "*, " << item.second << "*, size_t);\n";
  for (const auto& item : types.names) s << "static asn1_status_t " << item.second << "_defaults_at(" << item.second << "*, size_t);\n";
  for (auto id : types.order) { types.body(id,s); types.defaults_body(id,s); types.metadata(id,s); }
  header << h.str(); source << s.str();
  if (!header || !source) diag.error({}, "C types: failed writing generated streams");
} catch (const std::length_error& ex) {
  diag.error({}, std::string("C types capacity exceeded: ")+ex.what());
} catch (const std::ios_base::failure& ex) {
  diag.error({}, std::string("C types stream failure: ")+ex.what());
}
}}
