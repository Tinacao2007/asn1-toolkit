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
static int exercise(const asn1_codec_schema_t* schema,const void* input,asn1_schema_codec_t policy) {
  asn1_byte_writer_t* writer=NULL;CHECK(asn1_byte_writer_create(&writer)==ASN1_OK);
  CHECK(asn1_schema_encode(writer,policy,schema,input)==ASN1_OK);uint8_t* wire=NULL;size_t size=0;CHECK(asn1_byte_writer_take(writer,&wire,&size)==ASN1_OK);asn1_byte_writer_destroy(writer);
  size_t failures=0;
  for(size_t n=0;n<1024;++n){
    asn1_byte_reader_t* reader=NULL;CHECK(asn1_byte_reader_create(&reader,asn1_byte_view(wire,size))==ASN1_OK);
    void* output=calloc(1,schema->layout()->size);CHECK(output);schema->layout()->init(output);void* snapshot=malloc(schema->layout()->size);CHECK(snapshot);memcpy(snapshot,output,schema->layout()->size);
    const size_t baseline=live;calls=0;fail_at=n;asn1_status_t status=asn1_schema_decode(reader,policy,schema,output);fail_at=SIZE_MAX;
    if(status!=ASN1_OK){CHECK(status==ASN1_ERR_OUT_OF_MEMORY);CHECK(!memcmp(output,snapshot,schema->layout()->size));CHECK(asn1_byte_reader_position(reader)==0 && live==baseline);++failures;}
    else CHECK(asn1_byte_reader_position(reader)==size);
    schema->layout()->clear(output);free(output);free(snapshot);asn1_byte_reader_destroy(reader);
    if(status==ASN1_OK)break;
  }
  CHECK(failures>0 && failures<1024);failures=0;
  for(size_t n=0;n<1024;++n){
    CHECK(asn1_byte_writer_create(&writer)==ASN1_OK);const uint8_t marker=0xa5;CHECK(asn1_byte_writer_append(writer,asn1_byte_view(&marker,1))==ASN1_OK);
    const size_t baseline=live;calls=0;fail_at=n;asn1_status_t status=asn1_schema_encode(writer,policy,schema,input);fail_at=SIZE_MAX;
    if(status!=ASN1_OK){CHECK(status==ASN1_ERR_OUT_OF_MEMORY);CHECK(asn1_byte_writer_view(writer).size==1 && asn1_byte_writer_view(writer).data[0]==marker && live==baseline);++failures;}
    asn1_byte_writer_destroy(writer);if(status==ASN1_OK)break;
  }
  CHECK(failures>0 && failures<1024);asn1_test_free(wire);return 0;
}
int main(void){
  for(int codec=ASN1_CODEC_JER;codec<=ASN1_CODEC_EXER;++codec){
    const char* wire=codec<=ASN1_CODEC_JERI?"{\"value\":42,\"criticality\":\"ignore\",\"id\":0}":"<HandoverIE><id>0</id><criticality><ignore/></criticality><value><MME-UE-S1AP-ID>42</MME-UE-S1AP-ID></value></HandoverIE>";
    c7_mTCMod_tHandoverIE value;c7_mTCMod_tHandoverIE_init(&value);asn1_byte_reader_t* reader=NULL;
    CHECK(asn1_byte_reader_create(&reader,asn1_byte_view((const uint8_t*)wire,strlen(wire)))==ASN1_OK);
    CHECK(asn1_schema_decode(reader,(asn1_schema_codec_t)codec,c7_mTCMod_tHandoverIE_codec_schema(),&value)==ASN1_OK);asn1_byte_reader_destroy(reader);
    CHECK(exercise(c7_mTCMod_tHandoverIE_codec_schema(),&value,(asn1_schema_codec_t)codec)==0);c7_mTCMod_tHandoverIE_free(&value);CHECK(live==0);
  }
  for(int codec=ASN1_CODEC_OER;codec<=ASN1_CODEC_EXER;++codec){
    c7_mC7_tFixed value;c7_mC7_tFixed_init(&value);CHECK(asn1_bitstring_resize(&value.value,8)==ASN1_OK);value.value.bits[0]=128;
    CHECK(exercise(c7_mC7_tFixed_codec_schema(),&value,(asn1_schema_codec_t)codec)==0);c7_mC7_tFixed_free(&value);CHECK(live==0);
  }
  puts("CGEN7 typed open/named-bit every-allocation failure PASS");return 0;
}
