#include <asn1/codegen/c_types.hpp>
#include <asn1/codegen/plan.hpp>
#include <gtest/gtest.h>
#include <sstream>

TEST(CGenBatch2, UnsupportedLayoutsLeaveStreamsUntouched) {
  for (auto kind : {asn1::ir::TypeKind::InstanceOf, asn1::ir::TypeKind::ObjectClassField,
                    asn1::ir::TypeKind::Integer}) {
    asn1::ir::Model m;
    asn1::ir::Type t; t.name = "T"; t.module = "M"; t.kind = kind;
    t.object_class_field.open_type = false;
    t.integer.constraint.statically_foldable = false;
    m.exported_types.push_back(m.arena.add(t));
    asn1::Diagnostics d; std::ostringstream h,s;
    h << "old header"; s << "old source";
    asn1::codegen::emit_c_types(m,"p","generated.h",d,h,s);
    EXPECT_FALSE(d.ok()); EXPECT_EQ(h.str(),"old header"); EXPECT_EQ(s.str(),"old source");
  }
}
TEST(CGenBatch2, RejectsReservedPrefixAndUnresolvedReference) {
  for (const auto& prefix : {"", "_p", "p_", "p__q", "9p"}) {
    asn1::ir::Model m; asn1::Diagnostics d; std::ostringstream h,s;
    asn1::codegen::emit_c_types(m,prefix,"generated.h",d,h,s);
    EXPECT_FALSE(d.ok()); EXPECT_TRUE(h.str().empty());
  }
  asn1::ir::Model m; asn1::ir::Type t; t.kind = asn1::ir::TypeKind::Referenced;
  m.exported_types.push_back(m.arena.add(t));
  asn1::Diagnostics d; std::ostringstream h,s;
  asn1::codegen::emit_c_types(m,"p","generated.h",d,h,s);
  EXPECT_FALSE(d.ok()); EXPECT_TRUE(h.str().empty());
}
TEST(CGenBatch2, ChoicePlanBorrowsTagsSelectorsAndOrdinals) {
  asn1::ir::ChoiceDesc choice;
  asn1::ir::Field first, second;
  first.tag = {asn1::ir::TagClass::Application,42,true};
  second.table_constraint = asn1::ir::TableConstraintInfo{};
  second.table_constraint->governor_field_name = "id";
  choice.alternatives = {first,second}; choice.extensions = {second}; choice.extensible = true;
  const auto plan = asn1::codegen::planning::choice(choice);
  ASSERT_EQ(plan.root.size(),2u); ASSERT_EQ(plan.extensions.size(),1u);
  EXPECT_EQ(plan.root[0].field,&choice.alternatives[0]);
  EXPECT_EQ(plan.root[0].effective_tag.number,42u);
  EXPECT_TRUE(plan.root[0].effective_tag.is_explicit);
  EXPECT_EQ(plan.root[1].selector,&*choice.alternatives[1].table_constraint);
  EXPECT_EQ(plan.root[1].ordinal,1u); EXPECT_EQ(plan.extensions[0].ordinal,0u);
  EXPECT_TRUE(plan.extensible);
}
TEST(CGenBatch2, InvalidDefaultsLeaveStreamsUntouched) {
  using namespace asn1::ir;
  for (int invalid = 0; invalid != 4; ++invalid) {
    Model m;
    Type scalar; scalar.kind = TypeKind::Integer; scalar.integer.host_bits = 64;
    scalar.integer.constraint.lower = BigInt::from_i64(1);
    scalar.integer.constraint.upper = BigInt::from_i64(9);
    scalar.integer.constraint.root_ranges.push_back({BigInt::from_i64(1),BigInt::from_i64(9)});
    const auto scalar_id = m.arena.add(scalar);
    Value value; value.kind = invalid == 2 ? ValueKind::Boolean : ValueKind::Integer;
    value.integer = BigInt::from_i64(invalid == 3 ? 10 : 7);
    const auto value_id = m.arena.add_value(value);
    Type record; record.name = "Record"; record.module = "M"; record.kind = TypeKind::Sequence;
    Field f; f.name = "number"; f.type = scalar_id; f.presence = Presence::Default;
    if (invalid != 0) f.default_value = invalid == 1 ? kInvalidValue : value_id;
    record.sequence.root.push_back(f);
    m.exported_types.push_back(m.arena.add(record));
    asn1::Diagnostics d; std::ostringstream h,s; h << "header"; s << "source";
    asn1::codegen::emit_c_types(m,"p","generated.h",d,h,s);
    EXPECT_FALSE(d.ok()); EXPECT_EQ(h.str(),"header"); EXPECT_EQ(s.str(),"source");
  }
}
TEST(CGenBatch2, RejectsAmbiguousNamesAndStandardHeaderShadowing) {
  using namespace asn1::ir;
  for (bool duplicate_field : {false,true}) {
    Model m; Type t; t.kind = TypeKind::Boolean; t.module = "M"; t.name = "T";
    const auto id = m.arena.add(t);
    m.exported_types.push_back(id);
    if (duplicate_field) {
      Type record; record.kind = TypeKind::Sequence; record.module = "M"; record.name = "Record";
      Field f; f.name = "member"; f.type = id; record.sequence.root = {f,f};
      m.exported_types.push_back(m.arena.add(record));
    } else m.exported_types.push_back(m.arena.add(t));
    asn1::Diagnostics d; std::ostringstream h,s; h << "header"; s << "source";
    asn1::codegen::emit_c_types(m,"p","generated.h",d,h,s);
    EXPECT_FALSE(d.ok()); EXPECT_EQ(h.str(),"header"); EXPECT_EQ(s.str(),"source");
  }
  for (const auto& header : {"stdlib.h","STDINT.H","String.h","../generated.h","dir/generated.h","bad\".h","CON.h","lpt1.any.h",".","generated."}) {
    Model m; asn1::Diagnostics d; std::ostringstream h,s;
    asn1::codegen::emit_c_types(m,"p",header,d,h,s);
    EXPECT_FALSE(d.ok()); EXPECT_TRUE(h.str().empty()); EXPECT_TRUE(s.str().empty());
  }
}
TEST(CGenBatch2, InvalidSizePlansLeaveStreamsUntouched) {
  using namespace asn1::ir;
  for (int invalid = 0; invalid != 3; ++invalid) {
    Model m; Type t; t.kind = TypeKind::OctetString; t.module = "M"; t.name = "Bytes";
    auto& size = t.octet_string.size;
    size.is_size = true;
    size.statically_foldable = invalid != 0;
    if (invalid == 1) size.root_ranges.push_back({BigInt::from_i64(-1),BigInt::from_i64(4)});
    if (invalid == 2) {
      auto huge = BigInt::from_decimal("18446744073709551616",false);
      ASSERT_TRUE(huge);
      size.root_ranges.push_back({BigInt::from_i64(0),huge.value()});
    }
    m.exported_types.push_back(m.arena.add(t));
    asn1::Diagnostics d; std::ostringstream h,s; h << "header"; s << "source";
    asn1::codegen::emit_c_types(m,"p","generated.h",d,h,s);
    EXPECT_FALSE(d.ok()); EXPECT_EQ(h.str(),"header"); EXPECT_EQ(s.str(),"source");
  }
}

