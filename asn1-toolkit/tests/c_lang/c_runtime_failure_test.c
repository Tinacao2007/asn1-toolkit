/* REV-01..03: deterministic allocation failure and output/lifetime contracts.
 * Built with a separate copy of the C sources; production has no allocator hooks. */
#include <asn1/c_runtime/bigint.h>
#include <asn1/c_runtime/bit_io.h>
#include <asn1/c_runtime/bit_string.h>
#include <asn1/c_runtime/byte_io.h>
#include <asn1/c_runtime/ber_tlv.h>
#include <asn1/c_runtime/per.h>
#include <asn1/c_runtime/jer.h>
#include <asn1/c_runtime/xer.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t calls, fail_at = SIZE_MAX, live;
static unsigned checks;
#define CHECK(c) do { ++checks; if (!(c)) { \
  fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); exit(1); } } while (0)
static int fail(void) { return calls++ == fail_at; }
void* asn1_test_malloc(size_t n) {
  if (fail()) return NULL;
  void* p = malloc(n);
  if (p) ++live;
  return p;
}
void* asn1_test_calloc(size_t n, size_t size) {
  if (fail()) return NULL;
  void* p = calloc(n, size);
  if (p) ++live;
  return p;
}
void* asn1_test_realloc(void* ptr, size_t n) {
  if (fail()) return NULL;
  const int fresh = ptr == NULL;
  void* p = realloc(ptr, n);
  if (p && fresh) ++live;
  return p;
}
void asn1_test_free(void* ptr) {
  if (ptr) { CHECK(live != 0); --live; }
  free(ptr);
}
static void inject(size_t at) { calls = 0; fail_at = at; }
static void normal(void) { fail_at = SIZE_MAX; calls = 0; }

static void writer_failures(void) {
  uint8_t data[160]; memset(data, 0xA5, sizeof(data));
  for (int op = 0; op < 5; ++op) {
    for (int filled = 0; filled < 2; ++filled) {
      normal();
      asn1_bit_writer_t* w = NULL;
      CHECK(asn1_bit_writer_create(&w) == ASN1_OK);
      if (filled) CHECK(asn1_bit_writer_put_octets(w, asn1_byte_view(data, 16)) == ASN1_OK);
      size_t before = asn1_bit_writer_bit_size(w);
      inject(0);
      asn1_status_t s;
      switch (op) {
        case 0: s = asn1_bit_writer_put_bit(w, 1); break;
        case 1: s = asn1_bit_writer_put_bits(w, UINT64_MAX, 64); break;
        case 2: s = asn1_bit_writer_put_octets(w, asn1_byte_view(data, sizeof(data))); break;
        case 3: s = asn1_bit_writer_put_bits_from_bytes(w, asn1_byte_view(data, sizeof(data)), 129); break;
        default: s = asn1_bit_writer_put_bitmap(w, asn1_byte_view(data, sizeof(data))); break;
      }
      CHECK(s == ASN1_ERR_OUT_OF_MEMORY);
      CHECK(asn1_bit_writer_bit_size(w) == before);
      normal();
      CHECK(asn1_bit_writer_put_octet(w, 0x3C) == ASN1_OK);
      uint8_t* bytes = NULL; size_t n = 0;
      CHECK(asn1_bit_writer_take(w, &bytes, &n) == ASN1_OK);
      CHECK(n == (filled ? 17u : 1u));
      CHECK(bytes[n - 1] == 0x3C);
      if (filled) CHECK(memcmp(data, bytes, 16) == 0);
      asn1_test_free(bytes);
      CHECK(asn1_bit_writer_put_bit(w, 0) == ASN1_OK);
      inject(0);
      bytes = data; n = 77;
      CHECK(asn1_bit_writer_take(w, &bytes, &n) == ASN1_ERR_OUT_OF_MEMORY);
      CHECK(bytes == data && n == 77 && asn1_bit_writer_bit_size(w) == 1);
      normal();
      CHECK(asn1_bit_writer_take(w, &bytes, &n) == ASN1_OK);
      CHECK(n == 1 && bytes[0] == 0);
      asn1_test_free(bytes);
      CHECK(asn1_bit_writer_put_octets(w, asn1_byte_view(data, SIZE_MAX / 8u + 1u)) == ASN1_ERR_LENGTH_OVERFLOW);
      CHECK(asn1_bit_writer_put_bit(w, 1) == ASN1_OK);
      inject(0);
      CHECK(asn1_bit_writer_put_bitmap(w, asn1_byte_view(data, SIZE_MAX)) == ASN1_ERR_LENGTH_OVERFLOW);
      CHECK(calls == 0 && asn1_bit_writer_bit_size(w) == 1);
      normal();
      asn1_bit_writer_destroy(w);
      CHECK(live == 0);
    }
  }
  asn1_bit_writer_t* w = NULL;
  inject(0);
  CHECK(asn1_bit_writer_create(&w) == ASN1_ERR_OUT_OF_MEMORY && w == NULL);
  normal();
}

