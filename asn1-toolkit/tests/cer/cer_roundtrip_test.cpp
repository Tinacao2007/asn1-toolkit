/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/tests/cer/cer_roundtrip_test.cpp
**   $Version: 0.1
**   $Date:   2026-10-14
**   $Author: tina.cao
**
**  File Description:
**
**   GoogleTest exercising CER (X.690) encoding and decoding of the
**   cer_golden.asn fixture, pinning the byte-level CER rules:
**     - 9.1: constructed encodings use the indefinite length form
**       (30 80 ... 00 00), including CHOICE alternatives;
**     - 9.2: OCTET STRINGs over 1000 contents octets are fragmented into
**       primitive fragments of at most 1000 contents octets;
**     - 8.6.4: BIT STRING fragments carry their own initial (unused-bits)
**       octet inside the 1000-octet fragment budget;
**     - 9.3/11.6: SET components ordered by tag (X.680 8.6);
**       SET OF components ordered by encoding as octet strings;
**     - 9.3: a SET component that is an untagged CHOICE is ordered by
**       the SMALLEST tag in the choice, computed statically by the emitter;
**     - 11.2: unused bits of the last octet are zeroed;
**     - canonical strictness: the definite form is rejected where CER
**       requires the indefinite form.
**
**   No external CER oracle exists (asn1c v0.9.29 and asn1tools have no CER
**   codec), so the golden vectors are hand-derived from the X.690 clause
**   text and the DER equivalence cross-check pins value-level correctness.
**
** Specification: ITU-T X.690 (CER), X.680
** Design Spec:   asn1-toolkit/docs/architecture.md
**                 asn1-toolkit/README.md
**************************************************************************/

#include "generated.hpp"

#include <asn1/runtime/byte_io.hpp>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace {

namespace gen = asn1_cer;

using gen::BitFrag;
using gen::C;
using gen::C_seq;
using gen::ChoiceB;
using gen::Frag;
using gen::SetC;
using gen::SetOfI;
using gen::SetS;
using gen::Small;

std::vector<std::uint8_t> pattern(std::size_t n) {
  std::vector<std::uint8_t> v(n);
  for (std::size_t i = 0; i < n; ++i) {
    v[i] = static_cast<std::uint8_t>((i * 7 + 3) & 0xFF);
  }
  return v;
}

}  // namespace

// X.690 9.1: SEQUENCE encoded in the indefinite length form.
TEST(CerRoundTrip, SmallIndefiniteWrapper) {
  Small t;
  t.i = asn1::BigInteger::from_i64(7);
  t.b = true;

  asn1::ByteWriter w;
  ASSERT_TRUE(gen::encode_cer(w, t).ok());
  const auto bytes = w.take();

  // 30 80 | 02 01 07 | 01 01 FF | 00 00
  const std::vector<std::uint8_t> expected = {0x30, 0x80, 0x02, 0x01, 0x07,
                                              0x01, 0x01, 0xFF, 0x00, 0x00};
  EXPECT_EQ(bytes, expected);

  asn1::ByteReader r(bytes);
  Small got{};
  ASSERT_TRUE(gen::decode_cer(r, got).ok());
  EXPECT_TRUE(got.i == t.i);
  EXPECT_EQ(got.b, t.b);
}

// X.690 9.3 + X.680 8.6: SET components in ascending tag order. The fixture
// declares `second` (INTEGER, tag 02) before `first` (BOOLEAN, tag 01), so
// the encoded order must put the BOOLEAN first.
TEST(CerRoundTrip, SetComponentTagOrder) {
  SetS t;
  t.second = asn1::BigInteger::from_i64(7);
  t.first = true;

  asn1::ByteWriter w;
  ASSERT_TRUE(gen::encode_cer(w, t).ok());
  const auto bytes = w.take();

  // 31 80 | 01 01 FF (BOOLEAN first) | 02 01 07 (INTEGER second) | 00 00
  const std::vector<std::uint8_t> expected = {0x31, 0x80, 0x01, 0x01, 0xFF,
                                              0x02, 0x01, 0x07, 0x00, 0x00};
  EXPECT_EQ(bytes, expected);

  asn1::ByteReader r(bytes);
  SetS got{};
  ASSERT_TRUE(gen::decode_cer(r, got).ok());
  EXPECT_TRUE(got.second == t.second);
  EXPECT_EQ(got.first, t.first);
}

