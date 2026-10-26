#include "generated.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"CGEN6 PDU line %d\n",__LINE__);return 1;}}while(0)
typedef asn1_status_t (*encoder_t)(asn1_byte_writer_t*,const cgen6_mC6_tPdu*);
typedef asn1_status_t (*decoder_t)(asn1_byte_reader_t*,cgen6_mC6_tPdu*);
int main(void){
  const encoder_t encoders[]={cgen6_mC6_tPdu_encode_oer,cgen6_mC6_tPdu_encode_coer,cgen6_mC6_tPdu_encode_jer,cgen6_mC6_tPdu_encode_jeri,cgen6_mC6_tPdu_encode_xer,cgen6_mC6_tPdu_encode_cxer,cgen6_mC6_tPdu_encode_exer};
  const decoder_t decoders[]={cgen6_mC6_tPdu_decode_oer,cgen6_mC6_tPdu_decode_coer,cgen6_mC6_tPdu_decode_jer,cgen6_mC6_tPdu_decode_jeri,cgen6_mC6_tPdu_decode_xer,cgen6_mC6_tPdu_decode_cxer,cgen6_mC6_tPdu_decode_exer};
  const char* text[]={NULL,NULL,"{\"flag\":true,\"count\":3}","{\"flag\":true,\"count\":3}","<Pdu><flag><true /></flag><count>3</count></Pdu>","<Pdu><flag><true/></flag><count>3</count><defaultFlag><false/></defaultFlag></Pdu>","<Pdu><flag><true /></flag><count>3</count></Pdu>"};
  for(size_t codec=0;codec<7;++codec){
    cgen6_mC6_tPdu value,back;cgen6_mC6_tPdu_init(&value);cgen6_mC6_tPdu_init(&back);value.fflag.value=1;value.fcount.value=3;
    asn1_byte_writer_t* w=NULL;CHECK(asn1_byte_writer_create(&w)==ASN1_OK);CHECK(encoders[codec](w,&value)==ASN1_OK);asn1_byte_view_t wire=asn1_byte_writer_view(w);
    if(codec<2){const uint8_t expected[]={0,255,3};CHECK(wire.size==3&&!memcmp(wire.data,expected,3));}else CHECK(wire.size==strlen(text[codec])&&!memcmp(wire.data,text[codec],wire.size));
    asn1_byte_reader_t* r=NULL;CHECK(asn1_byte_reader_create(&r,wire)==ASN1_OK);CHECK(decoders[codec](r,&back)==ASN1_OK&&back.fflag.value==1&&back.fcount.value==3&&asn1_byte_reader_position(r)==wire.size);asn1_byte_reader_destroy(r);
    for(size_t i=0;i<wire.size;++i){back.fcount.value=7;CHECK(asn1_byte_reader_create(&r,asn1_byte_view(wire.data,i))==ASN1_OK);CHECK(decoders[codec](r,&back)!=ASN1_OK&&back.fcount.value==7&&asn1_byte_reader_position(r)==0);asn1_byte_reader_destroy(r);}
    value.fcount.value=8;size_t before=wire.size;CHECK(encoders[codec](w,&value)==ASN1_ERR_CONSTRAINT_VIOLATION&&asn1_byte_writer_view(w).size==before);
    asn1_byte_writer_destroy(w);cgen6_mC6_tPdu_free(&value);cgen6_mC6_tPdu_free(&back);
  }
  {
    const uint8_t alternative[]={0,1,3};asn1_byte_reader_t* r=NULL;cgen6_mC6_tPdu value;cgen6_mC6_tPdu_init(&value);
    CHECK(asn1_byte_reader_create(&r,asn1_byte_view(alternative,3))==ASN1_OK);CHECK(cgen6_mC6_tPdu_decode_coer(r,&value)==ASN1_ERR_NON_CANONICAL&&asn1_byte_reader_position(r)==0);CHECK(cgen6_mC6_tPdu_decode_oer(r,&value)==ASN1_OK&&value.fflag.value);asn1_byte_reader_destroy(r);cgen6_mC6_tPdu_free(&value);
  }
  {
    const char* decimal="340282366920938463463374607431768211456";cgen6_mC6_tHuge value,back;cgen6_mC6_tHuge_init(&value);cgen6_mC6_tHuge_init(&back);CHECK(asn1_bigint_init_from_decimal(&value.value,decimal,0)==ASN1_OK);
    asn1_byte_writer_t* w=NULL;CHECK(asn1_byte_writer_create(&w)==ASN1_OK);CHECK(cgen6_mC6_tHuge_encode_jer(w,&value)==ASN1_OK);asn1_byte_view_t bytes=asn1_byte_writer_view(w);CHECK(bytes.size==strlen(decimal)&&!memcmp(bytes.data,decimal,bytes.size));
    asn1_byte_reader_t* r=NULL;CHECK(asn1_byte_reader_create(&r,bytes)==ASN1_OK);CHECK(cgen6_mC6_tHuge_decode_jer(r,&back)==ASN1_OK&&!asn1_bigint_cmp(&value.value,&back.value));asn1_byte_reader_destroy(r);asn1_byte_writer_destroy(w);cgen6_mC6_tHuge_free(&value);cgen6_mC6_tHuge_free(&back);
  }
  puts("Generated C OER/COER JER/JERI XER/CXER/EXER real PDU policies PASS");return 0;
}
