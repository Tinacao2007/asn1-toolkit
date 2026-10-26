#include "generated.h"
#include <asn1/c_runtime/text_tree.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"limits line %d\n",__LINE__);return 1;}}while(0)
int main(void){
  const uint8_t oer[]={1,8,0,1,0,2,0,3,0,4,0,5,0,6,0,7,0,8};
  cgen6_mC6_tNumbers value;cgen6_mC6_tNumbers_init(&value);asn1_byte_reader_t* r=NULL;
  CHECK(asn1_byte_reader_create(&r,asn1_byte_view(oer,sizeof(oer)))==ASN1_OK);CHECK(cgen6_mC6_tNumbers_decode_oer(r,&value)==ASN1_ERR_RESOURCE_LIMIT&&!value.count&&!value.data&&!asn1_byte_reader_position(r));asn1_byte_reader_destroy(r);
  const char* documents[]={"[1,2,3,4,5,6,7,8]","<Numbers><INTEGER>1</INTEGER><INTEGER>2</INTEGER><INTEGER>3</INTEGER><INTEGER>4</INTEGER><INTEGER>5</INTEGER><INTEGER>6</INTEGER><INTEGER>7</INTEGER><INTEGER>8</INTEGER></Numbers>"};
  for(size_t i=0;i<2;++i){CHECK(asn1_byte_reader_create(&r,asn1_byte_view((const uint8_t*)documents[i],strlen(documents[i])))==ASN1_OK);asn1_status_t s=i?cgen6_mC6_tNumbers_decode_xer(r,&value):cgen6_mC6_tNumbers_decode_jer(r,&value);CHECK(s==ASN1_ERR_RESOURCE_LIMIT&&!value.count&&!value.data&&!asn1_byte_reader_position(r));asn1_byte_reader_destroy(r);}
  char long_string[68];long_string[0]='"';memset(long_string+1,'a',65);long_string[66]='"';long_string[67]=0;asn1_text_node_t* tree=NULL;size_t error=0;CHECK(asn1_json_parse(asn1_byte_view((const uint8_t*)long_string,67),&tree,&error)==ASN1_ERR_RESOURCE_LIMIT&&!tree);
  cgen6_mC6_tNumbers_free(&value);puts("CGEN6 cumulative counts/string/tree limits PASS");return 0;
}
