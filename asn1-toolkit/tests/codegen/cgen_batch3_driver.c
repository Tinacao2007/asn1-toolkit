#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <asn1/c_runtime/per.h>
static int fail_next;
void *stage3_test_malloc(size_t n) {
  if (fail_next) { fail_next=0; return NULL; }
  return malloc(n);
}
void *stage3_test_calloc(size_t n, size_t size) {
  if (fail_next) { fail_next=0; return NULL; }
  return calloc(n,size);
}
void stage3_test_free(void *p) { free(p); }
#define malloc stage3_test_malloc
#define calloc stage3_test_calloc
#define free stage3_test_free
#include "generated.h"
#undef malloc
#undef calloc
#undef free
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"line %d: %s\n",__LINE__,#x); return 1; } } while(0)
int second(void);
int main(void) {
  stage3_mA_tOwningDefaults a,b;
  stage3_mB_tNode node,copy;
  stage3_mA_tDefaults scalar;
  asn1_bit_writer_t *writer=NULL;
  uint8_t *wire=NULL; size_t size=0;
  const asn1_per_int_range_t range={0,1,127,1,0};
  CHECK(second()==0);
  stage3_mA_tOwningDefaults_init(&a); stage3_mA_tOwningDefaults_init(&b);
  CHECK(stage3_mA_tOwningDefaults_reset_defaults(&a)==ASN1_OK);
  CHECK(stage3_mA_tOwningDefaults_copy(&a,&b)==ASN1_OK);
  CHECK(a.ftext.data!=b.ftext.data && b.ftext.count==4 && memcmp(b.ftext.data,"demo",4)==0);
  CHECK(b.fnumber.value.nlimbs==5 && b.fnumber.value.limbs[4]==1);
  CHECK(b.fbits.value.bit_length==5 && b.fbits.value.bits[0]==0xa8);
  CHECK(b.foctets.count==2 && b.foctets.data[0]==0xab && b.foctets.data[1]==0xcd);
  fail_next=1;
  CHECK(stage3_mA_tOwningDefaults_copy(&a,&b)==ASN1_ERR_OUT_OF_MEMORY);
  CHECK(b.ftext.count==4 && memcmp(b.ftext.data,"demo",4)==0);
  CHECK(stage3_mA_tOwningDefaults_copy(&b,&b)==ASN1_OK);
  stage3_mA_tOwningDefaults_free(&a); stage3_mA_tOwningDefaults_free(&b);
  stage3_mA_tOwningDefaults_free(&b);
  stage3_mB_tNode_init(&node); stage3_mB_tNode_init(&copy);
  node.has_fnext=1; node.fnext.has_fnode=1;
  node.fnext.fnode=malloc(sizeof(*node.fnext.fnode)); CHECK(node.fnext.fnode);
  node.fnext.fnode->has_fnext=0; node.fnext.fnode->fnext=NULL;
  CHECK(stage3_mB_tNode_copy(&node,&copy)==ASN1_OK);
  CHECK(copy.fnext.fnode && copy.fnext.fnode!=node.fnext.fnode);
  stage3_mB_tNode_free(&node); stage3_mB_tNode_free(&copy);
  stage3_mA_tDefaults_init(&scalar);
  CHECK(scalar.fenabled.value==1 && scalar.fnumber.value==7);
  /* Existing primitive integration only, not a generated PDU codec. */
  CHECK(asn1_bit_writer_create(&writer)==ASN1_OK);
  CHECK(asn1_per_encode_bool(writer,scalar.fenabled.value)==ASN1_OK);
  CHECK(asn1_per_encode_int(writer,&range,scalar.fnumber.value)==ASN1_OK);
  CHECK(asn1_bit_writer_take(writer,&wire,&size)==ASN1_OK);
  CHECK(size==1 && wire[0]==0x87);
  printf("%02x\n",wire[0]); free(wire); asn1_bit_writer_destroy(writer);
  return 0;
}
