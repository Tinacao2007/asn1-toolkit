#include <asn1/codegen/c_types.hpp>
#include <asn1/codegen/plan.hpp>
#include "emit_output.hpp"
#include <functional>
#include <algorithm>
#include <limits>
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
const std::vector<std::pair<const char*,CodecKind>>& extra_entries() {
  static const std::vector<std::pair<const char*,CodecKind>> entries={{"oer",CodecKind::Oer},{"coer",CodecKind::Coer},{"jer",CodecKind::Jer},{"jeri",CodecKind::Jeri},{"xer",CodecKind::Xer},{"cxer",CodecKind::Cxer},{"exer",CodecKind::Exer}};
  return entries;
}
class Types {
 public:
  const Model& model;
  const std::string prefix;
  Diagnostics& diag;
  bool external_helpers = false;
  bool inline_functions = false;
  bool c_uper = false;
  bool c_aper = false;
  bool c_ber = false, c_der = false, c_cer = false;
  CodecKind c_extra_mask=CodecKind::None;
  bool c_extra(CodecKind codec) const { return static_cast<unsigned>(c_extra_mask&codec)!=0; }
  bool c_extra() const { return c_extra_mask!=CodecKind::None; }
  bool c_schema() const { return c_ber || c_der || c_cer || c_extra(); }
  std::string linkage(bool helper = false) const {
    return inline_functions ? "static inline " : helper && !external_helpers ? "static " : "";
  }
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
  TypeId scalar_underlying(TypeId id) const {
    std::set<TypeId> seen;
    while(seen.insert(id).second) {
      const auto& t=model.arena.get(id);
      if(t.kind==TypeKind::Referenced) id=t.referenced.resolved;
      else if(t.kind==TypeKind::ObjectClassField && !t.object_class_field.open_type) id=t.object_class_field.fixed_type;
      else return id;
    }
    return kInvalidType;
  }
  std::string scalar_value_path(TypeId id,std::string path) const {
    std::set<TypeId> seen;
    while(seen.insert(id).second) {
      const auto& t=model.arena.get(id);
      if(t.kind==TypeKind::Referenced) { path += ".value"; id=t.referenced.resolved; }
      else if(t.kind==TypeKind::ObjectClassField && !t.object_class_field.open_type) { path += ".value"; id=t.object_class_field.fixed_type; }
      else if(t.kind==TypeKind::Integer || t.kind==TypeKind::Enumerated) return path+".value";
      else return {};
    }
    return {};
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
    s << linkage(true) << "asn1_status_t " << n << "_defaults_at(" << n << " *v, size_t depth) {\n"
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
      << linkage() << "asn1_status_t " << n << "_reset_defaults(" << n << " *out) {\n"
      << n << " tmp; asn1_status_t status; if(!out) return ASN1_ERR_INVALID_ARGUMENT; " << n << "_init(&tmp);\n"
      << "status=" << n << "_defaults_at(&tmp,0); if(status!=ASN1_OK) { " << n << "_free(&tmp); return status; }\n"
      << n << "_free(out); *out=tmp; return ASN1_OK; }\n";
  }
  void metadata(TypeId id, std::ostream& s) const {
    const auto n=name(id); const auto fs=planning::fields(model.arena.get(id));
    const auto& parent=model.arena.get(id);
    if(parent.kind==TypeKind::Sequence || parent.kind==TypeKind::Set) {
      const auto& seq=per_sequence(parent);
      for(const auto* fields:{&seq.root,&seq.trailing_root})
        for(const auto& field:*fields) emit_selector_free(id,field,s);
    }
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
    s << linkage() << "size_t " << n << "_field_count(void) { return " << fs.size() << "; }\n"
      << linkage() << "const " << prefix << "_field_plan *" << n << "_field(size_t index) { "
      << (fs.empty()?"(void)index; return NULL;":"return index < "+std::to_string(fs.size())+" ? &"+n+"_fields[index] : NULL;") << " }\n"
      << linkage() << "asn1_status_t " << n << "_select(size_t index, const char *key, uint32_t *type) {\n"
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
    if(t.kind==TypeKind::Sequence||t.kind==TypeKind::Set) {
      const auto& seq=per_sequence(t);
      for(const auto* fields:{&seq.root,&seq.trailing_root}) for(const auto& field:*fields)
        if(typed_open_field(field))
          h << "  uint32_t " << decoded_type_member(field) << "; void* " << decoded_value_member(field) << ";\n";
    }
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
  void helper_declarations(std::ostream& s) const {
    for (const auto& item : names) {
      const auto& n=item.second;
      const auto& t=model.arena.get(item.first);
      if(c_extra()) s << linkage(true) << "const asn1_codec_schema_t* " << n << "_codec_schema(void);\n";
      if(c_schema()) s << linkage(true) << "const asn1_tlv_schema_t* " << n << "_tlv_schema(void);\n";
      s << linkage(true) << "asn1_status_t " << n << "_copy_at(const " << n << "*, " << n << "*, size_t);\n"
        << linkage(true) << "asn1_status_t " << n << "_defaults_at(" << n << "*, size_t);\n";
      for (const auto* variant : {"uper", "aper"}) {
        if ((std::string(variant)=="uper" && c_uper) || (std::string(variant)=="aper" && c_aper))
          s << linkage(true) << "asn1_status_t " << n << "_encode_" << variant << "_at(asn1_bit_writer_t*,const " << n << "*,size_t);\n"
            << linkage(true) << "asn1_status_t " << n << "_decode_" << variant << "_at(asn1_bit_reader_t*," << n << "*,size_t);\n";
      }
      if(t.kind==TypeKind::Sequence || t.kind==TypeKind::Set) {
        const auto& seq=per_sequence(t);
        for(const auto* fields:{&seq.root,&seq.trailing_root})
          for(const auto& field:*fields) if(typed_open_field(field))
            for(const auto* variant:{"uper","aper"})
              if((std::string(variant)=="uper"&&c_uper)||(std::string(variant)=="aper"&&c_aper))
                s << linkage(true) << "asn1_status_t " << n << "_decode_selected_" << selector_function_suffix(field) << "_" << variant << "_impl(const " << n << "*,uint32_t*,void**,asn1_bit_reader_t*);\n";
      }
    }
  }
  std::string per_variant(const char* variant) const {
    return std::string("ASN1_PER_") + (std::string(variant)=="aper" ? "ALIGNED" : "UNALIGNED");
  }
  std::string per_int_range(const Type& t) const {
    const auto& c=t.integer.constraint;
    if (c.root_ranges.size()>1) return {};
    std::optional<std::int64_t> lower,upper;
    if(c.lower) { if(!c.lower->as_i64) return {}; lower=*c.lower->as_i64; }
    if(c.upper) { if(!c.upper->as_i64) return {}; upper=*c.upper->as_i64; }
    std::ostringstream out;
    out << "{" << (lower?literal(*lower):"0") << "," << bool(lower) << ","
        << (upper?literal(*upper):"0") << "," << bool(upper) << "," << c.extensible << "}";
    return out.str();
  }
  std::string per_size_range(const ConstraintDesc& c) const {
    if(c.empty || c.root_ranges.size()>1 ||
       (c.lower && (!c.lower->as_i64 || *c.lower->as_i64<0)) ||
       (c.upper && (!c.upper->as_i64 || *c.upper->as_i64<0))) return {};
    std::ostringstream out;
    const auto kind=c.per_kind();
    const auto kind_name=kind==PerBoundKind::Constrained?"ASN1_PER_LENGTH_CONSTRAINED":
      kind==PerBoundKind::SemiConstrained?"ASN1_PER_LENGTH_SEMI_CONSTRAINED":"ASN1_PER_LENGTH_UNCONSTRAINED";
    out << "{" << kind_name << "," << (c.lower?"(size_t)UINT64_C("+std::to_string(static_cast<std::uint64_t>(*c.lower->as_i64))+")":"0")
        << "," << (c.upper?"(size_t)UINT64_C("+std::to_string(static_cast<std::uint64_t>(*c.upper->as_i64))+")":"0")
        << "," << c.extensible << "}";
    return out.str();
  }
  const SequenceDesc& per_sequence(const Type& t) const {
    return t.kind==TypeKind::Set?t.set:t.sequence;
  }
  std::vector<const ExtensionGroup*> per_extension_groups(const SequenceDesc& desc) const {
    std::vector<const ExtensionGroup*> out;
    for(const auto& group:desc.extension_groups) out.push_back(&group);
    return out;
  }
  std::string per_group_present(const ExtensionGroup& group,const std::string& value) const {
    std::string out="(";
    for(std::size_t i=0;i<group.fields.size();++i){if(i)out+=" || ";out+=value+".has_f"+spell(group.fields[i].name);}
    return out+")";
  }
  std::string per_default_test(const Field& field, const std::string& expression) const {
    if (!field.default_value || *field.default_value>=model.arena.value_count()) return {};
    TypeId id=field.type; std::set<TypeId> seen; std::string value=expression;
    while(model.arena.get(id).kind==TypeKind::Referenced) {
      if(!seen.insert(id).second) return {};
      value += ".value"; id=model.arena.get(id).referenced.resolved;
    }
    const auto& t=model.arena.get(id); const auto& def=model.arena.get_value(*field.default_value);
    if(t.kind==TypeKind::Boolean && def.kind==ValueKind::Boolean)
      return "("+value+".value == "+(def.boolean?"1":"0")+")";
    if(t.kind==TypeKind::Integer && !big_integer(t) && def.kind==ValueKind::Integer && def.integer.as_i64)
      return "("+value+".value == "+literal(*def.integer.as_i64)+")";
    if(t.kind==TypeKind::Enumerated && def.kind==ValueKind::Reference) {
      for(const auto* list:{&t.enumerated.root,&t.enumerated.extensions})
        for(const auto& item:*list) if(item.name==def.name && item.value.as_i64)
          return "("+value+".value == "+literal(*item.value.as_i64)+")";
    }
    if(const auto binary=planning::binary_default(def)) {
      std::string bytes;for(auto b:binary->bytes)bytes+=static_cast<char>(b);
      const bool bits=t.kind==TypeKind::BitString;
      const auto data=value+(bits?".value.bits":".data");
      return "("+value+(bits?".value.bit_length == "+std::to_string(binary->bits):".count == "+std::to_string(bytes.size()))+
        (bytes.empty()?"": " && "+data+" && memcmp("+data+","+c_string(bytes)+","+std::to_string(bytes.size())+")==0")+")";
    }
    if(const auto text=planning::string_default(def))
      return "("+value+".count == "+std::to_string(text->size())+(text->empty()?"":" && "+value+".data && memcmp("+value+".data,"+c_string(*text)+","+std::to_string(text->size())+")==0")+")";
    return {};
  }
  std::size_t owning_default_bytes(const Field& f) const {
    const auto& value=model.arena.get_value(*f.default_value);
    if(const auto b=planning::binary_default(value))return b->bytes.size();
    if(const auto text=planning::string_default(value))return text->size();return 0;
  }
  void emit_owned_default(const Field& f,std::string path,std::ostream& s) const {
    TypeId id=f.type;while(model.arena.get(id).kind==TypeKind::Referenced){path+=".value";id=model.arena.get(id).referenced.resolved;}
    const auto& value=model.arena.get_value(*f.default_value);std::string bytes;std::size_t bits=0;
    if(const auto binary=planning::binary_default(value)){for(auto b:binary->bytes)bytes+=static_cast<char>(b);bits=binary->bits;}
    else if(const auto text=planning::string_default(value))bytes=*text;
    const bool is_bits=model.arena.get(id).kind==TypeKind::BitString;const auto data=path+(is_bits?".value.bits":".data");
    s << "{ size_t n=" << bytes.size() << "; if(n) { " << data << "=(uint8_t*)malloc(n); if(!" << data << ") return ASN1_ERR_OUT_OF_MEMORY; memcpy(" << data << ',' << c_string(bytes) << ",n); } ";
    if(is_bits)s << path << ".value.bit_length=" << bits << "; " << path << ".value.cap_bytes=n;";
    else s << path << ".count=" << path << ".capacity=n;";
    s << " }\n";
  }
  bool c_per_supported(TypeId id, std::set<TypeId>& active, bool diagnose) const {
    if (!active.insert(id).second) {
      // Cycles are represented through pointer-bearing generated C structs;
      // the depth-checked *_at helpers terminate traversal at runtime.
      return true;
    }
    const auto& t=model.arena.get(id); bool ok=true;
    switch(t.kind) {
      case TypeKind::Boolean: case TypeKind::Null: case TypeKind::Real: break;
      case TypeKind::ObjectClassField:
        if(!t.object_class_field.open_type && !c_per_supported(t.object_class_field.fixed_type,active,diagnose)) ok=false;
        break;
      case TypeKind::BitString:
        if(!t.bit_string.named_bits.empty() || t.bit_string.containing!=kInvalidType ||
           per_size_range(t.bit_string.size).empty()) ok=false;
        break;
      case TypeKind::OctetString:
        if(t.octet_string.containing!=kInvalidType || per_size_range(t.octet_string.size).empty()) ok=false;
        break;
      case TypeKind::String:
        if(per_size_range(t.string.size).empty()) ok=false;
        break;
      case TypeKind::ObjectIdentifier: case TypeKind::RelativeOid: break;
      case TypeKind::Integer:
        if(big_integer(t)) {
          if(t.integer.constraint.lower || t.integer.constraint.upper || t.integer.constraint.extensible) ok=false;
        } else if(per_int_range(t).empty()) ok=false;
        break;
      case TypeKind::Enumerated:
        if(t.enumerated.root.empty()) ok=false;
        for(const auto* list:{&t.enumerated.root,&t.enumerated.extensions})
          for(const auto& value:*list) if(!value.value.as_i64) ok=false;
        break;
      case TypeKind::Choice:
        if(t.choice.alternatives.empty()) ok=false;
        for(const auto& field:t.choice.alternatives)
          if(!c_per_supported(field.type,active,diagnose)) ok=false;
        for(const auto& field:t.choice.extensions)
          if(!c_per_supported(field.type,active,diagnose)) ok=false;
        break;
      case TypeKind::Referenced:
        ok=c_per_supported(t.referenced.resolved,active,diagnose); break;
      case TypeKind::SequenceOf: case TypeKind::SetOf: {
        const auto element=t.kind==TypeKind::SequenceOf?t.sequence_of.element:t.set_of.element;
        const auto& size=t.kind==TypeKind::SequenceOf?t.sequence_of.size:t.set_of.size;
        if(per_size_range(size).empty()) ok=false;
        if(!c_per_supported(element,active,diagnose)) ok=false;
        break;
      }
      case TypeKind::Sequence: case TypeKind::Set: {
        const auto& sequence=per_sequence(t);
        auto check_selectors=[&](const std::vector<Field>& fields) {
          for(const auto& field:fields) if(field.table_constraint) {
            const auto field_type=underlying(field.type);
            if(field_type==kInvalidType) { ok=false; continue; }
            if(model.arena.get(field_type).kind!=TypeKind::ObjectClassField ||
               !model.arena.get(field_type).object_class_field.open_type) continue;
            const Field* governor=nullptr;
            for(const auto* list:{&sequence.root,&sequence.trailing_root})
              for(const auto& candidate:*list)
                if(candidate.name==field.table_constraint->governor_field_name) governor=&candidate;
            if(!governor || scalar_underlying(governor->type)==kInvalidType) { ok=false; continue; }
            const auto scalar=scalar_underlying(governor->type);
            if(big_integer(model.arena.get(scalar))) { ok=false; continue; }
            const auto governor_kind=model.arena.get(scalar).kind;
            if(governor_kind!=TypeKind::Integer && governor_kind!=TypeKind::Enumerated) ok=false;
            for(const auto& entry:field.table_constraint->entries)
              if(!entry.has_id_value || !entry.id_value.as_i64 || !c_per_supported(entry.type,active,diagnose)) ok=false;
          }
        };
        check_selectors(sequence.root);
        check_selectors(sequence.trailing_root);
        for(const auto& group:sequence.extension_groups) {
          if(group.fields.empty() || (!group.version_bracket && group.fields.size()!=1)) ok=false;
          for(const auto& f:group.fields) {
            if(f.presence==Presence::Default && default_assignment(f,"v").empty()) ok=false;
            if(!c_per_supported(f.type,active,diagnose)) ok=false;
          }
        }
        for(const auto& field:sequence.root) {
          if(field.presence==Presence::Default && per_default_test(field,"v").empty()) ok=false;
          if(field.presence==Presence::Default) {
            const auto dep=underlying(field.type);
            const auto kind=dep==kInvalidType?TypeKind::Null:model.arena.get(dep).kind;
            if(kind!=TypeKind::Boolean && kind!=TypeKind::Integer && kind!=TypeKind::Enumerated && kind!=TypeKind::BitString && kind!=TypeKind::OctetString && kind!=TypeKind::String) ok=false;
          }
          if(!c_per_supported(field.type,active,diagnose)) ok=false;
        }
        for(const auto& field:sequence.trailing_root) {
          if(field.presence==Presence::Default && per_default_test(field,"v").empty()) ok=false;
          if(field.presence==Presence::Default) {
            const auto dep=underlying(field.type);
            const auto kind=dep==kInvalidType?TypeKind::Null:model.arena.get(dep).kind;
            if(kind!=TypeKind::Boolean && kind!=TypeKind::Integer && kind!=TypeKind::Enumerated && kind!=TypeKind::BitString && kind!=TypeKind::OctetString && kind!=TypeKind::String) ok=false;
          }
          if(!c_per_supported(field.type,active,diagnose)) ok=false;
        }
        break;
      }
      default: ok=false; break;
    }
    active.erase(id);
    if(!ok && diagnose) diag.error({},"Unsupported C PER value layout for " + (t.name.empty()?std::string("anonymous type"):t.name));
    return ok;
  }
  std::string selector_function_suffix(const Field& field) const { return spell(field.name); }
  bool typed_open_field(const Field& field) const {
    if(!field.table_constraint) return false;
    const auto u=underlying(field.type);
    return u!=kInvalidType && model.arena.get(u).kind==TypeKind::ObjectClassField &&
           model.arena.get(u).object_class_field.open_type;
  }
  std::string decoded_type_member(const Field& field) const { return "decoded_type_f"+spell(field.name); }
  std::string decoded_value_member(const Field& field) const { return "decoded_value_f"+spell(field.name); }
  void emit_selector_decoder(TypeId id,const Field& field,const char* variant,
                             std::ostream& s) const {
    if(!typed_open_field(field) || field.table_constraint->entries.empty()) return;
    const auto& t=model.arena.get(id); const auto& seq=per_sequence(t);
    const Field* governor=nullptr;
    for(const auto* list:{&seq.root,&seq.trailing_root})
      for(const auto& candidate:*list)
        if(candidate.name==field.table_constraint->governor_field_name) governor=&candidate;
    if(!governor) return;
    const std::string key=scalar_value_path(governor->type,"value->f"+spell(governor->name));
    if(key.empty()) return;
    const auto suffix=selector_function_suffix(field);
    const auto func=name(id)+"_decode_selected_"+suffix+"_"+variant;
    const auto impl=func+"_impl";
    s << linkage(true) << "asn1_status_t " << impl << "(const " << name(id)
      << "* value,uint32_t* out_type,void** out_value,asn1_bit_reader_t* budget_parent) { if(!value||!out_type||!out_value) return ASN1_ERR_INVALID_ARGUMENT; if(value->f"
      << spell(field.name) << ".count>value->f" << spell(field.name) << ".capacity || (value->f" << spell(field.name)
      << ".count && !value->f" << spell(field.name) << ".data)) return ASN1_ERR_INVALID_ARGUMENT; switch(" << key << ") {\n";
    for(const auto& entry:field.table_constraint->entries) {
      if(!entry.has_id_value || !entry.id_value.as_i64) continue;
      const auto selected=name(entry.type);
      s << "case " << literal(*entry.id_value.as_i64) << ": { " << selected << "* decoded=NULL; asn1_bit_reader_t* sub=NULL; asn1_status_t status=asn1_bit_reader_create(&sub,asn1_byte_view(value->f"
        << spell(field.name) << ".data,value->f" << spell(field.name) << ".count)); if(status==ASN1_OK) status=budget_parent?asn1_bit_reader_share_budget(sub,budget_parent):asn1_bit_reader_begin_decode(sub); if(status==ASN1_OK) status=asn1_bit_reader_reserve(sub,sizeof(*decoded),1); if(status==ASN1_OK) { decoded=(" << selected << "*)calloc(1,sizeof(*decoded)); if(!decoded) status=ASN1_ERR_OUT_OF_MEMORY; else " << selected << "_init(decoded); } if(status==ASN1_OK) status=" << selected
        << "_decode_" << variant << "_at(sub,decoded,0); if(status==ASN1_OK) { size_t rem=asn1_bit_reader_bit_remaining(sub); uint64_t pad=0; if(rem>7u) status=ASN1_ERR_INVALID_ARGUMENT; else if(rem) { status=asn1_bit_reader_get_bits(sub,rem,&pad); if(status==ASN1_OK && pad) status=ASN1_ERR_NON_CANONICAL; } } asn1_bit_reader_destroy(sub); if(status!=ASN1_OK) { " << selected << "_free(decoded); free(decoded); return status; } *out_type=UINT32_C(" << entry.type << "); *out_value=decoded; return ASN1_OK; }\n";
    }
    s << "default: return ASN1_ERR_CONSTRAINT_VIOLATION; } }\n"
      << linkage() << "asn1_status_t " << func << "(const " << name(id) << "* value,uint32_t* type,void** decoded) { return " << impl << "(value,type,decoded,NULL); }\n";
  }
  void emit_selector_free(TypeId id,const Field& field,std::ostream& s) const {
    if(!typed_open_field(field)) return;
    const auto suffix=selector_function_suffix(field);
    s << linkage() << "void " << name(id) << "_free_selected_" << suffix << "(uint32_t type,void* value) { if(!value) return; switch(type) {\n";
    std::set<TypeId> emitted_types;
    for(const auto& entry:field.table_constraint->entries) if(entry.has_id_value && entry.id_value.as_i64 && emitted_types.insert(entry.type).second)
      s << "case UINT32_C(" << entry.type << "): " << name(entry.type) << "_free((" << name(entry.type) << "*)value); break;\n";
    s << "default: break; } free(value); }\n";
  }
  void emit_selector_encode(TypeId id,const Field& field,const std::string& value,
                            const char* variant,std::ostream& s,const std::string& ind) const {
    const auto& seq=per_sequence(model.arena.get(id));
    const Field* governor=nullptr;
    for(const auto* fields:{&seq.root,&seq.trailing_root})
      for(const auto& candidate:*fields)
        if(candidate.name==field.table_constraint->governor_field_name) governor=&candidate;
    if(!governor) { s << ind << "return ASN1_ERR_UNSUPPORTED;\n"; return; }
    const auto key=scalar_value_path(governor->type,"("+value+").f"+spell(governor->name));
    if(key.empty()) { s << ind << "return ASN1_ERR_UNSUPPORTED;\n"; return; }
    const auto type_member=decoded_type_member(field), value_member=decoded_value_member(field);
    s << ind << "if(" << value << "." << value_member << ") { switch(" << value << "." << type_member << ") {\n";
    std::set<TypeId> emitted_types;
    for(const auto& entry:field.table_constraint->entries) if(entry.has_id_value && entry.id_value.as_i64 && emitted_types.insert(entry.type).second) {
      const auto selected=name(entry.type);
      s << ind << "case UINT32_C(" << entry.type << "): if(!(";
      bool first_key=true;
      for(const auto& allowed:field.table_constraint->entries) if(allowed.type==entry.type && allowed.has_id_value && allowed.id_value.as_i64) {
        if(!first_key) s << " || ";
        first_key=false;
        s << key << "==" << literal(*allowed.id_value.as_i64);
      }
      s << ")) return ASN1_ERR_CONSTRAINT_VIOLATION; { asn1_bit_writer_t* ew=NULL; uint8_t* data=NULL; size_t size=0; status=asn1_bit_writer_create(&ew); if(status==ASN1_OK) status=" << selected << "_encode_" << variant << "_at(ew,(const " << selected << "*)" << value << "." << value_member << ",depth+1); if(status==ASN1_OK) status=asn1_bit_writer_take(ew,&data,&size); asn1_bit_writer_destroy(ew); if(status==ASN1_OK) status=asn1_per_encode_open_type(w," << per_variant(variant) << ",asn1_byte_view(data,size)); free(data); if(status!=ASN1_OK) return status; } break;\n";
    }
    const auto raw=value+".f"+spell(field.name);
    s << ind << "default: return ASN1_ERR_INVALID_ARGUMENT; } } else { if(" << raw << ".count>" << raw << ".capacity || (" << raw << ".count && !" << raw << ".data)) return ASN1_ERR_INVALID_ARGUMENT; status=asn1_per_encode_open_type(w," << per_variant(variant)
      << ",asn1_byte_view(" << raw << ".data," << raw << ".count)); if(status!=ASN1_OK) return status; }\n";
  }
  void emit_per_encode_value(TypeId id, const std::string& value,
                             const char* variant, std::ostream& s,
                             const std::string& ind) const {
    const auto& t=model.arena.get(id); const auto v=per_variant(variant);
    switch(t.kind) {
      case TypeKind::Boolean:
        s << ind << "if(" << value << ".value!=0 && " << value << ".value!=1) return ASN1_ERR_INVALID_ARGUMENT; status=asn1_bit_writer_put_bit(w," << value << ".value); if(status!=ASN1_OK) return status;\n"; break;
      case TypeKind::Null: break;
      case TypeKind::ObjectClassField:
        if(t.object_class_field.open_type)
          s << ind << "if(" << value << ".count>" << value << ".capacity || (" << value << ".count && !" << value << ".data)) return ASN1_ERR_INVALID_ARGUMENT; status=asn1_per_encode_open_type(w," << v << ",asn1_byte_view(" << value << ".data," << value << ".count)); if(status!=ASN1_OK) return status;\n";
        else s << ind << "status=" << name(t.object_class_field.fixed_type) << "_encode_" << variant << "_at(w,&" << value << ".value,depth+1); if(status!=ASN1_OK) return status;\n";
        break;
      case TypeKind::Real:
        s << ind << "status=asn1_per_encode_real_ex(w," << v << "," << value << ".value); if(status!=ASN1_OK) return status;\n"; break;
      case TypeKind::BitString:
        s << ind << "{ const asn1_per_length_constraint_t size=" << per_size_range(t.bit_string.size) << "; status=asn1_per_encode_bitstring_ex(w," << v << ",&size,&" << value << ".value); if(status!=ASN1_OK) return status; }\n"; break;
      case TypeKind::OctetString:
        s << ind << "{ const asn1_per_length_constraint_t size=" << per_size_range(t.octet_string.size) << "; if(" << value << ".count>" << value << ".capacity || (" << value << ".count && !" << value << ".data)) return ASN1_ERR_INVALID_ARGUMENT; status=asn1_per_encode_octetstring_ex(w," << v << ",&size,asn1_byte_view(" << value << ".data," << value << ".count)); if(status!=ASN1_OK) return status; }\n"; break;
      case TypeKind::String: {
        const bool restricted = t.string.kind==StringKind::IA5String || t.string.kind==StringKind::PrintableString ||
          t.string.kind==StringKind::VisibleString || t.string.kind==StringKind::NumericString ||
          t.string.kind==StringKind::BMPString || t.string.kind==StringKind::UniversalString;
        const char* sk = t.string.kind==StringKind::IA5String?"ASN1_PER_STRING_IA5":
          t.string.kind==StringKind::PrintableString?"ASN1_PER_STRING_PRINTABLE":
          t.string.kind==StringKind::VisibleString?"ASN1_PER_STRING_VISIBLE":
          t.string.kind==StringKind::NumericString?"ASN1_PER_STRING_NUMERIC":
          t.string.kind==StringKind::BMPString?"ASN1_PER_STRING_BMP":"ASN1_PER_STRING_UNIVERSAL";
        s << ind << "{ const asn1_per_length_constraint_t size=" << per_size_range(t.string.size) << "; if(" << value << ".count>" << value << ".capacity || (" << value << ".count && !" << value << ".data)) return ASN1_ERR_INVALID_ARGUMENT; status=asn1_per_" << (restricted?"encode_restricted_string_ex(w,":"encode_octetstring_ex(w,") << v << ",&size,";
        if(restricted) s << sk << ",";
        s << "asn1_byte_view(" << value << ".data," << value << ".count)); if(status!=ASN1_OK) return status; }\n";
        break;
      }
      case TypeKind::ObjectIdentifier: case TypeKind::RelativeOid:
        s << ind << "if(" << value << ".count>" << value << ".capacity || (" << value << ".count && !" << value << ".data)) return ASN1_ERR_INVALID_ARGUMENT; status=asn1_per_encode_oid_ex(w," << v << "," << value << ".data," << value << ".count," << (t.kind==TypeKind::RelativeOid?1:0) << "); if(status!=ASN1_OK) return status;\n"; break;
      case TypeKind::Integer:
        if(big_integer(t))
          s << ind << "status=asn1_per_encode_unbounded_int_ex(w," << v << ",&" << value << ".value); if(status!=ASN1_OK) return status;\n";
        else
          s << ind << "{ const asn1_per_int_range_t range=" << per_int_range(t) << "; status=asn1_per_encode_int_ex(w," << v << ",&range," << value << ".value); if(status!=ASN1_OK) return status; }\n";
        break;
      case TypeKind::Enumerated: {
        std::vector<std::pair<std::int64_t,std::size_t>> roots;
        for(std::size_t i=0;i<t.enumerated.root.size();++i) roots.push_back({*t.enumerated.root[i].value.as_i64,i});
        std::sort(roots.begin(),roots.end());
        s << ind << "switch(" << value << ".value) {\n";
        for(std::size_t i=0;i<roots.size();++i)
          s << ind << "case " << literal(roots[i].first) << ": status=asn1_per_encode_enumerated_ex(w," << v << "," << i << "," << roots.size() << "," << t.enumerated.extensible << ",0,0); break;\n";
        for(std::size_t i=0;i<t.enumerated.extensions.size();++i)
          s << ind << "case " << literal(*t.enumerated.extensions[i].value.as_i64) << ": status=asn1_per_encode_enumerated_ex(w," << v << ",0," << roots.size() << "," << t.enumerated.extensible << ",1," << i << "); break;\n";
        s << ind << "default: return ASN1_ERR_CONSTRAINT_VIOLATION; } if(status!=ASN1_OK) return status;\n";
        break;
      }
      case TypeKind::Referenced:
        if(pointer(id,t.referenced.resolved)) s << ind << "if(!" << value << ".value) return ASN1_ERR_INVALID_ARGUMENT; status=" << name(t.referenced.resolved) << "_encode_" << variant << "_at(w," << value << ".value,depth+1); if(status!=ASN1_OK) return status;\n";
        else s << ind << "status=" << name(t.referenced.resolved) << "_encode_" << variant << "_at(w,&" << value << ".value,depth+1); if(status!=ASN1_OK) return status;\n";
        break;
      case TypeKind::Sequence: case TypeKind::Set: {
        const auto& sequence=per_sequence(t);
        const auto groups=per_extension_groups(sequence);
        std::vector<const Field*> presence;
        for(const auto& f:sequence.root) if(f.presence!=Presence::Mandatory) presence.push_back(&f);
        for(const auto& f:sequence.trailing_root) if(f.presence!=Presence::Mandatory) presence.push_back(&f);
        for(const auto* group:groups) for(const auto& f:group->fields)
          s << ind << "if(" << value << ".has_f" << spell(f.name) << "!=0 && " << value << ".has_f" << spell(f.name) << "!=1) return ASN1_ERR_INVALID_ARGUMENT;\n";
        for(const auto* group:groups) if(group->version_bracket) for(const auto& f:group->fields)
          if(f.presence==Presence::Mandatory)
            s << ind << "if(" << per_group_present(*group,value) << " && !" << value << ".has_f" << spell(f.name) << ") return ASN1_ERR_CONSTRAINT_VIOLATION;\n";
        for(const auto* f:presence) if(f->presence==Presence::Optional)
          s << ind << "if(" << value << ".has_f" << spell(f->name) << "!=0 && " << value << ".has_f" << spell(f->name) << "!=1) return ASN1_ERR_INVALID_ARGUMENT;\n";
        if(!presence.empty()) {
          s << ind << "{ const uint8_t present[" << std::max<std::size_t>(1,presence.size()) << "]={";
          for(std::size_t i=0;i<presence.size();++i) {
            const auto& f=*presence[i]; if(i) s << ',';
            const auto member=value+".f"+spell(f.name);
            if(f.presence==Presence::Optional) s << "(uint8_t)(" << value << ".has_f" << spell(f.name) << "!=0)";
            else s << "(uint8_t)!(" << per_default_test(f,member) << ")";
          }
          s << "}; status=asn1_per_encode_sequence_preamble(w," << sequence.extensible << ",";
          if(sequence.extensible&&!groups.empty()) { s << "("; for(std::size_t i=0;i<groups.size();++i){if(i)s<<" || ";s<<per_group_present(*groups[i],value);} s << ")"; }
          else s << "0";
          s << ",present," << presence.size() << "); if(status!=ASN1_OK) return status; }\n";
        } else {
          s << ind << "status=asn1_per_encode_sequence_preamble(w," << sequence.extensible << ",";
          if(sequence.extensible&&!groups.empty()) { s << "("; for(std::size_t i=0;i<groups.size();++i){if(i)s<<" || ";s<<per_group_present(*groups[i],value);} s << ")"; }
          else s << "0";
          s << ",NULL,0); if(status!=ASN1_OK) return status;\n";
        }
        auto emit_root_encode=[&](const std::vector<Field>& fields) { for(const auto& f:fields) {
          const auto member=value+".f"+spell(f.name);
          if(f.presence==Presence::Optional) s << ind << "if(" << value << ".has_f" << spell(f.name) << ") {\n";
          else if(f.presence==Presence::Default) s << ind << "if(!( " << per_default_test(f,member) << ")) {\n";
          s << ind << ((f.presence==Presence::Mandatory)?"":"  ");
          if(typed_open_field(f)) emit_selector_encode(id,f,value,variant,s,ind+((f.presence==Presence::Mandatory)?"":"  "));
          else if(pointer(id,f.type)) s << "if(!" << member << ") return ASN1_ERR_INVALID_ARGUMENT; status=" << name(f.type) << "_encode_" << variant << "_at(w," << member << ",depth+1); if(status!=ASN1_OK) return status;\n";
          else s << "status=" << name(f.type) << "_encode_" << variant << "_at(w,&" << member << ",depth+1); if(status!=ASN1_OK) return status;\n";
          if(f.presence!=Presence::Mandatory) s << ind << "}\n";
        }};
        emit_root_encode(sequence.root);
        emit_root_encode(sequence.trailing_root);
        if(!groups.empty()) {
          s << ind << "{ const size_t extension_count=" << groups.size() << "; uint8_t extension_presence[" << groups.size() << "]={";
          for(std::size_t i=0;i<groups.size();++i){if(i)s<<',';s<<"(uint8_t)"<<per_group_present(*groups[i],value);}
          s << "}; asn1_byte_view_t extension_values[" << groups.size() << "]={{0}}; uint8_t* extension_data[" << groups.size() << "]={0};\n";
          for(std::size_t i=0;i<groups.size();++i) {
            const auto& group=*groups[i];
            s << ind << "  if(extension_presence[" << i << "]) { asn1_bit_writer_t* ew=NULL; status=asn1_bit_writer_create(&ew); if(status!=ASN1_OK) { for(size_t k=0;k<" << groups.size() << ";++k) free(extension_data[k]); return status; }\n";
            if(group.version_bracket) {
              std::vector<const Field*> group_presence;
              for(const auto& f:group.fields) if(f.presence!=Presence::Mandatory) group_presence.push_back(&f);
              s << ind << "    { const uint8_t gopt[" << std::max<std::size_t>(1,group_presence.size()) << "]={";
              for(std::size_t j=0;j<group_presence.size();++j){if(j)s<<',';s<<"(uint8_t)"<<value<<".has_f"<<spell(group_presence[j]->name);}
              s << "}; status=asn1_per_encode_sequence_preamble(ew,0,0," << (group_presence.empty()?"NULL":"gopt") << "," << group_presence.size() << "); }\n";
            }
            for(const auto& f:group.fields) {
              const auto member=value+".f"+spell(f.name);
              if(group.version_bracket && f.presence!=Presence::Mandatory) s << ind << "    if(" << value << ".has_f" << spell(f.name) << ") {\n";
              const std::string target=pointer(id,f.type)?member:"&"+member;
              if(pointer(id,f.type)) s << ind << (group.version_bracket&&f.presence!=Presence::Mandatory?"      ":"    ") << "if(!" << member << ") { status=ASN1_ERR_INVALID_ARGUMENT; } else status=" << name(f.type) << "_encode_" << variant << "_at(ew," << target << ",depth+1);\n";
              else s << ind << (group.version_bracket&&f.presence!=Presence::Mandatory?"      ":"    ") << "status=" << name(f.type) << "_encode_" << variant << "_at(ew," << target << ",depth+1);\n";
              s << ind << (group.version_bracket&&f.presence!=Presence::Mandatory?"      ":"    ") << "if(status!=ASN1_OK) { asn1_bit_writer_destroy(ew); for(size_t k=0;k<" << groups.size() << ";++k) free(extension_data[k]); return status; }\n";
              if(group.version_bracket && f.presence!=Presence::Mandatory) s << ind << "    }\n";
            }
            s << ind << "    status=asn1_bit_writer_take(ew,&extension_data[" << i << "],&extension_values[" << i << "].size); asn1_bit_writer_destroy(ew); if(status!=ASN1_OK) { for(size_t k=0;k<" << groups.size() << ";++k) free(extension_data[k]); return status; } extension_values[" << i << "].data=extension_data[" << i << "]; }\n";
          }
          s << ind << "  int any_extension=0; for(size_t k=0;k<extension_count;++k) any_extension|=extension_presence[k]; if(any_extension) status=asn1_per_encode_extension_additions(w," << v << ",extension_presence,extension_count,extension_values); for(size_t k=0;k<extension_count;++k) free(extension_data[k]); if(status!=ASN1_OK) return status; }\n";
        }
        break;
      }
      case TypeKind::SequenceOf: case TypeKind::SetOf: {
        const auto dep=t.kind==TypeKind::SequenceOf?t.sequence_of.element:t.set_of.element;
        const auto& constraint=t.kind==TypeKind::SequenceOf?t.sequence_of.size:t.set_of.size;
        s << ind << "{ const asn1_per_length_constraint_t size=" << per_size_range(constraint) << "; if(" << value << ".count>" << value << ".capacity || (" << value << ".count && !" << value << ".data)) return ASN1_ERR_INVALID_ARGUMENT; size_t remaining=" << value << ".count,offset=0,chunk=0; int more=0,first=1,is_extension=0; do { status=asn1_per_encode_collection_fragment_ex(w," << v << ",&size,remaining,first,&is_extension,&chunk,&more); if(status!=ASN1_OK) return status; for(size_t i=0;i<chunk;++i) { status=" << name(dep) << "_encode_" << variant << "_at(w,&" << value << ".data[offset+i],depth+1); if(status!=ASN1_OK) return status; } remaining-=chunk; offset+=chunk; first=0; } while(more); }\n";
        break;
      }
      case TypeKind::Choice: {
        const auto root_count=t.choice.alternatives.size();
        s << ind << "{ int is_extension=0; size_t root_index=0; uint64_t extension_index=0; switch(" << value << ".kind) {\n";
        for(std::size_t i=0;i<root_count;++i)
          s << ind << "case " << name(id) << "_arm_f" << spell(t.choice.alternatives[i].name) << ": root_index=" << i << "; break;\n";
        for(std::size_t i=0;i<t.choice.extensions.size();++i)
          s << ind << "case " << name(id) << "_arm_f" << spell(t.choice.extensions[i].name) << ": is_extension=1; extension_index=" << i << "; break;\n";
        s << ind << "default: return ASN1_ERR_INVALID_ARGUMENT; } status=asn1_per_encode_enumerated_ex(w," << v << ",root_index," << root_count << "," << t.choice.extensible << ",is_extension,extension_index); if(status!=ASN1_OK) return status; switch(" << value << ".kind) {\n";
        for(const auto& field:t.choice.alternatives)
          s << ind << "case " << name(id) << "_arm_f" << spell(field.name) << ": status=" << name(field.type) << "_encode_" << variant << "_at(w,&" << value << ".value.f" << spell(field.name) << ",depth+1); break;\n";
        for(const auto& field:t.choice.extensions) {
          s << ind << "case " << name(id) << "_arm_f" << spell(field.name) << ": { asn1_bit_writer_t* ew=NULL; uint8_t* data=NULL; size_t size=0; status=asn1_bit_writer_create(&ew); if(status==ASN1_OK) status=" << name(field.type) << "_encode_" << variant << "_at(ew,&" << value << ".value.f" << spell(field.name) << ",depth+1); if(status==ASN1_OK) status=asn1_bit_writer_take(ew,&data,&size); asn1_bit_writer_destroy(ew); if(status==ASN1_OK) status=asn1_per_encode_open_type(w," << v << ",asn1_byte_view(data,size)); free(data); break; }\n";
        }
        s << ind << "default: return ASN1_ERR_INVALID_ARGUMENT; } if(status!=ASN1_OK) return status; }\n";
        break;
      }
      default: s << ind << "return ASN1_ERR_UNSUPPORTED;\n"; break;
    }
  }
  void emit_per_decode_value(TypeId id, const std::string& value,
                             const char* variant, std::ostream& s,
                             const std::string& ind, const std::string& fail) const {
    const auto& t=model.arena.get(id); const auto v=per_variant(variant);
    switch(t.kind) {
      case TypeKind::Boolean:
        s << ind << "{ int bit=0; status=asn1_bit_reader_get_bit(r,&bit); if(status!=ASN1_OK) " << fail << " " << value << ".value=bit; }\n"; break;
      case TypeKind::Null: break;
      case TypeKind::ObjectClassField:
        if(t.object_class_field.open_type)
          s << ind << "{ uint8_t* decoded=NULL; size_t count=0; status=asn1_per_decode_open_type(r," << v << ",&decoded,&count); if(status!=ASN1_OK) " << fail << " " << value << ".data=decoded; " << value << ".count=" << value << ".capacity=count; }\n";
        else s << ind << "status=" << name(t.object_class_field.fixed_type) << "_decode_" << variant << "_at(r,&" << value << ".value,depth+1); if(status!=ASN1_OK) " << fail << "\n";
        break;
      case TypeKind::Real:
        s << ind << "status=asn1_per_decode_real_ex(r," << v << ",&" << value << ".value); if(status!=ASN1_OK) " << fail << "\n"; break;
      case TypeKind::BitString:
        s << ind << "{ const asn1_per_length_constraint_t size=" << per_size_range(t.bit_string.size) << "; status=asn1_per_decode_bitstring_ex(r," << v << ",&size,&" << value << ".value); if(status!=ASN1_OK) " << fail << " }\n"; break;
      case TypeKind::OctetString:
        s << ind << "{ const asn1_per_length_constraint_t size=" << per_size_range(t.octet_string.size) << "; uint8_t* decoded=NULL; size_t count=0; status=asn1_per_decode_octetstring_ex(r," << v << ",&size,&decoded,&count); if(status!=ASN1_OK) " << fail << " " << value << ".data=decoded; " << value << ".count=" << value << ".capacity=count; }\n"; break;
      case TypeKind::String: {
        const bool restricted = t.string.kind==StringKind::IA5String || t.string.kind==StringKind::PrintableString ||
          t.string.kind==StringKind::VisibleString || t.string.kind==StringKind::NumericString ||
          t.string.kind==StringKind::BMPString || t.string.kind==StringKind::UniversalString;
        const char* sk = t.string.kind==StringKind::IA5String?"ASN1_PER_STRING_IA5":
          t.string.kind==StringKind::PrintableString?"ASN1_PER_STRING_PRINTABLE":
          t.string.kind==StringKind::VisibleString?"ASN1_PER_STRING_VISIBLE":
          t.string.kind==StringKind::NumericString?"ASN1_PER_STRING_NUMERIC":
          t.string.kind==StringKind::BMPString?"ASN1_PER_STRING_BMP":"ASN1_PER_STRING_UNIVERSAL";
        s << ind << "{ const asn1_per_length_constraint_t size=" << per_size_range(t.string.size) << "; uint8_t* decoded=NULL; size_t count=0; status=asn1_per_" << (restricted?"decode_restricted_string_ex(r,":"decode_octetstring_ex(r,") << v << ",&size,";
        if(restricted) s << sk << ",";
        s << "&decoded,&count); if(status!=ASN1_OK) " << fail << " " << value << ".data=decoded; " << value << ".count=" << value << ".capacity=count; }\n";
        break;
      }
      case TypeKind::ObjectIdentifier: case TypeKind::RelativeOid:
        s << ind << "{ uint64_t* arcs=NULL; size_t count=0; status=asn1_per_decode_oid_ex(r," << v << ",&arcs,&count," << (t.kind==TypeKind::RelativeOid?1:0) << "); if(status!=ASN1_OK) " << fail << " " << value << ".data=arcs; " << value << ".count=" << value << ".capacity=count; }\n"; break;
      case TypeKind::Integer:
        if(big_integer(t))
          s << ind << "status=asn1_per_decode_unbounded_int_ex(r," << v << ",&" << value << ".value); if(status!=ASN1_OK) " << fail << "\n";
        else
          s << ind << "{ const asn1_per_int_range_t range=" << per_int_range(t) << "; status=asn1_per_decode_int_ex(r," << v << ",&range,&" << value << ".value); if(status!=ASN1_OK) " << fail << " }\n";
        break;
      case TypeKind::Enumerated:
        s << ind << "{ int is_extension=0; uint64_t ordinal=0; status=asn1_per_decode_enumerated_ex(r," << v << "," << t.enumerated.root.size() << "," << t.enumerated.extensible << ",&is_extension,&ordinal); if(status!=ASN1_OK) " << fail << "\n"
          << ind << "  if(is_extension) { switch(ordinal) {\n";
        for(std::size_t i=0;i<t.enumerated.extensions.size();++i)
          s << ind << "  case " << i << ": " << value << ".value=" << literal(*t.enumerated.extensions[i].value.as_i64) << "; break;\n";
        s << ind << "  default: status=ASN1_ERR_UNSUPPORTED; " << fail << " } } else { switch(ordinal) {\n";
        {
          std::vector<std::int64_t> roots; for(const auto& e:t.enumerated.root) roots.push_back(*e.value.as_i64);
          std::sort(roots.begin(),roots.end());
          for(std::size_t i=0;i<roots.size();++i) s << ind << "  case " << i << ": " << value << ".value=" << literal(roots[i]) << "; break;\n";
        }
        s << ind << "  default: status=ASN1_ERR_CONSTRAINT_VIOLATION; " << fail << " } } }\n";
        break;
      case TypeKind::Referenced:
        if(pointer(id,t.referenced.resolved)) s << ind << "status=asn1_bit_reader_reserve(r,sizeof(*" << value << ".value),1); if(status!=ASN1_OK) " << fail << " " << value << ".value=(" << name(t.referenced.resolved) << "*)calloc(1,sizeof(*" << value << ".value)); if(!" << value << ".value) { status=ASN1_ERR_OUT_OF_MEMORY; " << fail << " } " << name(t.referenced.resolved) << "_init(" << value << ".value); status=" << name(t.referenced.resolved) << "_decode_" << variant << "_at(r," << value << ".value,depth+1); if(status!=ASN1_OK) " << fail << "\n";
        else s << ind << "status=" << name(t.referenced.resolved) << "_decode_" << variant << "_at(r,&" << value << ".value,depth+1); if(status!=ASN1_OK) " << fail << "\n";
        break;
      case TypeKind::Sequence: case TypeKind::Set: {
        const auto& sequence=per_sequence(t);
        const auto groups=per_extension_groups(sequence);
        std::vector<const Field*> presence; for(const auto& f:sequence.root) if(f.presence!=Presence::Mandatory) presence.push_back(&f);
        for(const auto& f:sequence.trailing_root) if(f.presence!=Presence::Mandatory) presence.push_back(&f);
        s << ind << "{ asn1_per_sequence_preamble_t pre={0}; status=asn1_per_decode_sequence_preamble(r," << sequence.extensible << "," << presence.size() << ",&pre); if(status!=ASN1_OK) return status;\n";
        std::size_t i=0;
        auto emit_root_decode=[&](const std::vector<Field>& fields) { for(const auto& f:fields) {
          const auto member=value+".f"+spell(f.name);
          if(f.presence!=Presence::Mandatory) {
            if(f.presence==Presence::Optional) s << ind << "  " << value << ".has_f" << spell(f.name) << "=pre.optional_bits[" << i << "];\n";
            s << ind << "  if(pre.optional_bits[" << i << "]) {\n";
          }
          s << ind << (f.presence==Presence::Mandatory?"  ":"    ");
          if(pointer(id,f.type)) s << "status=asn1_bit_reader_reserve(r,sizeof(*" << member << "),1); if(status!=ASN1_OK) goto per_fail; " << member << "=(" << name(f.type) << "*)calloc(1,sizeof(*" << member << ")); if(!" << member << ") { status=ASN1_ERR_OUT_OF_MEMORY; goto per_fail; } " << name(f.type) << "_init(" << member << "); status=" << name(f.type) << "_decode_" << variant << "_at(r," << member << ",depth+1); if(status!=ASN1_OK) goto per_fail;\n";
          else s << "status=" << name(f.type) << "_decode_" << variant << "_at(r,&" << member << ",depth+1); if(status!=ASN1_OK) goto per_fail;\n";
          if(f.presence!=Presence::Mandatory) s << ind << "  }\n";
          if(f.presence==Presence::Default && owning_default(f)) {
            s << ind << "if(!pre.optional_bits[" << i << "]) { status=asn1_bit_reader_reserve(r," << owning_default_bytes(f) << ",0); if(status!=ASN1_OK) goto per_fail;\n";
            // Materialization cannot return early while the preamble owns memory.
            s << "{ " << name(f.type) << " def; " << name(f.type) << "_init(&def);\n";
            std::ostringstream materialize;emit_owned_default(f,"def",materialize);
            auto code=materialize.str();const std::string from="return ASN1_ERR_OUT_OF_MEMORY;";
            const auto pos=code.find(from);if(pos!=std::string::npos)code.replace(pos,from.size(),"{ status=ASN1_ERR_OUT_OF_MEMORY; goto per_fail; }");
            s << code << member << "=def; } }\n";
          }
          if(f.presence!=Presence::Mandatory) ++i;
        }};
        emit_root_decode(sequence.root);
        emit_root_decode(sequence.trailing_root);
        if(sequence.extensible) {
          s << ind << "  if(pre.extensions_present) { asn1_per_extension_additions_t ext={0}; status=asn1_per_decode_extension_additions(r," << v << ",&ext); if(status!=ASN1_OK) goto per_fail;\n";
          for(std::size_t i=0;i<groups.size();++i) {
            const auto& group=*groups[i];
            s << ind << "    if(ext.count>" << i << " && ext.presence[" << i << "]) { asn1_bit_reader_t* er=NULL; status=asn1_bit_reader_reserve(r,ext.open_type_sizes[" << i << "],0); if(status==ASN1_OK) status=asn1_bit_reader_create(&er,asn1_byte_view(ext.open_type_data[" << i << "],ext.open_type_sizes[" << i << "])); if(status==ASN1_OK) status=asn1_bit_reader_share_budget(er,r); if(status!=ASN1_OK) { asn1_bit_reader_destroy(er); asn1_per_extension_additions_free(&ext); goto per_fail; }\n";
            if(group.version_bracket) {
              std::vector<const Field*> group_presence;
              for(const auto& f:group.fields) if(f.presence!=Presence::Mandatory) group_presence.push_back(&f);
              s << ind << "      asn1_per_sequence_preamble_t gp={0}; status=asn1_per_decode_sequence_preamble(er,0," << group_presence.size() << ",&gp); if(status!=ASN1_OK) { asn1_bit_reader_destroy(er); asn1_per_extension_additions_free(&ext); goto per_fail; }\n";
            }
            std::size_t opt_i=0;
            for(const auto& f:group.fields) {
              const auto member=value+".f"+spell(f.name);
              const bool optional=f.presence!=Presence::Mandatory;
              if(group.version_bracket&&optional) s << ind << "      " << value << ".has_f" << spell(f.name) << "=gp.optional_bits[" << opt_i << "]; if(gp.optional_bits[" << opt_i << "]) {\n";
              else s << ind << "      " << value << ".has_f" << spell(f.name) << "=1;\n";
              if(pointer(id,f.type)) s << ind << (group.version_bracket&&optional?"        ":"      ") << "status=asn1_bit_reader_reserve(er,sizeof(*" << member << "),1); if(status==ASN1_OK) { " << member << "=(" << name(f.type) << "*)calloc(1,sizeof(*" << member << ")); if(!" << member << ") status=ASN1_ERR_OUT_OF_MEMORY; else { " << name(f.type) << "_init(" << member << "); status=" << name(f.type) << "_decode_" << variant << "_at(er," << member << ",depth+1); } }\n";
              else s << ind << (group.version_bracket&&optional?"        ":"      ") << "status=" << name(f.type) << "_decode_" << variant << "_at(er,&" << member << ",depth+1);\n";
              s << ind << (group.version_bracket&&optional?"        ":"      ") << "if(status!=ASN1_OK) { ";
              if(group.version_bracket) s << "asn1_per_sequence_preamble_free(&gp); ";
              s << "asn1_bit_reader_destroy(er); asn1_per_extension_additions_free(&ext); goto per_fail; }\n";
              if(group.version_bracket&&optional) s << ind << "      }\n";
              if(optional) ++opt_i;
            }
            if(group.version_bracket) s << ind << "      asn1_per_sequence_preamble_free(&gp);\n";
            s << ind << "      while(asn1_bit_reader_bit_remaining(er)>0) { uint64_t pad=0; size_t rem=asn1_bit_reader_bit_remaining(er); if(rem>7u) { status=ASN1_ERR_INVALID_ARGUMENT; break; } status=asn1_bit_reader_get_bits(er,rem,&pad); if(status!=ASN1_OK||pad) { if(status==ASN1_OK) status=ASN1_ERR_NON_CANONICAL; break; } } asn1_bit_reader_destroy(er); if(status!=ASN1_OK) { asn1_per_extension_additions_free(&ext); goto per_fail; } }\n";
          }
          s << ind << "    asn1_per_extension_additions_free(&ext); }\n";
        }
        for(const auto* fields:{&sequence.root,&sequence.trailing_root})
          for(const auto& field:*fields) if(typed_open_field(field))
            for(const auto* selected_variant:{"uper","aper"}) if(std::string(selected_variant)==variant) {
              s << ind << "  { uint32_t selected_type=0; void* selected_value=NULL; status=" << name(id) << "_decode_selected_" << selector_function_suffix(field) << "_" << variant << "_impl(value,&selected_type,&selected_value,r); if(status==ASN1_OK) { value->" << decoded_type_member(field) << "=selected_type; value->" << decoded_value_member(field) << "=selected_value; } else if(status==ASN1_ERR_CONSTRAINT_VIOLATION) status=ASN1_OK; else goto per_fail; }\n";
            }
        s << ind << "  goto per_done; per_fail: asn1_per_sequence_preamble_free(&pre); return status; per_done: asn1_per_sequence_preamble_free(&pre); }\n";
        break;
      }
      case TypeKind::SequenceOf: case TypeKind::SetOf: {
        const auto dep=t.kind==TypeKind::SequenceOf?t.sequence_of.element:t.set_of.element;
        const auto& constraint=t.kind==TypeKind::SequenceOf?t.sequence_of.size:t.set_of.size;
        s << ind << "{ const asn1_per_length_constraint_t size=" << per_size_range(constraint) << "; size_t count=0,chunk=0,total=0; int more=0,first=1,is_extension=0; do { status=asn1_per_decode_collection_fragment_ex(r," << v << ",&size,first,&is_extension,count,&chunk,&more); if(status!=ASN1_OK) return status; if(chunk>ASN1_MAX_PDU_ELEMENTS-count || chunk>SIZE_MAX-count) return ASN1_ERR_RESOURCE_LIMIT; total=count+chunk; if(total>SIZE_MAX/sizeof(*" << value << ".data)) return ASN1_ERR_LENGTH_OVERFLOW; if(total>count) { status=asn1_bit_reader_reserve(r,total*sizeof(*" << value << ".data),chunk); if(status!=ASN1_OK) return status; " << name(dep) << "* grown=(" << name(dep) << "*)realloc(" << value << ".data,total*sizeof(*" << value << ".data)); if(!grown) { for(size_t j=0;j<count;++j) " << name(dep) << "_free(&" << value << ".data[j]); free(" << value << ".data); " << value << ".data=NULL; " << value << ".count=" << value << ".capacity=0; return ASN1_ERR_OUT_OF_MEMORY; } " << value << ".data=grown; memset(" << value << ".data+count,0,chunk*sizeof(*" << value << ".data)); " << value << ".count=" << value << ".capacity=total; } for(size_t i=count;i<total;++i) { status=" << name(dep) << "_decode_" << variant << "_at(r,&" << value << ".data[i],depth+1); if(status!=ASN1_OK) { for(size_t j=0;j<total;++j) " << name(dep) << "_free(&" << value << ".data[j]); free(" << value << ".data); " << value << ".data=NULL; " << value << ".count=" << value << ".capacity=0; return status; } } count=total; first=0; } while(more); }\n";
        break;
      }
      case TypeKind::Choice: {
        const auto root_count=t.choice.alternatives.size();
        s << ind << "{ int is_extension=0; uint64_t ordinal=0; status=asn1_per_decode_enumerated_ex(r," << v << "," << root_count << "," << t.choice.extensible << ",&is_extension,&ordinal); if(status!=ASN1_OK) " << fail << " if(is_extension) { uint8_t* raw=NULL; size_t raw_size=0; asn1_bit_reader_t* cr=NULL; status=asn1_per_decode_open_type(r," << v << ",&raw,&raw_size); if(status==ASN1_OK) status=asn1_bit_reader_create(&cr,asn1_byte_view(raw,raw_size)); if(status==ASN1_OK) status=asn1_bit_reader_share_budget(cr,r); if(status==ASN1_OK) switch(ordinal) {\n";
        for(std::size_t i=0;i<t.choice.extensions.size();++i) {
          const auto& field=t.choice.extensions[i];
          s << ind << "case UINT64_C(" << i << "): " << value << ".kind=" << name(id) << "_arm_f" << spell(field.name) << "; status=" << name(field.type) << "_decode_" << variant << "_at(cr,&" << value << ".value.f" << spell(field.name) << ",depth+1); break;\n";
        }
        s << ind << "default: status=ASN1_ERR_UNSUPPORTED; break; } if(status==ASN1_OK) { size_t rem=asn1_bit_reader_bit_remaining(cr); uint64_t pad=0; if(rem>7u) status=ASN1_ERR_INVALID_ARGUMENT; else if(rem) { status=asn1_bit_reader_get_bits(cr,rem,&pad); if(status==ASN1_OK && pad) status=ASN1_ERR_NON_CANONICAL; } } asn1_bit_reader_destroy(cr); free(raw); if(status!=ASN1_OK) " << fail << " } else switch(ordinal) {\n";
        for(std::size_t i=0;i<root_count;++i) {
          const auto& field=t.choice.alternatives[i];
          s << ind << "case UINT64_C(" << i << "): " << value << ".kind=" << name(id) << "_arm_f" << spell(field.name) << "; status=" << name(field.type) << "_decode_" << variant << "_at(r,&" << value << ".value.f" << spell(field.name) << ",depth+1); if(status!=ASN1_OK) " << fail << " break;\n";
        }
        s << ind << "default: status=ASN1_ERR_CONSTRAINT_VIOLATION; " << fail << " } }\n";
        break;
      }
      default: s << ind << "return ASN1_ERR_UNSUPPORTED;\n"; break;
    }
  }
  void emit_per_codec(TypeId id, const char* variant, std::ostream& s) const {
    const auto n=name(id);
    s << linkage(true) << "asn1_status_t " << n << "_encode_" << variant << "_at(asn1_bit_writer_t *w,const " << n << " *value,size_t depth) { asn1_status_t status=ASN1_OK; if(!w||!value) return ASN1_ERR_INVALID_ARGUMENT; if(depth>=ASN1_MAX_RECURSION_DEPTH) return ASN1_ERR_RESOURCE_LIMIT;\n";
    emit_per_encode_value(id,"(*value)",variant,s,"  ");
    s << "  return ASN1_OK; }\n" << linkage() << "asn1_status_t " << n << "_encode_" << variant << "(asn1_bit_writer_t *w,const " << n << " *value) { return " << n << "_encode_" << variant << "_at(w,value,0); }\n";
    s << linkage(true) << "asn1_status_t " << n << "_decode_" << variant << "_at(asn1_bit_reader_t *r," << n << " *value,size_t depth) { asn1_status_t status=ASN1_OK; if(!r||!value) return ASN1_ERR_INVALID_ARGUMENT; if(depth>=ASN1_MAX_RECURSION_DEPTH) return ASN1_ERR_RESOURCE_LIMIT;\n";
    emit_per_decode_value(id,"(*value)",variant,s,"  ","return status;");
    s << "  return ASN1_OK; }\n" << linkage() << "asn1_status_t " << n << "_decode_" << variant << "(asn1_bit_reader_t *r," << n << " *out) { if(!r||!out) return ASN1_ERR_INVALID_ARGUMENT; asn1_status_t status=asn1_bit_reader_begin_decode(r); if(status!=ASN1_OK) return status; " << n << " tmp; " << n << "_init(&tmp); status=" << n << "_decode_" << variant << "_at(r,&tmp,0); if(status!=ASN1_OK) { " << n << "_free(&tmp); return status; } " << n << "_free(out); *out=tmp; return ASN1_OK; }\n";
    if(model.arena.get(id).kind==TypeKind::Sequence || model.arena.get(id).kind==TypeKind::Set) {
      const auto& seq=per_sequence(model.arena.get(id));
      for(const auto* fields:{&seq.root,&seq.trailing_root})
        for(const auto& field:*fields) if(field.table_constraint) emit_selector_decoder(id,field,variant,s);
    }
  }
  bool c_json_nullable(TypeId id,std::size_t depth=0,bool unwrapped=false) const {
    if(depth>=256)return true;const auto& t=model.arena.get(id);unwrapped=unwrapped||t.jer.unwrapped;
    if(t.kind==TypeKind::Referenced)return c_json_nullable(t.referenced.resolved,depth+1,unwrapped);
    if(t.kind==TypeKind::Null||(t.kind==TypeKind::ObjectClassField&&t.object_class_field.open_type))return true;
    if(t.kind==TypeKind::Choice&&unwrapped){if(t.choice.extensible)return true;for(const auto& field:members(t))if(c_json_nullable(field.field->type,depth+1))return true;}
    return false;
  }
  const Type& c_resolved(TypeId id) const {
    for(std::size_t depth=0;depth<256;++depth){const auto& t=model.arena.get(id);if(t.kind!=TypeKind::Referenced)return t;id=t.referenced.resolved;}
    return model.arena.get(id);
  }
  bool c_tlv_supported(TypeId id,std::set<TypeId>& seen) const {
    if(!seen.insert(id).second)return true;
    const auto& t=model.arena.get(id);bool ok=true;
    if(c_extra(CodecKind::Exer)&&t.exer.untagged){diag.error({},"Unsupported C E-XER UNTAGGED instruction: "+name(id));ok=false;}
    const bool json=c_extra(CodecKind::Jer)||c_extra(CodecKind::Jeri);
    const bool xml=c_extra(CodecKind::Xer)||c_extra(CodecKind::Cxer)||c_extra(CodecKind::Exer);
    if(json&&((t.jer.base64&&c_resolved(id).kind!=TypeKind::OctetString)||(t.jer.unwrapped&&c_resolved(id).kind!=TypeKind::Choice))){diag.error({},"Unsupported C JER instruction/type combination: "+name(id));ok=false;}
    if(json&&t.jer.text_form!=JerEncoding::TextForm::AsIs&&c_resolved(id).kind!=TypeKind::Enumerated){diag.error({},"Unsupported C JER TEXT instruction scope: "+name(id));ok=false;}
    if(json&&c_resolved(id).kind==TypeKind::Enumerated&&(t.kind==TypeKind::Enumerated||t.jer.text_form!=JerEncoding::TextForm::AsIs)){
      const auto& enumeration=c_resolved(id).enumerated;std::set<std::string> names;
      for(const auto* entries:{&enumeration.root,&enumeration.extensions})for(const auto& entry:*entries)
        if(!names.insert(instruction_name(entry.name,static_cast<unsigned>(t.jer.text_form),t.jer.text_literal)).second){diag.error({},"C JER TEXT produces duplicate enumeration names: "+name(id));ok=false;}
    }
    if(json&&t.jer.array){const auto& shape=c_resolved(id);if(shape.kind!=TypeKind::Sequence){diag.error({},"Unsupported C JER ARRAY requires SEQUENCE: "+name(id));ok=false;}
      else for(const auto& field:members(shape))if((field.field->presence!=Presence::Mandatory||field.extension)&&c_json_nullable(field.field->type)){diag.error({},"Unsupported C JER ARRAY nullable optional/extension component: "+field.field->name);ok=false;}}
    if(json&&t.jer.object){const auto& shape=c_resolved(id);bool valid=shape.kind==TypeKind::SetOf;
      if(valid){const auto& entry=c_resolved(shape.set_of.element);const auto fields=members(entry);valid=entry.kind==TypeKind::Sequence&&fields.size()==2;
        if(valid){for(const auto& field:fields)if(field.field->presence!=Presence::Mandatory||field.extension)valid=false;const auto& key=c_resolved(fields[0].field->type);valid=valid&&(key.kind==TypeKind::String||key.kind==TypeKind::Enumerated);}}
      if(!valid){diag.error({},"Unsupported C JER OBJECT requires SET OF two mandatory components with string/enumeration key: "+name(id));ok=false;}}
    if((json||xml)&&t.kind==TypeKind::ObjectClassField&&t.object_class_field.open_type){diag.error({},"Unsupported C text codec typed open-type schema: "+name(id));ok=false;}
    switch(t.kind) {
      case TypeKind::Boolean:case TypeKind::Integer:case TypeKind::Enumerated:
      case TypeKind::Null:break;
      case TypeKind::BitString:ok=t.bit_string.containing==kInvalidType && t.bit_string.named_bits.empty();break;
      case TypeKind::OctetString:ok=t.octet_string.containing==kInvalidType;break;
      case TypeKind::Real:case TypeKind::ObjectIdentifier:case TypeKind::RelativeOid:break;
      case TypeKind::String:
        ok=t.string.kind==StringKind::UTF8String||t.string.kind==StringKind::IA5String||t.string.kind==StringKind::NumericString||t.string.kind==StringKind::PrintableString||t.string.kind==StringKind::VisibleString;break;
      case TypeKind::Referenced:ok=c_tlv_supported(t.referenced.resolved,seen);break;
      case TypeKind::ObjectClassField:if(!t.object_class_field.open_type)ok=c_tlv_supported(t.object_class_field.fixed_type,seen);break;
      case TypeKind::SequenceOf:case TypeKind::SetOf:ok=c_tlv_supported(t.kind==TypeKind::SequenceOf?t.sequence_of.element:t.set_of.element,seen);break;
      case TypeKind::Sequence:case TypeKind::Set:case TypeKind::Choice:
        if(json||c_extra(CodecKind::Exer)){std::set<std::string> names;for(const auto& member:members(t))if(!names.insert(instruction_name(member.field->name,static_cast<unsigned>(member.field->jer_name_form),member.field->jer_name_literal)).second){diag.error({},"C NAME produces duplicate component names: "+name(id));ok=false;}}
        for(const auto& m:members(t)) {
          if(c_extra(CodecKind::Exer)&&m.field->exer.untagged){diag.error({},"Unsupported C E-XER UNTAGGED field: "+m.field->name);ok=false;}
          if(c_extra(CodecKind::Exer)){
            const auto xml_name=instruction_name(m.field->name,static_cast<unsigned>(m.field->jer_name_form),m.field->jer_name_literal);
            const auto start=[](unsigned char c){return (c>='A'&&c<='Z')||(c>='a'&&c<='z')||c=='_';};
            bool valid=!xml_name.empty()&&start(static_cast<unsigned char>(xml_name.front()));
            for(unsigned char c:xml_name)valid=valid&&(start(c)||(c>='0'&&c<='9')||c=='-'||c=='.');
            if(!valid){diag.error({},"Unsupported C E-XER NAME outside ASCII XML-name profile: "+m.field->name);ok=false;}
          }
          if(m.field->presence==Presence::Default && per_default_test(*m.field,"value").empty())ok=false;
          if(!c_tlv_supported(m.field->type,seen))ok=false;
        }break;
      default:ok=false;break;
    }
    if(!ok)diag.error({},"Unsupported C TLV schema/value profile: "+name(id));
    return ok;
  }
  std::string tlv_tag(const Tag& tag,bool present=true) const {
    return "{"+std::to_string(present?static_cast<unsigned>(tag.cls):255u)+",0,UINT64_C("+std::to_string(tag.number)+")}";
  }
  void emit_tlv_validation(TypeId id,std::ostream& s) const {
    const auto& t=model.arena.get(id);const auto n=name(id);
    s << linkage(true) << "asn1_status_t " << n << "_tlv_validate(const void* opaque) { const " << n << "* value=(const " << n << "*)opaque; (void)value;\n";
    const auto membership=[&](const ConstraintDesc& c,const std::string& expression){
      if(c.extensible || (!c.empty && c.root_ranges.empty()))return;
      s << "if(!(0";
      for(const auto& r:c.root_ranges){s << " || (1";if(r.lower && r.lower->as_i64)s << " && " << expression << ">=" << literal(*r.lower->as_i64);if(r.upper && r.upper->as_i64)s << " && " << expression << "<=" << literal(*r.upper->as_i64);s << ')';}
      s << ")) return ASN1_ERR_CONSTRAINT_VIOLATION;\n";
    };
    if(t.kind==TypeKind::Boolean)s << "if(value->value!=0 && value->value!=1)return ASN1_ERR_INVALID_ARGUMENT;\n";
    if(t.kind==TypeKind::Integer && !big_integer(t))membership(t.integer.constraint,"value->value");
    if(big_integer(t)){
      s << "if(value->value.nlimbs>value->value.cap || (value->value.nlimbs && (!value->value.limbs || !value->value.limbs[value->value.nlimbs-1] || (value->value.sign!=1 && value->value.sign!=-1))) || (!value->value.nlimbs && value->value.sign))return ASN1_ERR_INVALID_ARGUMENT; if(value->value.nlimbs>ASN1_MAX_BIGINT_LIMBS)return ASN1_ERR_RESOURCE_LIMIT;\n";
      if(!t.integer.constraint.extensible && (t.integer.constraint.empty || !t.integer.constraint.root_ranges.empty())) {
        s << "{ int member=0;\n";std::size_t i=0;
        for(const auto& range:t.integer.constraint.root_ranges){const auto stem="bound"+std::to_string(i++);if(range.lower)big_constant(*range.lower,stem+"lo",s);if(range.upper)big_constant(*range.upper,stem+"hi",s);
          s << "if(1";if(range.lower)s << " && asn1_bigint_cmp(&value->value,&" << stem << "lo)>=0";if(range.upper)s << " && asn1_bigint_cmp(&value->value,&" << stem << "hi)<=0";s << ")member=1;\n";}
        s << "if(!member)return ASN1_ERR_CONSTRAINT_VIOLATION;}\n";
      }
    }
    if(t.kind==TypeKind::Enumerated && !t.enumerated.extensible){s << "if(!(0";for(const auto& e:t.enumerated.root)s << " || value->value==" << literal(*e.value.as_i64);s << "))return ASN1_ERR_CONSTRAINT_VIOLATION;\n";}
    if(t.kind==TypeKind::BitString){s << "{size_t bytes=value->value.bit_length/8u+(value->value.bit_length%8u!=0); if(bytes>value->value.cap_bytes || (bytes&&!value->value.bits))return ASN1_ERR_INVALID_ARGUMENT;if(bytes>ASN1_MAX_STRING_BYTES)return ASN1_ERR_RESOURCE_LIMIT;}\n";membership(t.bit_string.size,"value->value.bit_length");}
    if(octets(t)||t.kind==TypeKind::SequenceOf||t.kind==TypeKind::SetOf||t.kind==TypeKind::ObjectIdentifier||t.kind==TypeKind::RelativeOid){s << "if(value->count>value->capacity || (value->count&&!value->data))return ASN1_ERR_INVALID_ARGUMENT;\n";
      if(octets(t))s << "if(value->count>ASN1_MAX_STRING_BYTES)return ASN1_ERR_RESOURCE_LIMIT;\n";
      if(t.kind!=TypeKind::String)if(const auto* constraint=planning::storage_size(t))membership(*constraint,"value->count");
    }
    if(t.kind==TypeKind::String){const unsigned tag=t.string.kind==StringKind::NumericString?18:t.string.kind==StringKind::PrintableString?19:t.string.kind==StringKind::IA5String?22:t.string.kind==StringKind::VisibleString?26:12;
      s << "{size_t characters=0;asn1_status_t status=asn1_tlv_string_length(" << tag << ",asn1_byte_view(value->data,value->count),&characters);if(status!=ASN1_OK)return status;\n";membership(t.string.size,"characters");s << "}\n";}
    if(t.kind==TypeKind::Sequence || t.kind==TypeKind::Set){
      for(const auto& member:members(t))if(typed_open_field(*member.field))s << "if(value->" << decoded_value_member(*member.field) << ")return ASN1_ERR_UNSUPPORTED;\n";
      const auto plan=planning::sequence(per_sequence(t));
      for(const auto& group:plan.extensions)if(group.nested_sequence){s << "if(0";for(const auto& f:group.group->fields)s << " || value->has_f" << spell(f.name);s << ") {\n";for(const auto& f:group.group->fields)if(f.presence==Presence::Mandatory)s << "if(!value->has_f" << spell(f.name) << ")return ASN1_ERR_CONSTRAINT_VIOLATION;\n";s << "}\n";}
    }
    s << "return ASN1_OK;}\n";
  }
  std::vector<planning::EmissionFieldPlan> layout_fields(const Type& t) const {
    auto fields=planning::fields(t);
    if(t.kind==TypeKind::Sequence){
      // BER declaration order: first root, extension additions, trailing root.
      fields.clear();const auto planned=planning::fields(t);
      for(const auto* list:{&t.sequence.root})for(const auto& f:*list)for(const auto& p:planned)if(p.field==&f)fields.push_back(p);
      for(const auto& group:t.sequence.extension_groups)for(const auto& f:group.fields)for(const auto& p:planned)if(p.field==&f)fields.push_back(p);
      for(const auto& f:t.sequence.trailing_root)for(const auto& p:planned)if(p.field==&f)fields.push_back(p);
    }
    return fields;
  }
  void emit_tlv_schema(TypeId id,std::ostream& s) const {
    const auto& t=model.arena.get(id);const auto n=name(id);std::string kind="",element="NULL";
    unsigned universal=0;std::string value_offset="0",data_offset="0",count_offset="0",capacity_offset="0";
    const auto offset=[&](const std::string& path){return "offsetof("+n+","+path+")";};
    TypeId dep=kInvalidType;
    switch(t.kind) {
      case TypeKind::Referenced:kind="ALIAS";dep=t.referenced.resolved;value_offset=offset("value");break;
      case TypeKind::ObjectClassField:if(!t.object_class_field.open_type){kind="ALIAS";dep=t.object_class_field.fixed_type;value_offset=offset("value");}else kind="OPEN";break;
      case TypeKind::Boolean:kind="BOOLEAN";universal=1;value_offset=offset("value");break;
      case TypeKind::Integer:kind=big_integer(t)?"BIGINT":"INT64";universal=2;value_offset=offset("value");break;
      case TypeKind::Enumerated:kind="INT64";universal=10;value_offset=offset("value");break;
      case TypeKind::Null:kind="NULL";universal=5;break;
      case TypeKind::Real:kind="REAL";universal=9;value_offset=offset("value");break;
      case TypeKind::ObjectIdentifier:kind="OID";universal=6;break;
      case TypeKind::RelativeOid:kind="RELATIVE_OID";universal=13;break;
      case TypeKind::BitString:kind="BITS";universal=3;value_offset=offset("value");break;
      case TypeKind::OctetString:kind="OCTETS";universal=4;break;
      case TypeKind::String:kind="OCTETS";
        universal=t.string.kind==StringKind::NumericString?18:t.string.kind==StringKind::PrintableString?19:t.string.kind==StringKind::IA5String?22:t.string.kind==StringKind::VisibleString?26:12;break;
      case TypeKind::Sequence:kind="SEQUENCE";universal=16;break;
      case TypeKind::Set:kind="SET";universal=17;break;
      case TypeKind::Choice:kind="CHOICE";break;
      case TypeKind::SequenceOf:kind="SEQUENCE_OF";universal=16;dep=t.sequence_of.element;break;
      case TypeKind::SetOf:kind="SET_OF";universal=17;dep=t.set_of.element;break;
      default:return;
    }
    const bool collection=t.kind==TypeKind::SequenceOf||t.kind==TypeKind::SetOf;
    if(octets(t)||collection||t.kind==TypeKind::ObjectIdentifier||t.kind==TypeKind::RelativeOid){data_offset=offset("data");count_offset=offset("count");capacity_offset=offset("capacity");}
    if(dep!=kInvalidType)element=name(dep)+"_tlv_schema";
    const bool alias=kind=="ALIAS";
    const auto fields=layout_fields(t);
    s << linkage(true) << "void " << n << "_tlv_init(void* v) { " << n << "_init((" << n << "*)v); }\n"
      << linkage(true) << "void " << n << "_tlv_clear(void* v) { " << n << "_free((" << n << "*)v); }\n";
    emit_tlv_validation(id,s);
    if(t.kind==TypeKind::Choice){
      s << linkage(true) << "size_t " << n << "_tlv_choice_get(const void* value) { switch(((const " << n << "*)value)->kind) {\n";
      for(std::size_t i=0;i<fields.size();++i)s << "case " << n << "_arm_f" << spell(fields[i].field->name) << ":return " << i << ";\n";
      s << "default:return SIZE_MAX;} }\n" << linkage(true) << "void " << n << "_tlv_choice_set(void* value,size_t index) { switch(index) {\n";
      for(std::size_t i=0;i<fields.size();++i)s << "case " << i << ": ( (" << n << "*)value)->kind=" << n << "_arm_f" << spell(fields[i].field->name) << ";break;\n";
      s << "default:break;} }\n";
    }
    for(std::size_t i=0;i<fields.size();++i)if(fields[i].field->presence==Presence::Default) {
      const auto& f=*fields[i].field;
      s << linkage(true) << "int " << n << "_tlv_default_" << i << "(const void* value) { return " << per_default_test(f,"((const "+n+"*)value)->f"+spell(f.name)) << "; }\n";
      if(owning_default(f)){s << linkage(true) << "asn1_status_t " << n << "_tlv_default_init_" << i << "(void* value) {\n";emit_owned_default(f,"(("+n+"*)value)->f"+spell(f.name),s);s << "return ASN1_OK;}\n";}
    }
    s << linkage(true) << "const asn1_tlv_schema_t* " << n << "_tlv_schema(void) {\n";
    if(!fields.empty()) {
      s << "static const asn1_tlv_field_t fields[] = {\n";
      for(std::size_t i=0;i<fields.size();++i){const auto& plan=fields[i];const auto& f=*plan.field;const auto& ft=model.arena.get(f.type);
        const bool tagged=f.tag.cls!=TagClass::Universal||f.tag.number!=0;
        const bool duplicate=f.tag.is_explicit && ft.tag.is_explicit && f.tag.cls==ft.tag.cls && f.tag.number==ft.tag.number;
        const bool presence=t.kind!=TypeKind::Choice && (plan.extension_group || f.presence==Presence::Optional);
        const auto path=(t.kind==TypeKind::Choice?"value.f":"f")+spell(f.name);
        const bool owned=f.presence==Presence::Default && owning_default(f);
        const bool after_root=static_cast<bool>(plan.extension_group) || (t.kind==TypeKind::Sequence && std::any_of(t.sequence.trailing_root.begin(),t.sequence.trailing_root.end(),[&](const Field& field){return &field==&f;}));
        s << "{" << name(f.type) << "_tlv_schema," << offset(path) << ',' << (presence?offset("has_f"+spell(f.name)):"SIZE_MAX") << ',' << pointer(id,f.type) << ',' << f.tag.is_explicit << ',' << tlv_tag(f.tag,tagged&&!duplicate) << ',' << (f.presence==Presence::Default?n+"_tlv_default_"+std::to_string(i):"NULL") << ',' << (owned?n+"_tlv_default_init_"+std::to_string(i):"NULL") << ',' << (owned?owning_default_bytes(f):0) << ',' << after_root << "},\n";
      }s << "};\n";
    }
    bool tagged=t.tag.cls!=TagClass::Universal||t.tag.number!=0;
    if(alias && dep!=kInvalidType){const auto& child=model.arena.get(dep);if(t.tag.is_explicit && child.tag.is_explicit && t.tag.cls==child.tag.cls && t.tag.number==child.tag.number)tagged=false;}
    const bool extensible=t.kind==TypeKind::Sequence?t.sequence.extensible:t.kind==TypeKind::Set?t.set.extensible:t.kind==TypeKind::Choice?t.choice.extensible:false;
    s << "static const asn1_tlv_schema_t schema={ASN1_TLV_" << kind << ",sizeof(" << n << ")," << universal << ',' << tlv_tag(t.tag,tagged) << ',' << t.tag.is_explicit << ',' << extensible << ',' << (alias&&pointer(id,dep)) << ','
      << value_offset << ',' << data_offset << ',' << count_offset << ',' << capacity_offset << ',' << element << ',' << (fields.empty()?"NULL":"fields") << ',' << fields.size() << ',' << n << "_tlv_init," << n << "_tlv_clear," << n << "_tlv_validate,"
      << (t.kind==TypeKind::Choice?n+"_tlv_choice_get":"NULL") << ',' << (t.kind==TypeKind::Choice?n+"_tlv_choice_set":"NULL") << "};return &schema;}\n";
    for(const auto* codec:{"ber","der","cer"})if((std::string(codec)=="ber"&&c_ber)||(std::string(codec)=="der"&&c_der)||(std::string(codec)=="cer"&&c_cer)) {
      const auto policy=std::string("ASN1_TLV_")+(std::string(codec)=="ber"?"BER":std::string(codec)=="der"?"DER":"CER");
      s << linkage() << "asn1_status_t " << n << "_encode_" << codec << "(asn1_byte_writer_t* w,const " << n << "* value) { return asn1_tlv_schema_encode(w," << policy << ',' << n << "_tlv_schema(),value); }\n"
        << linkage() << "asn1_status_t " << n << "_decode_" << codec << "(asn1_byte_reader_t* r," << n << "* value) { return asn1_tlv_schema_decode(r," << policy << ',' << n << "_tlv_schema(),value); }\n";
    }
  }
  std::string instruction_name(std::string text,unsigned form,const std::string& value) const {
    if(form==4)return value;
    for(std::size_t i=0;i<text.size();++i){char& c=text[i];
      if((form==2||(form==1&&i==0))&&c>='a'&&c<='z')c=static_cast<char>(c-'a'+'A');
      if(form==3&&c>='A'&&c<='Z')c=static_cast<char>(c-'A'+'a');
    }return text;
  }
  unsigned xml_flags(const ExerEncoding& e) const {
    return (e.attribute?256u:0u)|(e.base64?512u:0u)|(e.text?1024u:0u)|(e.use_number?2048u:0u)|
      (e.list?4096u:0u)|(e.untagged?8192u:0u)|(e.use_nil?16384u:0u);
  }
  void emit_codec_schema(TypeId id,std::ostream& s) const {
    const auto& t=model.arena.get(id);const auto n=name(id);const auto fields=layout_fields(t);
    TypeId element=kInvalidType;
    if(t.kind==TypeKind::Referenced)element=t.referenced.resolved;
    else if(t.kind==TypeKind::ObjectClassField&&!t.object_class_field.open_type)element=t.object_class_field.fixed_type;
    else if(t.kind==TypeKind::SequenceOf)element=t.sequence_of.element;
    else if(t.kind==TypeKind::SetOf)element=t.set_of.element;
    unsigned width=0;bool unsigned_form=false;
    if(t.kind==TypeKind::Integer&&!t.integer.constraint.extensible){const auto& c=t.integer.constraint;
      unsigned_form=c.lower&&!c.lower->negative;
      if(c.lower&&c.upper){
        if(unsigned_form){const auto& digits=c.upper->digits;
          if(digits.size()<20||(digits.size()==20&&digits<="18446744073709551615")){
            if(c.upper->as_i64&&*c.upper->as_i64<=255)width=1;
            else if(c.upper->as_i64&&*c.upper->as_i64<=65535)width=2;
            else if(c.upper->as_i64&&*c.upper->as_i64<=4294967295LL)width=4;
            else width=8;
          }
        }else if(c.lower->as_i64&&c.upper->as_i64){const auto lo=*c.lower->as_i64,hi=*c.upper->as_i64;
          width=lo>=-128&&hi<=127?1:lo>=-32768&&hi<=32767?2:lo>=(-2147483647LL-1)&&hi<=2147483647LL?4:8;
        }
      }
    }
    std::string fixed="SIZE_MAX";const auto* size=t.kind==TypeKind::String?&t.string.size:planning::storage_size(t);
    if(size&&!size->extensible&&size->lower&&size->upper&&size->lower->as_i64&&size->upper->as_i64&&*size->lower==*size->upper)
      fixed="UINT64_C("+std::to_string(*size->lower->as_i64)+")";
    unsigned flags=xml_flags(t.exer)|(t.jer.array?1u:0u)|(t.jer.base64?2u:0u)|(t.jer.object?4u:0u)|(t.jer.unwrapped?8u:0u);
    s << linkage(true) << "const asn1_codec_schema_t* " << n << "_codec_schema(void) {\n";
    if(!fields.empty()){
      s << "static const asn1_codec_field_t fields[]={\n";
      for(const auto& plan:fields){const auto& f=*plan.field;const bool tag=f.tag.cls!=TagClass::Universal||f.tag.number!=0;
        s << '{' << name(f.type) << "_codec_schema," << c_string(f.name) << ','
          << c_string(instruction_name(f.name,static_cast<unsigned>(f.jer_name_form),f.jer_name_literal)) << ','
          << (plan.extension_group?std::to_string(*plan.extension_group):"SIZE_MAX") << ',' << plan.nested_sequence << ','
          << (f.presence==Presence::Optional) << ',' << xml_flags(f.exer) << ',' << tlv_tag(f.tag,tag) << "},\n";
      }s << "};\n";
    }
    std::size_t enum_count=0;
    const auto& enum_type=c_resolved(id);
    if(enum_type.kind==TypeKind::Enumerated&&(t.kind==TypeKind::Enumerated||t.jer.text_form!=JerEncoding::TextForm::AsIs)){enum_count=enum_type.enumerated.root.size()+enum_type.enumerated.extensions.size();
      if(enum_count){s << "static const asn1_codec_enum_t enumerators[]={\n";
        for(const auto* list:{&enum_type.enumerated.root,&enum_type.enumerated.extensions})for(const auto& e:*list)
          s << '{' << literal(*e.value.as_i64) << ',' << c_string(instruction_name(e.name,static_cast<unsigned>(t.jer.text_form),t.jer.text_literal)) << ',' << c_string(e.name) << "},\n";
        s << "};\n";
      }
    }
    std::size_t root_count=0,group_count=0;
    if(t.kind==TypeKind::Sequence||t.kind==TypeKind::Set){const auto& seq=per_sequence(t);const auto roots=planning::oer_root_fields(model,seq,t.kind==TypeKind::Set);root_count=roots.size();group_count=seq.extension_groups.size();
      if(root_count){s << "static const size_t root_order[]={";for(const auto* f:roots){for(std::size_t i=0;i<fields.size();++i)if(fields[i].field==f)s << i << ',';}s << "};\n";}
    }
    const auto& shape=c_resolved(id);
    const char* builtin=shape.kind==TypeKind::Boolean?"BOOLEAN":shape.kind==TypeKind::Integer?"INTEGER":shape.kind==TypeKind::Null?"NULL":shape.kind==TypeKind::Real?"REAL":shape.kind==TypeKind::Enumerated?"ENUMERATED":shape.kind==TypeKind::BitString?"BIT_STRING":shape.kind==TypeKind::OctetString?"OCTET_STRING":shape.kind==TypeKind::ObjectIdentifier?"OBJECT_IDENTIFIER":shape.kind==TypeKind::RelativeOid?"RELATIVE_OID":shape.kind==TypeKind::Sequence?"SEQUENCE":shape.kind==TypeKind::Set?"SET":shape.kind==TypeKind::Choice?"CHOICE":shape.kind==TypeKind::SequenceOf?"SEQUENCE_OF":shape.kind==TypeKind::SetOf?"SET_OF":"Type";
    if(shape.kind==TypeKind::String)builtin=shape.string.kind==StringKind::IA5String?"IA5String":shape.string.kind==StringKind::NumericString?"NumericString":shape.string.kind==StringKind::PrintableString?"PrintableString":shape.string.kind==StringKind::VisibleString?"VisibleString":"UTF8String";
    const auto xml_name=!t.name.empty()?t.name:t.kind==TypeKind::Referenced&&!t.referenced.name.empty()?t.referenced.name:std::string(builtin);
    s << "static const asn1_codec_schema_t schema={" << n << "_tlv_schema," << c_string(xml_name) << ','
      << (element==kInvalidType?"NULL":name(element)+"_codec_schema") << ',' << (fields.empty()?"NULL":"fields") << ','
      << (enum_count?"enumerators":"NULL") << ',' << enum_count << ",{ " << width << ',' << unsigned_form << "}," << fixed << ',' << flags << ','
      << (root_count?"root_order":"NULL") << ',' << root_count << ',' << group_count << ',' << static_cast<unsigned>(t.real.ieee_form) << "};return &schema;}\n";
    for(const auto& entry:extra_entries())if(c_extra(entry.second)){
      std::string policy=entry.first;for(char& c:policy)if(c>='a'&&c<='z')c=static_cast<char>(c-'a'+'A');
      s << linkage() << "asn1_status_t " << n << "_encode_" << entry.first << "(asn1_byte_writer_t* w,const " << n << "* value){return asn1_schema_encode(w,ASN1_CODEC_" << policy << ',' << n << "_codec_schema(),value);}\n"
        << linkage() << "asn1_status_t " << n << "_decode_" << entry.first << "(asn1_byte_reader_t* r," << n << "* value){return asn1_schema_decode(r,ASN1_CODEC_" << policy << ',' << n << "_codec_schema(),value);}\n";
    }
  }
  void header(std::ostream& h) const {
  h << (c_uper||c_aper?"/* Generated C11 types and supported PER codecs. */\n":"/* Generated C11 types only; no codec implementation. */\n") << "#ifndef " << prefix << "_types_h\n#define " << prefix
    << "_types_h\n#define " << prefix << "_GENERATED_ABI_VERSION 2\n"
    << "#include <asn1/c_runtime/asn1_c_common.h>\n#include <asn1/c_runtime/bit_string.h>\n#include <asn1/c_runtime/limits.h>\n";
  if(c_uper||c_aper) h << "#include <asn1/c_runtime/per.h>\n";
  if(c_extra()) h << "#include <asn1/c_runtime/schema_codec.h>\n";
  if(c_schema()) h << "#include <asn1/c_runtime/tlv_schema.h>\n#include <stddef.h>\n";
  h << "#include <stdlib.h>\n#include <string.h>\n#ifdef __cplusplus\nextern \"C\" {\n#endif\n";
  h << "typedef struct { int has_key; const char *decimal_key; uint32_t type_id; } " << prefix << "_selector_entry;\n"
    << "typedef struct { const char *name; unsigned tag_class; uint64_t tag_number; int explicit_tag; size_t presence_index,extension_group; int nested_sequence; size_t ordinal; const char *governor; size_t selector_count; const " << prefix << "_selector_entry *entries; } " << prefix << "_field_plan;\n";
  for (const auto& item : names) h << "typedef struct " << item.second << ' ' << item.second << ";\n";
  for (auto id : order) declaration(id,h);
  for (const auto& item : names) {
    const auto& n = item.second;
    const auto& t = model.arena.get(item.first);
    h << linkage() << "void " << n << "_init(" << n << "*);\n" << linkage() << "void " << n << "_free(" << n << "*);\n"
      << linkage() << "asn1_status_t " << n << "_copy(const " << n << "*, " << n << "*);\n"
      << linkage() << "asn1_status_t " << n << "_reset_defaults(" << n << "*);\n"
      << linkage() << "size_t " << n << "_field_count(void);\n" << linkage() << "const " << prefix << "_field_plan *" << n << "_field(size_t);\n"
      << linkage() << "asn1_status_t " << n << "_select(size_t,const char*,uint32_t*);\n";
    if(c_uper) h << linkage() << "asn1_status_t " << n << "_encode_uper(asn1_bit_writer_t*,const " << n << "*);\n" << linkage() << "asn1_status_t " << n << "_decode_uper(asn1_bit_reader_t*," << n << "*);\n";
    if(c_aper) h << linkage() << "asn1_status_t " << n << "_encode_aper(asn1_bit_writer_t*,const " << n << "*);\n" << linkage() << "asn1_status_t " << n << "_decode_aper(asn1_bit_reader_t*," << n << "*);\n";
    for(const auto* codec:{"ber","der","cer"})if((std::string(codec)=="ber"&&c_ber)||(std::string(codec)=="der"&&c_der)||(std::string(codec)=="cer"&&c_cer))
      h << linkage() << "asn1_status_t " << n << "_encode_" << codec << "(asn1_byte_writer_t*,const " << n << "*);\n" << linkage() << "asn1_status_t " << n << "_decode_" << codec << "(asn1_byte_reader_t*," << n << "*);\n";
    for(const auto& entry:extra_entries())if(c_extra(entry.second))h << linkage() << "asn1_status_t " << n << "_encode_" << entry.first << "(asn1_byte_writer_t*,const " << n << "*);\n" << linkage() << "asn1_status_t " << n << "_decode_" << entry.first << "(asn1_byte_reader_t*," << n << "*);\n";
    if(t.kind==TypeKind::Sequence || t.kind==TypeKind::Set) {
      const auto& seq=per_sequence(t);
      for(const auto* fields:{&seq.root,&seq.trailing_root})
        for(const auto& field:*fields) if(typed_open_field(field)) {
          const auto suffix=selector_function_suffix(field);
          if(c_uper) h << linkage() << "asn1_status_t " << n << "_decode_selected_" << suffix << "_uper(const " << n << "*,uint32_t*,void**);\n";
          if(c_aper) h << linkage() << "asn1_status_t " << n << "_decode_selected_" << suffix << "_aper(const " << n << "*,uint32_t*,void**);\n";
          h << linkage() << "void " << n << "_free_selected_" << suffix << "(uint32_t,void*);\n";
        }
    }
  }
  if (inline_functions) {
    helper_declarations(h);
    for (auto id : order) { body(id,h); defaults_body(id,h); metadata(id,h); }
    for (auto id : order) {
      if(c_uper) emit_per_codec(id,"uper",h);
      if(c_aper) emit_per_codec(id,"aper",h);
      if(c_schema()) emit_tlv_schema(id,h);
      if(c_extra()) emit_codec_schema(id,h);
    }
  }
  h << "#ifdef __cplusplus\n}\n#endif\n#endif\n";
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
  void zero_declaration(const std::string& type, const char* variable, std::ostream& s) const {
    s << "#ifdef __cplusplus\n" << type << ' ' << variable << " = {};\n#else\n"
      << type << ' ' << variable << " = {0};\n#endif\n";
  }
  void body(TypeId id, std::ostream& s) const {
    const auto& t = model.arena.get(id);
    const auto n = name(id);
    s << linkage() << "void " << n << "_init(" << n << " *v) { if (v) {\n";
    zero_declaration(n,"zero",s); s << "*v = zero;\n";
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
    s << "} }\n" << linkage() << "void " << n << "_free(" << n << " *v) { if (!v) return;\n";
    contents(id, s, false);
    s << n << "_init(v); }\n"
      << linkage(true) << "asn1_status_t " << n << "_copy_at(const " << n << " *src, " << n << " *out, size_t depth) {\n"
      ;
    zero_declaration(n,"tmp",s);
    s << "asn1_status_t status = ASN1_OK;\n"
      << "if (!src || !out) return ASN1_ERR_INVALID_ARGUMENT;\n"
      << "if (depth >= 256) return ASN1_ERR_RESOURCE_LIMIT;\n";
    s << n << "_init(&tmp);\n";
    contents(id, s, true);
    // All generated copy paths share one transactional cleanup route.
    s << "if (status != ASN1_OK) goto fail;\n" << n << "_free(out); *out = tmp; return ASN1_OK;\n"
      << "fail: " << n << "_free(&tmp); return status; }\n"
      << linkage() << "asn1_status_t " << n << "_copy(const " << n << " *src, " << n << " *out) { return "
      << n << "_copy_at(src, out, 0); }\n";
  }
  void contents(TypeId id, std::ostream& s, bool copy) const {
    const auto& t = model.arena.get(id);
    const auto n = name(id);
    if (copy) {
      if (const auto* size = planning::storage_size(t)) {
        const auto plan = planning::constraint(*size);
        if(t.kind==TypeKind::String && c_schema()) {
          const unsigned tag=t.string.kind==StringKind::NumericString?18:t.string.kind==StringKind::PrintableString?19:t.string.kind==StringKind::IA5String?22:t.string.kind==StringKind::VisibleString?26:12;
          s << "size_t size_characters=0; if(src->count>src->capacity || (src->count&&!src->data)) { status=ASN1_ERR_INVALID_ARGUMENT;goto fail; } status=asn1_tlv_string_length(" << tag << ",asn1_byte_view(src->data,src->count),&size_characters);if(status!=ASN1_OK)goto fail;\n";
        }
        const std::string count = t.kind == TypeKind::String && c_schema()?"size_characters":t.kind == TypeKind::BitString ? "src->value.bit_length" : "src->count";
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
        if(typed_open_field(*m.field)) {
          const auto type_member=decoded_type_member(*m.field), value_member=decoded_value_member(*m.field);
          if(copy) {
            s << "if(src->" << value_member << ") { switch(src->" << type_member << ") {\n";
            std::set<TypeId> emitted_types;
            for(const auto& entry:m.field->table_constraint->entries) if(entry.has_id_value && emitted_types.insert(entry.type).second)
              s << "case UINT32_C(" << entry.type << "): tmp." << value_member << "=(void*)calloc(1,sizeof(" << name(entry.type) << ")); if(!tmp." << value_member << ") { status=ASN1_ERR_OUT_OF_MEMORY; goto fail; } " << name(entry.type) << "_init((" << name(entry.type) << "*)tmp." << value_member << "); status=" << name(entry.type) << "_copy_at((const " << name(entry.type) << "*)src->" << value_member << ",( " << name(entry.type) << "*)tmp." << value_member << ",depth+1); if(status!=ASN1_OK) goto fail; tmp." << type_member << "=src->" << type_member << "; break;\n";
            s << "default: status=ASN1_ERR_INVALID_ARGUMENT; goto fail; } }\n";
          } else {
            s << "if(v->" << value_member << ") " << n << "_free_selected_" << selector_function_suffix(*m.field) << "(v->" << type_member << ",v->" << value_member << ");\n";
          }
        }
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
std::string rewrite_generated_allocations(std::string text, const std::string& prefix) {
  const std::pair<const char*,const char*> operations[]={{"malloc(","malloc"},{"calloc(","calloc"},{"realloc(","realloc"},{"free(","free"}};
  for(const auto& op:operations) {
    const std::string from=op.first; const std::string to=prefix+"_alloc_"+op.second+"(";
    std::size_t at=0;
    while((at=text.find(from,at))!=std::string::npos){
      const bool identifier_before=at>0 && ((text[at-1]>='a'&&text[at-1]<='z') ||
        (text[at-1]>='A'&&text[at-1]<='Z') || (text[at-1]>='0'&&text[at-1]<='9') || text[at-1]=='_');
      if(identifier_before){at+=from.size();continue;}
      text.replace(at,from.size(),to);at+=to.size();
    }
  }
  return text;
}
std::string add_generated_allocator_aliases(std::string header,const std::string& prefix) {
  const std::string marker="#include <string.h>\n";
  const auto at=header.find(marker);
  if(at==std::string::npos)return header;
  std::ostringstream aliases;
  aliases << "#ifdef ASN1_C_RUNTIME_TEST_ALLOCATOR\n"
          << "#ifdef __cplusplus\nextern \"C\" {\n#endif\n"
          << "void* asn1_test_generated_malloc(size_t); void* asn1_test_generated_calloc(size_t,size_t);\n"
          << "void* asn1_test_generated_realloc(void*,size_t); void asn1_test_generated_free(void*);\n"
          << "#ifdef __cplusplus\n}\n#endif\n"
          << "#define " << prefix << "_alloc_malloc asn1_test_generated_malloc\n"
          << "#define " << prefix << "_alloc_calloc asn1_test_generated_calloc\n"
          << "#define " << prefix << "_alloc_realloc asn1_test_generated_realloc\n"
          << "#define " << prefix << "_alloc_free asn1_test_generated_free\n"
          << "#else\n#define " << prefix << "_alloc_malloc malloc\n#define " << prefix << "_alloc_calloc calloc\n"
          << "#define " << prefix << "_alloc_realloc realloc\n#define " << prefix << "_alloc_free free\n#endif\n";
  header.insert(at+marker.size(),aliases.str());
  return header;
}
}
void emit_c_types(const ir::Model& model, const std::string& prefix,
                  const std::string& header_name, Diagnostics& diag,
                  std::ostream& header, std::ostream& source) try {
  if (!valid_c_prefix(prefix)) { diag.error({}, "C types: invalid non-reserved symbol prefix"); return; }
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
  types.header(h);
  s << "#include \"" << header_name << "\"\n#include <stdlib.h>\n#include <string.h>\n";
  for (const auto& item : types.names) s << "static asn1_status_t " << item.second << "_copy_at(const " << item.second
    << "*, " << item.second << "*, size_t);\n";
  for (const auto& item : types.names) s << "static asn1_status_t " << item.second << "_defaults_at(" << item.second << "*, size_t);\n";
  for (auto id : types.order) { types.body(id,s); types.defaults_body(id,s); types.metadata(id,s); }
  header << add_generated_allocator_aliases(rewrite_generated_allocations(h.str(),prefix),prefix);
  source << rewrite_generated_allocations(s.str(),prefix);
  if (!header || !source) diag.error({}, "C types: failed writing generated streams");
} catch (const std::length_error& ex) {
  diag.error({}, std::string("C types capacity exceeded: ")+ex.what());
} catch (const std::ios_base::failure& ex) {
  diag.error({}, std::string("C types stream failure: ")+ex.what());
}
std::vector<CGeneratedFile> render_c_type_files(const ir::Model& model,
                                               const EmitOptions& opt, Diagnostics& diag) try {
  if (!validate_emit_options(opt,diag)) return {};
  const unsigned requested=static_cast<unsigned>(opt.codec);
  const unsigned per_mask=static_cast<unsigned>(CodecKind::Uper|CodecKind::Aper|CodecKind::Ber|CodecKind::Der|CodecKind::Cer|CodecKind::Oer|CodecKind::Coer|CodecKind::Jer|CodecKind::Jeri|CodecKind::Xer|CodecKind::Cxer|CodecKind::Exer);
  if(opt.language!=TargetLanguage::C || (opt.c_types_only && opt.codec!=CodecKind::None) ||
     (!opt.c_types_only && opt.codec==CodecKind::None) || (requested&~per_mask)!=0) {
    diag.error({},"C artifact rendering requires type-only mode or a registered codec entry"); return {};
  }
  const auto prefix=opt.c_prefix.empty()?std::string("asn1_gen"):opt.c_prefix;
  // Share the existing model/name/profile preflight, including untouched streams.
  std::ostringstream checked_header, checked_source;
  emit_c_types(model,prefix,opt.basename+".h",diag,checked_header,checked_source);
  if (!diag.ok()) return {};
  if (opt.emit_inline && (opt.emit_split || opt.split_max_types)) {
    diag.error({},"C inline output conflicts with module/source splitting"); return {};
  }
  Types types(model,prefix,diag);
  if (!types.prepare()) return {};
  types.external_helpers=true;
  types.inline_functions=opt.emit_inline;
  types.c_uper=static_cast<unsigned>(opt.codec&CodecKind::Uper)!=0;
  types.c_aper=static_cast<unsigned>(opt.codec&CodecKind::Aper)!=0;
  types.c_ber=static_cast<unsigned>(opt.codec&CodecKind::Ber)!=0;
  types.c_der=static_cast<unsigned>(opt.codec&CodecKind::Der)!=0;
  types.c_cer=static_cast<unsigned>(opt.codec&CodecKind::Cer)!=0;
  for(const auto& entry:extra_entries())if(static_cast<unsigned>(opt.codec&entry.second))types.c_extra_mask=types.c_extra_mask|entry.second;
  if(types.c_schema()) {
    std::set<TypeId> seen;for(auto id:model.exported_types)types.c_tlv_supported(id,seen);
    if(!diag.ok())return {};
  }
  if(types.c_uper||types.c_aper) {
    for(auto id:model.exported_types) {
      std::set<TypeId> active;
      types.c_per_supported(id,active,true);
    }
    if(!diag.ok()) return {};
  }
  std::ostringstream h; types.header(h);
  const std::size_t per_functions=(types.c_uper?4u:0u)+(types.c_aper?4u:0u);
  const auto function_count=[&](TypeId id) {
    const auto& t=model.arena.get(id);std::size_t count=9u+per_functions;
    if(t.kind==TypeKind::Sequence||t.kind==TypeKind::Set){const auto& sequence=types.per_sequence(t);
      for(const auto* list:{&sequence.root,&sequence.trailing_root})for(const auto& f:*list)if(types.typed_open_field(f))count+=1u+(types.c_uper?2u:0u)+(types.c_aper?2u:0u);
    }
    if(types.c_schema()){
      count+=4u+(types.c_ber?2u:0u)+(types.c_der?2u:0u)+(types.c_cer?2u:0u)+(t.kind==TypeKind::Choice?2u:0u);
      for(const auto& m:members(t))if(m.field->presence==Presence::Default)count+=types.owning_default(*m.field)?2u:1u;
    }
    if(types.c_extra()){++count;for(const auto& entry:extra_entries())if(types.c_extra(entry.second))count+=2u;}
    return count;
  };
  std::size_t total_functions=0;for(auto id:types.order)total_functions+=function_count(id);
  std::vector<CGeneratedFile> files{{opt.basename+".h",add_generated_allocator_aliases(rewrite_generated_allocations(h.str(),prefix),prefix),types.names.size(),opt.emit_inline?total_functions:0}};
  if (!opt.emit_inline) {
    std::ostringstream internal;
    internal << "/* Generated C11 internal declarations. */\n#ifndef " << prefix << "_internal_h\n#define " << prefix
      << "_internal_h\n#include \"" << opt.basename << ".h\"\n#ifdef __cplusplus\nextern \"C\" {\n#endif\n";
    types.helper_declarations(internal);
    internal << "#ifdef __cplusplus\n}\n#endif\n#endif\n";
    files.push_back({opt.basename+"_internal.h",internal.str(),0,0});
    // Tarjan SCC over the actual storage/selector dependency graph. Sort by
    // qualified emitted names, not unordered-container traversal.
    std::map<TypeId,std::size_t> index,low;
    std::set<TypeId> active;
    std::vector<TypeId> stack;
    std::vector<std::vector<TypeId>> groups;
    std::size_t next=0;
    std::function<void(TypeId)> visit=[&](TypeId id) {
      index[id]=low[id]=next++; stack.push_back(id);active.insert(id);
      auto deps=types.edges.at(id);
      std::sort(deps.begin(),deps.end(),[&](TypeId a,TypeId b){return types.name(a)<types.name(b);});
      for(auto dep:deps) {
        if(!index.count(dep)){ visit(dep);low[id]=std::min(low[id],low[dep]); }
        else if(active.count(dep)) low[id]=std::min(low[id],index[dep]);
      }
      if(low[id]==index[id]) {
        std::vector<TypeId> group;
        for(;;){const auto dep=stack.back();stack.pop_back();active.erase(dep);group.push_back(dep);if(dep==id)break;}
        std::sort(group.begin(),group.end(),[&](TypeId a,TypeId b){return types.name(a)<types.name(b);});
        groups.push_back(std::move(group));
      }
    };
    std::vector<TypeId> ids;
    for(const auto& item:types.names) ids.push_back(item.first);
    std::sort(ids.begin(),ids.end(),[&](TypeId a,TypeId b){return types.name(a)<types.name(b);});
    for(auto id:ids) if(!index.count(id)) visit(id);
    std::sort(groups.begin(),groups.end(),[&](const auto& a,const auto& b){return types.name(a.front())<types.name(b.front());});
    std::map<std::string,std::vector<std::vector<TypeId>>> modules;
    for(auto& group:groups) {
      std::string owner;
      if(opt.emit_split) {
        owner=model.arena.get(group.front()).module;
        for(auto id:group) owner=std::min(owner,model.arena.get(id).module);
        owner="c_m"+spell(owner);
      }
      if(opt.split_max_types && group.size()>opt.split_max_types)
        diag.warning({},"C source SCC exceeds split-max-types: "+std::to_string(group.size())+" types, owner "+owner);
      modules[owner].push_back(std::move(group));
    }
    for(const auto& module:modules) {
      std::vector<std::vector<TypeId>> chunks;
      for(const auto& group:module.second) {
        if(chunks.empty() || (opt.split_max_types && !chunks.back().empty() &&
            group.size()>opt.split_max_types-std::min(opt.split_max_types,chunks.back().size()))) chunks.emplace_back();
        chunks.back().insert(chunks.back().end(),group.begin(),group.end());
      }
      for(std::size_t i=0;i<chunks.size();++i) {
        std::string stem=opt.emit_split?module.first:opt.basename;
        if(opt.split_max_types){std::ostringstream suffix;suffix << '_' << std::setw(4) << std::setfill('0') << i+1;stem+=suffix.str();}
        std::ostringstream source;
      source << (types.c_uper||types.c_aper?"/* Generated C11 types and PER codecs. */\n":"/* Generated C11 types only; no codec implementation. */\n") << "#include \"" << opt.basename << "_internal.h\"\n#include <stdlib.h>\n#include <string.h>\n";
      for(auto id:chunks[i]){
        types.body(id,source);types.defaults_body(id,source);types.metadata(id,source);
        if(types.c_uper) types.emit_per_codec(id,"uper",source);
        if(types.c_aper) types.emit_per_codec(id,"aper",source);
        if(types.c_schema()) types.emit_tlv_schema(id,source);
        if(types.c_extra()) types.emit_codec_schema(id,source);
      }
      std::size_t functions=0;for(auto id:chunks[i])functions+=function_count(id);
      files.push_back({stem+".c",rewrite_generated_allocations(source.str(),prefix),chunks[i].size(),functions});
      }
    }
  }
  std::set<std::string> unique;
  for(const auto& file:files){
    if(!portable_output_name(std::filesystem::path(file.name).stem().string())) {
      diag.error({},"non-portable C output filename: "+file.name);return {};
    }
    auto key=file.name;for(auto& c:key)if(c>='A'&&c<='Z')c=char(c-'A'+'a');
    if(!unique.insert(key).second){diag.error({},"C output filename collision: "+file.name);return {};}}
  std::ostringstream json,cmake;
  json << "{\"format\":1,\"language\":\"c\",\"types_only\":" << (opt.codec==CodecKind::None?"true":"false") << ",\"codec_mask\":" << static_cast<unsigned>(opt.codec) << ",\"inline\":"
    << (opt.emit_inline?"true":"false") << ",\"split_modules\":" << (opt.emit_split?"true":"false")
    << ",\"split_max_types\":" << opt.split_max_types << ",\"sources\":[";
  cmake << "# Generated by asn1cxx; reconfigure after changing the schema/options.\nset(ASN1_GENERATED_SOURCES\n";
  bool first=true;
  for(const auto& file:files)if(file.name.size()>2 && file.name.substr(file.name.size()-2)==".c"){
    if(!first)json << ',';first=false;json << '\"' << file.name << '\"';
    cmake << "  \"${CMAKE_CURRENT_LIST_DIR}/" << file.name << "\"\n";
  }
  cmake << ")\nset(ASN1_GENERATED_HEADERS\n";
  json << "],\"headers\":[";first=true;
  for(const auto& file:files)if(file.name.size()>2 && file.name.substr(file.name.size()-2)==".h"){
    if(!first)json << ',';first=false;json << '\"' << file.name << '\"';
    cmake << "  \"${CMAKE_CURRENT_LIST_DIR}/" << file.name << "\"\n";
  }
  cmake << ")\n"; json << "],\"units\":[";first=true;
  for(const auto& file:files){if(!first)json << ',';first=false;
    json << "{\"file\":\"" << file.name << "\",\"types\":" << file.type_count << ",\"functions\":"
      << file.function_count << ",\"bytes\":" << file.contents.size() << '}';}
  json << "]}\n";
  files.push_back({"generated-sources.json",json.str(),0,0});
  files.push_back({"generated-sources.cmake",cmake.str(),0,0});
  return files;
} catch(const std::length_error& ex) {
  diag.error({},std::string("C artifact capacity exceeded: ")+ex.what());return {};
}

}}
