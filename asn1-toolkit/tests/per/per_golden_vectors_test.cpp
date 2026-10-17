/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/tests/per/per_golden_vectors_test.cpp
**   $Version: 0.1
**   $Date:   2026-10-13
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   Differential PER validation against externally-generated golden vectors.
**
** Specification: ITU-T X.691 — Packed Encoding Rules (PER); the vectors in
**                 tests/per/golden/per_golden_vectors.txt are produced by
**                 an independent implementation (asn1tools) via
**                 scripts/gen_per_golden.py, never by this toolchain.
** Design Spec:   asn1-toolkit/docs/architecture.md
**                 asn1-toolkit/README.md
***************************************************************************/
#include "generated.hpp"

#include <asn1/runtime/bit_io.hpp>
#include <asn1/runtime/per/codec.hpp>

#include <cstdint>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <gtest/gtest.h>

#ifndef ASN1_PER_GOLDEN_FILE
#error "ASN1_PER_GOLDEN_FILE must be defined by the build"
#endif

namespace {

struct VectorCase {
  std::string name;
  std::string uper_hex;
  std::string aper_hex;
};

std::vector<VectorCase> load_vectors(const char* path) {
  std::vector<VectorCase> cases;
  std::ifstream in(path);
  std::string line;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#') {
      continue;
    }
    std::istringstream ls(line);
    VectorCase c;
    if (ls >> c.name >> c.uper_hex >> c.aper_hex) {
      cases.push_back(std::move(c));
    }
  }
  return cases;
}

std::vector<std::uint8_t> parse_hex(const std::string& s) {
  std::vector<std::uint8_t> out;
  out.reserve(s.size() / 2);
  for (std::size_t i = 0; i + 1 < s.size(); i += 2) {
    out.push_back(static_cast<std::uint8_t>(std::stoul(s.substr(i, 2), nullptr, 16)));
  }
  return out;
}

std::string to_hex(const std::vector<std::uint8_t>& b) {
  static const char* kHex = "0123456789ABCDEF";
  std::string out;
  out.reserve(b.size() * 2);
  for (std::uint8_t v : b) {
    out.push_back(kHex[v >> 4]);
    out.push_back(kHex[v & 0xF]);
  }
  return out;
}

// ---- value builders (mirror the case values in scripts/gen_per_golden.py) ----

pg::PersonnelRecord build_personnel(const std::string& name) {
  pg::PersonnelRecord p;
  p.name = pg::Name("John");
  p.title = "Director";
  p.employeeNumber = 51;
  p.dateOfHire = pg::Date("19810905");
  if (name == "personnel_full") {
    p.nameOfSpouse = pg::Name("Mary");
    std::vector<pg::ChildInformation> kids;
    kids.push_back(pg::ChildInformation{pg::Name("Ralph"), pg::Date("19571111")});
    kids.push_back(pg::ChildInformation{pg::Name("Susan"), pg::Date("19590702")});
    p.children = kids;
  } else if (name == "personnel_minimal") {
    // no optionals
  } else if (name == "personnel_spouse_only") {
    p.nameOfSpouse = pg::Name("Mary");
  } else if (name == "personnel_children_only") {
    std::vector<pg::ChildInformation> kids;
    kids.push_back(pg::ChildInformation{pg::Name("Ann"), pg::Date("20200101")});
    p.children = kids;
  } else if (name == "personnel_empty_children") {
    p.children = std::vector<pg::ChildInformation>{};
  } else if (name == "personnel_bounds") {
    p.name = pg::Name(std::string(16, 'A'));
    p.title = std::string(16, 'T');
    p.employeeNumber = 0;
    p.dateOfHire = pg::Date("00000000");
    p.nameOfSpouse = pg::Name(std::string(16, 'B'));
    std::vector<pg::ChildInformation> kids;
    for (int i = 0; i < 4; ++i) {
      kids.push_back(pg::ChildInformation{pg::Name(std::string(16, 'C')),
                                          pg::Date("99999999")});
    }
    p.children = kids;
  } else if (name == "personnel_max_emp") {
    p.employeeNumber = 250;
  }
  return p;
}

