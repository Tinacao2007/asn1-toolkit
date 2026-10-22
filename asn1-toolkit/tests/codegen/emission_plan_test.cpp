/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/tests/codegen/emission_plan_test.cpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   GoogleTest exercising emission plan behaviour.
**
** Specification: Validates toolchain output against internal IR/codegen
**                 contracts (X.680 type model).
** Design Spec:   asn1-toolkit/docs/architecture.md
**                 asn1-toolkit/README.md
***************************************************************************/
#include <asn1/codegen/plan.hpp>
#include <gtest/gtest.h>

TEST(EmissionPlanning, RootPresenceTagsSelectorsAndExtensionKinds) {
  asn1::ir::SequenceDesc descriptor;
  asn1::ir::Field mandatory, optional, defaulted;
  mandatory.name = "id";
  optional.name = "payload";
  optional.presence = asn1::ir::Presence::Optional;
  optional.tag = {asn1::ir::TagClass::Context,7,true};
  optional.table_constraint = asn1::ir::TableConstraintInfo{};
  optional.table_constraint->governor_field_name = "id";
  defaulted.presence = asn1::ir::Presence::Default;
  descriptor.root = {mandatory,optional};
  descriptor.trailing_root = {defaulted};
  descriptor.extensible = true;
  descriptor.extension_groups = {{{optional},false},{{mandatory,defaulted},true}};
  const auto plan = asn1::codegen::planning::sequence(descriptor);
  ASSERT_EQ(plan.root.size(),3u);
  EXPECT_FALSE(plan.root[0].presence_index);
  EXPECT_EQ(plan.root[1].presence_index,0u);
  EXPECT_EQ(plan.root[2].presence_index,1u);
  EXPECT_EQ(plan.presence_fields[1],&descriptor.trailing_root[0]);
  EXPECT_EQ(plan.root[1].effective_tag.number,7u);
  EXPECT_TRUE(plan.root[1].effective_tag.is_explicit);
  ASSERT_NE(plan.root[1].selector,nullptr);
  EXPECT_EQ(plan.root[1].selector->governor_field_name,"id");
  ASSERT_EQ(plan.extensions.size(),2u);
  EXPECT_FALSE(plan.extensions[0].nested_sequence);
  EXPECT_TRUE(plan.extensions[1].nested_sequence);
  EXPECT_TRUE(plan.extensible);
}
TEST(EmissionPlanning, ConstraintKeepsDisjointRangesAndExtensibility) {
  asn1::ir::ConstraintDesc descriptor;
  descriptor.root_ranges = {{asn1::ir::BigInt::from_i64(0),asn1::ir::BigInt::from_i64(2)},
                            {asn1::ir::BigInt::from_i64(7),asn1::ir::BigInt::from_i64(9)}};
  descriptor.lower=asn1::ir::BigInt::from_i64(0);
  descriptor.upper=asn1::ir::BigInt::from_i64(9);
  const auto plan=asn1::codegen::planning::constraint(descriptor);
  EXPECT_EQ(plan.descriptor,&descriptor);
  EXPECT_EQ(plan.descriptor->root_ranges.size(),2u);
  EXPECT_TRUE(plan.requires_membership);
  descriptor.extensible=true;
  EXPECT_FALSE(asn1::codegen::planning::constraint(descriptor).requires_membership);
}