static void bigint_failures(void) {
  char decimal[601]; memset(decimal, '9', 600); decimal[600] = 0;
  size_t allocations = 0;
  for (size_t at = 0; ; ++at) {
    asn1_bigint_t b; asn1_bigint_init(&b);
    inject(at);
    asn1_status_t s = asn1_bigint_init_from_decimal(&b, decimal, 0);
    if (s == ASN1_OK) {
      allocations = calls;
      asn1_bigint_free(&b); CHECK(live == 0); break;
    }
    CHECK(s == ASN1_ERR_OUT_OF_MEMORY);
    CHECK(b.limbs == NULL && b.nlimbs == 0 && b.cap == 0 && b.sign == 0);
    asn1_bigint_free(&b); CHECK(live == 0);
    CHECK(at < 32);
  }
  CHECK(allocations > 1);
  normal();
  asn1_bigint_t b; asn1_bigint_init(&b);
  CHECK(asn1_bigint_init_from_decimal(&b, "340282366920938463463374607431768211456", 0) == ASN1_OK);
  CHECK(b.nlimbs == 5 && b.limbs[4] == 1);
  for (size_t at = 0; at < 2; ++at) {
    uint8_t sentinel; uint8_t* out = &sentinel; size_t n = 77;
    size_t before = live; inject(at);
    CHECK(asn1_bigint_to_bytes(&b, &out, &n) == ASN1_ERR_OUT_OF_MEMORY);
    CHECK(out == &sentinel && n == 77 && live == before);
  }
  normal(); asn1_bigint_free(&b); asn1_bigint_free(&b);
  CHECK(asn1_bigint_init_from_decimal(&b, "-9223372036854775808", 0) == ASN1_OK);
  int64_t value = 0;
  CHECK(asn1_bigint_to_int64(&b, &value) == ASN1_OK && value == INT64_MIN);
  asn1_bigint_free(&b);
  CHECK(asn1_bigint_init_from_decimal(&b, "000", 0) == ASN1_OK && b.sign == 0);
  asn1_bigint_free(&b);
  CHECK(asn1_bigint_init_from_decimal(&b, "12x", 0) == ASN1_ERR_INVALID_ARGUMENT && b.limbs == NULL);
  CHECK(asn1_bigint_init_from_decimal(&b, "", 0) == ASN1_ERR_INVALID_ARGUMENT);
  /* floor(log10(2^32768)) + 1 is 9865: 9864 nines fit; 9865 exceed the cap. */
  char* huge = (char*)malloc(9866); CHECK(huge != NULL);
  memset(huge, '9', 9865); huge[9864] = 0;
  CHECK(asn1_bigint_init_from_decimal(&b, huge, 0) == ASN1_OK);
  CHECK(b.nlimbs == ASN1_MAX_BIGINT_LIMBS);
  asn1_bigint_free(&b); huge[9864] = '9'; huge[9865] = 0;
  CHECK(asn1_bigint_init_from_decimal(&b, huge, 0) == ASN1_ERR_RESOURCE_LIMIT);
  CHECK(b.limbs == NULL); asn1_bigint_free(&b); free(huge);
  CHECK(live == 0);
}

