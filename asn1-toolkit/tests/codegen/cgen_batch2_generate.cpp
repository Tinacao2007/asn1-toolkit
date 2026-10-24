#include <asn1/codegen/c_types.hpp>
#include <asn1/frontend/lexer.hpp>
#include <asn1/frontend/parser.hpp>
#include <asn1/semantic/analyzer.hpp>
#include <asn1/support/source_file.hpp>
#include <fstream>
#include <iostream>
#include <sstream>
int main(int argc, char** argv) {
  if (argc != 4) return 2;
  asn1::Diagnostics diag;
  if (std::string(argv[1]) == "--names") {
    using namespace asn1::ir;
    Model model;
    Type flag; flag.kind = TypeKind::Boolean;
    const auto flag_id = model.arena.add(flag);
    const std::vector<std::string> fields = {
      "auto","break","case","char","const","continue","default","do","double","else",
      "enum","extern","float","for","goto","if","inline","int","long","register",
      "restrict","return","short","signed","sizeof","static","struct","switch",
      "typedef","union","unsigned","void","volatile","while","_Alignas","_Alignof",
      "_Atomic","_Bool","_Complex","_Generic","_Imaginary","_Noreturn","_Static_assert",
      "_Thread_local","class","template","new","delete","a-b","a_b","a_x2db","_x",
      "__x","has_x","value","kind","data","count","capacity"};
    for (const auto& module : {"M","m","M-X","M_x2dX"}) {
      for (const auto& type : {"T","t","T-X","T_x2dX","_T","__T","T_init","anon_0"}) {
        Type record; record.kind = TypeKind::Sequence; record.module = module; record.name = type;
        for (const auto& name : fields) {
          Field f; f.name = name; f.type = flag_id; f.presence = Presence::Optional;
          record.sequence.root.push_back(f);
        }
        model.exported_types.push_back(model.arena.add(record));
      }
    }
    for (const auto& alias : {"Alias","Alias-copy"}) {
      Type t; t.kind = TypeKind::Referenced; t.module = "M"; t.name = alias;
      t.referenced.resolved = flag_id;
      model.exported_types.push_back(model.arena.add(t));
    }
    std::ofstream h(argv[2]), s(argv[3]);
    asn1::codegen::emit_c_types(model,"stage2","generated.h",diag,h,s);
    if (!diag.ok()) diag.print(std::cerr);
    return diag.ok() && h && s ? 0 : 5;
  }
  auto loaded = asn1::SourceFile::from_path(argv[1]);
  if (!loaded) return 3;
  auto file = std::move(loaded.value());
  asn1::Lexer lexer(file,diag);
  asn1::Parser parser(lexer,diag);
  asn1::Analyzer analyzer(diag);
  auto model = analyzer.analyze(parser.parse_modules());
  if (diag.ok()) {
    using namespace asn1::ir;
    TypeId flag=kInvalidType, number=kInvalidType;
    for (auto id: model.exported_types) {
      const auto& t=model.arena.get(id);
      if(t.module=="A" && t.name=="Flag") flag=id;
      if(t.module=="A" && t.name=="Huge") number=id;
    }
    Type payload; payload.kind=TypeKind::ObjectClassField; payload.module="A"; payload.name="Payload";
    const auto payload_id=model.arena.add(payload); model.exported_types.push_back(payload_id);
    Type fixed; fixed.kind=TypeKind::ObjectClassField; fixed.module="A"; fixed.name="Fixed";
    fixed.object_class_field.open_type=false; fixed.object_class_field.fixed_type=flag;
    model.exported_types.push_back(model.arena.add(fixed));
    Type record; record.kind=TypeKind::Sequence; record.module="A"; record.name="Planning";
    record.sequence.extensible=true;
    Field id; id.name="id"; id.type=number; id.tag={TagClass::Context,0,false};
    Field data; data.name="payload"; data.type=payload_id; data.presence=Presence::Optional;
    data.tag={TagClass::Application,UINT64_C(4294967297),true};
    data.table_constraint=TableConstraintInfo{}; data.table_constraint->governor_field_name="id";
    for (const auto& key: {"340282366920938463463374607431768211456","-7"}) {
      TableConstraintEntry entry; entry.has_id_value=true;
      auto parsed=BigInt::from_decimal(key,false);
      if (key[0]=='-') parsed=BigInt::from_decimal("7",true);
      entry.id_value=parsed.value(); entry.type=flag;
      data.table_constraint->entries.push_back(entry);
    }
    record.sequence.root={id,data};
    Field tail; tail.name="tail"; tail.type=flag; tail.presence=Presence::Optional;
    record.sequence.trailing_root={tail};
    Field single=tail; single.name="single";
    Field required=tail; required.name="required"; required.presence=Presence::Mandatory;
    Field maybe=tail; maybe.name="maybe";
    record.sequence.extension_groups={{{single},false},{{required,maybe},true}};
    model.exported_types.push_back(model.arena.add(record));
  }
  std::ostringstream header, source;
  if (diag.ok()) asn1::codegen::emit_c_types(model,"stage2","generated.h",diag,header,source);
  if (!diag.ok()) { diag.print(std::cerr); return 4; }
  std::ofstream h(argv[2]), s(argv[3]);
  h << header.str(); s << source.str();
  return h && s ? 0 : 5;
}
