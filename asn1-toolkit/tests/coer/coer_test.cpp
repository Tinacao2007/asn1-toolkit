#include <asn1/runtime/coer/codec.hpp>
#include <asn1/runtime/byte_io.hpp>

#include <cstdint>
#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace {

std::vector<std::uint8_t> bytes(std::initializer_list<std::uint8_t> b) {
  return std::vector<std::uint8_t>(b);
}

void expect_eq(const std::vector<std::uint8_t>& got, const std::vector<std::uint8_t>& want) {
  ASSERT_EQ(got.size(), want.size());
  for (std::size_t i = 0; i < got.size(); ++i) {
    EXPECT_EQ(got[i], want[i]) << "byte " << i;
  }
}

template <typename Encode, typename Decode, typename T>
void roundtrip(Encode enc, Decode dec, const T& value,
               const std::vector<std::uint8_t>& expected) {
  asn1::ByteWriter w;
  enc(w, value);
  expect_eq(w.buffer(), expected);
  asn1::ByteReader r(w.buffer());
  auto got = dec(r);
  ASSERT_TRUE(got.ok()) << got.error().message;
  EXPECT_EQ(got.value(), value);
  EXPECT_TRUE(r.eof());
}

}  // namespace

TEST(Coer, BooleanCanonical) {
  roundtrip(
      [](asn1::ByteWriter& w, bool v) { asn1::coer::encode_boolean(w, v); },
      [](asn1::ByteReader& r) { return asn1::coer::decode_boolean(r); }, true, bytes({0xFF}));
  roundtrip(
      [](asn1::ByteWriter& w, bool v) { asn1::coer::encode_boolean(w, v); },
      [](asn1::ByteReader& r) { return asn1::coer::decode_boolean(r); }, false, bytes({0x00}));

  // BASIC-OER allows any non-zero TRUE; COER rejects.
  auto bad = bytes({0x01});
  asn1::ByteReader r(bad);
  auto v = asn1::coer::decode_boolean(r);
  ASSERT_FALSE(v.ok());
  EXPECT_EQ(v.error().code, asn1::Error::Code::NonCanonical);
}

TEST(Coer, LengthRejectsNonShortest) {
  // Long form for length 5 — forbidden in COER.
  auto bad = bytes({0x81, 0x05});
  asn1::ByteReader r(bad);
  auto len = asn1::coer::decode_length(r);
  ASSERT_FALSE(len.ok());
  EXPECT_EQ(len.error().code, asn1::Error::Code::NonCanonical);

  // Leading zero in long form.
  auto bad2 = bytes({0x82, 0x00, 0xC8});
  asn1::ByteReader r2(bad2);
  auto len2 = asn1::coer::decode_length(r2);
  ASSERT_FALSE(len2.ok());
  EXPECT_EQ(len2.error().code, asn1::Error::Code::NonCanonical);

  // Canonical long form for 200.
  asn1::ByteWriter w;
  asn1::coer::encode_length(w, 200);
  expect_eq(w.buffer(), bytes({0x81, 0xC8}));
  asn1::ByteReader r3(w.buffer());
  auto ok = asn1::coer::decode_length(r3);
  ASSERT_TRUE(ok.ok());
  EXPECT_EQ(ok.value(), 200u);
}

TEST(Coer, IntegerUnconstrainedVectors) {
  const asn1::coer::IntegerConstraint unc{};
  auto enc = [&](asn1::ByteWriter& w, std::int64_t v) {
    asn1::coer::encode_integer(w, v, unc);
  };
  auto dec = [&](asn1::ByteReader& r) { return asn1::coer::decode_integer(r, unc); };

  roundtrip(enc, dec, 0, bytes({0x01, 0x00}));
  roundtrip(enc, dec, 128, bytes({0x02, 0x00, 0x80}));
  roundtrip(enc, dec, -255, bytes({0x02, 0xFF, 0x01}));

  // Non-minimal positive INTEGER (extra leading 0x00).
  auto bad = bytes({0x03, 0x00, 0x00, 0x80});
  asn1::ByteReader r(bad);
  auto v = asn1::coer::decode_integer(r, unc);
  ASSERT_FALSE(v.ok());
  EXPECT_EQ(v.error().code, asn1::Error::Code::NonCanonical);
}

