#include "generated.hpp"

#include <asn1/runtime/bit_io.hpp>

#include <cstdint>
#include <string>

#include <gtest/gtest.h>

TEST(CodegenRoundTrip, PersonUper) {
  asn1_gen::Person person;
  person.id = 42;
  person.name = "Ada";
  person.age = 36;

  asn1::BitWriter w;
  auto enc = asn1_gen::encode_uper(w, person);
  ASSERT_TRUE(enc.ok());

  asn1::BitReader r(w.take());
  auto dec = asn1_gen::decode_uper(r);
  ASSERT_TRUE(dec.ok());
  EXPECT_EQ(dec.value().id, 42);
  EXPECT_EQ(dec.value().name, "Ada");
  ASSERT_TRUE(dec.value().age.has_value());
  EXPECT_EQ(*dec.value().age, 36);
}
