#include <asn1/c_runtime/tlv.h>
#include <asn1/c_runtime/bigint.h>
#include <asn1/c_runtime/limits.h>
#include <stdlib.h>
#include <string.h>
#include "alloc_internal.h"

#define TRY(expr) do { asn1_status_t status_=(expr); if(status_!=ASN1_OK)return status_; } while(0)
static int valid_policy(asn1_tlv_policy_t p) { return p>=ASN1_TLV_BER && p<=ASN1_TLV_CER; }
static asn1_status_t parse_at(asn1_byte_view_t input,asn1_tlv_policy_t policy,size_t depth,size_t* nodes,asn1_tlv_t* out) {
  if(depth>=ASN1_MAX_RECURSION_DEPTH)return ASN1_ERR_RESOURCE_LIMIT;
  if(*nodes>=ASN1_MAX_PDU_ELEMENTS)return ASN1_ERR_RESOURCE_LIMIT;++*nodes;
  if(!input.size)return ASN1_ERR_TRUNCATED;
  asn1_tlv_t result;size_t head=0;
  TRY(asn1_ber_tag_decode(input,&result.tag,&head));
  if(result.tag.cls==0 && result.tag.number==0)return ASN1_ERR_INVALID_ARGUMENT;
  if(head==input.size)return ASN1_ERR_TRUNCATED;
  const uint8_t length_octet=input.data[head];size_t length=0;
  result.indefinite=length_octet==0x80u;
  if(result.indefinite) {
    if(!result.tag.constructed)return ASN1_ERR_INVALID_ARGUMENT;
    if(policy==ASN1_TLV_DER)return ASN1_ERR_NON_CANONICAL;
    ++head;size_t at=head;
    for(;;) {
      if(input.size-at<2u)return ASN1_ERR_TRUNCATED;
      if(input.data[at]==0 && input.data[at+1u]==0){length=at-head;at+=2u;result.encoding=asn1_byte_view(input.data,at);break;}
      asn1_tlv_t child;TRY(parse_at(asn1_byte_view(input.data+at,input.size-at),policy,depth+1u,nodes,&child));
      at+=child.encoding.size;
    }
  } else {
    if(policy==ASN1_TLV_CER && result.tag.constructed)return ASN1_ERR_NON_CANONICAL;
    const size_t count=length_octet&0x7fu;
    if(length_octet==0xffu)return ASN1_ERR_INVALID_ARGUMENT;
    if(length_octet&0x80u) {
      if(count>sizeof(size_t))return ASN1_ERR_LENGTH_OVERFLOW;
      if(count>input.size-head-1u)return ASN1_ERR_TRUNCATED;
      if(policy!=ASN1_TLV_BER && (input.data[head+1u]==0 || (count==1 && input.data[head+1u]<128u)))return ASN1_ERR_NON_CANONICAL;
      for(size_t i=0;i<count;++i)length=(length<<8u)|input.data[head+1u+i];
      head+=count+1u;
    } else {length=length_octet;++head;}
    if(length>input.size-head)return ASN1_ERR_TRUNCATED;
    result.encoding=asn1_byte_view(input.data,head+length);
  }
  if(result.encoding.size>ASN1_MAX_PDU_ALLOCATION_BYTES)return ASN1_ERR_RESOURCE_LIMIT;
  result.content=asn1_byte_view(input.data+head,length);
  if(!result.indefinite && result.tag.constructed) {
    size_t at=0;while(at<length){asn1_tlv_t child;TRY(parse_at(asn1_byte_view(result.content.data+at,length-at),policy,depth+1u,nodes,&child));at+=child.encoding.size;}
  }
  *out=result;return ASN1_OK;
}
asn1_status_t asn1_tlv_parse(asn1_byte_view_t input,asn1_tlv_policy_t policy,asn1_tlv_t* out) {
  if(!out || !valid_policy(policy) || (input.size&&!input.data))return ASN1_ERR_INVALID_ARGUMENT;
  size_t nodes=0;return parse_at(input,policy,0,&nodes,out);
}
asn1_status_t asn1_tlv_read(asn1_byte_reader_t* r,asn1_tlv_policy_t policy,asn1_tlv_t* out) {
  if(!r||!out)return ASN1_ERR_INVALID_ARGUMENT;
  asn1_tlv_t tmp;TRY(asn1_tlv_parse(asn1_byte_reader_view(r),policy,&tmp));
  TRY(asn1_byte_reader_skip(r,tmp.encoding.size));*out=tmp;return ASN1_OK;
}
asn1_status_t asn1_tlv_write(asn1_byte_writer_t* w,asn1_tlv_policy_t policy,asn1_ber_tag_t tag,asn1_byte_view_t content) {
  if(!w||!valid_policy(policy)||(content.size&&!content.data)||(tag.cls==0&&tag.number==0))return ASN1_ERR_INVALID_ARGUMENT;
  uint8_t header[32];size_t tag_size=0,length_size=0;
  TRY(asn1_ber_tag_encode(header,sizeof(header),&tag,&tag_size));
  const int indefinite=policy==ASN1_TLV_CER && tag.constructed;
  if(indefinite){header[tag_size]=0x80u;length_size=1;}
  else TRY(asn1_ber_length_encode(header+tag_size,sizeof(header)-tag_size,content.size,&length_size));
  asn1_byte_writer_t* tmp=NULL;TRY(asn1_byte_writer_create(&tmp));
  asn1_status_t status=asn1_byte_writer_append(tmp,asn1_byte_view(header,tag_size+length_size));
  if(status==ASN1_OK)status=asn1_byte_writer_append(tmp,content);
  if(status==ASN1_OK && indefinite){const uint8_t eoc[2]={0,0};status=asn1_byte_writer_append(tmp,asn1_byte_view(eoc,2));}
  if(status==ASN1_OK)status=asn1_byte_writer_append(w,asn1_byte_writer_view(tmp));
  asn1_byte_writer_destroy(tmp);return status;
}
static asn1_ber_tag_t universal(uint64_t number,int constructed) {
  asn1_ber_tag_t tag={0,(uint8_t)constructed,number};return tag;
}
static int is_tag(const asn1_tlv_t* value,uint64_t number) {
  return value && value->tag.cls==0 && value->tag.number==number;
}
asn1_status_t asn1_tlv_encode_boolean(asn1_byte_writer_t* w,asn1_tlv_policy_t policy,int value) {
  if(value!=0&&value!=1)return ASN1_ERR_INVALID_ARGUMENT;
  const uint8_t content=value?0xffu:0;return asn1_tlv_write(w,policy,universal(1,0),asn1_byte_view(&content,1));
}
asn1_status_t asn1_tlv_decode_boolean(const asn1_tlv_t* value,asn1_tlv_policy_t policy,int* out) {
  if(!out||!valid_policy(policy))return ASN1_ERR_INVALID_ARGUMENT;
  if(!is_tag(value,1)||value->tag.constructed)return ASN1_ERR_TAG_MISMATCH;
  if(value->content.size!=1)return ASN1_ERR_CONSTRAINT_VIOLATION;
  const uint8_t b=value->content.data[0];if(policy!=ASN1_TLV_BER && b!=0 && b!=0xffu)return ASN1_ERR_NON_CANONICAL;
  *out=b!=0;return ASN1_OK;
}
asn1_status_t asn1_tlv_encode_integer(asn1_byte_writer_t* w,asn1_tlv_policy_t policy,const asn1_bigint_t* value) {
  uint8_t* content=NULL;size_t size=0;TRY(asn1_bigint_to_twos_complement(value,&content,&size));
  asn1_status_t status=asn1_tlv_write(w,policy,universal(2,0),asn1_byte_view(content,size));free(content);return status;
}
asn1_status_t asn1_tlv_decode_integer(const asn1_tlv_t* value,asn1_bigint_t* out) {
  if(!out)return ASN1_ERR_INVALID_ARGUMENT;
  if(!is_tag(value,2)||value->tag.constructed)return ASN1_ERR_TAG_MISMATCH;
  return asn1_bigint_init_from_twos_complement(out,value->content);
}
asn1_status_t asn1_tlv_encode_octets(asn1_byte_writer_t* w,asn1_tlv_policy_t policy,asn1_byte_view_t bytes) {
  if(!valid_policy(policy)||(bytes.size&&!bytes.data))return ASN1_ERR_INVALID_ARGUMENT;
  if(bytes.size>ASN1_MAX_STRING_BYTES)return ASN1_ERR_RESOURCE_LIMIT;
  if(policy!=ASN1_TLV_CER || bytes.size<=1000u)return asn1_tlv_write(w,policy,universal(4,0),bytes);
  asn1_byte_writer_t* children=NULL;TRY(asn1_byte_writer_create(&children));asn1_status_t status=ASN1_OK;
  for(size_t at=0;at<bytes.size && status==ASN1_OK;at+=1000u) {
    const size_t count=bytes.size-at>1000u?1000u:bytes.size-at;
    status=asn1_tlv_write(children,policy,universal(4,0),asn1_byte_view(bytes.data+at,count));
  }
  if(status==ASN1_OK)status=asn1_tlv_write(w,policy,universal(4,1),asn1_byte_writer_view(children));
  asn1_byte_writer_destroy(children);return status;
}
static asn1_status_t collect_octets(const asn1_tlv_t* value,asn1_tlv_policy_t policy,size_t depth,asn1_byte_writer_t* out) {
  if(depth>=ASN1_MAX_RECURSION_DEPTH)return ASN1_ERR_RESOURCE_LIMIT;
  if(!is_tag(value,4))return ASN1_ERR_TAG_MISMATCH;
  if(!value->tag.constructed){
    if(policy==ASN1_TLV_CER && value->content.size>1000u)return ASN1_ERR_NON_CANONICAL;
    if(value->content.size>ASN1_MAX_STRING_BYTES-asn1_byte_writer_view(out).size)return ASN1_ERR_RESOURCE_LIMIT;
    return asn1_byte_writer_append(out,value->content);
  }
  if(policy==ASN1_TLV_DER)return ASN1_ERR_NON_CANONICAL;
  size_t at=0,previous=0,count=0;
  while(at<value->content.size) {
    size_t nodes=0;asn1_tlv_t child;TRY(parse_at(asn1_byte_view(value->content.data+at,value->content.size-at),policy,depth+1u,&nodes,&child));
    if(policy==ASN1_TLV_CER && (child.tag.constructed || (count && previous!=1000u) || !child.content.size))return ASN1_ERR_NON_CANONICAL;
    TRY(collect_octets(&child,policy,depth+1u,out));previous=child.content.size;++count;at+=child.encoding.size;
  }
  if(policy==ASN1_TLV_CER && asn1_byte_writer_view(out).size<=1000u)return ASN1_ERR_NON_CANONICAL;
  return ASN1_OK;
}
asn1_status_t asn1_tlv_decode_octets(const asn1_tlv_t* value,asn1_tlv_policy_t policy,uint8_t** out,size_t* size) {
  if(!out||!size||!value||!valid_policy(policy))return ASN1_ERR_INVALID_ARGUMENT;
  asn1_byte_writer_t* buffer=NULL;TRY(asn1_byte_writer_create(&buffer));
  asn1_status_t status=collect_octets(value,policy,0,buffer);
  if(status==ASN1_OK)status=asn1_byte_writer_take(buffer,out,size);
  asn1_byte_writer_destroy(buffer);return status;
}
