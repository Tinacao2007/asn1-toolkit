#include "generated_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"instruction line %d\n",__LINE__);return 1;}}while(0)
static int exchange(const asn1_codec_schema_t* t,asn1_schema_codec_t codec,const char* input,const char* expected){
  void* v=calloc(1,t->layout()->size);CHECK(v);t->layout()->init(v);asn1_byte_reader_t* r=NULL;asn1_byte_writer_t* w=NULL;
  CHECK(asn1_byte_reader_create(&r,asn1_byte_view((const uint8_t*)input,strlen(input)))==ASN1_OK);
  CHECK(asn1_schema_decode(r,codec,t,v)==ASN1_OK);CHECK(asn1_byte_writer_create(&w)==ASN1_OK);CHECK(asn1_schema_encode(w,codec,t,v)==ASN1_OK);
  asn1_byte_view_t bytes=asn1_byte_writer_view(w);CHECK(bytes.size==strlen(expected)&&!memcmp(bytes.data,expected,bytes.size));
  asn1_byte_writer_destroy(w);asn1_byte_reader_destroy(r);t->layout()->clear(v);free(v);return 0;
}
int main(void){
#ifdef JER_TEST
  CHECK(!exchange(ix_mJ6_tRow_codec_schema(),ASN1_CODEC_JER,"[1,2,true]","[1,2,true]"));
  CHECK(!exchange(ix_mJ6_tBinary_codec_schema(),ASN1_CODEC_JERI,"\"AP+A\"","\"AP+A\""));
  CHECK(!exchange(ix_mJ6_tMap_codec_schema(),ASN1_CODEC_JER,"{\"RED\":3,\"BLUE\":4}","{\"RED\":3,\"BLUE\":4}"));
  CHECK(!exchange(ix_mJ6_tKeyAlias_codec_schema(),ASN1_CODEC_JER,"\"red\"","\"red\""));
  CHECK(!exchange(ix_mJ6_tKeyAlias_codec_schema(),ASN1_CODEC_EXER,"<KeyAlias>red</KeyAlias>","<KeyAlias>red</KeyAlias>"));
  CHECK(!exchange(ix_mJ6_tKey_codec_schema(),ASN1_CODEC_EXER,"<Key>RED</Key>","<Key>RED</Key>"));
  CHECK(!exchange(ix_mJ6_tRecord_codec_schema(),ASN1_CODEC_XER,"<Record><named>3</named></Record>","<Record><named>3</named></Record>"));
  CHECK(!exchange(ix_mJ6_tAliasMap_codec_schema(),ASN1_CODEC_JER,"{\"red\":true}","{\"red\":true}"));
  CHECK(!exchange(ix_mJ6_tChoice_codec_schema(),ASN1_CODEC_JER,"true","true"));
  CHECK(!exchange(ix_mJ6_tChoice_codec_schema(),ASN1_CODEC_JERI,"\"hi\"","\"hi\""));
  CHECK(!exchange(ix_mJ6_tRecord_codec_schema(),ASN1_CODEC_JER,"{\"renamed\":3,\"optionalValue\":null}","{\"renamed\":3}"));
  CHECK(!exchange(ix_mJ6_tRecord_codec_schema(),ASN1_CODEC_EXER,"<Record><renamed>3</renamed></Record>","<Record><renamed>3</renamed></Record>"));
#else
  CHECK(!exchange(ix_mX6_tRecord_codec_schema(),ASN1_CODEC_EXER,"<Record flag=\"true\"><count>3</count><text>a&amp;b</text></Record>","<Record flag=\"true\"><count>3</count><text>a&amp;b</text></Record>"));
  CHECK(!exchange(ix_mX6_tBinary_codec_schema(),ASN1_CODEC_EXER,"<Binary>AP+A</Binary>","<Binary>AP+A</Binary>"));
  CHECK(!exchange(ix_mX6_tList_codec_schema(),ASN1_CODEC_EXER,"<List>1 2 3</List>","<List>1 2 3</List>"));
  CHECK(!exchange(ix_mX6_tTextBool_codec_schema(),ASN1_CODEC_EXER,"<TextBool>true</TextBool>","<TextBool>true</TextBool>"));
  CHECK(!exchange(ix_mX6_tNumber_codec_schema(),ASN1_CODEC_EXER,"<Number>1</Number>","<Number>1</Number>"));
#endif
  puts("CGEN6 instructions PASS");return 0;
}
