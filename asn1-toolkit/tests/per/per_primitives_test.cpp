/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/tests/per/per_primitives_test.cpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   GoogleTest exercising per primitives behaviour.
**
** Specification: ITU-T X.691 — Packed Encoding Rules (PER); test vectors
**                 for UPER/APER/primitives.
** Design Spec:   asn1-toolkit/docs/architecture.md
**                 asn1-toolkit/README.md
***************************************************************************/
#include <asn1/runtime/bit_io.hpp>
#include <asn1/runtime/per/primitives.hpp>
#include <asn1/runtime/per/codec.hpp>
#include <asn1/c_runtime/per.h>

#include <cstdint>
#include <cstdlib>
#include <algorithm>
#include <iterator>
#include <vector>

#include <gtest/gtest.h>

namespace {

std::vector<std::uint8_t> hex(std::initializer_list<std::uint8_t> bytes) {
  return std::vector<std::uint8_t>(bytes);
}

void expect_bytes(const std::vector<std::uint8_t>& got,
                  const std::vector<std::uint8_t>& want) {
  ASSERT_EQ(got.size(), want.size());
  for (std::size_t i = 0; i < got.size(); ++i) {
    EXPECT_EQ(got[i], want[i]) << "byte " << i;
  }
}

}  // namespace

TEST(BitIo, PutGetBitsRoundTrip) {
  asn1::BitWriter w;
  w.put_bits(0xA, 4);  // 1010
  w.put_bits(0x5, 4);  // 0101 -> A5
  auto bytes = w.take();
  expect_bytes(bytes, hex({0xA5}));

  asn1::BitReader r(bytes);
  auto a = r.get_bits(4);
  auto b = r.get_bits(4);
  ASSERT_TRUE(a.ok());
  ASSERT_TRUE(b.ok());
  EXPECT_EQ(a.value(), 0xAu);
  EXPECT_EQ(b.value(), 0x5u);
}

TEST(BitIo, AlignToOctet) {
  asn1::BitWriter w;
  w.put_bits(1, 1);
  EXPECT_FALSE(w.octet_aligned());
  w.align_to_octet();
  EXPECT_TRUE(w.octet_aligned());
  EXPECT_EQ(w.bit_size(), 8u);
  expect_bytes(w.take(), hex({0x80}));

  const auto _bits13 = hex({0x80, 0xFF});
  asn1::BitReader r(_bits13);
  ASSERT_TRUE(r.get_bit().ok());
  ASSERT_TRUE(r.align_to_octet().ok());
  EXPECT_TRUE(r.octet_aligned());
  auto next = r.get_octet();
  ASSERT_TRUE(next.ok());
  EXPECT_EQ(next.value(), 0xFFu);
}

TEST(PerPrimitives, BitsForRange) {
  EXPECT_EQ(asn1::per::bits_for_range(1), 0u);
  EXPECT_EQ(asn1::per::bits_for_range(2), 1u);
  EXPECT_EQ(asn1::per::bits_for_range(8), 3u);
  EXPECT_EQ(asn1::per::bits_for_range(256), 8u);
  EXPECT_EQ(asn1::per::bits_for_range(257), 9u);
}

TEST(PerPrimitives, ConstrainedUperSmall) {
  // INTEGER (0..7) value 5 -> 3 bits 101, padded -> A0
  asn1::BitWriter w;
  ASSERT_TRUE((asn1::per::encode_constrained_whole_number(w, asn1::per::Variant::Unaligned, 5, 0, 7)).ok());
  expect_bytes(w.take(), hex({0xA0}));

  const auto _bits14 = hex({0xA0});
  asn1::BitReader r(_bits14);
  auto v = asn1::per::decode_constrained_whole_number(r, asn1::per::Variant::Unaligned, 0, 7);
  ASSERT_TRUE(v.ok());
  EXPECT_EQ(v.value(), 5);
}