// A primitive CHOICE alternative carries no wrapper at all.
TEST(CerRoundTrip, ChoicePrimitiveAlternative) {
  C c;
  c.alt = C::num_{asn1::BigInteger::from_i64(7)};

  asn1::ByteWriter w;
  ASSERT_TRUE(gen::encode_cer(w, c).ok());
  const auto bytes = w.take();

  const std::vector<std::uint8_t> expected = {0x02, 0x01, 0x07};
  EXPECT_EQ(bytes, expected);

  asn1::ByteReader r(bytes);
  C got{};
  ASSERT_TRUE(gen::decode_cer(r, got).ok());
  ASSERT_EQ(got.alt.index(), 0u);
  EXPECT_TRUE(std::get<C::num_>(got.alt).value == asn1::BigInteger::from_i64(7));
}

// X.690 9.1: a constructed CHOICE alternative arrives indefinite; the
// generated decoder dispatches on the tag WITHOUT consuming the header and
// then decodes the complete TLV in place.
TEST(CerRoundTrip, ChoiceConstructedAlternativeIndefinite) {
  C c;
  c.alt = C::seq_{C_seq{asn1::BigInteger::from_i64(5)}};

  asn1::ByteWriter w;
  ASSERT_TRUE(gen::encode_cer(w, c).ok());
  const auto bytes = w.take();

  // 30 80 | 02 01 05 | 00 00
  const std::vector<std::uint8_t> expected = {0x30, 0x80, 0x02, 0x01, 0x05,
                                              0x00, 0x00};
  EXPECT_EQ(bytes, expected);

  asn1::ByteReader r(bytes);
  C got{};
  ASSERT_TRUE(gen::decode_cer(r, got).ok());
  ASSERT_EQ(got.alt.index(), 1u);
  EXPECT_TRUE(std::get<C::seq_>(got.alt).value.x == asn1::BigInteger::from_i64(5));
}

// X.690 9.2: an OCTET STRING of 1500 octets becomes an indefinite
// constructed value holding two primitive fragments of 1000 and 500 octets.
TEST(CerRoundTrip, OctetStringFragmentation) {
  Frag t;
  t.n = asn1::BigInteger::from_i64(1);
  t.s = pattern(1500);

  asn1::ByteWriter w;
  ASSERT_TRUE(gen::encode_cer(w, t).ok());
  const auto bytes = w.take();

  const auto all = pattern(1500);
  const auto frag1 = std::vector<std::uint8_t>(all.begin(), all.begin() + 1000);
  const auto frag2 = std::vector<std::uint8_t>(all.begin() + 1000, all.end());
  std::vector<std::uint8_t> expected;
  const auto push = [&expected](std::initializer_list<std::uint8_t> b) {
    expected.insert(expected.end(), b.begin(), b.end());
  };
  push({0x30, 0x80});                // SEQUENCE, indefinite
  push({0x02, 0x01, 0x01});          // n = 1
  push({0x24, 0x80});                // OCTET STRING, constructed indefinite
  push({0x04, 0x82, 0x03, 0xE8});    // fragment 1: 1000 contents octets
  expected.insert(expected.end(), frag1.begin(), frag1.end());
  push({0x04, 0x82, 0x01, 0xF4});    // fragment 2: 500 contents octets (0x01F4)
  expected.insert(expected.end(), frag2.begin(), frag2.end());
  push({0x00, 0x00});                // end of fragment list
  push({0x00, 0x00});                // end of SEQUENCE
  EXPECT_EQ(bytes, expected);

  asn1::ByteReader r(bytes);
  Frag got{};
  ASSERT_TRUE(gen::decode_cer(r, got).ok());
  EXPECT_TRUE(got.n == t.n);
  EXPECT_EQ(got.s, t.s);
}

