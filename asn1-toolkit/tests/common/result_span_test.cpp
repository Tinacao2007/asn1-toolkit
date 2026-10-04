/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/tests/common/result_span_test.cpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   GoogleTest exercising result span behaviour.
**
** Specification: No external protocol; tests for shared C++ helpers.
** Design Spec:   asn1-toolkit/docs/ARCHITECTURE.md
**                 asn1-toolkit/README.md
***************************************************************************/
#include <asn1/common/result.hpp>
#include <asn1/common/span.hpp>

#include <cstdint>
#include <string>
#include <vector>

#include <gtest/gtest.h>

TEST(Result, SuccessHoldsValue) {
  asn1::Result<int> r = asn1::Result<int>::success(42);
  ASSERT_TRUE(r.ok());
  EXPECT_EQ(r.value(), 42);
}

TEST(Result, FailureHoldsError) {
  asn1::Error err;
  err.code = asn1::Error::Code::ConstraintViolation;
  err.offset = 7;
  err.message = "value 300 violates constraint INTEGER (0..255)";

  asn1::Result<int> r = asn1::Result<int>::failure(err);
  ASSERT_FALSE(r.ok());
  EXPECT_EQ(r.error().code, asn1::Error::Code::ConstraintViolation);
  EXPECT_EQ(r.error().offset, 7u);
  EXPECT_EQ(r.error().message, err.message);
}

TEST(ResultVoid, SuccessAndFailure) {
  auto ok = asn1::Result<void>::success();
  EXPECT_TRUE(ok.ok());

  asn1::Error err;
  err.code = asn1::Error::Code::Truncated;
  err.message = "unexpected end of input";
  auto bad = asn1::Result<void>::failure(err);
  ASSERT_FALSE(bad.ok());
  EXPECT_EQ(bad.error().code, asn1::Error::Code::Truncated);
}

TEST(Span, ViewsVectorAndSubspan) {
  std::vector<std::uint8_t> bytes{0x01, 0x02, 0x03, 0x04};
  asn1::Span<const std::uint8_t> view(bytes);

  ASSERT_EQ(view.size(), 4u);
  EXPECT_EQ(view[0], 0x01);
  EXPECT_EQ(view[3], 0x04);

  auto mid = view.subspan(1, 2);
  ASSERT_EQ(mid.size(), 2u);
  EXPECT_EQ(mid[0], 0x02);
  EXPECT_EQ(mid[1], 0x03);
}

TEST(Span, EmptyDefault) {
  asn1::Span<int> empty;
  EXPECT_TRUE(empty.empty());
  EXPECT_EQ(empty.size(), 0u);
  EXPECT_EQ(empty.data(), nullptr);
}
