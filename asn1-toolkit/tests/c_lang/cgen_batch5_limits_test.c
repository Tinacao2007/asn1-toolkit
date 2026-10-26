#include "generated.h"
#include <stdio.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"limit failure line %d\n",__LINE__);return 1;}}while(0)
int main(void){
  /* Each wire is below the reader byte cap; failure is aggregate decode
     allocation or element accounting, not a large input allocation. */
  const uint8_t bytes[]={0x31,21,2,1,0,2,1,1,2,1,2,2,1,3,2,1,4,2,1,5,2,1,6};
  asn1_byte_reader_t* r=NULL;cgen5_mC5_tNumbers value;cgen5_mC5_tNumbers_init(&value);
  CHECK(asn1_byte_reader_create(&r,asn1_byte_view(bytes,sizeof(bytes)))==ASN1_OK);
  CHECK(cgen5_mC5_tNumbers_decode_der(r,&value)==ASN1_ERR_RESOURCE_LIMIT);
  CHECK(asn1_byte_reader_position(r)==0&&value.count==0&&value.data==NULL);asn1_byte_reader_destroy(r);
  const uint8_t many[]={0x31,27,2,1,0,2,1,1,2,1,2,2,1,3,2,1,4,2,1,5,2,1,6,2,1,7,2,1,8};
  CHECK(asn1_byte_reader_create(&r,asn1_byte_view(many,sizeof(many)))==ASN1_OK);
  CHECK(cgen5_mC5_tNumbers_decode_der(r,&value)==ASN1_ERR_RESOURCE_LIMIT);
  CHECK(asn1_byte_reader_position(r)==0&&value.count==0);asn1_byte_reader_destroy(r);
  cgen5_mC5_tNumbers_free(&value);puts("TLV cumulative payload and element caps PASS");return 0;
}