TEST(Coer, IntegerFixedWidth) {
  asn1::coer::IntegerConstraint f{0, 255, false};
  asn1::ByteWriter w;
  asn1::coer::encode_integer(w, 128, f);
  expect_eq(w.buffer(), bytes({0x80}));
  asn1::ByteReader r(w.buffer());
  auto v = asn1::coer::decode_integer(r, f);
  ASSERT_TRUE(v.ok());
  EXPECT_EQ(v.value(), 128);
}

TEST(Coer, OctetAndUtf8) {
  asn1::ByteWriter w;
  auto data = bytes({0x12, 0x34});
  asn1::coer::encode_octet_string(w, data);
  expect_eq(w.buffer(), bytes({0x02, 0x12, 0x34}));
  asn1::ByteReader r(w.buffer());
  auto got = asn1::coer::decode_octet_string(r);
  ASSERT_TRUE(got.ok());
  expect_eq(got.value(), data);

  asn1::ByteWriter w2;
  asn1::coer::encode_utf8_string(w2, "ABC");
  expect_eq(w2.buffer(), bytes({0x03, 'A', 'B', 'C'}));
}

TEST(Coer, BitStringClearsAndRejectsDirtyUnused) {
  asn1::ByteWriter w;
  auto bits = bytes({0xF1});
  asn1::coer::encode_bit_string(w, bits, 4);
  expect_eq(w.buffer(), bytes({0x02, 0x04, 0xF0}));

  auto dirty = bytes({0x02, 0x04, 0xF1});
  asn1::ByteReader r(dirty);
  auto v = asn1::coer::decode_bit_string(r);
  ASSERT_FALSE(v.ok());
  EXPECT_EQ(v.error().code, asn1::Error::Code::NonCanonical);
}

TEST(Coer, EnumeratedShortAndLong) {
  asn1::ByteWriter w;
  asn1::coer::encode_enumerated(w, 3);
  expect_eq(w.buffer(), bytes({0x03}));

  asn1::ByteWriter w2;
  asn1::coer::encode_enumerated(w2, 128);
  expect_eq(w2.buffer(), bytes({0x82, 0x00, 0x80}));
  asn1::ByteReader r2(w2.buffer());
  auto got2 = asn1::coer::decode_enumerated(r2);
  ASSERT_TRUE(got2.ok()) << got2.error().message;
  EXPECT_EQ(got2.value(), 128);

  // Long form for value 3 — forbidden.
  auto bad = bytes({0x81, 0x03});
  asn1::ByteReader r(bad);
  auto v = asn1::coer::decode_enumerated(r);
  ASSERT_FALSE(v.ok());
  EXPECT_EQ(v.error().code, asn1::Error::Code::NonCanonical);
}

TEST(Coer, SequencePreamblePaddingMustBeZero) {
  asn1::ByteWriter w;
  bool opts[] = {true, false};
  asn1::coer::encode_sequence_preamble(w, false, false, opts);
  expect_eq(w.buffer(), bytes({0x80}));

  // Dirty padding in low 6 bits.
  auto dirty = bytes({0x83});
  asn1::ByteReader r(dirty);
  auto got = asn1::coer::decode_sequence_preamble(r, false, 2);
  ASSERT_FALSE(got.ok());
  EXPECT_EQ(got.error().code, asn1::Error::Code::NonCanonical);
}

TEST(Coer, SequenceExtensionAdditions) {
  asn1::ByteWriter w;
  asn1::coer::encode_sequence_preamble(w, true, true,
                                       asn1::Span<const bool>(nullptr, 0));
  asn1::coer::encode_boolean(w, true);
  const bool ep[] = {true, false};
  std::vector<std::vector<std::uint8_t>> ots;
  {
    asn1::ByteWriter ow;
    asn1::coer::encode_boolean(ow, true);
    ots.push_back(ow.take());
  }
  asn1::coer::encode_extension_additions(w, ep, ots);

  asn1::ByteReader r(w.buffer());
  auto pre = asn1::coer::decode_sequence_preamble(r, true, 0);
  ASSERT_TRUE(pre.ok());
  EXPECT_TRUE(pre.value().extensions_present);
  auto a = asn1::coer::decode_boolean(r);
  ASSERT_TRUE(a.ok());
  EXPECT_TRUE(a.value());
  auto ext = asn1::coer::decode_extension_additions(r);
  ASSERT_TRUE(ext.ok()) << ext.error().message;
  ASSERT_EQ(ext.value().presence.size(), 2u);
  EXPECT_TRUE(ext.value().presence[0]);
  EXPECT_FALSE(ext.value().presence[1]);
  ASSERT_EQ(ext.value().open_types.size(), 1u);
}

