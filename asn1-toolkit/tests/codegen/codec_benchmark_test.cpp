/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/tests/codegen/codec_benchmark_test.cpp
**   $Version: 0.1
**   $Date:   2026-10-12
**   $Author: tina.cao
***************************************************************************/
/**  File Description:
**
**   Throughput micro-benchmark for the Person codecs. Prints ops/sec and
**   asserts only a generous lower bound, so it is a regression smoke test
**   rather than a flaky timing gate.
**
** Specification: Per-codec rules (X.691/X.696/X.690/X.697).
** Design Spec:   asn1-toolkit/docs/architecture.md
**                 asn1-toolkit/README.md
***************************************************************************/
#include "per/generated.hpp"
#include "oer/generated.hpp"
#include "ber/generated.hpp"
#include "jer/generated.hpp"

#include <asn1/runtime/bit_io.hpp>
#include <asn1/runtime/byte_io.hpp>
#include <asn1/runtime/jer/json.hpp>
#include <asn1/runtime/per/primitives.hpp>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>

#include <gtest/gtest.h>

namespace {

constexpr int kIters = 20000;

template <typename Fn>
double ops_per_sec(Fn&& fn) {
  const auto t0 = std::chrono::steady_clock::now();
  fn();
  const auto t1 = std::chrono::steady_clock::now();
  const double secs = std::chrono::duration<double>(t1 - t0).count();
  return secs > 0.0 ? static_cast<double>(kIters) / secs : 0.0;
}

asn1_gen_per::Person make_per() {
  asn1_gen_per::Person p{};
  p.id = 42;
  p.name = "Ada";
  p.age = static_cast<std::uint8_t>(36);
  return p;
}

}  // namespace

TEST(CodecBenchmark, UperEncodeDecode) {
  const auto p = make_per();
  double ops = 0.0;
  ops = ops_per_sec([&] {
    for (int i = 0; i < kIters; ++i) {
      asn1::BitWriter w;
      if (!asn1_gen_per::encode_uper(w, p).ok()) { std::abort(); }
      const auto bytes = w.take();
      asn1::BitReader r(bytes);
      asn1_gen_per::Person got{};
      if (!asn1_gen_per::decode_uper(r, got).ok()) { std::abort(); }
    }
  });
  std::printf("[bench] UPER round-trip: %.0f ops/sec\n", ops);
  EXPECT_GT(ops, 1000.0);  // generous lower bound
}

TEST(CodecBenchmark, OerEncodeDecode) {
  asn1_gen_oer::Person p{};
  p.id = 42;
  p.name = "Ada";
  p.age = static_cast<std::uint8_t>(36);
  double ops = ops_per_sec([&] {
    for (int i = 0; i < kIters; ++i) {
      asn1::ByteWriter w;
      if (!asn1_gen_oer::encode_oer(w, p).ok()) { std::abort(); }
      asn1::ByteReader r(w.buffer());
      asn1_gen_oer::Person got{};
      if (!asn1_gen_oer::decode_oer(r, got).ok()) { std::abort(); }
    }
  });
  std::printf("[bench] OER round-trip: %.0f ops/sec\n", ops);
  EXPECT_GT(ops, 1000.0);
}

TEST(CodecBenchmark, BerEncodeDecode) {
  asn1_gen_ber::Person p{};
  p.id = 42;
  p.name = "Ada";
  p.age = static_cast<std::uint8_t>(36);
  double ops = ops_per_sec([&] {
    for (int i = 0; i < kIters; ++i) {
      asn1::ByteWriter w;
      if (!asn1_gen_ber::encode_ber(w, p).ok()) { std::abort(); }
      asn1::ByteReader r(w.buffer());
      asn1_gen_ber::Person got{};
      if (!asn1_gen_ber::decode_ber(r, got).ok()) { std::abort(); }
    }
  });
  std::printf("[bench] BER round-trip: %.0f ops/sec\n", ops);
  EXPECT_GT(ops, 1000.0);
}

TEST(CodecBenchmark, JerEncodeDecode) {
  asn1_gen_jer::Person p{};
  p.id = 42;
  p.name = "Ada";
  p.age = static_cast<std::uint8_t>(36);
  double ops = ops_per_sec([&] {
    for (int i = 0; i < kIters; ++i) {
      auto enc = asn1_gen_jer::encode_jer(p);
      if (!enc.ok()) { std::abort(); }
      asn1_gen_jer::Person got{};
      if (!asn1_gen_jer::decode_jer(enc.value(), got).ok()) { std::abort(); }
    }
  });
  std::printf("[bench] JER round-trip: %.0f ops/sec\n", ops);
  EXPECT_GT(ops, 1000.0);
}

TEST(BitIoBenchmark, PresenceBitmapWrite) {
  // P2-1 quantification: a bitmap-dense write pattern (10 x 64-bit presence
  // bitmaps + interleaved small fields per op), the shape UPER spends its
  // per-bit calls on. Output correctness is guarded by per_golden vectors;
  // this measures throughput only.
  std::vector<std::uint8_t> bm(64);
  for (std::size_t i = 0; i < bm.size(); ++i) {
    bm[i] = (i % 3 == 0) ? 1 : 0;
  }
  const auto bits = asn1::Span<const std::uint8_t>(bm.data(), bm.size());
  double ops = ops_per_sec([&] {
    for (int i = 0; i < kIters; ++i) {
      asn1::BitWriter w;
      for (int k = 0; k < 10; ++k) {
        asn1::per::encode_bitmap(w, bits);
        w.put_bits(static_cast<std::uint64_t>(k) * 7u + 1u, 6);
        w.put_bit((k & 1) != 0);
      }
      const auto bytes = w.take();
      if (bytes.size() != 89) { std::abort(); }
    }
  });
  std::printf("[bench] BitWriter bitmap write: %.0f ops/sec\n", ops);
  EXPECT_GT(ops, 1000.0);
}