TEST(PerPrimitives, ConstrainedAperVsUperRange300) {
  // range 300 (0..299): UPER uses 9 bits; APER uses aligned 16 bits.
  {
    asn1::BitWriter w;
    ASSERT_TRUE((asn1::per::encode_constrained_whole_number(w, asn1::per::Variant::Unaligned, 100, 0, 299)).ok());
    EXPECT_EQ(w.bit_size(), 9u);
    auto bytes = w.take();
    asn1::BitReader r(bytes);
    auto v =
        asn1::per::decode_constrained_whole_number(r, asn1::per::Variant::Unaligned, 0, 299);
    ASSERT_TRUE(v.ok());
    EXPECT_EQ(v.value(), 100);
  }
  {
    asn1::BitWriter w;
    w.put_bit(true);  // force misalignment
    ASSERT_TRUE((asn1::per::encode_constrained_whole_number(w, asn1::per::Variant::Aligned, 100, 0, 299)).ok());
    // 1 bit + 7 pad + 16 bits = 24 bits
    EXPECT_EQ(w.bit_size(), 24u);
    auto bytes = w.take();
    asn1::BitReader r(bytes);
    ASSERT_TRUE(r.get_bit().ok());
    auto v = asn1::per::decode_constrained_whole_number(r, asn1::per::Variant::Aligned, 0, 299);
    ASSERT_TRUE(v.ok());
    EXPECT_EQ(v.value(), 100);
  }
}

TEST(PerPrimitives, ConstrainedSingleValue) {
  asn1::BitWriter w;
  ASSERT_TRUE((asn1::per::encode_constrained_whole_number(w, asn1::per::Variant::Unaligned, 42, 42, 42)).ok());
  EXPECT_EQ(w.bit_size(), 0u);
  asn1::BitReader r(std::vector<std::uint8_t>{});
  auto v =
      asn1::per::decode_constrained_whole_number(r, asn1::per::Variant::Unaligned, 42, 42);
  ASSERT_TRUE(v.ok());
  EXPECT_EQ(v.value(), 42);
}

TEST(PerPrimitives, SemiAndUnconstrainedRoundTrip) {
  for (asn1::per::Variant v :
       {asn1::per::Variant::Unaligned, asn1::per::Variant::Aligned}) {
    asn1::BitWriter w;
    asn1::per::encode_semi_constrained_whole_number(w, v, 1000, 100);
    auto bytes = w.take();
    asn1::BitReader r(bytes);
    auto got = asn1::per::decode_semi_constrained_whole_number(r, v, 100);
    ASSERT_TRUE(got.ok());
    EXPECT_EQ(got.value(), 1000);

    asn1::BitWriter w2;
    asn1::per::encode_unconstrained_whole_number(w2, v, -129);
    auto bytes2 = w2.take();
    asn1::BitReader r2(bytes2);
    auto got2 = asn1::per::decode_unconstrained_whole_number(r2, v);
    ASSERT_TRUE(got2.ok());
    EXPECT_EQ(got2.value(), -129);
  }
}

TEST(PerPrimitives, NormallySmallAndLength) {
  asn1::BitWriter w;
  asn1::per::encode_normally_small_non_negative_whole_number(
      w, asn1::per::Variant::Unaligned, 3);
  // 0 + 000011 -> 00000110 padded -> 0x06
  expect_bytes(w.take(), hex({0x06}));

  const auto _bits15 = hex({0x06});
  asn1::BitReader r(_bits15);
  auto v = asn1::per::decode_normally_small_non_negative_whole_number(
      r, asn1::per::Variant::Unaligned);
  ASSERT_TRUE(v.ok());
  EXPECT_EQ(v.value(), 3u);

  asn1::BitWriter w2;
  asn1::per::encode_normally_small_length(w2, asn1::per::Variant::Unaligned, 4);
  // length 4 -> 0 + (4-1=3 as 6 bits) = 0000011 -> 0x06
  expect_bytes(w2.take(), hex({0x06}));
  const auto len_bytes = hex({0x06});
  asn1::BitReader r2(len_bytes);
  auto len = asn1::per::decode_normally_small_length(r2, asn1::per::Variant::Unaligned);
  ASSERT_TRUE(len.ok());
  EXPECT_EQ(len.value(), 4u);

  // X.691 11.9.3.4: length > 64 encodes as bit 1 followed by length determinant
  asn1::BitWriter w3;
  asn1::per::encode_normally_small_length(w3, asn1::per::Variant::Unaligned, 70);
  auto b3 = w3.take();
  asn1::BitReader r3(b3);
  auto len3 = asn1::per::decode_normally_small_length(r3, asn1::per::Variant::Unaligned);
  ASSERT_TRUE(len3.ok());
  EXPECT_EQ(len3.value(), 70u);
}