static void reader_failures(void) {
  uint8_t data[2] = {0xAB, 0xCD};
  for (size_t at = 0; at < 2; ++at) {
    asn1_bit_reader_t* r = NULL;
    inject(at);
    CHECK(asn1_bit_reader_create(&r, asn1_byte_view(data, 2)) == ASN1_ERR_OUT_OF_MEMORY);
    CHECK(r == NULL && live == 0);
    asn1_byte_reader_t* br = NULL; inject(at);
    CHECK(asn1_byte_reader_create(&br, asn1_byte_view(data, 2)) == ASN1_ERR_OUT_OF_MEMORY);
    CHECK(br == NULL && live == 0);
  }
  normal();
  asn1_bit_reader_t* r = NULL;
  CHECK(asn1_bit_reader_create(&r, asn1_byte_view(NULL, 1)) == ASN1_ERR_INVALID_ARGUMENT);
  CHECK(asn1_bit_reader_create(&r, asn1_byte_view(data, SIZE_MAX)) == ASN1_ERR_LENGTH_OVERFLOW);
  CHECK(asn1_bit_reader_create(&r, asn1_byte_view(data, 2)) == ASN1_OK);
  data[0] = 0; /* reader owns its copy */
  for (int bits = 0; bits < 2; ++bits) {
    uint8_t* out = data; size_t n = 77;
    inject(0);
    asn1_status_t s = bits ? asn1_bit_reader_get_bits_as_bytes(r, 9, &out, &n)
                          : asn1_bit_reader_get_octets(r, 2, &out, &n);
    CHECK(s == ASN1_ERR_OUT_OF_MEMORY && out == data && n == 77);
    CHECK(asn1_bit_reader_bit_offset(r) == 0);
  }
  normal();
  uint8_t* out = data; size_t n = 77;
  CHECK(asn1_bit_reader_get_octets(r, 3, &out, &n) == ASN1_ERR_TRUNCATED);
  CHECK(out == data && n == 77 && asn1_bit_reader_bit_offset(r) == 0);
  CHECK(asn1_bit_reader_get_octets(r, ASN1_MAX_STRING_BYTES + 1u, &out, &n) == ASN1_ERR_RESOURCE_LIMIT);
  uint64_t value = 77;
  CHECK(asn1_bit_reader_get_bits(r, 17, &value) == ASN1_ERR_TRUNCATED && value == 77);
  CHECK(asn1_bit_reader_get_octets(r, 2, &out, &n) == ASN1_OK);
  CHECK(n == 2 && out[0] == 0xAB && out[1] == 0xCD);
  asn1_test_free(out); asn1_bit_reader_destroy(r); CHECK(live == 0);
}

static void output_contracts(void) {
  normal();
  asn1_bitstring_t bits; asn1_bitstring_init(&bits);
  CHECK(asn1_bitstring_resize(&bits, 8) == ASN1_OK); bits.bits[0] = 0x5A;
  uint8_t* original = bits.bits; size_t before = live;
  inject(0);
  CHECK(asn1_bitstring_resize(&bits, 1024) == ASN1_ERR_OUT_OF_MEMORY);
  CHECK(bits.bits == original && bits.bit_length == 8 && bits.bits[0] == 0x5A && live == before);
  normal();
  CHECK(asn1_bitstring_resize(&bits, (size_t)ASN1_MAX_STRING_BYTES * 8u + 1u) == ASN1_ERR_RESOURCE_LIMIT);
  asn1_bit_reader_t* r = NULL;
  CHECK(asn1_bit_reader_create(&r, asn1_byte_view(NULL, 0)) == ASN1_OK);
  asn1_per_size_range_t range = {16, 16};
  CHECK(asn1_per_decode_bitstring(r, &range, &bits) == ASN1_ERR_TRUNCATED);
  CHECK(bits.bits == original && bits.bit_length == 8 && bits.bits[0] == 0x5A);
  asn1_bit_reader_destroy(r); asn1_bitstring_free(&bits); asn1_bitstring_free(&bits);
  const uint8_t high[] = {0x1F, 0x81};
  asn1_ber_tag_t tag = {2, 1, 777}; size_t consumed = 77;
  CHECK(asn1_ber_tag_decode(asn1_byte_view(high, 2), &tag, &consumed) == ASN1_ERR_TRUNCATED);
  CHECK(tag.cls == 2 && tag.constructed == 1 && tag.number == 777 && consumed == 77);
  uint8_t buf[3] = {0xAA, 0xAA, 0xAA}; size_t n = 77;
  CHECK(asn1_ber_tag_encode(buf, 2, &tag, &n) == ASN1_ERR_LENGTH_OVERFLOW);
  CHECK(buf[0] == 0xAA && buf[1] == 0xAA && n == 77);
  const uint8_t length[] = {0x83, 1, 0, 1}; size_t len = 77;
  CHECK(asn1_ber_length_decode(asn1_byte_view(length, 4), &len, &consumed) == ASN1_ERR_RESOURCE_LIMIT);
  CHECK(len == 77 && consumed == 77);
  asn1_jer_reader_t jr = {"old", 1, 3};
  CHECK(asn1_jer_reader_init(&jr, NULL, 1) == ASN1_ERR_INVALID_ARGUMENT && jr.pos == 1);
  asn1_xer_reader_t xr = {"old", 1, 3};
  CHECK(asn1_xer_reader_init(&xr, NULL, 1) == ASN1_ERR_INVALID_ARGUMENT && xr.pos == 1);
  asn1_byte_reader_t* br=NULL;
  CHECK(asn1_byte_reader_create(&br,asn1_byte_view(NULL,1))==ASN1_ERR_INVALID_ARGUMENT && br==NULL);
  char text[4] = {0}; asn1_jer_writer_t jw;
  CHECK(asn1_jer_writer_init(&jw,text,sizeof(text))==ASN1_OK);
  CHECK(asn1_jer_writer_puts(&jw,"A")==ASN1_OK);
  CHECK(asn1_jer_writer_puts(&jw,"overflow")==ASN1_ERR_LENGTH_OVERFLOW && jw.used==1 && text[0]=='A');
  CHECK(asn1_jer_writer_printf(&jw,"%s","overflow")==ASN1_ERR_LENGTH_OVERFLOW && jw.used==1 && text[0]=='A');
  asn1_xer_writer_t xw;
  CHECK(asn1_xer_writer_init(&xw,text,sizeof(text))==ASN1_OK);
  CHECK(asn1_xer_writer_puts(&xw,"A")==ASN1_OK);
  CHECK(asn1_xer_writer_puts(&xw,"overflow")==ASN1_ERR_LENGTH_OVERFLOW && xw.used==1 && text[0]=='A');
  CHECK(live == 0);
}

