/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/tests/codegen/split_cycle_test.cpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   GoogleTest exercising split cycle behaviour.
**
** Specification: Validates toolchain output against internal IR/codegen
**                 contracts (X.680 type model).
** Design Spec:   asn1-toolkit/docs/architecture.md
**                 asn1-toolkit/README.md
***************************************************************************/
#include "generated.hpp"
#include <gtest/gtest.h>
TEST(SplitModules, DependencyCycleMergesUnitAndCallsBothNamespaces) {
  asn1_gen::A::Alpha value{};
  value.beta.leaf.value = 7;
  asn1::BitWriter writer;
  ASSERT_TRUE(asn1_gen::A::encode_uper(writer,value));
  EXPECT_EQ(writer.buffer(),(std::vector<std::uint8_t>{7}));
  asn1::BitReader reader(writer.buffer());
  asn1_gen::A::Alpha decoded{};
  ASSERT_TRUE(asn1_gen::A::decode_uper(reader,decoded));
  EXPECT_EQ(decoded.beta.leaf.value,7);
}
