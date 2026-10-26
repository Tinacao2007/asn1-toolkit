#include "generated_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"boundary line %d\n",__LINE__);return 1;}}while(0)
static int frozen(const asn1_codec_schema_t* t,asn1_schema_codec_t codec,const uint8_t* bytes,size_t n){
  void* value=calloc(1,t->layout()->size);CHECK(value);t->layout()->init(value);asn1_byte_reader_t* r=NULL;asn1_byte_writer_t* w=NULL;
  CHECK(asn1_byte_reader_create(&r,asn1_byte_view(bytes,n))==ASN1_OK);CHECK(asn1_schema_decode(r,codec,t,value)==ASN1_OK&&asn1_byte_reader_position(r)==n);
  CHECK(asn1_byte_writer_create(&w)==ASN1_OK);CHECK(asn1_schema_encode(w,codec,t,value)==ASN1_OK);asn1_byte_view_t wire=asn1_byte_writer_view(w);CHECK(wire.size==n&&!memcmp(wire.data,bytes,n));
  asn1_byte_writer_destroy(w);asn1_byte_reader_destroy(r);t->layout()->clear(value);free(value);return 0;
}
int main(void){
  /* X.696 16.5: one bitmap bit per group, group-local OPTIONAL preamble. */
  const uint8_t extension[]={0x80,4,2,6,0xc0,1,0xff,3,0x80,7,0xff};
  CHECK(!frozen(cgen6_mC6_tExtension_codec_schema(),ASN1_CODEC_COER,extension,sizeof(extension)));
  /* CHOICE extension tag is retained, followed by a complete-encoding open. */
  const uint8_t choice[]={0x82,1,0xff};CHECK(!frozen(cgen6_mC6_tPick_codec_schema(),ASN1_CODEC_COER,choice,sizeof(choice)));
  /* SET order ignores extension tags of an untagged CHOICE (X.696 18).
     Nested untagged CHOICE repeats the selected tag (20.1 note 3). */
  const uint8_t ordered[]={0,0x85,0xff};CHECK(!frozen(cgen6_mC6_tOrdered_codec_schema(),ASN1_CODEC_COER,ordered,sizeof(ordered)));
  const uint8_t nested[]={0x85,0x85,0xff};CHECK(!frozen(cgen6_mC6_tNestedChoice_codec_schema(),ASN1_CODEC_COER,nested,sizeof(nested)));
  const uint8_t real32[]={0x3f,0xc0,0,0};CHECK(!frozen(cgen6_mC6_tReal32_codec_schema(),ASN1_CODEC_COER,real32,sizeof(real32)));
  const uint8_t real64[]={0xbf,0xf8,0,0,0,0,0,0};CHECK(!frozen(cgen6_mC6_tReal64_codec_schema(),ASN1_CODEC_COER,real64,sizeof(real64)));
  {const uint8_t non_der[]={3,0x80,0xfe,6};cgen6_mC6_tReading v;cgen6_mC6_tReading_init(&v);v.value=7;asn1_byte_reader_t* input=NULL;CHECK(asn1_byte_reader_create(&input,asn1_byte_view(non_der,sizeof(non_der)))==ASN1_OK);CHECK(cgen6_mC6_tReading_decode_oer(input,&v)==ASN1_ERR_NON_CANONICAL&&v.value==7&&!asn1_byte_reader_position(input));asn1_byte_reader_destroy(input);cgen6_mC6_tReading_free(&v);}
  {cgen6_mC6_tReal32 v;cgen6_mC6_tReal32_init(&v);v.value=0.1;asn1_byte_writer_t* output=NULL;CHECK(asn1_byte_writer_create(&output)==ASN1_OK);CHECK(cgen6_mC6_tReal32_encode_oer(output,&v)==ASN1_ERR_CONSTRAINT_VIOLATION&&!asn1_byte_writer_view(output).size);asn1_byte_writer_destroy(output);cgen6_mC6_tReal32_free(&v);}
  const char* real="<Reading>1.5E0</Reading>";CHECK(!frozen(cgen6_mC6_tReading_codec_schema(),ASN1_CODEC_CXER,(const uint8_t*)real,strlen(real)));
  const char* controls="<Text>a<nul/>b</Text>";CHECK(!frozen(cgen6_mC6_tText_codec_schema(),ASN1_CODEC_CXER,(const uint8_t*)controls,strlen(controls)));
  const char* negatives[]={"<Reading>1.5</Reading>","<Reading>1.50E0</Reading>","<Reading>1.5E+0</Reading>"};
  {const char* malformed="<Pdu><flag><true>garbage</true></flag><count>3</count></Pdu>";cgen6_mC6_tPdu v;cgen6_mC6_tPdu_init(&v);v.fcount.value=7;asn1_byte_reader_t* input=NULL;CHECK(asn1_byte_reader_create(&input,asn1_byte_view((const uint8_t*)malformed,strlen(malformed)))==ASN1_OK);CHECK(cgen6_mC6_tPdu_decode_xer(input,&v)==ASN1_ERR_INVALID_ARGUMENT&&v.fcount.value==7&&!asn1_byte_reader_position(input));asn1_byte_reader_destroy(input);cgen6_mC6_tPdu_free(&v);}
  for(size_t i=0;i<3;++i){cgen6_mC6_tReading value;cgen6_mC6_tReading_init(&value);value.value=7;asn1_byte_reader_t* r=NULL;CHECK(asn1_byte_reader_create(&r,asn1_byte_view((const uint8_t*)negatives[i],strlen(negatives[i])))==ASN1_OK);CHECK(cgen6_mC6_tReading_decode_cxer(r,&value)==ASN1_ERR_NON_CANONICAL&&value.value==7&&!asn1_byte_reader_position(r));asn1_byte_reader_destroy(r);cgen6_mC6_tReading_free(&value);}
  const uint8_t long_length[]={0x82,0,1};asn1_byte_reader_t* r=NULL;size_t n=99;CHECK(asn1_byte_reader_create(&r,asn1_byte_view(long_length,3))==ASN1_OK);CHECK(asn1_oer_decode_length(r,ASN1_OER_CANONICAL,&n)==ASN1_ERR_NON_CANONICAL&&n==99&&!asn1_byte_reader_position(r));CHECK(asn1_oer_decode_length(r,ASN1_OER_BASIC,&n)==ASN1_OK&&n==1);asn1_byte_reader_destroy(r);
  asn1_byte_writer_t* w=NULL;CHECK(asn1_byte_writer_create(&w)==ASN1_OK);
  {const int64_t values[]={INT64_MIN,-129,-128,-1,0,127,128,256,INT64_MAX};
    for(size_t i=0;i<sizeof(values)/sizeof(*values);++i){asn1_byte_writer_destroy(w);w=NULL;CHECK(asn1_byte_writer_create(&w)==ASN1_OK);CHECK(asn1_oer_encode_enumerated(w,values[i])==ASN1_OK);asn1_byte_view_t wire=asn1_byte_writer_view(w);int64_t value=0;CHECK(asn1_byte_reader_create(&r,wire)==ASN1_OK);CHECK(asn1_oer_decode_enumerated(r,ASN1_OER_CANONICAL,&value)==ASN1_OK&&value==values[i]);asn1_byte_reader_destroy(r);}}
  asn1_byte_writer_destroy(w);w=NULL;CHECK(asn1_byte_writer_create(&w)==ASN1_OK);
  const asn1_ber_tag_t tag={3,0,UINT64_MAX};CHECK(asn1_oer_encode_tag(w,tag)==ASN1_OK);asn1_ber_tag_t back={0,0,0};CHECK(asn1_byte_reader_create(&r,asn1_byte_writer_view(w))==ASN1_OK);CHECK(asn1_oer_decode_tag(r,&back)==ASN1_OK&&back.cls==3&&back.number==UINT64_MAX);asn1_byte_reader_destroy(r);asn1_byte_writer_destroy(w);
  asn1_bigint_t integer;asn1_bigint_init(&integer);CHECK(asn1_bigint_init_from_decimal(&integer,"128",0)==ASN1_OK);CHECK(asn1_byte_writer_create(&w)==ASN1_OK);const asn1_oer_integer_form_t signed_byte={1,0};CHECK(asn1_oer_encode_integer(w,signed_byte,&integer)==ASN1_ERR_CONSTRAINT_VIOLATION&&!asn1_byte_writer_view(w).size);asn1_byte_writer_destroy(w);asn1_bigint_free(&integer);
  puts("CGEN6 standard frozen groups/choice/canonical boundaries PASS");return 0;
}
