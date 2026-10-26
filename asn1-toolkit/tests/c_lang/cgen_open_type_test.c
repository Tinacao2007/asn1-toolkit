#include "generated.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x); return 1; } } while (0)

int main(void) {
  const uint8_t encoded_value[] = {0x00,0x00,0x00,0x2a};
  asn1_gen_mTCMod_tHandoverIE value;
  asn1_gen_mTCMod_tHandoverIE_init(&value);
  value.fid.value.value=0;       /* MME-UE-S1AP-ID object-set key */
  value.fcriticality.value.value=1; /* ignore */
  value.fvalue.data=(uint8_t*)malloc(sizeof(encoded_value));
  CHECK(value.fvalue.data!=NULL);
  memcpy(value.fvalue.data,encoded_value,sizeof(encoded_value));
  value.fvalue.count=value.fvalue.capacity=sizeof(encoded_value);
  uint32_t selected_type=0;
  CHECK(asn1_gen_mTCMod_tHandoverIE_select(2,"0",&selected_type)==ASN1_OK);
  CHECK(selected_type!=0);
  void* typed_value=NULL;uint32_t typed_type=0;
  CHECK(asn1_gen_mTCMod_tHandoverIE_decode_selected_value_uper(&value,&typed_type,&typed_value)==ASN1_OK);
  CHECK(typed_type==selected_type && typed_value!=NULL);
  CHECK(((asn1_gen_mTCMod_tMME_x2dUE_x2dS1AP_x2dID*)typed_value)->value==42);
  asn1_gen_mTCMod_tHandoverIE_free_selected_value(typed_type,typed_value);
  {
    const uint8_t enb_payload[]={0xff,0xff,0xff};uint32_t enb_type=0;void* enb_value=NULL;
    asn1_gen_mTCMod_tHandoverIE second;asn1_gen_mTCMod_tHandoverIE_init(&second);
    second.fid.value.value=1;second.fcriticality.value.value=0;
    second.fvalue.data=(uint8_t*)malloc(sizeof(enb_payload));CHECK(second.fvalue.data!=NULL);
    memcpy(second.fvalue.data,enb_payload,sizeof(enb_payload));second.fvalue.count=second.fvalue.capacity=sizeof(enb_payload);
    CHECK(asn1_gen_mTCMod_tHandoverIE_select(2,"1",&enb_type)==ASN1_OK);
    asn1_bit_writer_t* second_writer=NULL;CHECK(asn1_bit_writer_create(&second_writer)==ASN1_OK);
    CHECK(asn1_gen_mTCMod_tHandoverIE_encode_uper(second_writer,&second)==ASN1_OK);
    uint8_t* second_wire=NULL;size_t second_size=0;CHECK(asn1_bit_writer_take(second_writer,&second_wire,&second_size)==ASN1_OK);
    asn1_bit_reader_t* second_reader=NULL;CHECK(asn1_bit_reader_create(&second_reader,asn1_byte_view(second_wire,second_size))==ASN1_OK);
    asn1_gen_mTCMod_tHandoverIE second_decoded;asn1_gen_mTCMod_tHandoverIE_init(&second_decoded);
    CHECK(asn1_gen_mTCMod_tHandoverIE_decode_uper(second_reader,&second_decoded)==ASN1_OK);
    CHECK(second_decoded.decoded_type_fvalue==enb_type&&second_decoded.decoded_value_fvalue!=NULL);
    CHECK(((asn1_gen_mTCMod_tENB_x2dUE_x2dS1AP_x2dID*)second_decoded.decoded_value_fvalue)->value==16777215);
    CHECK(asn1_gen_mTCMod_tHandoverIE_decode_selected_value_uper(&second_decoded,&enb_type,&enb_value)==ASN1_OK);
    CHECK(((asn1_gen_mTCMod_tENB_x2dUE_x2dS1AP_x2dID*)enb_value)->value==16777215);
    asn1_gen_mTCMod_tHandoverIE_free_selected_value(enb_type,enb_value);asn1_gen_mTCMod_tHandoverIE_free(&second_decoded);
    asn1_gen_mTCMod_tHandoverIE_free(&second);asn1_bit_reader_destroy(second_reader);asn1_bit_writer_destroy(second_writer);free(second_wire);
  }
  asn1_gen_mTCMod_tMME_x2dUE_x2dS1AP_x2dID aper_integer;asn1_gen_mTCMod_tMME_x2dUE_x2dS1AP_x2dID_init(&aper_integer);aper_integer.value=42;
  asn1_bit_writer_t* aper_value_writer=NULL;CHECK(asn1_bit_writer_create(&aper_value_writer)==ASN1_OK);
  CHECK(asn1_gen_mTCMod_tMME_x2dUE_x2dS1AP_x2dID_encode_aper(aper_value_writer,&aper_integer)==ASN1_OK);
  uint8_t* aper_payload=NULL;size_t aper_payload_size=0;CHECK(asn1_bit_writer_take(aper_value_writer,&aper_payload,&aper_payload_size)==ASN1_OK);
  asn1_gen_mTCMod_tHandoverIE aper_input;asn1_gen_mTCMod_tHandoverIE_init(&aper_input);
  aper_input.fid.value.value=0;aper_input.fcriticality.value.value=1;aper_input.fvalue.data=aper_payload;aper_input.fvalue.count=aper_input.fvalue.capacity=aper_payload_size;
  asn1_bit_writer_t* aper_writer=NULL;CHECK(asn1_bit_writer_create(&aper_writer)==ASN1_OK);
  CHECK(asn1_gen_mTCMod_tHandoverIE_encode_aper(aper_writer,&aper_input)==ASN1_OK);
  uint8_t* aper_wire=NULL;size_t aper_size=0;CHECK(asn1_bit_writer_take(aper_writer,&aper_wire,&aper_size)==ASN1_OK);
  asn1_bit_reader_t* aper_reader=NULL;CHECK(asn1_bit_reader_create(&aper_reader,asn1_byte_view(aper_wire,aper_size))==ASN1_OK);
  asn1_gen_mTCMod_tHandoverIE aper_decoded;asn1_gen_mTCMod_tHandoverIE_init(&aper_decoded);
  CHECK(asn1_gen_mTCMod_tHandoverIE_decode_aper(aper_reader,&aper_decoded)==ASN1_OK);
  CHECK(aper_decoded.decoded_type_fvalue==selected_type && aper_decoded.decoded_value_fvalue!=NULL);
  CHECK(((asn1_gen_mTCMod_tMME_x2dUE_x2dS1AP_x2dID*)aper_decoded.decoded_value_fvalue)->value==42);
  asn1_gen_mTCMod_tHandoverIE_free(&aper_decoded);asn1_gen_mTCMod_tHandoverIE_free(&aper_input);asn1_gen_mTCMod_tMME_x2dUE_x2dS1AP_x2dID_free(&aper_integer);asn1_bit_reader_destroy(aper_reader);free(aper_wire);asn1_bit_writer_destroy(aper_writer);asn1_bit_writer_destroy(aper_value_writer);

  asn1_bit_writer_t* writer=NULL;CHECK(asn1_bit_writer_create(&writer)==ASN1_OK);
  CHECK(asn1_gen_mTCMod_tHandoverIE_encode_uper(writer,&value)==ASN1_OK);
  uint8_t* wire=NULL;size_t wire_size=0;CHECK(asn1_bit_writer_take(writer,&wire,&wire_size)==ASN1_OK);
  asn1_bit_reader_t* reader=NULL;CHECK(asn1_bit_reader_create(&reader,asn1_byte_view(wire,wire_size))==ASN1_OK);
  asn1_gen_mTCMod_tHandoverIE decoded;asn1_gen_mTCMod_tHandoverIE_init(&decoded);
  CHECK(asn1_gen_mTCMod_tHandoverIE_decode_uper(reader,&decoded)==ASN1_OK);
  CHECK(decoded.fid.value.value==0 && decoded.fcriticality.value.value==1);
  CHECK(decoded.fvalue.count==sizeof(encoded_value) && memcmp(decoded.fvalue.data,encoded_value,sizeof(encoded_value))==0);
  CHECK(decoded.decoded_type_fvalue==selected_type && decoded.decoded_value_fvalue!=NULL);
  CHECK(((asn1_gen_mTCMod_tMME_x2dUE_x2dS1AP_x2dID*)decoded.decoded_value_fvalue)->value==42);
  asn1_gen_mTCMod_tHandoverIE copied;asn1_gen_mTCMod_tHandoverIE_init(&copied);
  CHECK(asn1_gen_mTCMod_tHandoverIE_copy(&decoded,&copied)==ASN1_OK);
  CHECK(copied.decoded_type_fvalue==selected_type && copied.decoded_value_fvalue!=decoded.decoded_value_fvalue);
  CHECK(((asn1_gen_mTCMod_tMME_x2dUE_x2dS1AP_x2dID*)copied.decoded_value_fvalue)->value==42);
  ((asn1_gen_mTCMod_tMME_x2dUE_x2dS1AP_x2dID*)decoded.decoded_value_fvalue)->value=43;
  asn1_bit_writer_t* typed_writer=NULL;CHECK(asn1_bit_writer_create(&typed_writer)==ASN1_OK);
  CHECK(asn1_gen_mTCMod_tHandoverIE_encode_uper(typed_writer,&decoded)==ASN1_OK);
  uint8_t* typed_wire=NULL;size_t typed_wire_size=0;CHECK(asn1_bit_writer_take(typed_writer,&typed_wire,&typed_wire_size)==ASN1_OK);
  asn1_bit_reader_t* typed_reader=NULL;CHECK(asn1_bit_reader_create(&typed_reader,asn1_byte_view(typed_wire,typed_wire_size))==ASN1_OK);
  asn1_gen_mTCMod_tHandoverIE typed_back;asn1_gen_mTCMod_tHandoverIE_init(&typed_back);
  CHECK(asn1_gen_mTCMod_tHandoverIE_decode_uper(typed_reader,&typed_back)==ASN1_OK);
  CHECK(((asn1_gen_mTCMod_tMME_x2dUE_x2dS1AP_x2dID*)typed_back.decoded_value_fvalue)->value==43);
  asn1_gen_mTCMod_tHandoverIE_free(&typed_back);asn1_bit_reader_destroy(typed_reader);free(typed_wire);asn1_bit_writer_destroy(typed_writer);
  asn1_gen_mTCMod_tHandoverIE_free(&copied);
  typed_value=NULL;typed_type=0;
  CHECK(asn1_gen_mTCMod_tHandoverIE_decode_selected_value_uper(&decoded,&typed_type,&typed_value)==ASN1_OK);
  CHECK(typed_type==selected_type && typed_value!=NULL);
  CHECK(((asn1_gen_mTCMod_tMME_x2dUE_x2dS1AP_x2dID*)typed_value)->value==42);
  asn1_gen_mTCMod_tHandoverIE_free_selected_value(typed_type,typed_value);
  decoded.fid.value.value=99;typed_type=UINT32_C(0xfeedbeef);typed_value=(void*)1;
  CHECK(asn1_gen_mTCMod_tHandoverIE_decode_selected_value_uper(&decoded,&typed_type,&typed_value)==ASN1_ERR_CONSTRAINT_VIOLATION);
  CHECK(typed_type==UINT32_C(0xfeedbeef) && typed_value==(void*)1);
  asn1_gen_mTCMod_tHandoverIE_free(&decoded);asn1_gen_mTCMod_tHandoverIE_free(&value);
  asn1_bit_reader_destroy(reader);asn1_bit_writer_destroy(writer);free(wire);
  puts("C generated open type payload roundtrip PASS");
  return 0;
}
