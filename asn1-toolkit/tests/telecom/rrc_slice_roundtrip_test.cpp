#include "generated.hpp"

#include <asn1/runtime/bit_io.hpp>

#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

namespace {

std::vector<std::uint8_t> finish(asn1::BitWriter& w) {
  w.align_to_octet();
  return w.take();
}

void expect_bytes(const std::vector<std::uint8_t>& got,
                  const std::vector<std::uint8_t>& want) {
  ASSERT_EQ(got.size(), want.size());
  for (std::size_t i = 0; i < got.size(); ++i) {
    EXPECT_EQ(got[i], want[i]) << "byte " << i;
  }
}

asn1::per::BitStringValue bits(std::vector<std::uint8_t> data, std::size_t nbits) {
  asn1::per::BitStringValue v;
  v.bits = std::move(data);
  v.bit_length = nbits;
  return v;
}

}  // namespace

// Hex vectors from Python_asn1tools tests/test_uper.py::test_rrc_8_6_0.

TEST(TelecomRoundTrip, BcchBchMessageUper) {
  asn1_gen::BCCH_BCH_Message msg{};
  msg.message.dl_Bandwidth = 0;                 // n6
  msg.message.phich_Config.phich_Duration = 0;  // normal
  msg.message.phich_Config.phich_Resource = 1;   // half
  msg.message.systemFrameNumber = bits({0x12}, 8);
  msg.message.spare = bits({0x34, 0x40}, 10);

  asn1::BitWriter w;
  ASSERT_TRUE(asn1_gen::encode_uper(w, msg).ok());
  const auto encoded = finish(w);
  expect_bytes(encoded, {0x04, 0x48, 0xd1});

  asn1::BitReader r(encoded);
  asn1_gen::BCCH_BCH_Message got{};
  ASSERT_TRUE(asn1_gen::decode_uper(r, got).ok());
  EXPECT_EQ(got.message.dl_Bandwidth, 0);
  EXPECT_EQ(got.message.phich_Config.phich_Duration, 0);
  EXPECT_EQ(got.message.phich_Config.phich_Resource, 1);
  ASSERT_EQ(got.message.systemFrameNumber.bit_length, 8u);
  ASSERT_FALSE(got.message.systemFrameNumber.bits.empty());
  EXPECT_EQ(got.message.systemFrameNumber.bits[0], 0x12);
  EXPECT_EQ(got.message.spare.bit_length, 10u);
}

TEST(TelecomRoundTrip, PcchPagingEmptyUper) {
  asn1_gen::PCCH_Message msg{};
  asn1_gen::PCCH_MessageType::c1_ c1{};
  asn1_gen::PCCH_MessageType_c1::paging_ paging{};
  c1.value.alt = paging;
  msg.message.alt = c1;

  asn1::BitWriter w;
  ASSERT_TRUE(asn1_gen::encode_uper(w, msg).ok());
  const auto encoded = finish(w);
  expect_bytes(encoded, {0x00});

  asn1::BitReader r(encoded);
  asn1_gen::PCCH_Message got{};
  ASSERT_TRUE(asn1_gen::decode_uper(r, got).ok());
  ASSERT_EQ(got.message.alt.index(), 0u);
  const auto& got_c1 = std::get<asn1_gen::PCCH_MessageType::c1_>(got.message.alt);
  ASSERT_EQ(got_c1.value.alt.index(), 0u);
  const auto& got_paging =
      std::get<asn1_gen::PCCH_MessageType_c1::paging_>(got_c1.value.alt).value;
  EXPECT_FALSE(got_paging.systemInfoModification.has_value());
  EXPECT_FALSE(got_paging.pagingRecordList.has_value());
}

TEST(TelecomRoundTrip, PcchPagingSystemInfoModificationUper) {
  asn1_gen::PCCH_Message msg{};
  asn1_gen::PCCH_MessageType::c1_ c1{};
  asn1_gen::PCCH_MessageType_c1::paging_ paging{};
  paging.value.systemInfoModification = asn1_gen::SystemInfoModification::true_;
  paging.value.nonCriticalExtension = asn1_gen::EmptySeq{};
  c1.value.alt = paging;
  msg.message.alt = c1;

  asn1::BitWriter w;
  ASSERT_TRUE(asn1_gen::encode_uper(w, msg).ok());
  const auto encoded = finish(w);
  expect_bytes(encoded, {0x28});

  asn1::BitReader r(encoded);
  asn1_gen::PCCH_Message got{};
  ASSERT_TRUE(asn1_gen::decode_uper(r, got).ok());
  const auto& got_c1 = std::get<asn1_gen::PCCH_MessageType::c1_>(got.message.alt);
  const auto& got_paging =
      std::get<asn1_gen::PCCH_MessageType_c1::paging_>(got_c1.value.alt).value;
  ASSERT_TRUE(got_paging.systemInfoModification.has_value());
  EXPECT_EQ(*got_paging.systemInfoModification, asn1_gen::SystemInfoModification::true_);
  ASSERT_TRUE(got_paging.nonCriticalExtension.has_value());
}
