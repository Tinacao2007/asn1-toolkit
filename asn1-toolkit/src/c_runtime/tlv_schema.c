#include <asn1/c_runtime/tlv_schema.h>
#include <asn1/c_runtime/bigint.h>
#include <asn1/c_runtime/bit_string.h>
#include <asn1/c_runtime/limits.h>
#include <asn1/c_runtime/value_content.h>
#include <stdlib.h>
#include <string.h>
#include "alloc_internal.h"
#define TRY(x) do { asn1_status_t s_=(x);if(s_!=ASN1_OK)return s_; }while(0)
typedef struct {size_t bytes,elements;} budget_t;
typedef struct {uint8_t* data;size_t size;asn1_ber_tag_t key;} piece_t;
static asn1_status_t reserve(budget_t* b,size_t bytes,size_t elements) {
  if(bytes>ASN1_MAX_PDU_ALLOCATION_BYTES-b->bytes || elements>ASN1_MAX_PDU_ELEMENTS-b->elements)return ASN1_ERR_RESOURCE_LIMIT;
  b->bytes+=bytes;b->elements+=elements;return ASN1_OK;
}
asn1_status_t asn1_tlv_string_length(uint64_t tag,asn1_byte_view_t data,size_t* count) {
  if(!count || (data.size&&!data.data))return ASN1_ERR_INVALID_ARGUMENT;
  size_t used=0,characters=0;
  while(used<data.size){uint32_t cp=data.data[used++];
    if(tag==12 && cp>=128u){unsigned need=cp>=0xc2u&&cp<=0xdfu?1u:cp>=0xe0u&&cp<=0xefu?2u:cp>=0xf0u&&cp<=0xf4u?3u:0u;
      if(!need || need>data.size-used)return ASN1_ERR_CONSTRAINT_VIOLATION;const uint32_t minimum=need==1?128u:need==2?2048u:65536u;cp&=(1u<<(6u-need))-1u;
      for(unsigned i=0;i<need;++i){const uint8_t b=data.data[used++];if((b&0xc0u)!=0x80u)return ASN1_ERR_CONSTRAINT_VIOLATION;cp=(cp<<6u)|(b&63u);}
      if(cp<minimum||cp>0x10ffffu||(cp>=0xd800u&&cp<=0xdfffu))return ASN1_ERR_CONSTRAINT_VIOLATION;
    } else if(tag==22 && cp>127u)return ASN1_ERR_CONSTRAINT_VIOLATION;
    else if(tag==26 && (cp<32u||cp>126u))return ASN1_ERR_CONSTRAINT_VIOLATION;
    else if(tag==18 && cp!=' ' && (cp<'0'||cp>'9'))return ASN1_ERR_CONSTRAINT_VIOLATION;
    else if(tag==19 && !((cp>='a'&&cp<='z')||(cp>='A'&&cp<='Z')||(cp>='0'&&cp<='9')|| (cp<128u && strchr(" '()+,-./:=?",(int)cp)!=NULL && cp!=0)))return ASN1_ERR_CONSTRAINT_VIOLATION;
    ++characters;
  }*count=characters;return ASN1_OK;
}
static asn1_ber_tag_t natural(const asn1_tlv_schema_t* t) {
  asn1_ber_tag_t tag={0,0,t->universal_tag};
  tag.constructed=t->kind==ASN1_TLV_SEQUENCE||t->kind==ASN1_TLV_SET||t->kind==ASN1_TLV_SEQUENCE_OF||t->kind==ASN1_TLV_SET_OF;
  return tag;
}
static int same_tag(asn1_ber_tag_t a,asn1_ber_tag_t b){return a.cls==b.cls&&a.number==b.number;}
static int tag_order(asn1_ber_tag_t a,asn1_ber_tag_t b) {
  if(a.cls!=b.cls)return a.cls>b.cls?1:-1;return (a.number>b.number)-(a.number<b.number);
}
static asn1_ber_tag_t smallest_tag(const asn1_tlv_schema_t* t,size_t depth) {
  asn1_ber_tag_t best={255,0,0};if(depth>=ASN1_MAX_RECURSION_DEPTH)return best;
  if(t->tag.cls!=255)return t->tag;
  if(t->kind==ASN1_TLV_ALIAS)return smallest_tag(t->element(),depth+1u);
  if(t->kind!=ASN1_TLV_CHOICE)return natural(t);
  for(size_t i=0;i<t->field_count;++i){const asn1_tlv_field_t* f=t->fields+i;
    asn1_ber_tag_t candidate=f->tag.cls==255?smallest_tag(f->type(),depth+1u):f->tag;
    if(tag_order(candidate,best)<0)best=candidate;
  }return best;
}
static const void* field_value(const void* v,const asn1_tlv_field_t* f){
  const void* p=(const uint8_t*)v+f->offset;return f->indirect?*(void*const*)p:p;
}
static int matches(const asn1_tlv_schema_t* t,asn1_ber_tag_t tag,size_t depth);
static int field_matches(const asn1_tlv_field_t* f,asn1_ber_tag_t tag,size_t depth) {
  return f->tag.cls!=255?same_tag(f->tag,tag):matches(f->type(),tag,depth+1u);
}
static int matches(const asn1_tlv_schema_t* t,asn1_ber_tag_t tag,size_t depth) {
  if(depth>=ASN1_MAX_RECURSION_DEPTH)return 0;
  if(t->tag.cls!=255)return same_tag(t->tag,tag);
  if(t->kind==ASN1_TLV_ALIAS)return matches(t->element(),tag,depth+1u);
  if(t->kind==ASN1_TLV_OPEN)return 1;
  if(t->kind==ASN1_TLV_CHOICE){for(size_t i=0;i<t->field_count;++i)if(field_matches(t->fields+i,tag,depth))return 1;return 0;}
  return same_tag(natural(t),tag);
}
static int compare_bytes(const void* a,const void* b) {
  const piece_t* x=(const piece_t*)a;const piece_t* y=(const piece_t*)b;
  const size_t n=x->size<y->size?x->size:y->size;const int c=n?memcmp(x->data,y->data,n):0;
  return c?c:(x->size>y->size)-(x->size<y->size);
}
static int compare_tags(const void* a,const void* b) {
  const piece_t* x=(const piece_t*)a;const piece_t* y=(const piece_t*)b;
  asn1_ber_tag_t xt,yt;size_t unused;
  xt=x->key;yt=y->key;
  if((xt.cls==255 && asn1_ber_tag_decode(asn1_byte_view(x->data,x->size),&xt,&unused)!=ASN1_OK) ||
     (yt.cls==255 && asn1_ber_tag_decode(asn1_byte_view(y->data,y->size),&yt,&unused)!=ASN1_OK))return 0;
  return tag_order(xt,yt);
}
/* X.680 automatic tagging of an untagged CHOICE retains the selected tag
 * inside an explicit wrapper. An already tagged alias has an outer tag that
 * an IMPLICIT tag may replace. */