// X.690 8.6.4 + 9.2: a BIT STRING of 8100 bits (1013 data octets, 4 unused
// bits) is fragmented as 999 + 14 data octets; each fragment's own initial
// (unused-bits) octet counts inside its 1000 contents octets. X.690 11.2:
// the 4 unused bits of the final octet are zeroed in the encoding.
TEST(CerRoundTrip, BitStringFragmentation) {
  BitFrag t;
  t.n = asn1::BigInteger::from_i64(2);
  t.b.bits = pattern(1013);
  t.b.bits.back() = 0xA0;  // low nibble already zero (X.690 11.2)
  t.b.bit_length = 8100;

  asn1::ByteWriter w;
  ASSERT_TRUE(gen::encode_cer(w, t).ok());
  const auto bytes = w.take();

  const auto bits1 = std::vector<std::uint8_t>(t.b.bits.begin(), t.b.bits.begin() + 999);
  const auto bits2 = std::vector<std::uint8_t>(t.b.bits.begin() + 999, t.b.bits.end());
  std::vector<std::uint8_t> expected;
  const auto push = [&expected](std::initializer_list<std::uint8_t> b) {
    expected.insert(expected.end(), b.begin(), b.end());
  };
  push({0x30, 0x80});                // SEQUENCE, indefinite
  push({0x02, 0x01, 0x02});          // n = 2
  push({0x23, 0x80});                // BIT STRING, constructed indefinite
  push({0x03, 0x82, 0x03, 0xE8});    // fragment 1: 1000 contents octets
  push({0x00});                      // 0 unused bits
  expected.insert(expected.end(), bits1.begin(), bits1.end());
  push({0x03, 0x0F});               // fragment 2: 15 contents octets (short form)
  push({0x04});                      // 4 unused bits
  expected.insert(expected.end(), bits2.begin(), bits2.end());
  push({0x00, 0x00});                // end of fragment list
  push({0x00, 0x00});                // end of SEQUENCE
  EXPECT_EQ(bytes, expected);

  asn1::ByteReader r(bytes);
  BitFrag got{};
  ASSERT_TRUE(gen::decode_cer(r, got).ok());
  EXPECT_TRUE(got.n == t.n);
  EXPECT_EQ(got.b.bits, t.b.bits);
  EXPECT_EQ(got.b.bit_length, 8100u);
}

// X.690 9.3: SET OF components appear in ascending order, compared as
// octet strings (shorter padded with trailing 0-octets). All elements share
// the INTEGER tag, so the order is the plain ascending encoding order —
// including the longer encoding of 256 sorting after 7.
TEST(CerRoundTrip, SetOfOctetOrder) {
  SetOfI t;
  t.value = {asn1::BigInteger::from_i64(7), asn1::BigInteger::from_i64(3),
             asn1::BigInteger::from_i64(256)};

  asn1::ByteWriter w;
  ASSERT_TRUE(gen::encode_cer(w, t).ok());
  const auto bytes = w.take();

  // 31 80 | 02 01 03 | 02 01 07 | 02 02 01 00 | 00 00
  const std::vector<std::uint8_t> expected = {0x31, 0x80, 0x02, 0x01, 0x03,
                                              0x02, 0x01, 0x07, 0x02, 0x02,
                                              0x01, 0x00, 0x00, 0x00};
  EXPECT_EQ(bytes, expected);

  asn1::ByteReader r(bytes);
  SetOfI got{};
  ASSERT_TRUE(gen::decode_cer(r, got).ok());
  ASSERT_EQ(got.value.size(), 3u);
  EXPECT_TRUE(got.value[0] == asn1::BigInteger::from_i64(3));
  EXPECT_TRUE(got.value[1] == asn1::BigInteger::from_i64(7));
  EXPECT_TRUE(got.value[2] == asn1::BigInteger::from_i64(256));
}