pg::Prim build_prim(const std::string& name) {
  pg::Prim p;
  p.flag = true;
  p.small = 5;
  p.med = 200;
  p.wide = 60000;
  p.semi = 5000;
  p.unb = asn1::BigInteger::from_i64(12345);
  p.neg = -777;
  p.en = pg::Color::green;
  p.extEn = pg::ExtColor::two;
  p.bits = asn1::BitStringValue{std::vector<std::uint8_t>{0x3F, 0xC0}, 10};
  // varBits is 5 bits (10100); the trailing padding bits are normalized to
  // zero on decode, matching asn1tools' canonical byte form (0xA0).
  p.varBits = asn1::BitStringValue{std::vector<std::uint8_t>{0xA0}, 5};
  p.octs = {0xDE, 0xAD, 0xBE, 0xEF};
  p.varOcts = {0x01, 0x02};
  p.oid = {1, 3, 6, 1, 4, 1};
  p.txt = "Hi!";
  p.ps = "Ab-?";
  p.ns = "42 9";
  p.u8 = "Héllo";
  if (name == "prim_flag_false") {
    p.flag = false;
  } else if (name == "prim_small_bounds") {
    p.small = 0;
  } else if (name == "prim_small_max") {
    p.small = 15;
  } else if (name == "prim_med_bounds") {
    p.med = 0;
  } else if (name == "prim_med_max") {
    p.med = 255;
  } else if (name == "prim_wide_bounds") {
    p.wide = 0;
  } else if (name == "prim_wide_max") {
    p.wide = 65535;
  } else if (name == "prim_semi_min") {
    p.semi = 1000;
  } else if (name == "prim_semi_big") {
    p.semi = 100000;
  } else if (name == "prim_unb_zero") {
    p.unb = asn1::BigInteger::zero();
  } else if (name == "prim_unb_127") {
    p.unb = asn1::BigInteger::from_i64(127);
  } else if (name == "prim_unb_128") {
    p.unb = asn1::BigInteger::from_i64(128);
  } else if (name == "prim_unb_neg1") {
    p.unb = asn1::BigInteger::from_i64(-1);
  } else if (name == "prim_unb_big") {
    p.unb = asn1::BigInteger::from_i64(1LL << 40);
  } else if (name == "prim_neg_bounds") {
    p.neg = -1000;
  } else if (name == "prim_neg_max") {
    p.neg = 1000;
  } else if (name == "prim_enum_red") {
    p.en = pg::Color::red;
  } else if (name == "prim_enum_blue") {
    p.en = pg::Color::blue;
  } else if (name == "prim_extenum_root") {
    p.extEn = pg::ExtColor::one;
  } else if (name == "prim_extenum_ext") {
    p.extEn = pg::ExtColor::three;
  } else if (name == "prim_bits_min") {
    p.bits = asn1::BitStringValue{std::vector<std::uint8_t>{0x00, 0x00}, 10};
  } else if (name == "prim_bits_max") {
    p.bits = asn1::BitStringValue{std::vector<std::uint8_t>{0xFF, 0xC0}, 10};
  } else if (name == "prim_varbits_empty") {
    p.varBits = asn1::BitStringValue{std::vector<std::uint8_t>{}, 0};
  } else if (name == "prim_varbits_max") {
    p.varBits = asn1::BitStringValue{std::vector<std::uint8_t>{0xFF, 0xFF, 0xFF, 0xFF},
                                     32};
  } else if (name == "prim_octs_bounds") {
    p.octs = {0, 0, 0, 0};
  } else if (name == "prim_varocts_empty") {
    p.varOcts = {};
  } else if (name == "prim_varocts_max") {
    p.varOcts = std::vector<std::uint8_t>(16, 0xAA);
  } else if (name == "prim_oid_empty") {
    p.oid = {2, 999};
  } else if (name == "prim_txt_empty") {
    p.txt = "";
  } else if (name == "prim_txt_max") {
    p.txt = std::string(20, '~');
  } else if (name == "prim_u8_empty") {
    p.u8 = "";
  } else if (name == "prim_u8_utf8") {
    p.u8 = "na\u00EFve \u65E5\u672C";
  } else if (name == "prim_ps_empty") {
    p.ps = "A";
  } else if (name == "prim_ps_full") {
    p.ps = "(A-z9?)";
  } else if (name == "prim_ns_empty") {
    p.ns = "0";
  } else if (name == "prim_ns_full") {
    p.ns = "9876543 ";
  }
  return p;
}

