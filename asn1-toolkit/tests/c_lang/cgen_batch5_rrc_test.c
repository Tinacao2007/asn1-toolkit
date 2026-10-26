#include "generated.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define R(n) rrc5_mEUTRA_x2dRRC_x2dDefinitions_t##n
#define CHECK(x) do {if(!(x)){fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x);return 1;}}while(0)
static void dump(const char* codec,asn1_byte_view_t wire){printf("%s ",codec);for(size_t i=0;i<wire.size;++i)printf("%02x",wire.data[i]);puts("");fflush(stdout);}
static int check_value(const R(RRCConnectionRequest)* value) {
  const R(RRCConnectionRequest_x2dr8_x2dIEs)* ie=&value->fcriticalExtensions.value.frrcConnectionRequest_x2dr8;
  const uint8_t expected[]={1,2,3,4,5};
  return value->fcriticalExtensions.kind==1 && ie->fue_x2dIdentity.kind==2 && ie->festablishmentCause.value==3 &&
    ie->fue_x2dIdentity.value.frandomValue.value.bit_length==40 && !memcmp(ie->fue_x2dIdentity.value.frandomValue.value.bits,expected,5) &&
    ie->fspare.value.bit_length==1 && ie->fspare.value.bits[0]==0;
}
int main(void) {
  R(RRCConnectionRequest) input;R(RRCConnectionRequest_init)(&input);input.fcriticalExtensions.kind=1;
  R(RRCConnectionRequest_x2dr8_x2dIEs)* ie=&input.fcriticalExtensions.value.frrcConnectionRequest_x2dr8;
  ie->fue_x2dIdentity.kind=2;ie->festablishmentCause.value=3;
  ie->fue_x2dIdentity.value.frandomValue.value.bits=(uint8_t*)malloc(5);CHECK(ie->fue_x2dIdentity.value.frandomValue.value.bits);
  for(size_t i=0;i<5;++i)ie->fue_x2dIdentity.value.frandomValue.value.bits[i]=(uint8_t)(i+1);
  ie->fue_x2dIdentity.value.frandomValue.value.bit_length=40;ie->fue_x2dIdentity.value.frandomValue.value.cap_bytes=5;
  ie->fspare.value.bits=(uint8_t*)calloc(1,1);CHECK(ie->fspare.value.bits);ie->fspare.value.bit_length=1;ie->fspare.value.cap_bytes=1;
  asn1_bit_writer_t* pw=NULL;CHECK(asn1_bit_writer_create(&pw)==ASN1_OK);CHECK(R(RRCConnectionRequest_encode_uper)(pw,&input)==ASN1_OK);
  uint8_t* per=NULL;size_t size=0;CHECK(asn1_bit_writer_take(pw,&per,&size)==ASN1_OK);dump("uper",asn1_byte_view(per,size));
  asn1_bit_reader_t* pr=NULL;CHECK(asn1_bit_reader_create(&pr,asn1_byte_view(per,size))==ASN1_OK);R(RRCConnectionRequest) output;R(RRCConnectionRequest_init)(&output);
  CHECK(R(RRCConnectionRequest_decode_uper)(pr,&output)==ASN1_OK && check_value(&output));R(RRCConnectionRequest_free)(&output);
  asn1_bit_reader_destroy(pr);asn1_bit_writer_destroy(pw);free(per);
  asn1_byte_writer_t* bw=NULL;CHECK(asn1_byte_writer_create(&bw)==ASN1_OK);CHECK(R(RRCConnectionRequest_encode_ber)(bw,&input)==ASN1_OK);
  dump("ber",asn1_byte_writer_view(bw));asn1_byte_reader_t* br=NULL;CHECK(asn1_byte_reader_create(&br,asn1_byte_writer_view(bw))==ASN1_OK);R(RRCConnectionRequest_init)(&output);
  asn1_status_t status=R(RRCConnectionRequest_decode_ber)(br,&output);if(status!=ASN1_OK)fprintf(stderr,"RRC BER decode: %s\n",asn1_status_name(status));
  CHECK(status==ASN1_OK && check_value(&output));CHECK(asn1_byte_reader_remaining(br)==0);
  R(RRCConnectionRequest_free)(&output);R(RRCConnectionRequest_free)(&input);asn1_byte_reader_destroy(br);asn1_byte_writer_destroy(bw);return 0;
}
