#include <asn1/c_runtime/oer.h>
#include <asn1/c_runtime/limits.h>
#include <stdlib.h>
#include <string.h>
#include "alloc_internal.h"
#define TRY(x) do {asn1_status_t s_=(x);if(s_!=ASN1_OK)return s_;}while(0)
static int policy(asn1_oer_policy_t p){return p==ASN1_OER_BASIC||p==ASN1_OER_CANONICAL;}
static int form_valid(asn1_oer_integer_form_t f){return (f.width==0||f.width==1||f.width==2||f.width==4||f.width==8)&&(f.unsigned_form==0||f.unsigned_form==1);}
asn1_status_t asn1_oer_encode_length(asn1_byte_writer_t* w,size_t n){
  uint8_t bytes[1+sizeof(size_t)];size_t size=0;TRY(asn1_ber_length_encode(bytes,sizeof(bytes),n,&size));return asn1_byte_writer_append(w,asn1_byte_view(bytes,size));
}
static asn1_status_t length(asn1_byte_view_t wire,asn1_oer_policy_t p,size_t* value,size_t* used){
  if(!policy(p))return ASN1_ERR_INVALID_ARGUMENT;if(!wire.size)return ASN1_ERR_TRUNCATED;
  const unsigned first=wire.data[0];size_t result=0,n=first&127u;
  if(!(first&128u)){*value=first;*used=1;return ASN1_OK;}
  if(!n)return ASN1_ERR_INVALID_ARGUMENT;if(n>wire.size-1u)return ASN1_ERR_TRUNCATED;
  if(p==ASN1_OER_CANONICAL&&(wire.data[1]==0||(n==1&&wire.data[1]<128u)))return ASN1_ERR_NON_CANONICAL;
  for(size_t i=0;i<n;++i){if(result>(SIZE_MAX-wire.data[i+1u])/256u)return ASN1_ERR_LENGTH_OVERFLOW;result=result*256u+wire.data[i+1u];}
  *value=result;*used=n+1u;return ASN1_OK;
}
asn1_status_t asn1_oer_decode_length(asn1_byte_reader_t* r,asn1_oer_policy_t p,size_t* out){
  if(!r||!out)return ASN1_ERR_INVALID_ARGUMENT;size_t value,used;TRY(length(asn1_byte_reader_view(r),p,&value,&used));TRY(asn1_byte_reader_skip(r,used));*out=value;return ASN1_OK;
}
asn1_status_t asn1_oer_encode_boolean(asn1_byte_writer_t* w,int v){if(v!=0&&v!=1)return ASN1_ERR_INVALID_ARGUMENT;const uint8_t b=v?255:0;return asn1_byte_writer_append(w,asn1_byte_view(&b,1));}
asn1_status_t asn1_oer_decode_boolean(asn1_byte_reader_t* r,asn1_oer_policy_t p,int* out){
  if(!r||!out||!policy(p))return ASN1_ERR_INVALID_ARGUMENT;asn1_byte_view_t bytes=asn1_byte_reader_view(r);if(!bytes.size)return ASN1_ERR_TRUNCATED;
  if(p==ASN1_OER_CANONICAL&&bytes.data[0]!=0&&bytes.data[0]!=255)return ASN1_ERR_NON_CANONICAL;
  int v=bytes.data[0]!=0;TRY(asn1_byte_reader_skip(r,1));*out=v;return ASN1_OK;
}
asn1_status_t asn1_oer_encode_open(asn1_byte_writer_t* w,asn1_byte_view_t content){
  if(!w||(content.size&&!content.data))return ASN1_ERR_INVALID_ARGUMENT;
  asn1_byte_writer_t* tmp=NULL;TRY(asn1_byte_writer_create(&tmp));asn1_status_t s=asn1_oer_encode_length(tmp,content.size);
  if(s==ASN1_OK)s=asn1_byte_writer_append(tmp,content);if(s==ASN1_OK)s=asn1_byte_writer_append(w,asn1_byte_writer_view(tmp));asn1_byte_writer_destroy(tmp);return s;
}
asn1_status_t asn1_oer_decode_open(asn1_byte_reader_t* r,asn1_oer_policy_t p,asn1_byte_view_t* out){
  if(!r||!out)return ASN1_ERR_INVALID_ARGUMENT;asn1_byte_view_t wire=asn1_byte_reader_view(r);size_t n,head;TRY(length(wire,p,&n,&head));
  if(n>wire.size-head)return ASN1_ERR_TRUNCATED;asn1_byte_view_t result=asn1_byte_view(wire.data+head,n);TRY(asn1_byte_reader_skip(r,head+n));*out=result;return ASN1_OK;
}
asn1_status_t asn1_oer_encode_integer(asn1_byte_writer_t* w,asn1_oer_integer_form_t f,const asn1_bigint_t* v){
  if(!w||!v||!form_valid(f))return ASN1_ERR_INVALID_ARGUMENT;
  if(f.unsigned_form&&v->sign<0)return ASN1_ERR_CONSTRAINT_VIOLATION;
  uint8_t* content=NULL;size_t n=0;TRY(asn1_bigint_to_twos_complement(v,&content,&n));
  size_t at=0;if(f.unsigned_form&&n>1&&content[0]==0)++at;n-=at;asn1_status_t s;
  if(!f.width)s=asn1_oer_encode_open(w,asn1_byte_view(content+at,n));
  else if(n>f.width)s=ASN1_ERR_CONSTRAINT_VIOLATION;
  else{uint8_t bytes[8];memset(bytes,v->sign<0?255:0,f.width);memcpy(bytes+f.width-n,content+at,n);s=asn1_byte_writer_append(w,asn1_byte_view(bytes,f.width));}
  free(content);return s;
}
/* Decode arbitrary contents without passing BASIC-OER's permitted redundant
   sign/zero octets to the strict BER BigInt constructor. */
