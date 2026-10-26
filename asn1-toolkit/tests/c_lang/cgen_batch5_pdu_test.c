#include "generated.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x);return 1;}}while(0)
int main(void) {
  const uint8_t der_pdu[]={0x30,9,0x80,1,0xff,0x81,1,5,0x82,1,2};
  const uint8_t cer_pdu[]={0x30,0x80,0x80,1,0xff,0x81,1,5,0x82,1,2,0,0};
  for(int policy=0;policy<3;++policy){
    cgen5_mC5_tPdu value;cgen5_mC5_tPdu_init(&value);value.fflag.value=1;value.fcount.value=5;value.has_foptionalValue=1;value.foptionalValue.value=2;
    asn1_byte_writer_t* writer=NULL;CHECK(asn1_byte_writer_create(&writer)==ASN1_OK);
    CHECK((policy==0?cgen5_mC5_tPdu_encode_ber(writer,&value):policy==1?cgen5_mC5_tPdu_encode_der(writer,&value):cgen5_mC5_tPdu_encode_cer(writer,&value))==ASN1_OK);
    const asn1_byte_view_t wire=asn1_byte_writer_view(writer);const uint8_t* expected=policy==2?cer_pdu:der_pdu;
    CHECK(wire.size==(policy==2?sizeof(cer_pdu):sizeof(der_pdu)) && !memcmp(wire.data,expected,wire.size));
    for(size_t cut=0;cut<=wire.size;++cut){
      asn1_byte_reader_t* reader=NULL;CHECK(asn1_byte_reader_create(&reader,asn1_byte_view(wire.data,cut))==ASN1_OK);
      cgen5_mC5_tPdu decoded;cgen5_mC5_tPdu_init(&decoded);decoded.fcount.value=3;
      asn1_status_t status=policy==0?cgen5_mC5_tPdu_decode_ber(reader,&decoded):policy==1?cgen5_mC5_tPdu_decode_der(reader,&decoded):cgen5_mC5_tPdu_decode_cer(reader,&decoded);
      if(cut<wire.size)CHECK(status!=ASN1_OK && decoded.fcount.value==3 && asn1_byte_reader_position(reader)==0);
      else CHECK(status==ASN1_OK && decoded.fflag.value==1 && decoded.fcount.value==5 && decoded.has_foptionalValue==1 && decoded.foptionalValue.value==2 && decoded.fdefaultFlag.value==0);
      cgen5_mC5_tPdu_free(&decoded);asn1_byte_reader_destroy(reader);
    }
    asn1_byte_writer_destroy(writer);cgen5_mC5_tPdu_free(&value);
  }
  {
    const uint8_t explicit_default[]={0x30,9,0x80,1,0xff,0x81,1,5,0x83,1,0};
    asn1_byte_reader_t* r=NULL;CHECK(asn1_byte_reader_create(&r,asn1_byte_view(explicit_default,sizeof(explicit_default)))==ASN1_OK);
    cgen5_mC5_tPdu value;cgen5_mC5_tPdu_init(&value);CHECK(cgen5_mC5_tPdu_decode_ber(r,&value)==ASN1_OK);asn1_byte_reader_destroy(r);cgen5_mC5_tPdu_free(&value);
    CHECK(asn1_byte_reader_create(&r,asn1_byte_view(explicit_default,sizeof(explicit_default)))==ASN1_OK);cgen5_mC5_tPdu_init(&value);
    CHECK(cgen5_mC5_tPdu_decode_der(r,&value)==ASN1_ERR_NON_CANONICAL && asn1_byte_reader_position(r)==0);asn1_byte_reader_destroy(r);cgen5_mC5_tPdu_free(&value);
  }
  {
    cgen5_mC5_tPair value;cgen5_mC5_tPair_init(&value);value.fright.value=1;value.fleft.value=7;
    const uint8_t expected[]={0x31,6,0x81,1,7,0x83,1,0xff};asn1_byte_writer_t* w=NULL;CHECK(asn1_byte_writer_create(&w)==ASN1_OK);
    CHECK(cgen5_mC5_tPair_encode_der(w,&value)==ASN1_OK);CHECK(asn1_byte_writer_view(w).size==sizeof(expected)&&!memcmp(asn1_byte_writer_view(w).data,expected,sizeof(expected)));
    asn1_byte_writer_destroy(w);cgen5_mC5_tPair_free(&value);
  }
  {
    cgen5_mC5_tHigh high;cgen5_mC5_tHigh_init(&high);high.value=42;
    const uint8_t expected[]={0x7f,0x81,0x49,3,2,1,42};asn1_byte_writer_t* w=NULL;CHECK(asn1_byte_writer_create(&w)==ASN1_OK);
    CHECK(cgen5_mC5_tHigh_encode_der(w,&high)==ASN1_OK);CHECK(asn1_byte_writer_view(w).size==sizeof(expected)&&!memcmp(asn1_byte_writer_view(w).data,expected,sizeof(expected)));
    asn1_byte_reader_t* r=NULL;CHECK(asn1_byte_reader_create(&r,asn1_byte_writer_view(w))==ASN1_OK);cgen5_mC5_tHigh back;cgen5_mC5_tHigh_init(&back);CHECK(cgen5_mC5_tHigh_decode_der(r,&back)==ASN1_OK&&back.value==42);
    asn1_byte_reader_destroy(r);asn1_byte_writer_destroy(w);cgen5_mC5_tHigh_free(&high);cgen5_mC5_tHigh_free(&back);
  }
  {
    cgen5_mC5_tBlob blob;cgen5_mC5_tBlob_init(&blob);blob.data=(uint8_t*)malloc(1001);CHECK(blob.data);memset(blob.data,0x55,1001);blob.count=blob.capacity=1001;
    asn1_byte_writer_t* w=NULL;CHECK(asn1_byte_writer_create(&w)==ASN1_OK);CHECK(cgen5_mC5_tBlob_encode_cer(w,&blob)==ASN1_OK);
    asn1_byte_reader_t* r=NULL;CHECK(asn1_byte_reader_create(&r,asn1_byte_writer_view(w))==ASN1_OK);cgen5_mC5_tBlob back;cgen5_mC5_tBlob_init(&back);CHECK(cgen5_mC5_tBlob_decode_cer(r,&back)==ASN1_OK&&back.count==1001&&!memcmp(back.data,blob.data,1001));
    asn1_byte_reader_destroy(r);asn1_byte_writer_destroy(w);cgen5_mC5_tBlob_free(&blob);cgen5_mC5_tBlob_free(&back);
  }
  {
    cgen5_mC5_tRadioDefault value;cgen5_mC5_tRadioDefault_init(&value);CHECK(cgen5_mC5_tRadioDefault_reset_defaults(&value)==ASN1_OK);value.ftail.value=1;
    const uint8_t expected[]={0x30,3,0x81,1,0xff};asn1_byte_writer_t* w=NULL;CHECK(asn1_byte_writer_create(&w)==ASN1_OK);
    CHECK(cgen5_mC5_tRadioDefault_encode_der(w,&value)==ASN1_OK);CHECK(asn1_byte_writer_view(w).size==sizeof(expected)&&!memcmp(asn1_byte_writer_view(w).data,expected,sizeof(expected)));
    asn1_byte_reader_t* r=NULL;CHECK(asn1_byte_reader_create(&r,asn1_byte_writer_view(w))==ASN1_OK);cgen5_mC5_tRadioDefault back;cgen5_mC5_tRadioDefault_init(&back);
    CHECK(cgen5_mC5_tRadioDefault_decode_der(r,&back)==ASN1_OK && back.fbits.value.bit_length==8 && back.fbits.value.bits[0]==0xff && back.ftail.value==1);
    asn1_bit_writer_t* pw=NULL;CHECK(asn1_bit_writer_create(&pw)==ASN1_OK);CHECK(cgen5_mC5_tRadioDefault_encode_uper(pw,&value)==ASN1_OK);uint8_t* per=NULL;size_t size=0;CHECK(asn1_bit_writer_take(pw,&per,&size)==ASN1_OK);CHECK(size==1&&per[0]==0x40);
    asn1_bit_reader_t* pr=NULL;CHECK(asn1_bit_reader_create(&pr,asn1_byte_view(per,size))==ASN1_OK);cgen5_mC5_tRadioDefault_free(&back);CHECK(cgen5_mC5_tRadioDefault_decode_uper(pr,&back)==ASN1_OK&&back.fbits.value.bits[0]==0xff&&back.ftail.value==1);
    asn1_bit_reader_destroy(pr);asn1_bit_writer_destroy(pw);free(per);asn1_byte_reader_destroy(r);asn1_byte_writer_destroy(w);cgen5_mC5_tRadioDefault_free(&value);cgen5_mC5_tRadioDefault_free(&back);
  }
  {
    cgen5_mC5_tNumbers numbers;cgen5_mC5_tNumbers_init(&numbers);numbers.data=calloc(3,sizeof(*numbers.data));CHECK(numbers.data);numbers.count=numbers.capacity=3;numbers.data[0].value=1;numbers.data[1].value=-1;
    const uint8_t expected[]={0x31,9,2,1,0,2,1,1,2,1,0xff};asn1_byte_writer_t* w=NULL;CHECK(asn1_byte_writer_create(&w)==ASN1_OK);CHECK(cgen5_mC5_tNumbers_encode_der(w,&numbers)==ASN1_OK);
    CHECK(asn1_byte_writer_view(w).size==sizeof(expected)&&!memcmp(asn1_byte_writer_view(w).data,expected,sizeof(expected)));
    asn1_byte_reader_t* r=NULL;CHECK(asn1_byte_reader_create(&r,asn1_byte_writer_view(w))==ASN1_OK);cgen5_mC5_tNumbers back;cgen5_mC5_tNumbers_init(&back);CHECK(cgen5_mC5_tNumbers_decode_der(r,&back)==ASN1_OK&&back.count==3&&back.data[0].value==0&&back.data[1].value==1&&back.data[2].value==-1);
    asn1_byte_reader_destroy(r);asn1_byte_writer_destroy(w);cgen5_mC5_tNumbers_free(&numbers);cgen5_mC5_tNumbers_free(&back);
  }
  for(int cer=0;cer<2;++cer){
    cgen5_mC5_tSorted value;cgen5_mC5_tSorted_init(&value);value.fpick.kind=2; /* high alternative in the generated field view */value.fpick.value.fhigh.value=2;value.fmiddle.value=3;
    const uint8_t der[]={0x31,6,0x83,1,3,0x85,1,2},canonical[]={0x31,0x80,0x85,1,2,0x83,1,3,0,0};asn1_byte_writer_t* w=NULL;CHECK(asn1_byte_writer_create(&w)==ASN1_OK);
    CHECK((cer?cgen5_mC5_tSorted_encode_cer(w,&value):cgen5_mC5_tSorted_encode_der(w,&value))==ASN1_OK);CHECK(asn1_byte_writer_view(w).size==(cer?sizeof(canonical):sizeof(der))&&!memcmp(asn1_byte_writer_view(w).data,cer?canonical:der,asn1_byte_writer_view(w).size));
    asn1_byte_reader_t* r=NULL;CHECK(asn1_byte_reader_create(&r,asn1_byte_writer_view(w))==ASN1_OK);cgen5_mC5_tSorted back;cgen5_mC5_tSorted_init(&back);
    CHECK((cer?cgen5_mC5_tSorted_decode_cer(r,&back):cgen5_mC5_tSorted_decode_der(r,&back))==ASN1_OK&&back.fpick.kind==2&&back.fpick.value.fhigh.value==2&&back.fmiddle.value==3);
    asn1_byte_reader_destroy(r);asn1_byte_writer_destroy(w);cgen5_mC5_tSorted_free(&value);cgen5_mC5_tSorted_free(&back);
  }
  {
    cgen5_mC5_tReading reading;cgen5_mC5_tReading_init(&reading);reading.value=1.5;const uint8_t expected[]={9,3,0x80,0xff,3};asn1_byte_writer_t* w=NULL;CHECK(asn1_byte_writer_create(&w)==ASN1_OK);
    CHECK(cgen5_mC5_tReading_encode_der(w,&reading)==ASN1_OK && asn1_byte_writer_view(w).size==sizeof(expected)&&!memcmp(asn1_byte_writer_view(w).data,expected,sizeof(expected)));asn1_byte_writer_destroy(w);
    const uint8_t decimal[]={9,6,3,'1','.','5','E','0'};asn1_byte_reader_t* r=NULL;CHECK(asn1_byte_reader_create(&r,asn1_byte_view(decimal,sizeof(decimal)))==ASN1_OK);CHECK(cgen5_mC5_tReading_decode_ber(r,&reading)==ASN1_OK&&reading.value==1.5);asn1_byte_reader_destroy(r);
    CHECK(asn1_byte_reader_create(&r,asn1_byte_view(decimal,sizeof(decimal)))==ASN1_OK);CHECK(cgen5_mC5_tReading_decode_der(r,&reading)==ASN1_ERR_UNSUPPORTED);asn1_byte_reader_destroy(r);cgen5_mC5_tReading_free(&reading);
  }
  {
    cgen5_mC5_tIdentifier oid;cgen5_mC5_tIdentifier_init(&oid);oid.data=calloc(4,sizeof(*oid.data));CHECK(oid.data);oid.count=oid.capacity=4;oid.data[0]=1;oid.data[1]=2;oid.data[2]=840;oid.data[3]=113549;
    const uint8_t expected[]={6,6,0x2a,0x86,0x48,0x86,0xf7,0x0d};asn1_byte_writer_t* w=NULL;CHECK(asn1_byte_writer_create(&w)==ASN1_OK);CHECK(cgen5_mC5_tIdentifier_encode_der(w,&oid)==ASN1_OK && asn1_byte_writer_view(w).size==sizeof(expected)&&!memcmp(asn1_byte_writer_view(w).data,expected,sizeof(expected)));
    asn1_byte_reader_t* r=NULL;CHECK(asn1_byte_reader_create(&r,asn1_byte_writer_view(w))==ASN1_OK);cgen5_mC5_tIdentifier back;cgen5_mC5_tIdentifier_init(&back);CHECK(cgen5_mC5_tIdentifier_decode_der(r,&back)==ASN1_OK&&back.count==4&&back.data[3]==113549);
    asn1_byte_reader_destroy(r);asn1_byte_writer_destroy(w);cgen5_mC5_tIdentifier_free(&oid);cgen5_mC5_tIdentifier_free(&back);
  }
  {
    const uint8_t good[]={12,4,0xc3,0xa9,0xc3,0xa9},bad[]={12,2,0xc0,0x80};cgen5_mC5_tText text;cgen5_mC5_tText_init(&text);asn1_byte_reader_t* r=NULL;
    CHECK(asn1_byte_reader_create(&r,asn1_byte_view(good,sizeof(good)))==ASN1_OK);CHECK(cgen5_mC5_tText_decode_der(r,&text)==ASN1_OK&&text.count==4);asn1_byte_reader_destroy(r);cgen5_mC5_tText copied;cgen5_mC5_tText_init(&copied);CHECK(cgen5_mC5_tText_copy(&text,&copied)==ASN1_OK&&copied.count==4&&!memcmp(copied.data,text.data,4));cgen5_mC5_tText_free(&copied);
    CHECK(asn1_byte_reader_create(&r,asn1_byte_view(bad,sizeof(bad)))==ASN1_OK);CHECK(cgen5_mC5_tText_decode_der(r,&text)==ASN1_ERR_CONSTRAINT_VIOLATION && text.count==4);asn1_byte_reader_destroy(r);cgen5_mC5_tText_free(&text);
  }
  {
    /* An automatic/implicit tag on an untagged CHOICE is an explicit wrapper. */
    cgen5_mC5_tAutoPick value,back;cgen5_mC5_tAutoPick_init(&value);cgen5_mC5_tAutoPick_init(&back);
    value.fpick.kind=2;value.fpick.value.fflag.value=1;
    const uint8_t expected[]={0x30,5,0xa0,3,0x81,1,0xff};
    asn1_byte_writer_t* w=NULL;CHECK(asn1_byte_writer_create(&w)==ASN1_OK);
    CHECK(cgen5_mC5_tAutoPick_encode_der(w,&value)==ASN1_OK);
    CHECK(asn1_byte_writer_view(w).size==sizeof(expected)&&!memcmp(asn1_byte_writer_view(w).data,expected,sizeof(expected)));
    asn1_byte_reader_t* r=NULL;CHECK(asn1_byte_reader_create(&r,asn1_byte_writer_view(w))==ASN1_OK);
    CHECK(cgen5_mC5_tAutoPick_decode_der(r,&back)==ASN1_OK&&back.fpick.kind==2&&back.fpick.value.fflag.value==1);
    asn1_byte_reader_destroy(r);asn1_byte_writer_destroy(w);cgen5_mC5_tAutoPick_free(&value);cgen5_mC5_tAutoPick_free(&back);
  }
  {
    const uint8_t extra[]={0x30,6,0x80,1,0xff,0x89,1,0};
    cgen5_mC5_tExtended value;cgen5_mC5_tExtended_init(&value);asn1_byte_reader_t* r=NULL;
    CHECK(asn1_byte_reader_create(&r,asn1_byte_view(extra,sizeof(extra)))==ASN1_OK);
    CHECK(cgen5_mC5_tExtended_decode_ber(r,&value)==ASN1_OK&&value.fflag.value==1);
    asn1_byte_reader_destroy(r);cgen5_mC5_tExtended_free(&value);
    const uint8_t duplicate[]={0x30,6,0x80,1,0xff,0x80,1,0};
    CHECK(asn1_byte_reader_create(&r,asn1_byte_view(duplicate,sizeof(duplicate)))==ASN1_OK);
    CHECK(cgen5_mC5_tExtended_decode_ber(r,&value)!=ASN1_OK&&asn1_byte_reader_position(r)==0);
    asn1_byte_reader_destroy(r);cgen5_mC5_tExtended_free(&value);
    const uint8_t unordered[]={0x31,6,0x83,1,0xff,0x81,1,3};
    cgen5_mC5_tPair pair;cgen5_mC5_tPair_init(&pair);
    CHECK(asn1_byte_reader_create(&r,asn1_byte_view(unordered,sizeof(unordered)))==ASN1_OK);
    CHECK(cgen5_mC5_tPair_decode_der(r,&pair)==ASN1_ERR_NON_CANONICAL&&asn1_byte_reader_position(r)==0);
    asn1_byte_reader_destroy(r);cgen5_mC5_tPair_free(&pair);
  }
  {
    cgen5_mC5_tLongText value,back;cgen5_mC5_tLongText_init(&value);cgen5_mC5_tLongText_init(&back);
    value.data=(uint8_t*)malloc(1001);CHECK(value.data);memset(value.data,'a',1001);value.count=value.capacity=1001;
    asn1_byte_writer_t* w=NULL;CHECK(asn1_byte_writer_create(&w)==ASN1_OK);
    CHECK(cgen5_mC5_tLongText_encode_cer(w,&value)==ASN1_OK);
    const asn1_byte_view_t wire=asn1_byte_writer_view(w);CHECK(wire.data[0]==0x2c&&wire.data[1]==0x80&&wire.data[2]==4);
    asn1_byte_reader_t* r=NULL;CHECK(asn1_byte_reader_create(&r,wire)==ASN1_OK);
    CHECK(cgen5_mC5_tLongText_decode_cer(r,&back)==ASN1_OK&&back.count==1001&&!memcmp(value.data,back.data,1001));
    asn1_byte_reader_destroy(r);asn1_byte_writer_destroy(w);cgen5_mC5_tLongText_free(&value);cgen5_mC5_tLongText_free(&back);
  }
  puts("Generated C BER/DER/CER policies, tags, values and transactional decode PASS");return 0;
}