static void unaligned_writer_failures(void) {
  uint8_t data[32]; memset(data, 0xA5, sizeof(data));
  for (int op = 0; op < 4; ++op) {
    normal(); asn1_bit_writer_t* w = NULL;
    CHECK(asn1_bit_writer_create(&w) == ASN1_OK);
    CHECK(asn1_bit_writer_put_octets(w, asn1_byte_view(data, 15)) == ASN1_OK);
    CHECK(asn1_bit_writer_put_bits(w, 0x7F, 7) == ASN1_OK);
    inject(0);
    asn1_status_t s;
    switch (op) {
      case 0: s = asn1_bit_writer_put_bits(w, UINT64_MAX, 64); break;
      case 1: s = asn1_bit_writer_put_octets(w, asn1_byte_view(data, 2)); break;
      case 2: s = asn1_bit_writer_put_bits_from_bytes(w, asn1_byte_view(data, 2), 9); break;
      default: s = asn1_bit_writer_put_bitmap(w, asn1_byte_view(data, 2)); break;
    }
    CHECK(s == ASN1_ERR_OUT_OF_MEMORY && asn1_bit_writer_bit_size(w) == 127);
    normal(); uint8_t* out = NULL; size_t n = 0;
    CHECK(asn1_bit_writer_take(w, &out, &n) == ASN1_OK);
    CHECK(n == 16 && memcmp(out, data, 15) == 0 && out[15] == 0xFE);
    asn1_test_free(out); asn1_bit_writer_destroy(w); CHECK(live == 0);
  }
  asn1_bigint_t zero; asn1_bigint_init(&zero);
  uint8_t* out = data; size_t n = 77;
  inject(0);
  CHECK(asn1_bigint_to_bytes(&zero, &out, &n) == ASN1_ERR_OUT_OF_MEMORY);
  CHECK(out == data && n == 77 && live == 0);
  normal();
  asn1_bit_writer_t* w = NULL;
  CHECK(asn1_bit_writer_create(&w) == ASN1_OK);
  inject(0);
  CHECK(asn1_bit_writer_take(w, &out, &n) == ASN1_ERR_OUT_OF_MEMORY);
  CHECK(out == data && n == 77 && asn1_bit_writer_bit_size(w) == 0);
  normal(); asn1_bit_writer_destroy(w); CHECK(live == 0);
}

