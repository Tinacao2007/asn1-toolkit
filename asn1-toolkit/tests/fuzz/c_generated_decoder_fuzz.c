/* Bounded harness for generated C wrappers, not just runtime primitives. */
#include "generated.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
int LLVMFuzzerTestOneInput(const uint8_t* data,size_t size) {
  if(!size || size>4096)return 0;
  const unsigned codec=data[0]%12u;
  asn1_byte_view_t wire=asn1_byte_view(data+1,size-1);
  cgen6_mC6_tPdu value;cgen6_mC6_tPdu_init(&value);value.fcount.value=7;
  cgen6_mC6_tPdu before;memcpy(&before,&value,sizeof(value));
  asn1_status_t status;
  if(codec<2){
    asn1_bit_reader_t* reader=NULL;
    if(asn1_bit_reader_create(&reader,wire)!=ASN1_OK)return 0;
    status=codec?cgen6_mC6_tPdu_decode_aper(reader,&value):cgen6_mC6_tPdu_decode_uper(reader,&value);
    asn1_bit_reader_destroy(reader);
  }else{
    asn1_byte_reader_t* reader=NULL;
    if(asn1_byte_reader_create(&reader,wire)!=ASN1_OK)return 0;
    switch(codec){
      case 2:status=cgen6_mC6_tPdu_decode_ber(reader,&value);break;
      case 3:status=cgen6_mC6_tPdu_decode_der(reader,&value);break;
      case 4:status=cgen6_mC6_tPdu_decode_cer(reader,&value);break;
      case 5:status=cgen6_mC6_tPdu_decode_oer(reader,&value);break;
      case 6:status=cgen6_mC6_tPdu_decode_coer(reader,&value);break;
      case 7:status=cgen6_mC6_tPdu_decode_jer(reader,&value);break;
      case 8:status=cgen6_mC6_tPdu_decode_jeri(reader,&value);break;
      case 9:status=cgen6_mC6_tPdu_decode_xer(reader,&value);break;
      case 10:status=cgen6_mC6_tPdu_decode_cxer(reader,&value);break;
      default:status=cgen6_mC6_tPdu_decode_exer(reader,&value);break;
    }
    if(status!=ASN1_OK && asn1_byte_reader_position(reader))abort();
    asn1_byte_reader_destroy(reader);
  }
  if(status!=ASN1_OK && memcmp(&before,&value,sizeof(value)))abort();
  cgen6_mC6_tPdu_free(&value);
  return 0;
}
#ifdef ASN1_C_FUZZ_REGRESSION_MAIN
int main(void){
  uint8_t bytes[257];uint32_t state=UINT32_C(0x4917a35b);
  const char* seeds[]={"", "{}", "{\"flag\":true,\"count\":3}", "<Pdu><flag><true/></flag><count>3</count></Pdu>", "\x30\x06\x80\x01\xff\x81\x01\x03"};
  for(unsigned codec=0;codec<12;++codec){
    for(size_t i=0;i<sizeof(seeds)/sizeof(seeds[0]);++i){bytes[0]=(uint8_t)codec;const size_t n=strlen(seeds[i]);memcpy(bytes+1,seeds[i],n);LLVMFuzzerTestOneInput(bytes,n+1);}
    for(size_t trial=0;trial<512;++trial){
      const size_t n=trial%257;bytes[0]=(uint8_t)codec;
      for(size_t i=1;i<=n;++i){state^=state<<13;state^=state>>17;state^=state<<5;bytes[i]=(uint8_t)state;}
      LLVMFuzzerTestOneInput(bytes,n+1);
    }
  }
  return 0;
}
#endif
