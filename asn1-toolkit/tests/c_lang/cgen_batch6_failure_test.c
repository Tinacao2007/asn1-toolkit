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
  for(int codec=ASN1_CODEC_OER;codec<=ASN1_CODEC_EXER;++codec){
    cgen6_mC6_tPdu value;cgen6_mC6_tPdu_init(&value);value.fflag.value=1;value.fcount.value=3;
    value.has_fnote=1;value.fnote.data=asn1_test_malloc(3);CHECK(value.fnote.data);memcpy(value.fnote.data,"abc",3);value.fnote.count=value.fnote.capacity=3;
    CHECK(exercise(cgen6_mC6_tPdu_codec_schema(),&value,(asn1_schema_codec_t)codec)==0);cgen6_mC6_tPdu_free(&value);CHECK(live==0);
    cgen6_mC6_tNumbers numbers;cgen6_mC6_tNumbers_init(&numbers);numbers.data=asn1_test_calloc(3,sizeof(*numbers.data));CHECK(numbers.data);numbers.count=numbers.capacity=3;numbers.data[0].value=3;numbers.data[1].value=-10;
    CHECK(exercise(cgen6_mC6_tNumbers_codec_schema(),&numbers,(asn1_schema_codec_t)codec)==0);cgen6_mC6_tNumbers_free(&numbers);CHECK(live==0);
    cgen6_mC6_tHuge huge;cgen6_mC6_tHuge_init(&huge);CHECK(asn1_bigint_init_from_decimal(&huge.value,"340282366920938463463374607431768211456",0)==ASN1_OK);
    CHECK(exercise(cgen6_mC6_tHuge_codec_schema(),&huge,(asn1_schema_codec_t)codec)==0);cgen6_mC6_tHuge_free(&huge);CHECK(live==0);
    cgen6_mC6_tReading reading;cgen6_mC6_tReading_init(&reading);reading.value=1.5;
    CHECK(exercise(cgen6_mC6_tReading_codec_schema(),&reading,(asn1_schema_codec_t)codec)==0);cgen6_mC6_tReading_free(&reading);CHECK(live==0);
    cgen6_mC6_tText text;cgen6_mC6_tText_init(&text);text.data=asn1_test_malloc(3);CHECK(text.data);memcpy(text.data,"a\0b",3);text.count=text.capacity=3;
    CHECK(exercise(cgen6_mC6_tText_codec_schema(),&text,(asn1_schema_codec_t)codec)==0);cgen6_mC6_tText_free(&text);CHECK(live==0);
  }
  puts("CGEN6 every allocation encode/decode transactional PASS");return 0;
}