TEST(PerPrimitives, CNormallySmallMatchesCppForBothVariants) {
  for (const auto variant : {asn1::per::Variant::Unaligned, asn1::per::Variant::Aligned}) {
    const auto c_variant = variant == asn1::per::Variant::Aligned
        ? ASN1_PER_ALIGNED : ASN1_PER_UNALIGNED;
    const std::uint64_t values[] = {0u, 63u, 64u, 255u, 256u, UINT64_C(0x100000000), UINT64_MAX};
    for (const std::uint64_t value : values) {
      asn1::BitWriter cpp_writer;
      asn1::per::encode_normally_small_non_negative_whole_number(cpp_writer, variant, value);
      const auto cpp_bytes = cpp_writer.take();

      asn1_bit_writer_t* c_writer = nullptr;
      ASSERT_EQ(asn1_bit_writer_create(&c_writer), ASN1_OK);
      ASSERT_EQ(asn1_per_encode_normally_small_uint_ex(c_writer, c_variant, value), ASN1_OK);
      std::uint8_t* c_bytes = nullptr;
      std::size_t c_size = 0;
      ASSERT_EQ(asn1_bit_writer_take(c_writer, &c_bytes, &c_size), ASN1_OK);
      ASSERT_EQ(c_size, cpp_bytes.size());
      EXPECT_TRUE(std::equal(cpp_bytes.begin(), cpp_bytes.end(), c_bytes));
      std::free(c_bytes);
      asn1_bit_writer_destroy(c_writer);
    }
  }
}

TEST(PerPrimitives, CEnumeratedOrdinalsMatchCppForBothVariants) {
  for (const auto variant : {asn1::per::Variant::Unaligned, asn1::per::Variant::Aligned}) {
    const auto c_variant = variant == asn1::per::Variant::Aligned
        ? ASN1_PER_ALIGNED : ASN1_PER_UNALIGNED;
    for (std::size_t ordinal = 0; ordinal < 5; ++ordinal) {
      asn1::BitWriter cpp_writer;
      asn1::per::encode_enumerated(cpp_writer, variant, ordinal, 5, true);
      asn1_bit_writer_t* c_writer = nullptr;
      ASSERT_EQ(asn1_bit_writer_create(&c_writer), ASN1_OK);
      ASSERT_EQ(asn1_per_encode_enumerated_ex(c_writer, c_variant, ordinal, 5, 1, 0, 0), ASN1_OK);
      std::uint8_t* c_bytes = nullptr; std::size_t c_size = 0;
      ASSERT_EQ(asn1_bit_writer_take(c_writer, &c_bytes, &c_size), ASN1_OK);
      const auto cpp_bytes = cpp_writer.take();
      ASSERT_EQ(c_size, cpp_bytes.size());
      EXPECT_TRUE(std::equal(cpp_bytes.begin(), cpp_bytes.end(), c_bytes));
      std::free(c_bytes); asn1_bit_writer_destroy(c_writer);
    }
    const std::uint64_t extension_ordinals[] = {0u, 63u, 64u, 500u};
    for (const std::uint64_t ordinal : extension_ordinals) {
      asn1::BitWriter cpp_writer;
      asn1::per::encode_enumerated_extension(cpp_writer, variant, static_cast<std::size_t>(ordinal));
      asn1_bit_writer_t* c_writer = nullptr;
      ASSERT_EQ(asn1_bit_writer_create(&c_writer), ASN1_OK);
      ASSERT_EQ(asn1_per_encode_enumerated_ex(c_writer, c_variant, 0, 5, 1, 1, ordinal), ASN1_OK);
      std::uint8_t* c_bytes = nullptr; std::size_t c_size = 0;
      ASSERT_EQ(asn1_bit_writer_take(c_writer, &c_bytes, &c_size), ASN1_OK);
      const auto cpp_bytes = cpp_writer.take();
      ASSERT_EQ(c_size, cpp_bytes.size());
      EXPECT_TRUE(std::equal(cpp_bytes.begin(), cpp_bytes.end(), c_bytes));
      std::free(c_bytes); asn1_bit_writer_destroy(c_writer);
    }
  }
}

