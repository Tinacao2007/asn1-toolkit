/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/tests/tlv/tlv_golden_vectors_test.cpp$
**   $Version: 0.1
**   $Date:   2026-10-15
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   Differential BER/DER/OER validation against externally-generated golden
**   vectors (P1-1: extend byte-level golden vectors beyond PER).
**
**   The vectors in tests/tlv/golden/tlv_golden_vectors.txt are produced by
**   independent implementations — asn1tools (BER/OER source; DER cross-check)
**   and asn1c (DER arbiter, X.690 11.6 SET/SET OF ordering; OER cross-check)
**   — via scripts/gen_tlv_golden.py, never by this toolchain.
**
**   Oracle policy (every disagreement adjudicated by clause text):
**     - DER: asn1c wins when the oracles disagree (asn1tools does not sort
**       SET/SET OF components; X.690 11.6 requires tag order per X.680 8.6
**       for SET and ascending octet-string order for SET OF);
**     - OER: asn1tools is the source (asn1c's oer_encode segfaults on SET
**       values — a known asn1c bug);
**     - BER: asn1tools is the source, but BER has no canonical SET order
**       (X.690 8.14 only requires the constructed form), so SET-bearing
**       cases carry '-' in the BER column — the DER column carries the
**       ordering proof there.
**
** Specification: ITU-T X.680 / X.690 / X.696
** Design Spec:   asn1-toolkit/docs/architecture.md
**                 asn1-toolkit/README.md
**************************************************************************/
#include "generated.hpp"

#include <asn1/runtime/byte_io.hpp>

#include <cstdint>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <gtest/gtest.h>

#ifndef ASN1_TLV_GOLDEN_FILE
#error "ASN1_TLV_GOLDEN_FILE must be defined by the build"
#endif