static void per_allocation_failures(void) {
  uint8_t bytes[34]; bytes[0] = 33; memset(bytes + 1, 0x11, 33);
  for (size_t at = 0; ; ++at) {
    normal(); asn1_bit_reader_t* r = NULL;
    CHECK(asn1_bit_reader_create(&r, asn1_byte_view(bytes, sizeof(bytes))) == ASN1_OK);
    size_t baseline = live;
    asn1_bigint_t out; asn1_bigint_init(&out);
    inject(at);
    asn1_status_t s = asn1_per_decode_unbounded_int(r, &out);
    if (s == ASN1_OK) {
      CHECK(out.nlimbs == 9);
      asn1_bigint_free(&out); asn1_bit_reader_destroy(r); CHECK(live == 0); break;
    }
    CHECK(s == ASN1_ERR_OUT_OF_MEMORY && live == baseline);
    CHECK(out.limbs == NULL && out.nlimbs == 0);
    asn1_bigint_free(&out); asn1_bit_reader_destroy(r); CHECK(live == 0);
    CHECK(at < 16);
  }
  normal();
  {
    const uint8_t preamble_wire=0xA0;
    asn1_bit_reader_t* r=NULL;
    CHECK(asn1_bit_reader_create(&r,asn1_byte_view(&preamble_wire,1))==ASN1_OK);
    asn1_per_sequence_preamble_t out={1,(uint8_t*)(uintptr_t)1,77};
    size_t baseline=live; inject(0);
    CHECK(asn1_per_decode_sequence_preamble(r,0,3,&out)==ASN1_ERR_OUT_OF_MEMORY);
    CHECK(out.extensions_present==1 && out.optional_bits==(uint8_t*)(uintptr_t)1 &&
          out.optional_count==77 && live==baseline);
    normal(); asn1_bit_reader_destroy(r); CHECK(live==0);
  }
  uint8_t zero = 0;
  for (size_t at = 0; at < 2; ++at) {
    asn1_bit_reader_t* r = NULL;
    CHECK(asn1_bit_reader_create(&r, asn1_byte_view(&zero, 1)) == ASN1_OK);
    asn1_bitstring_t out; asn1_bitstring_init(&out);
    CHECK(asn1_bitstring_resize(&out, 1) == ASN1_OK); out.bits[0] = 0x80;
    size_t baseline = live; uint8_t* old = out.bits;
    asn1_per_size_range_t range = {8, 8}; inject(at);
    CHECK(asn1_per_decode_bitstring(r, &range, &out) == ASN1_ERR_OUT_OF_MEMORY);
    CHECK(out.bits == old && out.bit_length == 1 && out.bits[0] == 0x80 && live == baseline);
    normal(); asn1_bitstring_free(&out); asn1_bit_reader_destroy(r);
  }
  CHECK(live == 0);
}

static void signed_and_fragment_failures(void) {
  normal();
  asn1_bigint_t b; asn1_bigint_init(&b);
  CHECK(asn1_bigint_init_from_decimal(&b,"-18446744073709551616",0)==ASN1_OK);
  size_t baseline=live;
  for(size_t at=0;;++at) {
    uint8_t sentinel=0; uint8_t* out=&sentinel; size_t n=77;
    inject(at);
    asn1_status_t s=asn1_bigint_to_twos_complement(&b,&out,&n);
    if(s==ASN1_OK) { CHECK(n==9 && out[0]==0xFF && out[1]==0); asn1_test_free(out); break; }
    CHECK(s==ASN1_ERR_OUT_OF_MEMORY && out==&sentinel && n==77 && live==baseline);
    CHECK(at<8);
  }
  normal(); asn1_bigint_free(&b); CHECK(live==0);
  /* Fail after the first fragment has already been decoded and aggregated. */
  uint8_t* wire=(uint8_t*)malloc(16387); CHECK(wire!=NULL);
  wire[0]=0xC1; memset(wire+1,0xA5,16384); wire[16385]=1; wire[16386]=0xBB;
  const asn1_per_length_constraint_t c={ASN1_PER_LENGTH_UNCONSTRAINED,0,0,0};
  for(size_t at=0;;++at) {
    normal(); asn1_bit_reader_t* r=NULL;
    CHECK(asn1_bit_reader_create(&r,asn1_byte_view(wire,16387))==ASN1_OK); baseline=live;
    uint8_t* out=NULL; size_t n=77; inject(at);
    asn1_status_t s=asn1_per_decode_octetstring_ex(r,ASN1_PER_UNALIGNED,&c,&out,&n);
    if(s==ASN1_OK) { CHECK(n==16385 && out[16384]==0xBB); asn1_test_free(out); asn1_bit_reader_destroy(r); break; }
    CHECK(s==ASN1_ERR_OUT_OF_MEMORY && out==NULL && n==77 && live==baseline);
    asn1_bit_reader_destroy(r); CHECK(live==0); CHECK(at<16);
  }
  free(wire); normal(); CHECK(live==0);
}

int main(void) {
  CHECK(ASN1_ERR_UNSUPPORTED == 7 && ASN1_ERR_LENGTH_OVERFLOW == 3);
  CHECK(strcmp(asn1_status_name(ASN1_ERR_OUT_OF_MEMORY), "ASN1_ERR_OUT_OF_MEMORY") == 0);
  CHECK(strcmp(asn1_status_name(ASN1_ERR_RESOURCE_LIMIT), "ASN1_ERR_RESOURCE_LIMIT") == 0);
  writer_failures(); unaligned_writer_failures(); bigint_failures();
  reader_failures(); output_contracts(); per_allocation_failures(); signed_and_fragment_failures();
  CHECK(live == 0);
  printf("C runtime failure contracts: %u checks passed\n", checks);
  return 0;
}