TEST(Coer, SetOfSortsAndValidatesOrder) {
  std::vector<std::vector<std::uint8_t>> comps = {
      bytes({0x02, 0x02}),
      bytes({0x01, 0x01}),
      bytes({0x03}),
  };
  asn1::ByteWriter w;
  asn1::coer::encode_set_of(w, comps);
  // Sorted: {01 01}, {02 02}, {03}; quantity 3
  expect_eq(w.buffer(), bytes({0x03, 0x01, 0x01, 0x02, 0x02, 0x03}));

  std::vector<std::vector<std::uint8_t>> ordered = {
      bytes({0x01}),
      bytes({0x02}),
  };
  ASSERT_TRUE(asn1::coer::require_set_of_order(ordered).ok());

  std::vector<std::vector<std::uint8_t>> unordered = {
      bytes({0x02}),
      bytes({0x01}),
  };
  auto bad = asn1::coer::require_set_of_order(unordered);
  ASSERT_FALSE(bad.ok());
  EXPECT_EQ(bad.error().code, asn1::Error::Code::NonCanonical);
}

TEST(Coer, ChoiceAndOidRoundTrip) {
  asn1::ByteWriter w;
  asn1::coer::encode_choice_tag(w, 1, false);
  expect_eq(w.buffer(), bytes({0x81}));
  asn1::ByteReader r(w.buffer());
  auto tag = asn1::coer::decode_choice_tag(r);
  ASSERT_TRUE(tag.ok());
  EXPECT_EQ(tag.value(), 1u);

  std::uint64_t arcs[] = {1, 2, 840, 113549};
  asn1::ByteWriter w2;
  asn1::coer::encode_object_identifier(w2, arcs);
  asn1::ByteReader r2(w2.buffer());
  auto oid = asn1::coer::decode_object_identifier(r2);
  ASSERT_TRUE(oid.ok()) << oid.error().message;
  ASSERT_EQ(oid.value().size(), 4u);
  EXPECT_EQ(oid.value()[0], 1u);
  EXPECT_EQ(oid.value()[1], 2u);
  EXPECT_EQ(oid.value()[2], 840u);
  EXPECT_EQ(oid.value()[3], 113549u);
}

TEST(Coer, NullAndSequenceOfLength) {
  asn1::ByteWriter w;
  asn1::coer::encode_null(w);
  EXPECT_TRUE(w.buffer().empty());

  asn1::ByteWriter w2;
  asn1::coer::encode_sequence_of_length(w2, 4);
  expect_eq(w2.buffer(), bytes({0x01, 0x04}));
  asn1::ByteReader r(w2.buffer());
  auto n = asn1::coer::decode_sequence_of_length(r);
  ASSERT_TRUE(n.ok());
  EXPECT_EQ(n.value(), 4u);
}

TEST(Coer, RealBinary32And64) {
  using Form = asn1::coer::RealIeeeForm;
  {
    asn1::ByteWriter w;
    ASSERT_TRUE(asn1::coer::encode_real(w, 1.0, Form::Binary32).ok());
    expect_eq(w.buffer(), bytes({0x3F, 0x80, 0x00, 0x00}));
    asn1::ByteReader r(w.buffer());
    auto got = asn1::coer::decode_real(r, Form::Binary32);
    ASSERT_TRUE(got.ok());
    EXPECT_EQ(got.value(), 1.0);
  }
  {
    asn1::ByteWriter w;
    ASSERT_TRUE(asn1::coer::encode_real(w, 1.0, Form::Binary64).ok());
    expect_eq(w.buffer(), bytes({0x3F, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}));
    asn1::ByteReader r(w.buffer());
    auto got = asn1::coer::decode_real(r, Form::Binary64);
    ASSERT_TRUE(got.ok());
    EXPECT_EQ(got.value(), 1.0);
  }
}
