#include "generated_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static size_t calls,fail_at=SIZE_MAX,live;
static int fails(void){return calls++==fail_at;}
void* asn1_test_malloc(size_t n){if(fails())return NULL;void* p=malloc(n);if(p)++live;return p;}
void* asn1_test_calloc(size_t n,size_t s){if(fails())return NULL;void* p=calloc(n,s);if(p)++live;return p;}
void* asn1_test_realloc(void* p,size_t n){if(fails())return NULL;const int fresh=p==NULL;void* q=realloc(p,n);if(q&&fresh)++live;return q;}
void asn1_test_free(void* p){if(p){if(!live)abort();--live;}free(p);}
void* asn1_test_generated_malloc(size_t n){return asn1_test_malloc(n);}
void* asn1_test_generated_calloc(size_t n,size_t s){return asn1_test_calloc(n,s);}
void* asn1_test_generated_realloc(void* p,size_t n){return asn1_test_realloc(p,n);}
void asn1_test_generated_free(void* p){asn1_test_free(p);}
#define CHECK(x) do {if(!(x)){fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x);return 1;}}while(0)
static int exercise(const asn1_tlv_schema_t* schema,const void* input,asn1_tlv_policy_t policy) {
  asn1_byte_writer_t* writer=NULL;CHECK(asn1_byte_writer_create(&writer)==ASN1_OK);
  CHECK(asn1_tlv_schema_encode(writer,policy,schema,input)==ASN1_OK);uint8_t* wire=NULL;size_t size=0;CHECK(asn1_byte_writer_take(writer,&wire,&size)==ASN1_OK);asn1_byte_writer_destroy(writer);
  size_t failures=0;
  for(size_t n=0;n<1024;++n){
    asn1_byte_reader_t* reader=NULL;CHECK(asn1_byte_reader_create(&reader,asn1_byte_view(wire,size))==ASN1_OK);
    void* output=calloc(1,schema->size);CHECK(output);schema->init(output);void* snapshot=malloc(schema->size);CHECK(snapshot);memcpy(snapshot,output,schema->size);
    const size_t baseline=live;calls=0;fail_at=n;asn1_status_t status=asn1_tlv_schema_decode(reader,policy,schema,output);fail_at=SIZE_MAX;
    if(status!=ASN1_OK){CHECK(status==ASN1_ERR_OUT_OF_MEMORY);CHECK(!memcmp(output,snapshot,schema->size));CHECK(asn1_byte_reader_position(reader)==0 && live==baseline);++failures;}
    else CHECK(asn1_byte_reader_position(reader)==size);
    schema->clear(output);free(output);free(snapshot);asn1_byte_reader_destroy(reader);
    if(status==ASN1_OK)break;
  }
  CHECK(failures>0 && failures<1024);failures=0;
  for(size_t n=0;n<1024;++n){
    CHECK(asn1_byte_writer_create(&writer)==ASN1_OK);const uint8_t marker=0xa5;CHECK(asn1_byte_writer_append(writer,asn1_byte_view(&marker,1))==ASN1_OK);
    const size_t baseline=live;calls=0;fail_at=n;asn1_status_t status=asn1_tlv_schema_encode(writer,policy,schema,input);fail_at=SIZE_MAX;
    if(status!=ASN1_OK){CHECK(status==ASN1_ERR_OUT_OF_MEMORY);CHECK(asn1_byte_writer_view(writer).size==1 && asn1_byte_writer_view(writer).data[0]==marker && live==baseline);++failures;}
    asn1_byte_writer_destroy(writer);if(status==ASN1_OK)break;
  }
  CHECK(failures>0 && failures<1024);asn1_test_free(wire);return 0;
}
int main(void){
  cgen5_mC5_tRadioDefault radio;cgen5_mC5_tRadioDefault_init(&radio);CHECK(cgen5_mC5_tRadioDefault_reset_defaults(&radio)==ASN1_OK);radio.ftail.value=1;
  CHECK(exercise(cgen5_mC5_tRadioDefault_tlv_schema(),&radio,ASN1_TLV_DER)==0);cgen5_mC5_tRadioDefault_free(&radio);CHECK(live==0);
  cgen5_mC5_tNumbers numbers;cgen5_mC5_tNumbers_init(&numbers);numbers.data=asn1_test_calloc(3,sizeof(*numbers.data));CHECK(numbers.data);numbers.count=numbers.capacity=3;numbers.data[0].value=3;numbers.data[1].value=-10;
  CHECK(exercise(cgen5_mC5_tNumbers_tlv_schema(),&numbers,ASN1_TLV_DER)==0);cgen5_mC5_tNumbers_free(&numbers);CHECK(live==0);
  cgen5_mC5_tTree tree;cgen5_mC5_tTree_init(&tree);tree.fvalue.value=7;tree.fchildren=asn1_test_calloc(1,sizeof(*tree.fchildren));CHECK(tree.fchildren);
  tree.fchildren->data=asn1_test_calloc(2,sizeof(*tree.fchildren->data));CHECK(tree.fchildren->data);tree.fchildren->count=tree.fchildren->capacity=2;
  for(size_t i=0;i<2;++i){tree.fchildren->data[i].fvalue.value=(int64_t)i;tree.fchildren->data[i].fchildren=asn1_test_calloc(1,sizeof(*tree.fchildren));CHECK(tree.fchildren->data[i].fchildren);}
  CHECK(exercise(cgen5_mC5_tTree_tlv_schema(),&tree,ASN1_TLV_BER)==0);cgen5_mC5_tTree_free(&tree);CHECK(live==0);
  cgen5_mC5_tBlob blob;cgen5_mC5_tBlob_init(&blob);blob.data=asn1_test_malloc(2001);CHECK(blob.data);memset(blob.data,0x5a,2001);blob.count=blob.capacity=2001;
  CHECK(exercise(cgen5_mC5_tBlob_tlv_schema(),&blob,ASN1_TLV_CER)==0);cgen5_mC5_tBlob_free(&blob);CHECK(live==0);
  puts("C TLV PDU allocation faults preserve live state at every allocation PASS");return 0;
}