namespace {

struct VectorCase {
  std::string name;
  std::string ber_hex;  // "-" when BER has no canonical byte layout (SET)
  std::string der_hex;
  std::string oer_hex;
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
    if (ls >> c.name >> c.ber_hex >> c.der_hex >> c.oer_hex) {
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

// ---- value builders (mirror the case values in scripts/gen_tlv_golden.py) ----

tg::Tagged build_tagged(const std::string& name) {
  tg::Tagged t;
  if (name == "tagged_base") {
    t.plain = asn1::BigInteger::from_i64(7);
    t.impl = asn1::BigInteger::from_i64(8);
    t.expl = asn1::BigInteger::from_i64(9);
  } else {  // tagged_neg
    t.plain = asn1::BigInteger::from_i64(-300);
    t.impl = asn1::BigInteger::from_i64(-1);
    t.expl = asn1::BigInteger::from_i64(0);
  }
  return t;
}

tg::TaggedSet build_tagged_set(const std::string& name) {
  tg::TaggedSet s;
  if (name == "set_order") {
    s.second = asn1::BigInteger::from_i64(7);
    s.first = true;
  } else {  // set_false
    s.second = asn1::BigInteger::from_i64(300);
    s.first = false;
  }
  return s;
}

tg::IntSet build_int_set(const std::string& name) {
  tg::IntSet s;
  if (name == "setof_order") {
    s.value.push_back(asn1::BigInteger::from_i64(7));
    s.value.push_back(asn1::BigInteger::from_i64(3));
    s.value.push_back(asn1::BigInteger::from_i64(256));
  }  // setof_empty: empty list
  return s;
}

tg::WithDefault build_with_default(const std::string& name) {
  tg::WithDefault d;
  if (name == "def_other") {
    d.kind = asn1::BigInteger::from_i64(51);
    d.name = "mary";
  }  // def_default: both equal to the declared default
  return d;
}

tg::Prims build_prims(const std::string& name) {
  tg::Prims p;
  if (name == "prims_base") {
    p.neg = asn1::BigInteger::from_i64(-42);
    p.r = 1.5;
  } else {  // prims_real2
    p.neg = asn1::BigInteger::from_i64(0);
    p.r = 2.25;
  }
  return p;
}

tg::Color build_color(const std::string& name) {
  return name == "color_ext" ? tg::Color::yellow : tg::Color::green;
}

tg::Inner build_inner(const std::string& name) {
  tg::Inner i;
  if (name == "inner_seq") {
    tg::Inner_seq s;
    s.x = asn1::BigInteger::from_i64(5);
    i.alt = tg::Inner::seq_{s};
  } else {  // inner_num
    i.alt = tg::Inner::num_{asn1::BigInteger::from_i64(7)};
  }
  return i;
}

tg::Ext build_ext(const std::string& name) {
  tg::Ext e;
  e.a = asn1::BigInteger::from_i64(1);
  e.b = true;
  if (name == "ext_add") {
    e.c = asn1::BigInteger::from_i64(9);
  }
  return e;
}

// ---- equality ----

bool eq_tagged(const tg::Tagged& a, const tg::Tagged& b) {
  return a.plain == b.plain && a.impl == b.impl && a.expl == b.expl;
}

bool eq_tagged_set(const tg::TaggedSet& a, const tg::TaggedSet& b) {
  return a.second == b.second && a.first == b.first;
}

bool eq_int_set(const tg::IntSet& a, const tg::IntSet& b) {
  if (a.value.size() != b.value.size()) {
    return false;
  }
  for (std::size_t i = 0; i < a.value.size(); ++i) {
    if (a.value[i] != b.value[i]) {
      return false;
    }
  }
  return true;
}

bool eq_with_default(const tg::WithDefault& a, const tg::WithDefault& b) {
  return a.kind == b.kind && a.name == b.name;
}

bool eq_prims(const tg::Prims& a, const tg::Prims& b) {
  return a.neg == b.neg && a.r == b.r;
}

bool eq_inner(const tg::Inner& a, const tg::Inner& b) {
  if (a.alt.index() != b.alt.index()) {
    return false;
  }
  if (a.alt.index() == 0) {
    return std::get<tg::Inner::num_>(a.alt).value ==
           std::get<tg::Inner::num_>(b.alt).value;
  }
  return std::get<tg::Inner::seq_>(a.alt).value.x ==
         std::get<tg::Inner::seq_>(b.alt).value.x;
}

bool eq_ext(const tg::Ext& a, const tg::Ext& b) {
  if (a.a != b.a) {
    return false;
  }
  if (a.b.has_value() != b.b.has_value() ||
      (a.b && *a.b != *b.b)) {
    return false;
  }
  if (a.c.has_value() != b.c.has_value() ||
      (a.c && *a.c != *b.c)) {
    return false;
  }
  return true;
}

// ---- generic driver ----
//
// Encode the value with the codec under test, assert the exact oracle bytes
// (when the column is pinned), then decode the oracle bytes and assert the
// value survives. For a '-' column (BER SET ordering, X.690 8.14) the byte
// assertion is skipped — there is no canonical layout to differentially
// prove — and the round-trip decodes our own encoder's bytes instead.

template <typename T, typename Eq>
void check_case(const VectorCase& c, const std::string& want_hex,
                asn1::Result<void> (*enc)(asn1::ByteWriter&, const T&),
                asn1::Result<void> (*dec)(asn1::ByteReader&, T&),
                Eq eq, const T& v, const T& expected) {
  SCOPED_TRACE(c.name);
  asn1::ByteWriter w;
  auto e = enc(w, v);
  ASSERT_TRUE(e.ok()) << e.error().message;
  const auto produced = w.take();

  const bool pinned = want_hex != "-";
  if (pinned) {
    EXPECT_EQ(to_hex(produced), want_hex);
  }

  T got{};
  const auto decode_bytes = pinned ? parse_hex(want_hex) : produced;
  asn1::ByteReader r(decode_bytes);
  auto d = dec(r, got);
  ASSERT_TRUE(d.ok()) << d.error().message;
  EXPECT_TRUE(eq(got, expected));
}

enum class Codec { Ber, Der, Oer };

const std::string& column(const VectorCase& c, Codec codec) {
  switch (codec) {
    case Codec::Ber: return c.ber_hex;
    case Codec::Der: return c.der_hex;
    default: return c.oer_hex;
  }
}

// Bind the codec function pair for one type: assignment from the overloaded
// name resolves encode_ber/decode_ber/... to the exact pointer type.
// `expected` is the value the decoder must produce — it differs from the
// encode input only for DER SET OF, whose canonical octet-string order
// (X.690 11.6) reorders the components on the wire.
template <typename T, typename Eq>
void run_case(const VectorCase& c, Codec codec, Eq eq, const T& v,
              const T& expected) {
  asn1::Result<void> (*enc)(asn1::ByteWriter&, const T&) = nullptr;
  asn1::Result<void> (*dec)(asn1::ByteReader&, T&) = nullptr;
  switch (codec) {
    case Codec::Ber: enc = &tg::encode_ber; dec = &tg::decode_ber; break;
    case Codec::Der: enc = &tg::encode_der; dec = &tg::decode_der; break;
    default: enc = &tg::encode_oer; dec = &tg::decode_oer; break;
  }
  check_case(c, column(c, codec), enc, dec, eq, v, expected);
}

template <typename T, typename Eq>
void run_case(const VectorCase& c, Codec codec, Eq eq, const T& v) {
  run_case(c, codec, eq, v, v);
}

// Dispatch a case to its type by name prefix; the prefix selects both the
// value builder and the equality predicate.

template <Codec K>
void run_codec(const char* path) {
  for (const VectorCase& c : load_vectors(path)) {
    if (c.name.rfind("tagged_", 0) == 0) {
      run_case(c, K, eq_tagged, build_tagged(c.name));
    } else if (c.name.rfind("set_", 0) == 0) {
      run_case(c, K, eq_tagged_set, build_tagged_set(c.name));
    } else if (c.name.rfind("setof_", 0) == 0) {
      // DER canonical SET OF order (X.690 11.6): the wire order is ascending
      // octet-string order, so a decoder round-trip yields the SORTED list —
      // the encode assertion still proves the encoder sorts the input.
      // BER/OER have no canonical SET OF order, so their round-trip keeps
      // the input order.
      const tg::IntSet v = build_int_set(c.name);
      if (K == Codec::Der) {
        tg::IntSet sorted = v;
        std::sort(sorted.value.begin(), sorted.value.end(),
                  [](const asn1::BigInteger& a, const asn1::BigInteger& b) {
                    return a < b;
                  });
        run_case(c, K, eq_int_set, v, sorted);
      } else {
        run_case(c, K, eq_int_set, v);
      }
    } else if (c.name.rfind("def_", 0) == 0) {
      run_case(c, K, eq_with_default, build_with_default(c.name));
    } else if (c.name.rfind("prims_", 0) == 0) {
      run_case(c, K, eq_prims, build_prims(c.name));
    } else if (c.name.rfind("color_", 0) == 0) {
      run_case(c, K, [](tg::Color a, tg::Color b) { return a == b; },
               build_color(c.name));
    } else if (c.name.rfind("inner_", 0) == 0) {
      run_case(c, K, eq_inner, build_inner(c.name));
    } else if (c.name.rfind("ext_", 0) == 0) {
      run_case(c, K, eq_ext, build_ext(c.name));
    }
  }
}

}  // namespace

TEST(TlvGoldenVectors, BerMatchesOracleBytes) {
  run_codec<Codec::Ber>(ASN1_TLV_GOLDEN_FILE);
}

TEST(TlvGoldenVectors, DerMatchesOracleBytes) {
  run_codec<Codec::Der>(ASN1_TLV_GOLDEN_FILE);
}

TEST(TlvGoldenVectors, OerMatchesOracleBytes) {
  run_codec<Codec::Oer>(ASN1_TLV_GOLDEN_FILE);
}