TEST(PerPrimitives, COidMatchesCppForBothVariants) {
  const std::vector<std::uint64_t> absolute{1, 2, 840, 113549};
  const std::vector<std::uint64_t> relative{0, 127, 128, UINT64_MAX};
  for (const auto variant : {asn1::per::Variant::Unaligned, asn1::per::Variant::Aligned}) {
    const auto c_variant = variant == asn1::per::Variant::Aligned
        ? ASN1_PER_ALIGNED : ASN1_PER_UNALIGNED;
    for (const auto* arcs : {&absolute, &relative}) {
      const bool is_relative = arcs == &relative;
      asn1::BitWriter cpp_writer;
      asn1::per::encode_object_identifier(cpp_writer, variant, *arcs, is_relative);
      const auto cpp_bytes = cpp_writer.take();
      asn1_bit_writer_t* c_writer = nullptr;
      ASSERT_EQ(asn1_bit_writer_create(&c_writer), ASN1_OK);
      ASSERT_EQ(asn1_per_encode_oid_ex(c_writer, c_variant, arcs->data(), arcs->size(), is_relative), ASN1_OK);
      std::uint8_t* c_bytes = nullptr; std::size_t c_size = 0;
      ASSERT_EQ(asn1_bit_writer_take(c_writer, &c_bytes, &c_size), ASN1_OK);
      ASSERT_EQ(c_size, cpp_bytes.size());
      EXPECT_TRUE(std::equal(cpp_bytes.begin(), cpp_bytes.end(), c_bytes));
      asn1_bit_writer_destroy(c_writer);
      asn1_bit_reader_t* reader = nullptr;
      ASSERT_EQ(asn1_bit_reader_create(&reader, asn1_byte_view(c_bytes, c_size)), ASN1_OK);
      std::uint64_t* decoded = nullptr; std::size_t decoded_count = 0;
      ASSERT_EQ(asn1_per_decode_oid_ex(reader, c_variant, &decoded, &decoded_count, is_relative), ASN1_OK);
      ASSERT_EQ(decoded_count, arcs->size());
      EXPECT_TRUE(std::equal(arcs->begin(), arcs->end(), decoded));
      std::free(decoded); std::free(c_bytes); asn1_bit_reader_destroy(reader);
    }
  }
}

TEST(PerPrimitives, CSequencePreambleMatchesCppAndPreservesFailureOutput) {
  const std::uint8_t optional_bits[] = {1, 0, 1};
  for (const bool extensible : {false, true}) {
    asn1::BitWriter cpp_writer;
    asn1::per::encode_sequence_preamble(
        cpp_writer, asn1::per::Variant::Unaligned, extensible, extensible,
        asn1::Span<const std::uint8_t>(optional_bits, 3));
    const auto expected = cpp_writer.take();
    asn1_bit_writer_t* writer = nullptr;
    ASSERT_EQ(asn1_bit_writer_create(&writer), ASN1_OK);
    ASSERT_EQ(asn1_per_encode_sequence_preamble(writer, extensible, extensible,
                                                optional_bits, 3), ASN1_OK);
    std::uint8_t* actual = nullptr; std::size_t size = 0;
    ASSERT_EQ(asn1_bit_writer_take(writer, &actual, &size), ASN1_OK);
    ASSERT_EQ(size, expected.size());
    EXPECT_TRUE(std::equal(expected.begin(), expected.end(), actual));
    asn1_bit_reader_t* reader = nullptr;
    ASSERT_EQ(asn1_bit_reader_create(&reader, asn1_byte_view(actual, size)), ASN1_OK);
    asn1_per_sequence_preamble_t decoded{};
    ASSERT_EQ(asn1_per_decode_sequence_preamble(reader, extensible, 3, &decoded), ASN1_OK);
    EXPECT_EQ(decoded.extensions_present, extensible);
    ASSERT_EQ(decoded.optional_count, 3u);
    EXPECT_TRUE(std::equal(std::begin(optional_bits), std::end(optional_bits), decoded.optional_bits));
    asn1_per_sequence_preamble_free(&decoded);
    asn1_bit_reader_destroy(reader); asn1_bit_writer_destroy(writer); std::free(actual);
  }

  const std::uint8_t empty = 0;
  asn1_bit_reader_t* reader = nullptr;
  ASSERT_EQ(asn1_bit_reader_create(&reader, asn1_byte_view(&empty, 0)), ASN1_OK);
  asn1_per_sequence_preamble_t untouched{1, reinterpret_cast<std::uint8_t*>(1), 77};
  EXPECT_EQ(asn1_per_decode_sequence_preamble(reader, true, 3, &untouched), ASN1_ERR_TRUNCATED);
  EXPECT_EQ(untouched.extensions_present, 1);
  EXPECT_EQ(untouched.optional_bits, reinterpret_cast<std::uint8_t*>(1));
  EXPECT_EQ(untouched.optional_count, 77u);
  asn1_bit_reader_destroy(reader);
}

