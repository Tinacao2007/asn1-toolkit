// 仅测试：消费从完整电信模式生成的 C++17 接口。
#include "generated.hpp"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static int nibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}
static bool unhex(const std::string& text, std::vector<std::uint8_t>& out) {
  if (text.size() % 2 || text.size() > 131072) return false;
  for (std::size_t i = 0; i < text.size(); i += 2) {
    const int a = nibble(text[i]), b = nibble(text[i+1]);
    if (a < 0 || b < 0) return false;
    out.push_back(static_cast<std::uint8_t>((a << 4) | b));
  }
  return true;
}
static void dump(const std::vector<std::uint8_t>& bytes) {
  for (auto b : bytes) std::printf("%02x", b);
  std::puts("");
}
#define CHECK(result) do { auto checked = (result); if (!checked) { \
  std::fprintf(stderr,"%s: %s\n",#result,checked.error().message.c_str()); return 1; } } while (0)

// The existing C++ BIT STRING value API carries explicit value/length members.
// Adapt fixture JER at the test boundary; this is not a wire-format conversion.
static void bit_input(asn1::jer::Value& value) {
#if defined(PROTOCOL_RRC)
  for (auto& member : value.object) {
    auto& child = member.second;
    if (child.kind == asn1::jer::ValueKind::String &&
        (member.first == "systemFrameNumber" || member.first == "randomValue" || member.first == "spare")) {
      const auto length = member.first == "systemFrameNumber" ? 8 : member.first == "randomValue" ? 40 :
                          child.string_value.size() == 4 ? 10 : 1;
      child = asn1::jer::Value::make_object({{"value", child}, {"length", asn1::jer::Value::integer(length)}});
    } else bit_input(child);
  }
#else
  (void)value;
#endif
}
static void bit_output(asn1::jer::Value& value) {
#if defined(PROTOCOL_RRC)
  for (auto& member : value.object) {
    auto& child = member.second;
    if (member.first == "systemFrameNumber" || member.first == "randomValue" || member.first == "spare") {
      for (const auto& field : child.object) if(field.first=="value") { auto text=field.second; child=std::move(text); break; }
    } else bit_output(child);
  }
#else
  (void)value;
#endif
}

template <typename T>
static int run(const std::string& codec, const std::string& mode, const std::string& input) {
  T value{};
  if (mode == "encode") {
    auto json = asn1::jer::parse_document(input);
    if (!json) return 1;
    bit_input(json.value());
    CHECK(asn1_gen::decode_jer(json.value(), value));
  }
  std::vector<std::uint8_t> bytes;
  if (mode == "decode" && !unhex(input, bytes)) return 2;
#define BIT(rule) if (codec == #rule) { \
  if (mode == "decode") { asn1::BitReader r(bytes); CHECK(asn1_gen::decode_##rule(r,value)); \
    if (r.bit_remaining() > 7) return 1; } \
  if (mode == "decode") { auto j=asn1_gen::encode_jer(value); if(!j)return 1; \
    bit_output(j.value()); \
    std::puts(asn1::jer::to_string(j.value()).c_str()); } \
  asn1::BitWriter w; CHECK(asn1_gen::encode_##rule(w,value)); dump(w.buffer()); return 0; }
  BIT(uper)
  BIT(aper)
#undef BIT
  if (codec == "ber") {
    if (mode == "decode") { asn1::ByteReader r(bytes); CHECK(asn1_gen::decode_ber(r,value)); if(!r.eof())return 1; }
    if (mode == "decode") { auto j=asn1_gen::encode_jer(value); if(!j)return 1;
      bit_output(j.value());
      std::puts(asn1::jer::to_string(j.value()).c_str()); }
    asn1::ByteWriter w; CHECK(asn1_gen::encode_ber(w,value)); dump(w.buffer()); return 0;
  }
  return 2;
}
int main(int argc,char** argv) {
  if(argc!=5 || (std::strcmp(argv[3],"encode") && std::strcmp(argv[3],"decode")))return 2;
#define TYPE(name, cpp) if(!std::strcmp(argv[1],name))return run<asn1_gen::cpp>(argv[2],argv[3],argv[4]);
#if defined(PROTOCOL_LPP)
  TYPE("LPP-Message", LPP_Message)
#elif defined(PROTOCOL_RRC)
  TYPE("BCCH-BCH-Message", BCCH_BCH_Message)
  TYPE("UL-CCCH-Message", UL_CCCH_Message)
#elif defined(PROTOCOL_S1AP)
  TYPE("S1AP-PDU", S1AP_PDU)
  TYPE("ErrorIndication", ErrorIndication)
  TYPE("MMEConfigurationUpdateAcknowledge", MMEConfigurationUpdateAcknowledge)
  TYPE("MME-UE-S1AP-ID", MME_UE_S1AP_ID)
  TYPE("ENB-UE-S1AP-ID", ENB_UE_S1AP_ID)
  TYPE("Cause", Cause)
#endif
#undef TYPE
  return 2;
}