TEST(CGenBatch2, UnsupportedOwningDefaultsLeaveStreamsUntouched) {
  using namespace asn1::ir;
  for(int mode=0;mode<3;++mode) {
    Model m; Type type; type.kind=mode==0?TypeKind::Integer:TypeKind::OctetString;
    type.integer.constraint.lower=BigInt::from_i64(0);
    type.integer.constraint.upper=BigInt::from_i64(9);
    type.integer.constraint.root_ranges={{type.integer.constraint.lower,type.integer.constraint.upper}};
    const auto tid=m.arena.add(type);
    Value value; value.kind=mode==0?ValueKind::Integer:ValueKind::BitOrOctet;
    value.integer=BigInt::from_i64(10); value.text=mode==1?"'F'H":"'X'H";
    Type record; record.kind=TypeKind::Sequence; record.module="M"; record.name="Record";
    Field field; field.name="value"; field.type=tid; field.presence=Presence::Default;
    field.default_value=m.arena.add_value(value); record.sequence.root={field};
    m.exported_types.push_back(m.arena.add(record));
    asn1::Diagnostics d; std::ostringstream h,s; h<<"old h";s<<"old c";
    asn1::codegen::emit_c_types(m,"p","generated.h",d,h,s);
    EXPECT_FALSE(d.ok()); EXPECT_EQ(h.str(),"old h"); EXPECT_EQ(s.str(),"old c");
  }
}
TEST(CGenBatch2, AmbiguousSelectorsLeaveStreamsUntouched) {
  using namespace asn1::ir;
  for(bool unresolved:{false,true}) {
    Model m; Type flag; flag.kind=TypeKind::Boolean; const auto tid=m.arena.add(flag);
    Type record; record.kind=TypeKind::Sequence; record.module="M"; record.name="Record";
    Field field; field.name="payload"; field.type=tid; field.table_constraint=TableConstraintInfo{};
    TableConstraintEntry entry; entry.type=tid;entry.has_id_value=!unresolved;entry.id_value=BigInt::from_i64(7);
    field.table_constraint->entries={entry,entry}; record.sequence.root={field};
    m.exported_types.push_back(m.arena.add(record));
    asn1::Diagnostics d; std::ostringstream h,s; h<<"old h";s<<"old c";
    asn1::codegen::emit_c_types(m,"p","generated.h",d,h,s);
    EXPECT_FALSE(d.ok());EXPECT_EQ(h.str(),"old h");EXPECT_EQ(s.str(),"old c");
  }
}
TEST(CGenBatch2, LiteralPlanningNormalizesWithoutRuntimeDependencies) {
  asn1::ir::Value v; v.kind=asn1::ir::ValueKind::String;v.text="\"a\"\"b\"";
  EXPECT_EQ(asn1::codegen::planning::string_default(v),"a\"b");
  v.kind=asn1::ir::ValueKind::BitOrOctet;v.text="'A8'H";
  const auto hex=asn1::codegen::planning::binary_default(v);
  ASSERT_TRUE(hex);EXPECT_EQ(hex->bits,8u);EXPECT_EQ(hex->bytes,(std::vector<std::uint8_t>{0xa8}));
  v.text="'10101'B";const auto bits=asn1::codegen::planning::binary_default(v);
  ASSERT_TRUE(bits);EXPECT_EQ(bits->bits,5u);EXPECT_EQ(bits->bytes,hex->bytes);
  v.text="'2'B";EXPECT_FALSE(asn1::codegen::planning::binary_default(v));
}
