#include <asn1/c_runtime/tlv.h>
#include <asn1/c_runtime/bigint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x);return 1;}}while(0)

int main(void) {
  asn1_byte_writer_t* writer=NULL;
  CHECK(asn1_byte_writer_create(&writer)==ASN1_OK);
  CHECK(asn1_tlv_encode_boolean(writer,ASN1_TLV_DER,1)==ASN1_OK);
  const uint8_t boolean_wire[]={1,1,255};
  CHECK(asn1_byte_writer_view(writer).size==3 && !memcmp(asn1_byte_writer_view(writer).data,boolean_wire,3));
  asn1_byte_writer_destroy(writer);
  const uint8_t alternate[]={1,1,1};asn1_tlv_t value;int boolean=9;
  CHECK(asn1_tlv_parse(asn1_byte_view(alternate,3),ASN1_TLV_BER,&value)==ASN1_OK);
  CHECK(asn1_tlv_decode_boolean(&value,ASN1_TLV_BER,&boolean)==ASN1_OK && boolean==1);
  boolean=9;CHECK(asn1_tlv_decode_boolean(&value,ASN1_TLV_DER,&boolean)==ASN1_ERR_NON_CANONICAL && boolean==9);
  const uint8_t long_length[]={1,0x81,1,0xff};
  CHECK(asn1_tlv_parse(asn1_byte_view(long_length,4),ASN1_TLV_BER,&value)==ASN1_OK);
  CHECK(asn1_tlv_parse(asn1_byte_view(long_length,4),ASN1_TLV_DER,&value)==ASN1_ERR_NON_CANONICAL);
  const uint8_t constructed[]={0x24,0x80,4,1,0x41,0x24,3,4,1,0x42,0,0};
  CHECK(asn1_tlv_parse(asn1_byte_view(constructed,sizeof(constructed)),ASN1_TLV_BER,&value)==ASN1_OK);
  uint8_t* bytes=NULL;size_t size=0;
  CHECK(asn1_tlv_decode_octets(&value,ASN1_TLV_BER,&bytes,&size)==ASN1_OK && size==2 && bytes[0]=='A' && bytes[1]=='B');free(bytes);
  CHECK(asn1_tlv_parse(asn1_byte_view(constructed,sizeof(constructed)),ASN1_TLV_DER,&value)==ASN1_ERR_NON_CANONICAL);
  const uint8_t primitive_indefinite[]={4,0x80,0,0}, standalone_eoc[]={0,0};
  CHECK(asn1_tlv_parse(asn1_byte_view(primitive_indefinite,4),ASN1_TLV_BER,&value)==ASN1_ERR_INVALID_ARGUMENT);
  CHECK(asn1_tlv_parse(asn1_byte_view(standalone_eoc,2),ASN1_TLV_BER,&value)==ASN1_ERR_INVALID_ARGUMENT);

  uint8_t data[2001];memset(data,0xa5,sizeof(data));
  for(size_t n=999;n<=2001;n+=501) {
    CHECK(asn1_byte_writer_create(&writer)==ASN1_OK);
    CHECK(asn1_tlv_encode_octets(writer,ASN1_TLV_CER,asn1_byte_view(data,n))==ASN1_OK);
    asn1_byte_view_t wire=asn1_byte_writer_view(writer);
    CHECK(asn1_tlv_parse(wire,ASN1_TLV_CER,&value)==ASN1_OK);
    CHECK(value.tag.constructed==(n>1000));
    CHECK(asn1_tlv_decode_octets(&value,ASN1_TLV_CER,&bytes,&size)==ASN1_OK && size==n && !memcmp(data,bytes,n));free(bytes);
    for(size_t cut=0;cut<wire.size;++cut) CHECK(asn1_tlv_parse(asn1_byte_view(wire.data,cut),ASN1_TLV_CER,&value)==ASN1_ERR_TRUNCATED);
    asn1_byte_writer_destroy(writer);
  }
  CHECK(asn1_byte_writer_create(&writer)==ASN1_OK);
  CHECK(asn1_tlv_encode_octets(writer,ASN1_TLV_DER,asn1_byte_view(data,1001))==ASN1_OK);
  CHECK(asn1_tlv_parse(asn1_byte_writer_view(writer),ASN1_TLV_CER,&value)==ASN1_OK);
  bytes=(uint8_t*)1;size=99;
  CHECK(asn1_tlv_decode_octets(&value,ASN1_TLV_CER,&bytes,&size)==ASN1_ERR_NON_CANONICAL && bytes==(uint8_t*)1 && size==99);
  asn1_byte_writer_destroy(writer);

  asn1_bigint_t integer,decoded;asn1_bigint_init(&integer);asn1_bigint_init(&decoded);
  CHECK(asn1_bigint_init_from_decimal(&integer,"-340282366920938463463374607431768211456",0)==ASN1_OK);
  CHECK(asn1_byte_writer_create(&writer)==ASN1_OK);
  CHECK(asn1_tlv_encode_integer(writer,ASN1_TLV_DER,&integer)==ASN1_OK);
  CHECK(asn1_tlv_parse(asn1_byte_writer_view(writer),ASN1_TLV_DER,&value)==ASN1_OK);
  CHECK(asn1_tlv_decode_integer(&value,&decoded)==ASN1_OK && !asn1_bigint_cmp(&integer,&decoded));
  asn1_byte_writer_destroy(writer);asn1_bigint_free(&integer);asn1_bigint_free(&decoded);

  asn1_ber_tag_t tag={3,0,UINT64_MAX},parsed;uint8_t encoded[16];size_t used=0,consumed=0;
  CHECK(asn1_ber_tag_encode(encoded,sizeof(encoded),&tag,&used)==ASN1_OK);
  CHECK(asn1_ber_tag_decode(asn1_byte_view(encoded,used),&parsed,&consumed)==ASN1_OK);
  CHECK(consumed==used && parsed.number==UINT64_MAX && parsed.cls==3);
  const uint8_t nonminimal_tag[]={0x1f,0x1e};
  CHECK(asn1_ber_tag_decode(asn1_byte_view(nonminimal_tag,2),&parsed,&consumed)==ASN1_ERR_INVALID_ARGUMENT);
  asn1_byte_reader_t* reader=NULL;CHECK(asn1_byte_reader_create(&reader,asn1_byte_view(boolean_wire,2))==ASN1_OK);
  CHECK(asn1_tlv_read(reader,ASN1_TLV_BER,&value)==ASN1_ERR_TRUNCATED && asn1_byte_reader_position(reader)==0);
  asn1_byte_reader_destroy(reader);
  {
    uint8_t nested[1040];size_t n=0;
    for(size_t i=0;i<260;++i){nested[n++]=0x30;nested[n++]=0x80;}
    for(size_t i=0;i<260;++i){nested[n++]=0;nested[n++]=0;}
    CHECK(asn1_tlv_parse(asn1_byte_view(nested,n),ASN1_TLV_BER,&value)==ASN1_ERR_RESOURCE_LIMIT);
  }
  puts("C TLV envelopes, policies, values and transactional byte I/O PASS");return 0;
}