// X.690 9.3 + X.680 8.6: a SET component that is an untagged CHOICE is
// ordered as though it carried the SMALLEST tag in the choice (BOOLEAN,
// 01) — not the selected alternative's tag. Selecting the BIT STRING (03)
// alternative must still place the CHOICE ahead of the INTEGER (02);
// the emitter computes the key statically and the runtime must not
// re-sort the pre-ordered parts.
TEST(CerRoundTrip, SetWithUntaggedChoiceSmallestTag) {
  SetC t;
  t.small = asn1::BigInteger::from_i64(7);
  asn1::BitStringValue bi;
  bi.bits = {0x07};
  bi.bit_length = 8;
  t.ch.alt = ChoiceB::bi_{bi};

  asn1::ByteWriter w;
  ASSERT_TRUE(gen::encode_cer(w, t).ok());
  const auto bytes = w.take();

  // 31 80 | 03 02 00 07 (CHOICE alternative first) | 02 01 07 | 00 00
  const std::vector<std::uint8_t> expected = {0x31, 0x80, 0x03, 0x02, 0x00,
                                              0x07, 0x02, 0x01, 0x07, 0x00,
                                              0x00};
  EXPECT_EQ(bytes, expected);

  asn1::ByteReader r(bytes);
  SetC got{};
  ASSERT_TRUE(gen::decode_cer(r, got).ok());
  EXPECT_TRUE(got.small == t.small);
  ASSERT_EQ(got.ch.alt.index(), 1u);
  const auto& bv = std::get<ChoiceB::bi_>(got.ch.alt).value;
  EXPECT_EQ(bv.bits, bi.bits);
  EXPECT_EQ(bv.bit_length, 8u);
}

// CER and DER agree on VALUES; they disagree on bytes (indefinite vs
// definite wrappers). Round-tripping the same value through both rules must
// yield equal values.
TEST(CerRoundTrip, CerDerValueEquivalence) {
  Small t;
  t.i = asn1::BigInteger::from_i64(-42);
  t.b = false;

  asn1::ByteWriter cw;
  ASSERT_TRUE(gen::encode_cer(cw, t).ok());
  const auto cer_bytes = cw.take();

  asn1::ByteWriter dw;
  ASSERT_TRUE(gen::encode_der(dw, t).ok());
  const auto der_bytes = dw.take();

  EXPECT_NE(cer_bytes, der_bytes);
  // DER uses the definite form for the same value.
  const std::vector<std::uint8_t> der_expected = {0x30, 0x06, 0x02, 0x01, 0xD6,
                                                  0x01, 0x01, 0x00};
  EXPECT_EQ(der_bytes, der_expected);

  Small from_cer{};
  asn1::ByteReader cr(cer_bytes);
  ASSERT_TRUE(gen::decode_cer(cr, from_cer).ok());

  Small from_der{};
  asn1::ByteReader dr(der_bytes);
  ASSERT_TRUE(gen::decode_der(dr, from_der).ok());

  EXPECT_TRUE(from_cer.i == t.i);
  EXPECT_TRUE(from_der.i == t.i);
  EXPECT_EQ(from_cer.b, t.b);
  EXPECT_EQ(from_der.b, t.b);
}

// Canonical strictness: DER's definite-length SEQUENCE is not a valid CER
// encoding (X.690 9.1 requires the indefinite form for constructed values).
TEST(CerRoundTrip, DefiniteWrapperRejected) {
  const std::vector<std::uint8_t> der_bytes = {0x30, 0x06, 0x02, 0x01, 0x07,
                                               0x01, 0x01, 0xFF};
  asn1::ByteReader r(der_bytes);
  Small got{};
  const auto res = gen::decode_cer(r, got);
  EXPECT_FALSE(res.ok());
  EXPECT_EQ(res.error().code, asn1::Error::Code::NonCanonical);
}
