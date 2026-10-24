#include "generated.hpp"
#include <asn1/runtime/bit_string.hpp>
#include <gtest/gtest.h>
#ifdef CGEN2_CPP_SPLIT
namespace generated = asn1_gen::CppDefaults;
#else
namespace generated = asn1_gen;
#endif
TEST(CGenBatch2, CppBinaryDefaultCompatibility) {
  generated::Record value;
  ASSERT_EQ(value.flags.bit_length,8u);
  EXPECT_EQ(value.flags.bits,(std::vector<std::uint8_t>{0xff}));
  EXPECT_EQ(value.octets,(std::vector<std::uint8_t>{0xab,0xcd}));
  EXPECT_EQ(value.text,"demo");
  asn1::BitWriter writer;
  ASSERT_TRUE(generated::encode_uper(writer,value));
  EXPECT_EQ(writer.buffer(),(std::vector<std::uint8_t>{0}));
  generated::Record decoded;
  asn1::BitReader reader(writer.buffer());
  ASSERT_TRUE(generated::decode_uper(reader,decoded));
  EXPECT_EQ(decoded.flags,value.flags);
  EXPECT_EQ(decoded.octets,value.octets);
  value.flags.bits[0]=0;
  asn1::BitWriter changed;
  ASSERT_TRUE(generated::encode_uper(changed,value));
  EXPECT_EQ(changed.buffer(),(std::vector<std::uint8_t>{0x80,0}));
  asn1::BitReader changed_reader(changed.buffer());
  ASSERT_TRUE(generated::decode_uper(changed_reader,decoded));
  EXPECT_EQ(decoded.flags.bits[0],0);
  asn1::BitStringValue a{{0xa0},3}, b{{0xbf,0xff},3};
  EXPECT_EQ(a,b);
  b.bit_length=4; EXPECT_NE(a,b);
}