pg::ExtSeq build_extseq(const std::string& name) {
  pg::ExtSeq s;
  s.a = 7;
  s.b = true;
  if (name == "extseq_ext") {
    s.c = 200;
  }
  return s;
}

pg::ExtSeqVB build_extseqvb(const std::string& name) {
  pg::ExtSeqVB s;
  s.a = 7;
  s.b = true;
  if (name == "vbseq_full") {
    s.c = 200;
    s.d = "xy";
  } else if (name == "vbseq_partial") {
    s.c = 200;
  }
  return s;
}

pg::ExtChoice build_extchoice(const std::string& name) {
  pg::ExtChoice c;
  if (name == "extchoice_a") {
    c.alt = pg::ExtChoice::a_{42};
  } else if (name == "extchoice_b") {
    c.alt = pg::ExtChoice::b_{"xy"};
  } else if (name == "extchoice_ext") {
    c.alt = pg::ExtChoice::c_{true};
  }
  return c;
}

std::vector<bool> build_frag(const std::string& name) {
  if (name == "frag_small") {
    return {true, false, true};
  }
  std::vector<bool> v;
  v.reserve(16385);
  for (int i = 0; i < 16385; ++i) {
    v.push_back(i % 2 == 0);
  }
  return v;
}

// ---- equality ----

bool eq_personnel(const pg::PersonnelRecord& a, const pg::PersonnelRecord& b) {
  if (a.name != b.name || a.title != b.title || a.employeeNumber != b.employeeNumber ||
      a.dateOfHire != b.dateOfHire) {
    return false;
  }
  if (a.nameOfSpouse.has_value() != b.nameOfSpouse.has_value()) {
    return false;
  }
  if (a.nameOfSpouse && *a.nameOfSpouse != *b.nameOfSpouse) {
    return false;
  }
  if (a.children.has_value() != b.children.has_value()) {
    return false;
  }
  if (a.children) {
    if (a.children->size() != b.children->size()) {
      return false;
    }
    for (std::size_t i = 0; i < a.children->size(); ++i) {
      if ((*a.children)[i].name != (*b.children)[i].name ||
          (*a.children)[i].dateOfBirth != (*b.children)[i].dateOfBirth) {
        return false;
      }
    }
  }
  return true;
}

bool eq_prim(const pg::Prim& a, const pg::Prim& b) {
  auto eq_bits = [](const asn1::BitStringValue& x, const asn1::BitStringValue& y) {
    return x.bit_length == y.bit_length && x.bits == y.bits;
  };
  return a.flag == b.flag && a.small == b.small && a.med == b.med &&
         a.wide == b.wide && a.semi == b.semi && a.unb == b.unb &&
         a.neg == b.neg && a.en == b.en && a.extEn == b.extEn &&
         eq_bits(a.bits, b.bits) && eq_bits(a.varBits, b.varBits) &&
         a.octs == b.octs && a.varOcts == b.varOcts && a.oid == b.oid &&
         a.txt == b.txt && a.ps == b.ps && a.ns == b.ns && a.u8 == b.u8;
}

bool eq_extseq(const pg::ExtSeq& a, const pg::ExtSeq& b) {
  return a.a == b.a && a.b == b.b && a.c == b.c;
}

bool eq_extseqvb(const pg::ExtSeqVB& a, const pg::ExtSeqVB& b) {
  return a.a == b.a && a.b == b.b && a.c == b.c && a.d == b.d;
}