static int untagged_choice(const asn1_tlv_schema_t* t,size_t depth) {
  if(depth>=ASN1_MAX_RECURSION_DEPTH || t->tag.cls!=255)return 0;
  if(t->kind==ASN1_TLV_ALIAS)return untagged_choice(t->element(),depth+1u);
  return t->kind==ASN1_TLV_CHOICE;
}
static int schema_explicit(const asn1_tlv_schema_t* t) {
  return t->explicit_tag || t->kind==ASN1_TLV_CHOICE ||
    (t->kind==ASN1_TLV_ALIAS && untagged_choice(t->element(),0));
}
static asn1_status_t transform(asn1_byte_writer_t* out,asn1_tlv_policy_t p,asn1_ber_tag_t tag,int explicit_tag,asn1_byte_view_t wire) {
  if(tag.cls==255)return asn1_byte_writer_append(out,wire);
  if(explicit_tag){tag.constructed=1;return asn1_tlv_write(out,p,tag,wire);}
  asn1_tlv_t inner;TRY(asn1_tlv_parse(wire,p,&inner));tag.constructed=inner.tag.constructed;
  return asn1_tlv_write(out,p,tag,inner.content);
}
static asn1_status_t encode_at(asn1_byte_writer_t*,asn1_tlv_policy_t,const asn1_tlv_schema_t*,const void*,size_t);
static asn1_status_t encode_field(asn1_byte_writer_t* out,asn1_tlv_policy_t p,const asn1_tlv_field_t* f,const void* parent,size_t depth) {
  const void* value=field_value(parent,f);if(!value)return ASN1_ERR_CONSTRAINT_VIOLATION;
  asn1_byte_writer_t* w=NULL;TRY(asn1_byte_writer_create(&w));
  asn1_status_t s=encode_at(w,p,f->type(),value,depth+1u);
  if(s==ASN1_OK)s=transform(out,p,f->tag,f->explicit_tag || untagged_choice(f->type(),0),asn1_byte_writer_view(w));
  asn1_byte_writer_destroy(w);return s;
}
static asn1_status_t blob_encode(asn1_byte_writer_t* out,asn1_tlv_policy_t p,asn1_ber_tag_t tag,asn1_byte_view_t data,int unused) {
  const int bits=tag.number==3;
  if(data.size>ASN1_MAX_STRING_BYTES)return ASN1_ERR_RESOURCE_LIMIT;
  asn1_byte_writer_t* content=NULL;TRY(asn1_byte_writer_create(&content));asn1_status_t s=ASN1_OK;
  const size_t limit=bits?999u:1000u;
  if(p==ASN1_TLV_CER && data.size+(bits?1u:0u)>1000u) {
    for(size_t at=0;at<data.size && s==ASN1_OK;at+=limit) {
      const size_t n=data.size-at>limit?limit:data.size-at;
      asn1_byte_writer_t* fragment=NULL;s=asn1_byte_writer_create(&fragment);
      if(s==ASN1_OK && bits){const uint8_t u=at+n==data.size?(uint8_t)unused:0;s=asn1_byte_writer_append(fragment,asn1_byte_view(&u,1));}
      if(s==ASN1_OK)s=asn1_byte_writer_append(fragment,asn1_byte_view(data.data+at,n));
      if(s==ASN1_OK){asn1_ber_tag_t fragment_tag=tag;if(!bits)fragment_tag.number=4;s=asn1_tlv_write(content,p,fragment_tag,asn1_byte_writer_view(fragment));}
      asn1_byte_writer_destroy(fragment);
    }
    tag.constructed=1;
  } else {
    if(bits){const uint8_t u=(uint8_t)unused;s=asn1_byte_writer_append(content,asn1_byte_view(&u,1));}
    if(s==ASN1_OK)s=asn1_byte_writer_append(content,data);
  }
  if(s==ASN1_OK)s=asn1_tlv_write(out,p,tag,asn1_byte_writer_view(content));
  asn1_byte_writer_destroy(content);return s;
}
static asn1_status_t encode_body(asn1_byte_writer_t* out,asn1_tlv_policy_t p,const asn1_tlv_schema_t* t,const void* v,size_t depth) {
  const asn1_ber_tag_t tag=natural(t);
  const uint8_t* base=(const uint8_t*)v;
  switch(t->kind) {
    case ASN1_TLV_ALIAS:{const void* value=base+t->value_offset;if(t->indirect)value=*(void*const*)value;return value?encode_at(out,p,t->element(),value,depth+1u):ASN1_ERR_CONSTRAINT_VIOLATION;}
    case ASN1_TLV_BOOLEAN:return asn1_tlv_encode_boolean(out,p,*(const int*)(base+t->value_offset));
    case ASN1_TLV_NULL:return asn1_tlv_write(out,p,tag,asn1_byte_view(NULL,0));
    case ASN1_TLV_INT64:{
      const uint64_t number=(uint64_t)*(const int64_t*)(base+t->value_offset);uint8_t data[8];
      for(size_t i=0;i<8;++i)data[7-i]=(uint8_t)(number>>(i*8u));size_t at=0;
      while(at<7 && ((data[at]==0 && !(data[at+1]&128u)) || (data[at]==255u && (data[at+1]&128u))))++at;
      return asn1_tlv_write(out,p,tag,asn1_byte_view(data+at,8-at));}
    case ASN1_TLV_BIGINT:{uint8_t* data=NULL;size_t n=0;TRY(asn1_bigint_to_twos_complement((const asn1_bigint_t*)(base+t->value_offset),&data,&n));
      asn1_status_t s=asn1_tlv_write(out,p,tag,asn1_byte_view(data,n));free(data);return s;}
    case ASN1_TLV_REAL:case ASN1_TLV_OID:case ASN1_TLV_RELATIVE_OID:{uint8_t* data=NULL;size_t n=0;
      asn1_status_t s=t->kind==ASN1_TLV_REAL?asn1_value_encode_real(*(const double*)(base+t->value_offset),&data,&n):asn1_value_encode_oid(*(uint64_t*const*)(base+t->data_offset),*(const size_t*)(base+t->count_offset),t->kind==ASN1_TLV_RELATIVE_OID,&data,&n);
      if(s==ASN1_OK)s=asn1_tlv_write(out,p,tag,asn1_byte_view(data,n));free(data);return s;}
    case ASN1_TLV_OCTETS:case ASN1_TLV_OPEN:{
      const size_t n=*(const size_t*)(base+t->count_offset),cap=*(const size_t*)(base+t->capacity_offset);const uint8_t* data=*(uint8_t*const*)(base+t->data_offset);
      if(n>cap || (n&&!data))return ASN1_ERR_INVALID_ARGUMENT;
      if(t->kind==ASN1_TLV_OPEN){asn1_tlv_t value;TRY(asn1_tlv_parse(asn1_byte_view(data,n),p,&value));if(value.encoding.size!=n)return ASN1_ERR_INVALID_ARGUMENT;return asn1_byte_writer_append(out,value.encoding);}
      return blob_encode(out,p,tag,asn1_byte_view(data,n),0);}
    case ASN1_TLV_BITS:{const asn1_bitstring_t* bits=(const asn1_bitstring_t*)(base+t->value_offset);const size_t n=bits->bit_length/8u+(bits->bit_length%8u!=0);
      if(n>bits->cap_bytes || (n&&!bits->bits))return ASN1_ERR_INVALID_ARGUMENT;
      const int unused=(int)((8u-bits->bit_length%8u)%8u);
      if(n&&unused&&(bits->bits[n-1]&((1u<<unused)-1u)))return ASN1_ERR_CONSTRAINT_VIOLATION;
      return blob_encode(out,p,tag,asn1_byte_view(bits->bits,n),unused);}
    case ASN1_TLV_CHOICE:{const size_t selected=t->choice_get(v);if(selected>=t->field_count)return ASN1_ERR_CONSTRAINT_VIOLATION;return encode_field(out,p,t->fields+selected,v,depth);}
    default:break;
  }
  const int collection=t->kind==ASN1_TLV_SEQUENCE_OF || t->kind==ASN1_TLV_SET_OF;
  const size_t count=collection?*(const size_t*)(base+t->count_offset):t->field_count;
  if(count>ASN1_MAX_PDU_ELEMENTS)return ASN1_ERR_RESOURCE_LIMIT;
  if(collection && count>*(const size_t*)(base+t->capacity_offset))return ASN1_ERR_INVALID_ARGUMENT;
  const void* data=collection?*(void*const*)(base+t->data_offset):NULL;
  if(collection && count&&!data)return ASN1_ERR_INVALID_ARGUMENT;
  piece_t* pieces=(piece_t*)calloc(count?count:1u,sizeof(*pieces));if(!pieces)return ASN1_ERR_OUT_OF_MEMORY;
  size_t used=0;asn1_status_t s=ASN1_OK;
  for(size_t i=0;i<count && s==ASN1_OK;++i) {
    if(!collection){const asn1_tlv_field_t* f=t->fields+i;
      if(f->presence!=SIZE_MAX){const int present=*(const int*)(base+f->presence);if(present!=0&&present!=1){s=ASN1_ERR_INVALID_ARGUMENT;break;}if(!present)continue;}
      if(f->is_default && f->is_default(v))continue;
    }
    asn1_byte_writer_t* w=NULL;s=asn1_byte_writer_create(&w);
    if(s==ASN1_OK)s=collection?encode_at(w,p,t->element(),(const uint8_t*)data+i*t->element()->size,depth+1u):encode_field(w,p,t->fields+i,v,depth);
    if(s==ASN1_OK)s=asn1_byte_writer_take(w,&pieces[used].data,&pieces[used].size);
    pieces[used].key.cls=255;
    if(s==ASN1_OK && p==ASN1_TLV_CER && t->kind==ASN1_TLV_SET) {
      const asn1_tlv_field_t* f=t->fields+i;pieces[used].key=f->tag.cls==255?smallest_tag(f->type(),0):f->tag;
    }
    asn1_byte_writer_destroy(w);if(s==ASN1_OK)++used;
  }
  if(p!=ASN1_TLV_BER && (t->kind==ASN1_TLV_SET||t->kind==ASN1_TLV_SET_OF))qsort(pieces,used,sizeof(*pieces),t->kind==ASN1_TLV_SET?compare_tags:compare_bytes);
  asn1_byte_writer_t* contents=NULL;if(s==ASN1_OK)s=asn1_byte_writer_create(&contents);
  for(size_t i=0;i<used;++i){if(s==ASN1_OK)s=asn1_byte_writer_append(contents,asn1_byte_view(pieces[i].data,pieces[i].size));free(pieces[i].data);}free(pieces);
  if(s==ASN1_OK)s=asn1_tlv_write(out,p,tag,asn1_byte_writer_view(contents));asn1_byte_writer_destroy(contents);return s;
}
static asn1_status_t encode_at(asn1_byte_writer_t* out,asn1_tlv_policy_t p,const asn1_tlv_schema_t* t,const void* v,size_t depth) {
  if(depth>=ASN1_MAX_RECURSION_DEPTH)return ASN1_ERR_RESOURCE_LIMIT;
  TRY(t->validate(v));
  asn1_byte_writer_t* w=NULL;TRY(asn1_byte_writer_create(&w));asn1_status_t s=encode_body(w,p,t,v,depth);
  if(s==ASN1_OK)s=transform(out,p,t->tag,schema_explicit(t),asn1_byte_writer_view(w));asn1_byte_writer_destroy(w);return s;
}
static asn1_status_t decode_at(const asn1_tlv_schema_t*,void*,asn1_tlv_policy_t,asn1_byte_view_t,size_t*,budget_t*,size_t,int);
static asn1_status_t decode_field(const asn1_tlv_field_t* f,void* v,asn1_tlv_policy_t p,asn1_byte_view_t wire,size_t* used,budget_t* b,size_t depth) {
  asn1_tlv_t outer;TRY(asn1_tlv_parse(wire,p,&outer));if(!field_matches(f,outer.tag,depth))return ASN1_ERR_TAG_MISMATCH;
  uint8_t* place=(uint8_t*)v+f->offset;
  if(f->indirect){TRY(reserve(b,f->type()->size,1));void* value=calloc(1,f->type()->size);if(!value)return ASN1_ERR_OUT_OF_MEMORY;*(void**)place=value;f->type()->init(value);place=(uint8_t*)value;}
  size_t consumed=0;asn1_status_t s;
  if(f->tag.cls!=255 && (f->explicit_tag || untagged_choice(f->type(),0))){if(!outer.tag.constructed)return ASN1_ERR_TAG_MISMATCH;s=decode_at(f->type(),place,p,outer.content,&consumed,b,depth+1u,0);if(s==ASN1_OK && consumed!=outer.content.size)s=ASN1_ERR_INVALID_ARGUMENT;}
  else s=decode_at(f->type(),place,p,outer.encoding,&consumed,b,depth+1u,f->tag.cls!=255);
  if(s!=ASN1_OK)return s;
  if(p!=ASN1_TLV_BER && f->is_default && f->is_default(v))return ASN1_ERR_NON_CANONICAL;
  if(f->presence!=SIZE_MAX)*(int*)((uint8_t*)v+f->presence)=1;
  *used=outer.encoding.size;return ASN1_OK;
}
static asn1_status_t collect_blob(const asn1_tlv_t* value,asn1_tlv_policy_t p,uint64_t tag,int bits,size_t depth,uint8_t* out,size_t* size,int* unused,budget_t* budget) {
  if(depth>=ASN1_MAX_RECURSION_DEPTH)return ASN1_ERR_RESOURCE_LIMIT;
  if(!value->tag.constructed){const size_t n=value->content.size;
    if(p==ASN1_TLV_CER && n>1000u)return ASN1_ERR_NON_CANONICAL;
    if(bits){if(!n || value->content.data[0]>7u || (n==1 && value->content.data[0]))return ASN1_ERR_INVALID_ARGUMENT;
      *unused=value->content.data[0];if(p!=ASN1_TLV_BER && n>1 && *unused && (value->content.data[n-1]&((1u<<*unused)-1u)))return ASN1_ERR_NON_CANONICAL;
      if(n-1>ASN1_MAX_STRING_BYTES-*size)return ASN1_ERR_RESOURCE_LIMIT;
      if(budget)TRY(reserve(budget,n-1,0));
      if(out&&n>1)memcpy(out+*size,value->content.data+1,n-1);*size+=n-1;return ASN1_OK;}
    if(n>ASN1_MAX_STRING_BYTES-*size)return ASN1_ERR_RESOURCE_LIMIT;
    if(budget)TRY(reserve(budget,n,0));
    if(out&&n)memcpy(out+*size,value->content.data,n);*size+=n;return ASN1_OK;
  }
  if(p==ASN1_TLV_DER)return ASN1_ERR_NON_CANONICAL;
  size_t at=0,previous=0,count=0;int previous_unused=0;
  while(at<value->content.size){asn1_tlv_t child;TRY(asn1_tlv_parse(asn1_byte_view(value->content.data+at,value->content.size-at),p,&child));
    if(child.tag.cls!=0 || child.tag.number!=(bits?3u:4u))return ASN1_ERR_TAG_MISMATCH;
    if(bits && count && previous_unused)return ASN1_ERR_INVALID_ARGUMENT;
    if(p==ASN1_TLV_CER && (child.tag.constructed || (count && previous!=1000u) || !child.content.size))return ASN1_ERR_NON_CANONICAL;
    TRY(collect_blob(&child,p,tag,bits,depth+1u,out,size,unused,budget));previous_unused=*unused;previous=child.content.size;++count;at+=child.encoding.size;
  }
  if(p==ASN1_TLV_CER && *size+(bits?1u:0u)<=1000u)return ASN1_ERR_NON_CANONICAL;
  return ASN1_OK;
}
static asn1_status_t decode_body(const asn1_tlv_schema_t* t,void* v,asn1_tlv_policy_t p,const asn1_tlv_t* value,budget_t* b,size_t depth,int overridden) {
  uint8_t* base=(uint8_t*)v;
  if(t->kind==ASN1_TLV_ALIAS){uint8_t* child=base+t->value_offset;if(t->indirect){TRY(reserve(b,t->element()->size,1));void* allocation=calloc(1,t->element()->size);if(!allocation)return ASN1_ERR_OUT_OF_MEMORY;*(void**)child=allocation;t->element()->init(allocation);child=(uint8_t*)allocation;}
    size_t used=0;return decode_at(t->element(),child,p,value->encoding,&used,b,depth+1u,overridden);}
  if(t->kind==ASN1_TLV_CHOICE){for(size_t i=0;i<t->field_count;++i)if(field_matches(t->fields+i,value->tag,depth)){t->choice_set(v,i);size_t used=0;return decode_field(t->fields+i,v,p,value->encoding,&used,b,depth);}return ASN1_ERR_TAG_MISMATCH;}
  if(t->kind==ASN1_TLV_OPEN){TRY(reserve(b,value->encoding.size,0));uint8_t* data=(uint8_t*)malloc(value->encoding.size);if(!data)return ASN1_ERR_OUT_OF_MEMORY;
    memcpy(data,value->encoding.data,value->encoding.size);*(uint8_t**)(base+t->data_offset)=data;*(size_t*)(base+t->count_offset)=*(size_t*)(base+t->capacity_offset)=value->encoding.size;return ASN1_OK;}
  if(!overridden && !same_tag(natural(t),value->tag))return ASN1_ERR_TAG_MISMATCH;
  const int constructed=t->kind==ASN1_TLV_SEQUENCE||t->kind==ASN1_TLV_SET||t->kind==ASN1_TLV_SEQUENCE_OF||t->kind==ASN1_TLV_SET_OF;
  if(constructed && !value->tag.constructed)return ASN1_ERR_TAG_MISMATCH;
  if(!constructed && t->kind!=ASN1_TLV_BITS && t->kind!=ASN1_TLV_OCTETS && value->tag.constructed)return ASN1_ERR_TAG_MISMATCH;
  switch(t->kind) {
    case ASN1_TLV_BOOLEAN:{asn1_tlv_t boolean=*value;boolean.tag=natural(t);return asn1_tlv_decode_boolean(&boolean,p,(int*)(base+t->value_offset));}
    case ASN1_TLV_NULL:return value->content.size?ASN1_ERR_INVALID_ARGUMENT:ASN1_OK;
    case ASN1_TLV_INT64:case ASN1_TLV_BIGINT:{
      const size_t limbs=value->content.size/4u+(value->content.size%4u!=0);
      if(limbs>ASN1_MAX_BIGINT_LIMBS+1u)return ASN1_ERR_RESOURCE_LIMIT;
      size_t allocation=0,capacity=4;for(;;){allocation+=capacity*sizeof(uint32_t);if(capacity>=limbs||capacity>=ASN1_MAX_BIGINT_LIMBS)break;capacity*=2u;}
      TRY(reserve(b,allocation,0));asn1_bigint_t number;asn1_bigint_init(&number);TRY(asn1_bigint_init_from_twos_complement(&number,value->content));
      if(t->kind==ASN1_TLV_BIGINT){*(asn1_bigint_t*)(base+t->value_offset)=number;return ASN1_OK;}
      asn1_status_t s=asn1_bigint_to_int64(&number,(int64_t*)(base+t->value_offset));asn1_bigint_free(&number);return s;}
    case ASN1_TLV_REAL:{double decoded=0;
      /* A double stores base-2 values, not the abstract base-10 REAL triple.
         Do not misclassify a valid decimal DER/CER value as non-canonical. */
      if(p!=ASN1_TLV_BER && value->content.size && value->content.data[0]>=1u && value->content.data[0]<=3u)return ASN1_ERR_UNSUPPORTED;
      TRY(asn1_value_decode_real(value->content,&decoded));
      if(p!=ASN1_TLV_BER){uint8_t* canonical=NULL;size_t n=0;TRY(asn1_value_encode_real(decoded,&canonical,&n));const int equal=n==value->content.size && (!n || !memcmp(canonical,value->content.data,n));free(canonical);if(!equal)return ASN1_ERR_NON_CANONICAL;}
      *(double*)(base+t->value_offset)=decoded;return ASN1_OK;}
    case ASN1_TLV_OID:case ASN1_TLV_RELATIVE_OID:{
      if(value->content.size>ASN1_MAX_STRING_BYTES)return ASN1_ERR_RESOURCE_LIMIT;const size_t capacity=value->content.size+(t->kind==ASN1_TLV_OID?1u:0u);TRY(reserve(b,capacity*sizeof(uint64_t),capacity));
      uint64_t* arcs=NULL;size_t count=0;TRY(asn1_value_decode_oid(value->content,&arcs,&count,t->kind==ASN1_TLV_RELATIVE_OID));
      *(uint64_t**)(base+t->data_offset)=arcs;*(size_t*)(base+t->count_offset)=count;*(size_t*)(base+t->capacity_offset)=capacity;return ASN1_OK;}
    case ASN1_TLV_OCTETS:case ASN1_TLV_BITS:{
      int unused=0;size_t n=0;
      TRY(collect_blob(value,p,t->universal_tag,t->kind==ASN1_TLV_BITS,depth,NULL,&n,&unused,b));
      uint8_t* data=n?(uint8_t*)malloc(n):NULL;if(n&&!data)return ASN1_ERR_OUT_OF_MEMORY;
      size_t copied=0;asn1_status_t s=collect_blob(value,p,t->universal_tag,t->kind==ASN1_TLV_BITS,depth,data,&copied,&unused,NULL);
      if(s!=ASN1_OK){free(data);return s;}
      if(t->kind==ASN1_TLV_BITS){asn1_bitstring_t* bits=(asn1_bitstring_t*)(base+t->value_offset);if(n&&unused)data[n-1]&=(uint8_t)(0xffu<<unused);bits->bits=data;bits->cap_bytes=n;bits->bit_length=n*8u-(size_t)unused;}
      else {*(uint8_t**)(base+t->data_offset)=data;*(size_t*)(base+t->count_offset)=*(size_t*)(base+t->capacity_offset)=n;}return ASN1_OK;}
    default:break;
  }
  const int collection=t->kind==ASN1_TLV_SEQUENCE_OF||t->kind==ASN1_TLV_SET_OF;
  size_t at=0,next=0,count=0;asn1_byte_view_t previous=asn1_byte_view(NULL,0);asn1_ber_tag_t previous_tag={255,0,0};
  uint8_t* seen=NULL;if(!collection){TRY(reserve(b,t->field_count,0));seen=(uint8_t*)calloc(t->field_count?t->field_count:1u,1);if(!seen)return ASN1_ERR_OUT_OF_MEMORY;}
  asn1_status_t s=ASN1_OK;
  while(at<value->content.size && s==ASN1_OK){
    asn1_tlv_t child;s=asn1_tlv_parse(asn1_byte_view(value->content.data+at,value->content.size-at),p,&child);if(s!=ASN1_OK)break;
    if(p!=ASN1_TLV_BER && previous.size && t->kind==ASN1_TLV_SET_OF){
      piece_t a={(uint8_t*)previous.data,previous.size,{255,0,0}},c={(uint8_t*)child.encoding.data,child.encoding.size,{255,0,0}};
      if(compare_bytes(&a,&c)>0){s=ASN1_ERR_NON_CANONICAL;break;}}
    previous=child.encoding;
    if(collection){
      if(count==SIZE_MAX/t->element()->size){s=ASN1_ERR_LENGTH_OVERFLOW;break;}
      s=reserve(b,t->element()->size,1);if(s!=ASN1_OK)break;
      void* data=*(void**)(base+t->data_offset);void* grown=realloc(data,(count+1u)*t->element()->size);if(!grown){s=ASN1_ERR_OUT_OF_MEMORY;break;}
      *(void**)(base+t->data_offset)=grown;*(size_t*)(base+t->capacity_offset)=count+1u;
      void* element=(uint8_t*)grown+count*t->element()->size;t->element()->init(element);*(size_t*)(base+t->count_offset)=++count;
      size_t used=0;s=decode_at(t->element(),element,p,child.encoding,&used,b,depth+1u,0);
    } else {
      size_t selected=t->field_count;
      for(size_t i=t->kind==ASN1_TLV_SET?0:next;i<t->field_count;++i) {
        const asn1_tlv_field_t* f=t->fields+i;
        if(field_matches(f,child.tag,depth)){selected=i;break;}
        if(t->kind!=ASN1_TLV_SET && f->presence==SIZE_MAX && !f->is_default)break;
      }
      if(p!=ASN1_TLV_BER && t->kind==ASN1_TLV_SET) {
        asn1_ber_tag_t key=child.tag;
        if(p==ASN1_TLV_CER && selected<t->field_count){const asn1_tlv_field_t* f=t->fields+selected;key=f->tag.cls==255?smallest_tag(f->type(),0):f->tag;}
        if(previous_tag.cls!=255 && tag_order(previous_tag,key)>0){s=ASN1_ERR_NON_CANONICAL;break;}
        previous_tag=key;
      }
      if(selected==t->field_count){
        for(size_t i=0;i<t->field_count;++i)if(field_matches(t->fields+i,child.tag,depth)){s=ASN1_ERR_INVALID_ARGUMENT;break;}
        if(s!=ASN1_OK)break;if(!t->extensible){s=ASN1_ERR_TAG_MISMATCH;break;}
        if(t->kind==ASN1_TLV_SEQUENCE){
          size_t extension_start=0;
          while(extension_start<t->field_count && !t->fields[extension_start].extension)++extension_start;
          for(size_t i=next;i<extension_start;++i)if(t->fields[i].presence==SIZE_MAX && !t->fields[i].is_default){s=ASN1_ERR_CONSTRAINT_VIOLATION;break;}
          if(s!=ASN1_OK)break;if(next<extension_start)next=extension_start;
        }}
      else if(seen[selected]){s=ASN1_ERR_INVALID_ARGUMENT;break;}
      else {seen[selected]=1;next=selected+1u;size_t used=0;s=decode_field(t->fields+selected,v,p,child.encoding,&used,b,depth);}
    }
    at+=child.encoding.size;
  }
  if(!collection && s==ASN1_OK)for(size_t i=0;i<t->field_count;++i)if(!seen[i] && t->fields[i].presence==SIZE_MAX && !t->fields[i].is_default){s=ASN1_ERR_CONSTRAINT_VIOLATION;break;}
  if(!collection && s==ASN1_OK)for(size_t i=0;i<t->field_count;++i)if(!seen[i] && t->fields[i].default_init){s=reserve(b,t->fields[i].default_bytes,0);if(s==ASN1_OK)s=t->fields[i].default_init(v);if(s!=ASN1_OK)break;}
  free(seen);return s;
}
static asn1_status_t decode_at(const asn1_tlv_schema_t* t,void* v,asn1_tlv_policy_t p,asn1_byte_view_t wire,size_t* used,budget_t* b,size_t depth,int overridden) {
  if(depth>=ASN1_MAX_RECURSION_DEPTH)return ASN1_ERR_RESOURCE_LIMIT;
  asn1_tlv_t value;TRY(asn1_tlv_parse(wire,p,&value));
  if(t->tag.cls!=255){if(!overridden && !same_tag(t->tag,value.tag))return ASN1_ERR_TAG_MISMATCH;
    if(schema_explicit(t)){if(!value.tag.constructed)return ASN1_ERR_TAG_MISMATCH;asn1_tlv_t inner;TRY(asn1_tlv_parse(value.content,p,&inner));if(inner.encoding.size!=value.content.size)return ASN1_ERR_INVALID_ARGUMENT;
      TRY(decode_body(t,v,p,&inner,b,depth,0));}
    else TRY(decode_body(t,v,p,&value,b,depth,1));
  } else TRY(decode_body(t,v,p,&value,b,depth,overridden));
  TRY(t->validate(v));*used=value.encoding.size;return ASN1_OK;
}
asn1_status_t asn1_tlv_schema_encode(asn1_byte_writer_t* w,asn1_tlv_policy_t p,const asn1_tlv_schema_t* t,const void* v) {
  if(!w||!t||!v)return ASN1_ERR_INVALID_ARGUMENT;return encode_at(w,p,t,v,0);
}
asn1_status_t asn1_tlv_schema_decode(asn1_byte_reader_t* r,asn1_tlv_policy_t p,const asn1_tlv_schema_t* t,void* out) {
  if(!r||!t||!out)return ASN1_ERR_INVALID_ARGUMENT;
  budget_t budget={0,0};TRY(reserve(&budget,t->size,1));void* value=calloc(1,t->size);if(!value)return ASN1_ERR_OUT_OF_MEMORY;t->init(value);
  size_t used=0;asn1_status_t s=decode_at(t,value,p,asn1_byte_reader_view(r),&used,&budget,0,0);
  if(s==ASN1_OK){t->clear(out);memcpy(out,value,t->size);s=asn1_byte_reader_skip(r,used);}else t->clear(value);
  free(value);return s;
}