static asn1_status_t integer_content(asn1_byte_view_t bytes,asn1_oer_policy_t p,int unsigned_form,asn1_bigint_t* out){
  if(!bytes.size)return ASN1_ERR_INVALID_ARGUMENT;
  size_t at=0;
  if(unsigned_form){while(at+1u<bytes.size&&bytes.data[at]==0)++at;}
  else while(at+1u<bytes.size&&((bytes.data[at]==0&&!(bytes.data[at+1]&128u))||(bytes.data[at]==255&&(bytes.data[at+1]&128u))))++at;
  if(p==ASN1_OER_CANONICAL&&at)return ASN1_ERR_NON_CANONICAL;
  bytes=asn1_byte_view(bytes.data+at,bytes.size-at);
  if(bytes.size>ASN1_MAX_BIGINT_LIMBS*4u+1u)return ASN1_ERR_RESOURCE_LIMIT;
  if(unsigned_form&&(bytes.data[0]&128u)){
    uint8_t* positive=(uint8_t*)malloc(bytes.size+1u);if(!positive)return ASN1_ERR_OUT_OF_MEMORY;positive[0]=0;memcpy(positive+1,bytes.data,bytes.size);
    asn1_status_t s=asn1_bigint_init_from_twos_complement(out,asn1_byte_view(positive,bytes.size+1u));free(positive);return s;
  }
  return asn1_bigint_init_from_twos_complement(out,bytes);
}
asn1_status_t asn1_oer_decode_integer(asn1_byte_reader_t* r,asn1_oer_policy_t p,asn1_oer_integer_form_t f,asn1_bigint_t* out){
  if(!r||!out||!policy(p)||!form_valid(f))return ASN1_ERR_INVALID_ARGUMENT;asn1_byte_view_t wire=asn1_byte_reader_view(r);size_t n=f.width,head=0;
  if(!n)TRY(length(wire,p,&n,&head));if(n>wire.size-head)return ASN1_ERR_TRUNCATED;
  asn1_bigint_t value;asn1_bigint_init(&value);TRY(integer_content(asn1_byte_view(wire.data+head,n),f.width?ASN1_OER_BASIC:p,f.unsigned_form,&value));
  asn1_status_t s=asn1_byte_reader_skip(r,head+n);if(s==ASN1_OK)*out=value;else asn1_bigint_free(&value);return s;
}
asn1_status_t asn1_oer_encode_enumerated(asn1_byte_writer_t* w,int64_t value){
  if(value>=0&&value<=127){const uint8_t b=(uint8_t)value;return asn1_byte_writer_append(w,asn1_byte_view(&b,1));}
  uint8_t bytes[9];uint64_t raw=(uint64_t)value;for(size_t i=0;i<8;++i)bytes[8-i]=(uint8_t)(raw>>(i*8u));size_t at=1;
  while(at<8&&((bytes[at]==0&&!(bytes[at+1]&128u))||(bytes[at]==255&&(bytes[at+1]&128u))))++at;
  bytes[at-1]=(uint8_t)(128u+9u-at);return asn1_byte_writer_append(w,asn1_byte_view(bytes+at-1,10u-at));
}
asn1_status_t asn1_oer_decode_enumerated(asn1_byte_reader_t* r,asn1_oer_policy_t p,int64_t* out){
  if(!r||!out||!policy(p))return ASN1_ERR_INVALID_ARGUMENT;asn1_byte_view_t wire=asn1_byte_reader_view(r);if(!wire.size)return ASN1_ERR_TRUNCATED;
  if(!(wire.data[0]&128u)){int64_t value=wire.data[0];TRY(asn1_byte_reader_skip(r,1));*out=value;return ASN1_OK;}
  size_t n=wire.data[0]&127u;if(!n)return ASN1_ERR_INVALID_ARGUMENT;if(n>wire.size-1)return ASN1_ERR_TRUNCATED;
  size_t at=1;while(at<n&&((wire.data[at]==0&&!(wire.data[at+1]&128u))||(wire.data[at]==255&&(wire.data[at+1]&128u))))++at;
  if(p==ASN1_OER_CANONICAL&&at!=1)return ASN1_ERR_NON_CANONICAL;
  size_t width=n+1-at;if(width>8)return ASN1_ERR_CONSTRAINT_VIOLATION;
  uint64_t raw=0;for(size_t i=at;i<=n;++i)raw=(raw<<8)|wire.data[i];
  int negative=(wire.data[at]&128u)!=0;if(negative&&width<8)raw|=UINT64_MAX<<(width*8u);
  int64_t narrow=negative?-1-(int64_t)(UINT64_MAX-raw):(int64_t)raw;
  if(p==ASN1_OER_CANONICAL&&narrow>=0&&narrow<=127)return ASN1_ERR_NON_CANONICAL;
  TRY(asn1_byte_reader_skip(r,n+1u));*out=narrow;return ASN1_OK;
}
asn1_status_t asn1_oer_encode_tag(asn1_byte_writer_t* w,asn1_ber_tag_t tag){
  if(tag.cls>3)return ASN1_ERR_INVALID_ARGUMENT;uint8_t bytes[11];bytes[0]=(uint8_t)(tag.cls<<6);
  if(tag.number<63){bytes[0]|=(uint8_t)tag.number;return asn1_byte_writer_append(w,asn1_byte_view(bytes,1));}
  bytes[0]|=63;uint8_t reversed[10];size_t n=0;uint64_t value=tag.number;do{reversed[n++]=(uint8_t)(value&127u);value>>=7;}while(value);
  for(size_t i=0;i<n;++i)bytes[i+1]=(uint8_t)(reversed[n-1-i]|(i+1<n?128:0));return asn1_byte_writer_append(w,asn1_byte_view(bytes,n+1u));
}
asn1_status_t asn1_oer_decode_tag(asn1_byte_reader_t* r,asn1_ber_tag_t* out){
  if(!r||!out)return ASN1_ERR_INVALID_ARGUMENT;asn1_byte_view_t wire=asn1_byte_reader_view(r);if(!wire.size)return ASN1_ERR_TRUNCATED;
  asn1_ber_tag_t tag={(uint8_t)(wire.data[0]>>6),0,wire.data[0]&63u};size_t used=1;
  if(tag.number==63){tag.number=0;
    do{if(used==wire.size)return ASN1_ERR_TRUNCATED;unsigned b=wire.data[used++];if(used==2&&(b&127u)==0)return ASN1_ERR_INVALID_ARGUMENT;
      if(tag.number>(UINT64_MAX-(b&127u))/128u)return ASN1_ERR_LENGTH_OVERFLOW;tag.number=tag.number*128u+(b&127u);if(!(b&128u))break;
    }while(1);if(tag.number<63)return ASN1_ERR_INVALID_ARGUMENT;
  }TRY(asn1_byte_reader_skip(r,used));*out=tag;return ASN1_OK;
}