bool eq_extchoice(const pg::ExtChoice& a, const pg::ExtChoice& b) {
  return a.alt.index() == b.alt.index() &&
         std::visit(
             [](const auto& x, const auto& y) {
               if constexpr (std::is_same_v<decltype(x), decltype(y)>) {
                 return x.value == y.value;
               } else {
                 return false;
               }
             },
             a.alt, b.alt);
}

}  // namespace

TEST(PerGoldenVectors, UperMatchesOracleBytes) {
  for (const VectorCase& c : load_vectors(ASN1_PER_GOLDEN_FILE)) {
    SCOPED_TRACE(c.name);
    const std::vector<std::uint8_t> want = parse_hex(c.uper_hex);
    if (c.name.rfind("personnel_", 0) == 0) {
      const pg::PersonnelRecord v = build_personnel(c.name);
      asn1::BitWriter w;
      auto e = pg::encode_uper(w, v);
      ASSERT_TRUE(e.ok()) << e.error().message;
      EXPECT_EQ(to_hex(w.take()), c.uper_hex);
      pg::PersonnelRecord got{};
      asn1::BitReader r(want);
      auto d = pg::decode_uper(r, got);
      ASSERT_TRUE(d.ok()) << d.error().message;
      EXPECT_TRUE(eq_personnel(got, v));
    } else if (c.name.rfind("prim_", 0) == 0) {
      const pg::Prim v = build_prim(c.name);
      asn1::BitWriter w;
      auto e = pg::encode_uper(w, v);
      ASSERT_TRUE(e.ok()) << e.error().message;
      EXPECT_EQ(to_hex(w.take()), c.uper_hex);
      pg::Prim got{};
      asn1::BitReader r(want);
      auto d = pg::decode_uper(r, got);
      ASSERT_TRUE(d.ok()) << d.error().message;
      EXPECT_TRUE(eq_prim(got, v));
    } else if (c.name.rfind("extseq_", 0) == 0) {
      const pg::ExtSeq v = build_extseq(c.name);
      asn1::BitWriter w;
      auto e = pg::encode_uper(w, v);
      ASSERT_TRUE(e.ok()) << e.error().message;
      EXPECT_EQ(to_hex(w.take()), c.uper_hex);
      pg::ExtSeq got{};
      asn1::BitReader r(want);
      auto d = pg::decode_uper(r, got);
      ASSERT_TRUE(d.ok()) << d.error().message;
      EXPECT_TRUE(eq_extseq(got, v));
    } else if (c.name.rfind("extchoice_", 0) == 0) {
      const pg::ExtChoice v = build_extchoice(c.name);
      asn1::BitWriter w;
      auto e = pg::encode_uper(w, v);
      ASSERT_TRUE(e.ok()) << e.error().message;
      EXPECT_EQ(to_hex(w.take()), c.uper_hex);
      pg::ExtChoice got{};
      asn1::BitReader r(want);
      auto d = pg::decode_uper(r, got);
      ASSERT_TRUE(d.ok()) << d.error().message;
      EXPECT_TRUE(eq_extchoice(got, v));
    } else if (c.name.rfind("vbseq_", 0) == 0) {
      const pg::ExtSeqVB v = build_extseqvb(c.name);
      asn1::BitWriter w;
      auto e = pg::encode_uper(w, v);
      ASSERT_TRUE(e.ok()) << e.error().message;
      EXPECT_EQ(to_hex(w.take()), c.uper_hex);
      pg::ExtSeqVB got{};
      asn1::BitReader r(want);
      auto d = pg::decode_uper(r, got);
      ASSERT_TRUE(d.ok()) << d.error().message;
      EXPECT_TRUE(eq_extseqvb(got, v));
    } else if (c.name.rfind("frag_", 0) == 0) {
      const std::vector<bool> v = build_frag(c.name);
      pg::Frag frag;
      frag.value = v;
      asn1::BitWriter w;
      auto e = pg::encode_uper(w, frag);
      ASSERT_TRUE(e.ok()) << e.error().message;
      EXPECT_EQ(to_hex(w.take()), c.uper_hex);
      pg::Frag got{};
      asn1::BitReader r(want);
      auto d = pg::decode_uper(r, got);
      ASSERT_TRUE(d.ok()) << d.error().message;
      EXPECT_EQ(got.value, v);
    }
  }
}

