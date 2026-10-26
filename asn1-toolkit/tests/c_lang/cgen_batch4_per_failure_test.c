#include "generated.h"
#include <stdio.h>
#include <stdlib.h>

static size_t calls, fail_at = SIZE_MAX, live_allocations;
static int should_fail(void) { return calls++ == fail_at; }
void* asn1_test_malloc(size_t n) {
  if (should_fail()) return NULL;
  void* p = malloc(n); if (p) ++live_allocations; return p;
}
void* asn1_test_calloc(size_t n, size_t size) {
  if (should_fail()) return NULL;
  void* p = calloc(n,size); if (p) ++live_allocations; return p;
}
void* asn1_test_realloc(void* old, size_t n) {
  if (should_fail()) return NULL;
  const int fresh=old==NULL; void* p=realloc(old,n);
  if (p && fresh) ++live_allocations; return p;
}
void asn1_test_free(void* p) { if(p) { if(!live_allocations) abort(); --live_allocations; } free(p); }
void* asn1_test_generated_malloc(size_t n) { return asn1_test_malloc(n); }
void* asn1_test_generated_calloc(size_t n,size_t size) { return asn1_test_calloc(n,size); }
void* asn1_test_generated_realloc(void* p,size_t n) { return asn1_test_realloc(p,n); }
void asn1_test_generated_free(void* p) { asn1_test_free(p); }

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x); return 1; } } while (0)

int main(void) {
  const uint8_t wire[]={0xB6};
  asn1_bit_reader_t* reader=NULL;
  CHECK(asn1_bit_reader_create(&reader,asn1_byte_view(wire,sizeof(wire)))==ASN1_OK);
  const size_t baseline=live_allocations;
  cgen4_mM_tPdu value; cgen4_mM_tPdu_init(&value);
  value.fflag.value=0; value.fcount.value=3;
  calls=0; fail_at=0;
  CHECK(cgen4_mM_tPdu_decode_uper(reader,&value)==ASN1_ERR_OUT_OF_MEMORY);
  CHECK(value.fflag.value==0 && value.fcount.value==3 && live_allocations==baseline);
  fail_at=SIZE_MAX; calls=0;
  asn1_bit_reader_destroy(reader); cgen4_mM_tPdu_free(&value);
  CHECK(live_allocations==0);

  /* The collection uses two PER length fragments. Inject failure at both
     generated growth allocations and verify output/lifetimes stay sound. */
  const size_t item_count=16385u;
  cgen4_mM_tLargeFlags input; cgen4_mM_tLargeFlags_init(&input);
  input.data=asn1_test_calloc(item_count,sizeof(*input.data)); CHECK(input.data!=NULL);
  input.count=input.capacity=item_count;
  for(size_t i=0;i<item_count;++i) input.data[i].value=(int)(i&1u);
  asn1_bit_writer_t* writer=NULL; CHECK(asn1_bit_writer_create(&writer)==ASN1_OK);
  CHECK(cgen4_mM_tLargeFlags_encode_uper(writer,&input)==ASN1_OK);
  uint8_t* collection_wire=NULL; size_t wire_size=0; CHECK(asn1_bit_writer_take(writer,&collection_wire,&wire_size)==ASN1_OK);
  asn1_bit_writer_destroy(writer); cgen4_mM_tLargeFlags_free(&input);
  CHECK(live_allocations==1); /* collection_wire remains caller-owned */
  for(size_t failure=0;failure<2u;++failure) {
    CHECK(asn1_bit_reader_create(&reader,asn1_byte_view(collection_wire,wire_size))==ASN1_OK);
    cgen4_mM_tLargeFlags output; cgen4_mM_tLargeFlags_init(&output);
    output.data=asn1_test_calloc(1,sizeof(*output.data)); CHECK(output.data!=NULL);
    output.count=output.capacity=1; output.data[0].value=1;
    const size_t before=live_allocations; calls=0; fail_at=failure;
    CHECK(cgen4_mM_tLargeFlags_decode_uper(reader,&output)==ASN1_ERR_OUT_OF_MEMORY);
    CHECK(output.count==1 && output.data[0].value==1 && live_allocations==before);
    fail_at=SIZE_MAX; calls=0; cgen4_mM_tLargeFlags_free(&output);
    asn1_bit_reader_destroy(reader); CHECK(live_allocations==1);
  }
  CHECK(asn1_bit_reader_create(&reader,asn1_byte_view(collection_wire,wire_size))==ASN1_OK);
  cgen4_mM_tLargeFlags output; cgen4_mM_tLargeFlags_init(&output);
  calls=0; fail_at=SIZE_MAX;
  CHECK(cgen4_mM_tLargeFlags_decode_uper(reader,&output)==ASN1_OK);
  CHECK(output.count==item_count && calls==2);
  cgen4_mM_tLargeFlags_free(&output); asn1_bit_reader_destroy(reader); asn1_test_free(collection_wire);
  CHECK(live_allocations==0);
  puts("Generated C PDU: allocator failure preserves output and releases temporary state PASS");
  return 0;
}
