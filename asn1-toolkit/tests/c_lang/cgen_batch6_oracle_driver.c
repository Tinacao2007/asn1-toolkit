#include "generated_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned digit(char c){return c>='0'&&c<='9'?(unsigned)(c-'0'):c>='a'&&c<='f'?(unsigned)(c-'a'+10):99u;}
int main(int argc,char** argv){
  if(argc!=4)return 1;
  const asn1_codec_schema_t* schemas[]={cgen6_mC6_tPdu_codec_schema(),cgen6_mC6_tHuge_codec_schema(),cgen6_mC6_tText_codec_schema(),cgen6_mC6_tBits_codec_schema(),cgen6_mC6_tBlob_codec_schema(),cgen6_mC6_tNumbers_codec_schema(),cgen6_mC6_tPick_codec_schema(),cgen6_mC6_tExtension_codec_schema(),cgen6_mC6_tReading_codec_schema(),cgen6_mC6_tIdentifier_codec_schema(),cgen6_mC6_tPair_codec_schema(),cgen6_mC6_tReal32_codec_schema(),cgen6_mC6_tReal64_codec_schema(),cgen6_mC6_tRecords_codec_schema(),cgen6_mC6_tLabels_codec_schema(),cgen6_mC6_tModes_codec_schema(),cgen6_mC6_tFlags_codec_schema()};
  unsigned index=(unsigned)strtoul(argv[1],NULL,10),codec=(unsigned)strtoul(argv[2],NULL,10);if(index>=sizeof(schemas)/sizeof(*schemas)||codec>ASN1_CODEC_EXER)return 2;
  size_t n=strlen(argv[3]);if(n%2)return 3;uint8_t* bytes=n?(uint8_t*)malloc(n/2):NULL;if(n&&!bytes)return 4;
  for(size_t i=0;i<n;i+=2){unsigned a=digit(argv[3][i]),b=digit(argv[3][i+1]);if(a>15||b>15){free(bytes);return 5;}bytes[i/2]=(uint8_t)((a<<4)|b);}
  const asn1_codec_schema_t* schema=schemas[index];void* value=calloc(1,schema->layout()->size);if(!value){free(bytes);return 6;}schema->layout()->init(value);
  asn1_byte_reader_t* r=NULL;asn1_byte_writer_t* w=NULL;asn1_status_t s=asn1_byte_reader_create(&r,asn1_byte_view(bytes,n/2));if(!s)s=asn1_schema_decode(r,(asn1_schema_codec_t)codec,schema,value);
  if(!s&&asn1_byte_reader_position(r)!=n/2)s=ASN1_ERR_INVALID_ARGUMENT;if(!s)s=asn1_byte_writer_create(&w);if(!s)s=asn1_schema_encode(w,(asn1_schema_codec_t)codec,schema,value);
  if(!s){asn1_byte_view_t out=asn1_byte_writer_view(w);for(size_t i=0;i<out.size;++i)printf("%02x",out.data[i]);puts("");}else fprintf(stderr,"status=%d\n",(int)s);
  asn1_byte_writer_destroy(w);asn1_byte_reader_destroy(r);schema->layout()->clear(value);free(value);free(bytes);return s?7:0;
}