TEST(PerGoldenVectors, AperMatchesOracleBytes) {
  for (const VectorCase& c : load_vectors(ASN1_PER_GOLDEN_FILE)) {
    SCOPED_TRACE(c.name);
    const std::vector<std::uint8_t> want = parse_hex(c.aper_hex);
    if (c.name.rfind("personnel_", 0) == 0) {
      const pg::PersonnelRecord v = build_personnel(c.name);
      asn1::BitWriter w;
      auto e = pg::encode_aper(w, v);
      ASSERT_TRUE(e.ok()) << e.error().message;
      EXPECT_EQ(to_hex(w.take()), c.aper_hex);
      pg::PersonnelRecord got{};
      asn1::BitReader r(want);
      auto d = pg::decode_aper(r, got);
      ASSERT_TRUE(d.ok()) << d.error().message;
      EXPECT_TRUE(eq_personnel(got, v));
    } else if (c.name.rfind("prim_", 0) == 0) {
      const pg::Prim v = build_prim(c.name);
      asn1::BitWriter w;
      auto e = pg::encode_aper(w, v);
      ASSERT_TRUE(e.ok()) << e.error().message;
      EXPECT_EQ(to_hex(w.take()), c.aper_hex);
      pg::Prim got{};
      asn1::BitReader r(want);
      auto d = pg::decode_aper(r, got);
      ASSERT_TRUE(d.ok()) << d.error().message;
      EXPECT_TRUE(eq_prim(got, v));
    } else if (c.name.rfind("extseq_", 0) == 0) {
      const pg::ExtSeq v = build_extseq(c.name);
      asn1::BitWriter w;
      auto e = pg::encode_aper(w, v);
      ASSERT_TRUE(e.ok()) << e.error().message;
      EXPECT_EQ(to_hex(w.take()), c.aper_hex);
      pg::ExtSeq got{};
      asn1::BitReader r(want);
      auto d = pg::decode_aper(r, got);
      ASSERT_TRUE(d.ok()) << d.error().message;
      EXPECT_TRUE(eq_extseq(got, v));
    } else if (c.name.rfind("extchoice_", 0) == 0) {
      const pg::ExtChoice v = build_extchoice(c.name);
      asn1::BitWriter w;
      auto e = pg::encode_aper(w, v);
      ASSERT_TRUE(e.ok()) << e.error().message;
      EXPECT_EQ(to_hex(w.take()), c.aper_hex);
      pg::ExtChoice got{};
      asn1::BitReader r(want);
      auto d = pg::decode_aper(r, got);
      ASSERT_TRUE(d.ok()) << d.error().message;
      EXPECT_TRUE(eq_extchoice(got, v));
    } else if (c.name.rfind("vbseq_", 0) == 0) {
      const pg::ExtSeqVB v = build_extseqvb(c.name);
      asn1::BitWriter w;
      auto e = pg::encode_aper(w, v);
      ASSERT_TRUE(e.ok()) << e.error().message;
      EXPECT_EQ(to_hex(w.take()), c.aper_hex);
      pg::ExtSeqVB got{};
      asn1::BitReader r(want);
      auto d = pg::decode_aper(r, got);
      ASSERT_TRUE(d.ok()) << d.error().message;
      EXPECT_TRUE(eq_extseqvb(got, v));
    } else if (c.name.rfind("frag_", 0) == 0) {
      const std::vector<bool> v = build_frag(c.name);
      pg::Frag frag;
      frag.value = v;
      asn1::BitWriter w;
      auto e = pg::encode_aper(w, frag);
      ASSERT_TRUE(e.ok()) << e.error().message;
      EXPECT_EQ(to_hex(w.take()), c.aper_hex);
      pg::Frag got{};
      asn1::BitReader r(want);
      auto d = pg::decode_aper(r, got);
      ASSERT_TRUE(d.ok()) << d.error().message;
      EXPECT_EQ(got.value, v);
    }
  }
}