TEST(PerPrimitives, LengthDeterminant) {
  asn1::BitWriter w;
  EXPECT_EQ(asn1::per::encode_length_determinant(w, asn1::per::Variant::Unaligned, 5), 5u);
  EXPECT_EQ(asn1::per::encode_length_determinant(w, asn1::per::Variant::Unaligned, 200), 200u);
  auto bytes = w.take();
  expect_bytes(bytes, hex({0x05, 0x80 | 0x00, 0xC8}));  // 05, 80 C8 for 200

  asn1::BitReader r(bytes);
  auto a = asn1::per::decode_length_determinant(r, asn1::per::Variant::Unaligned);
  auto b = asn1::per::decode_length_determinant(r, asn1::per::Variant::Unaligned);
  ASSERT_TRUE(a.ok());
  ASSERT_TRUE(b.ok());
  EXPECT_EQ(a.value(), 5u);
  EXPECT_EQ(b.value(), 200u);
}

TEST(PerPrimitives, ChoiceBitmapOpenType) {
  asn1::BitWriter w;
  asn1::per::encode_choice_index(w, asn1::per::Variant::Unaligned, 2, 4);  // 2 bits: 10
  const std::uint8_t bm[] = {1, 0, 1};
  asn1::per::encode_bitmap(w, asn1::Span<const std::uint8_t>(bm, 3));
  // so far 5 bits; then open type with 1 byte 0xAB
  asn1::per::encode_open_type(w, asn1::per::Variant::Unaligned, hex({0xAB}));
  auto bytes = w.take();

  asn1::BitReader r(bytes);
  auto idx = asn1::per::decode_choice_index(r, asn1::per::Variant::Unaligned, 4);
  auto bits = asn1::per::decode_bitmap(r, 3);
  auto ot = asn1::per::decode_open_type(r, asn1::per::Variant::Unaligned);
  ASSERT_TRUE(idx.ok());
  ASSERT_TRUE(bits.ok());
  ASSERT_TRUE(ot.ok());
  EXPECT_EQ(idx.value(), 2u);
  ASSERT_EQ(bits.value().size(), 3u);
  EXPECT_EQ(bits.value()[0], 1);
  EXPECT_EQ(bits.value()[1], 0);
  EXPECT_EQ(bits.value()[2], 1);
  expect_bytes(ot.value(), hex({0xAB}));
}

TEST(PerPrimitives, OpenTypeAperAligns) {
  asn1::BitWriter w;
  w.put_bit(true);
  asn1::per::encode_open_type(w, asn1::per::Variant::Aligned, hex({0x11, 0x22}));
  // 1 + 7 pad + length 02 + 11 22
  expect_bytes(w.take(), hex({0x80, 0x02, 0x11, 0x22}));

  const auto _bits16 = hex({0x80, 0x02, 0x11, 0x22});
  asn1::BitReader r(_bits16);
  ASSERT_TRUE(r.get_bit().ok());
  auto ot = asn1::per::decode_open_type(r, asn1::per::Variant::Aligned);
  ASSERT_TRUE(ot.ok());
  expect_bytes(ot.value(), hex({0x11, 0x22}));
}

TEST(PerPrimitives, PrintableStringIncludesPeriod) {
  asn1::per::SizeConstraint size;
  size.lower=0; size.upper=8;
  asn1::BitWriter writer;
  ASSERT_TRUE(asn1::per::encode_restricted_string(writer,asn1::per::Variant::Unaligned,
      "A.B",asn1::per::CharStringKind::PrintableString,size).ok());
  const auto wire=writer.take();
  asn1::BitReader reader(wire);
  auto decoded=asn1::per::decode_restricted_string(reader,asn1::per::Variant::Unaligned,
      asn1::per::CharStringKind::PrintableString,size);
  ASSERT_TRUE(decoded.ok());
  EXPECT_EQ(decoded.value(),"A.B");
}
