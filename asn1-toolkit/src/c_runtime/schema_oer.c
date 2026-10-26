#include "schema_internal.h"
#include <asn1/c_runtime/bigint.h>
#include <asn1/c_runtime/bit_string.h>
#include <asn1/c_runtime/value_content.h>
#include <stdlib.h>
#include <float.h>
#include <math.h>
#include "alloc_internal.h"
#define TRY(x) do{asn1_status_t s_=(x);if(s_!=ASN1_OK)return s_;}while(0)
static asn1_status_t write_byte(asn1_byte_writer_t* w,uint8_t b){return asn1_byte_writer_append(w,asn1_byte_view(&b,1));}
static int tag_equal(asn1_ber_tag_t a,asn1_ber_tag_t b){return a.cls==b.cls&&a.number==b.number;}
static int field_optional(const asn1_codec_schema_t* t,size_t i){return t->fields[i].optional||t->layout()->fields[i].is_default!=NULL;}
static asn1_status_t ieee_encode(asn1_byte_writer_t* w,unsigned form,double value){
  if(FLT_RADIX!=2||DBL_MANT_DIG!=53||sizeof(double)!=8||FLT_MANT_DIG!=24||sizeof(float)!=4)return ASN1_ERR_UNSUPPORTED;
  uint64_t bits=0;size_t n=form==1?4:8;
  if(form==1){if(isfinite(value)&&fabs(value)>FLT_MAX)return ASN1_ERR_CONSTRAINT_VIOLATION;float narrow=(float)value;if(isfinite(value)&&(!isfinite(narrow)||(double)narrow!=value))return ASN1_ERR_CONSTRAINT_VIOLATION;uint32_t word;memcpy(&word,&narrow,4);bits=word;}
  else memcpy(&bits,&value,8);
  uint8_t bytes[8];for(size_t i=0;i<n;++i)bytes[n-1-i]=(uint8_t)(bits>>(i*8u));return asn1_byte_writer_append(w,asn1_byte_view(bytes,n));
}
static asn1_status_t ieee_decode(asn1_byte_reader_t* r,unsigned form,double* out){
  if(FLT_RADIX!=2||DBL_MANT_DIG!=53||sizeof(double)!=8||FLT_MANT_DIG!=24||sizeof(float)!=4)return ASN1_ERR_UNSUPPORTED;
  size_t n=form==1?4:8;asn1_byte_view_t bytes;TRY(asn1_byte_reader_read(r,n,&bytes));uint64_t bits=0;for(size_t i=0;i<n;++i)bits=(bits<<8)|bytes.data[i];
  if(form==1){uint32_t word=(uint32_t)bits;float value;memcpy(&value,&word,4);*out=value;}else memcpy(out,&bits,8);return ASN1_OK;
}
static asn1_status_t encode_field(asn1_byte_writer_t* w,asn1_oer_policy_t p,const asn1_codec_schema_t* t,size_t i,const void* value,size_t d){
  const void* child=asn1_schema_field_value(value,t->layout()->fields+i);if(!child)return ASN1_ERR_CONSTRAINT_VIOLATION;return asn1_schema_oer_encode(w,p,t->fields[i].type(),child,d+1);
}
static asn1_status_t choice_tag(const asn1_codec_schema_t* t,const void* v,asn1_ber_tag_t* tag,size_t depth){
  if(depth>=ASN1_MAX_RECURSION_DEPTH)return ASN1_ERR_RESOURCE_LIMIT;const asn1_tlv_schema_t* l=t->layout();
  if(l->tag.cls!=255){*tag=l->tag;return ASN1_OK;}
  if(l->kind==ASN1_TLV_ALIAS){const void* child=(const uint8_t*)v+l->value_offset;if(l->indirect)child=*(void*const*)child;return child?choice_tag(t->element(),child,tag,depth+1):ASN1_ERR_CONSTRAINT_VIOLATION;}
  if(l->kind!=ASN1_TLV_CHOICE){if(!l->universal_tag)return ASN1_ERR_TAG_MISMATCH;tag->cls=0;tag->constructed=0;tag->number=l->universal_tag;return ASN1_OK;}size_t i=l->choice_get(v);if(i>=l->field_count)return ASN1_ERR_CONSTRAINT_VIOLATION;
  *tag=t->fields[i].tag;if(tag->cls!=255)return ASN1_OK;const void* child=asn1_schema_field_value(v,l->fields+i);return child?choice_tag(t->fields[i].type(),child,tag,depth+1):ASN1_ERR_CONSTRAINT_VIOLATION;
}
static int choice_matches(const asn1_codec_schema_t* t,asn1_ber_tag_t tag,size_t depth){
  if(depth>=ASN1_MAX_RECURSION_DEPTH)return 0;const asn1_tlv_schema_t* l=t->layout();if(l->tag.cls!=255)return tag_equal(l->tag,tag);if(l->kind==ASN1_TLV_ALIAS)return choice_matches(t->element(),tag,depth+1);if(l->kind!=ASN1_TLV_CHOICE)return tag.cls==0&&tag.number==l->universal_tag&&l->universal_tag!=0;
  for(size_t i=0;i<l->field_count;++i)if(t->fields[i].tag.cls!=255?tag_equal(t->fields[i].tag,tag):choice_matches(t->fields[i].type(),tag,depth+1))return 1;return 0;
}
static int group_present(const asn1_codec_schema_t* t,size_t group,const void* v){for(size_t i=0;i<t->layout()->field_count;++i)if(t->fields[i].group==group&&asn1_schema_field_present(v,t->layout()->fields+i))return 1;return 0;}
static asn1_status_t encode_group(asn1_byte_writer_t* w,asn1_oer_policy_t p,const asn1_codec_schema_t* t,size_t group,const void* v,size_t depth){
  const asn1_tlv_schema_t* l=t->layout();size_t bit=0;uint8_t byte=0;int nested=0;
  for(size_t i=0;i<l->field_count;++i)if(t->fields[i].group==group){nested=t->fields[i].nested_group;if(!nested)break;if(field_optional(t,i)){if(asn1_schema_field_present(v,l->fields+i))byte|=(uint8_t)(128u>>(bit%8u));if(++bit%8u==0){TRY(write_byte(w,byte));byte=0;}}}
  if(nested&&bit%8u)TRY(write_byte(w,byte));
  for(size_t i=0;i<l->field_count;++i)if(t->fields[i].group==group){if(!nested||!field_optional(t,i)||asn1_schema_field_present(v,l->fields+i))TRY(encode_field(w,p,t,i,v,depth));}
  return ASN1_OK;
}
static asn1_status_t encode_record(asn1_byte_writer_t* w,asn1_oer_policy_t p,const asn1_codec_schema_t* t,const void* v,size_t depth){
  const asn1_tlv_schema_t* l=t->layout();int extensions=0;for(size_t g=0;g<t->group_count;++g)extensions|=group_present(t,g,v);
  size_t bit=l->extensible?1u:0u;uint8_t byte=extensions?128:0;
  for(size_t index=0;index<t->root_count;++index){size_t i=t->root_order[index];if(field_optional(t,i)){if(asn1_schema_field_present(v,l->fields+i))byte|=(uint8_t)(128u>>(bit%8u));if(++bit%8u==0){TRY(write_byte(w,byte));byte=0;}}}
  if(bit%8u)TRY(write_byte(w,byte));
  for(size_t index=0;index<t->root_count;++index){size_t i=t->root_order[index];if(asn1_schema_field_present(v,l->fields+i))TRY(encode_field(w,p,t,i,v,depth));}
  if(!extensions)return ASN1_OK;
  TRY(asn1_oer_encode_length(w,1u+(t->group_count+7u)/8u));TRY(write_byte(w,(uint8_t)((8u-t->group_count%8u)%8u)));byte=0;
  for(size_t g=0;g<t->group_count;++g){if(group_present(t,g,v))byte|=(uint8_t)(128u>>(g%8u));if(g%8u==7u||g+1u==t->group_count){TRY(write_byte(w,byte));byte=0;}}
  for(size_t g=0;g<t->group_count;++g)if(group_present(t,g,v)){
    asn1_byte_writer_t* child=NULL;TRY(asn1_byte_writer_create(&child));asn1_status_t s=encode_group(child,p,t,g,v,depth+1);if(s==ASN1_OK)s=asn1_oer_encode_open(w,asn1_byte_writer_view(child));asn1_byte_writer_destroy(child);if(s!=ASN1_OK)return s;
  }return ASN1_OK;
}
typedef struct {uint8_t* bytes;size_t size;} piece_t;
/* X.696 31.8 compares zero-padded octet strings, not ordinary prefix order. */
static int compare_pieces(const void* a,const void* b){const piece_t* x=(const piece_t*)a;const piece_t* y=(const piece_t*)b;size_t n=x->size>y->size?x->size:y->size;for(size_t i=0;i<n;++i){unsigned left=i<x->size?x->bytes[i]:0,right=i<y->size?y->bytes[i]:0;if(left!=right)return left>right?1:-1;}return 0;}
asn1_status_t asn1_schema_oer_encode(asn1_byte_writer_t* w,asn1_oer_policy_t p,const asn1_codec_schema_t* t,const void* v,size_t depth){
  if(depth>=ASN1_MAX_RECURSION_DEPTH)return ASN1_ERR_RESOURCE_LIMIT;const asn1_tlv_schema_t* l=t->layout();const uint8_t* base=(const uint8_t*)v;TRY(l->validate(v));
  switch(l->kind){
    case ASN1_TLV_ALIAS:{const void* child=base+l->value_offset;if(l->indirect)child=*(void*const*)child;return child?asn1_schema_oer_encode(w,p,t->element(),child,depth+1):ASN1_ERR_CONSTRAINT_VIOLATION;}
    case ASN1_TLV_BOOLEAN:return asn1_oer_encode_boolean(w,*(const int*)(base+l->value_offset));
    case ASN1_TLV_NULL:return ASN1_OK;
    case ASN1_TLV_INT64:{int64_t value=*(const int64_t*)(base+l->value_offset);if(t->enumerator_count)return asn1_oer_encode_enumerated(w,value);
      uint64_t magnitude=value<0?UINT64_C(0)-(uint64_t)value:(uint64_t)value;uint32_t words[]={(uint32_t)magnitude,(uint32_t)(magnitude>>32)};asn1_bigint_t b={value<0?-1:value?1:0,words,words[1]?2u:words[0]?1u:0u,2};return asn1_oer_encode_integer(w,t->integer_form,&b);}
    case ASN1_TLV_BIGINT:return asn1_oer_encode_integer(w,t->integer_form,(const asn1_bigint_t*)(base+l->value_offset));
    case ASN1_TLV_REAL:if(t->real_form)return ieee_encode(w,t->real_form,*(const double*)(base+l->value_offset));
      /* fall through */
    case ASN1_TLV_OID:case ASN1_TLV_RELATIVE_OID:{uint8_t* bytes=NULL;size_t n=0;asn1_status_t s=l->kind==ASN1_TLV_REAL?asn1_value_encode_real(*(const double*)(base+l->value_offset),&bytes,&n):asn1_value_encode_oid(*(uint64_t*const*)(base+l->data_offset),*(const size_t*)(base+l->count_offset),l->kind==ASN1_TLV_RELATIVE_OID,&bytes,&n);if(s==ASN1_OK)s=asn1_oer_encode_open(w,asn1_byte_view(bytes,n));free(bytes);return s;}
    case ASN1_TLV_OCTETS:case ASN1_TLV_OPEN:{asn1_byte_view_t bytes=asn1_byte_view(*(uint8_t*const*)(base+l->data_offset),*(const size_t*)(base+l->count_offset));return t->fixed_size!=SIZE_MAX&&l->kind!=ASN1_TLV_OPEN&&l->universal_tag!=12?asn1_byte_writer_append(w,bytes):asn1_oer_encode_open(w,bytes);}
    case ASN1_TLV_BITS:{const asn1_bitstring_t* bits=(const asn1_bitstring_t*)(base+l->value_offset);size_t n=bits->bit_length/8u+(bits->bit_length%8u!=0);if(n&&bits->bit_length%8u&&(bits->bits[n-1]&((1u<<(8u-bits->bit_length%8u))-1u)))return ASN1_ERR_CONSTRAINT_VIOLATION;
      if(t->fixed_size==SIZE_MAX){TRY(asn1_oer_encode_length(w,n+1u));TRY(write_byte(w,(uint8_t)((8u-bits->bit_length%8u)%8u)));}return asn1_byte_writer_append(w,asn1_byte_view(bits->bits,n));}
    case ASN1_TLV_SEQUENCE:case ASN1_TLV_SET:return encode_record(w,p,t,v,depth);
    case ASN1_TLV_CHOICE:{size_t i=l->choice_get(v);if(i>=l->field_count)return ASN1_ERR_CONSTRAINT_VIOLATION;asn1_ber_tag_t tag;TRY(choice_tag(t,v,&tag,depth));TRY(asn1_oer_encode_tag(w,tag));
      if(t->fields[i].group==SIZE_MAX)return encode_field(w,p,t,i,v,depth);asn1_byte_writer_t* child=NULL;TRY(asn1_byte_writer_create(&child));asn1_status_t s=encode_field(child,p,t,i,v,depth);if(s==ASN1_OK)s=asn1_oer_encode_open(w,asn1_byte_writer_view(child));asn1_byte_writer_destroy(child);return s;}
    case ASN1_TLV_SEQUENCE_OF:case ASN1_TLV_SET_OF:{size_t count=*(const size_t*)(base+l->count_offset);if(count>ASN1_MAX_PDU_ELEMENTS)return ASN1_ERR_RESOURCE_LIMIT;uint8_t quantity[sizeof(size_t)];size_t n=0,number=count;do{quantity[sizeof(quantity)-1-n++]=(uint8_t)number;number>>=8;}while(number);TRY(asn1_oer_encode_open(w,asn1_byte_view(quantity+sizeof(quantity)-n,n)));
      const uint8_t* data=*(uint8_t*const*)(base+l->data_offset);if(l->kind!=ASN1_TLV_SET_OF||p!=ASN1_OER_CANONICAL){for(size_t i=0;i<count;++i)TRY(asn1_schema_oer_encode(w,p,t->element(),data+i*l->element()->size,depth+1));return ASN1_OK;}
      piece_t* pieces=count?(piece_t*)calloc(count,sizeof(*pieces)):NULL;if(count&&!pieces)return ASN1_ERR_OUT_OF_MEMORY;asn1_status_t s=ASN1_OK;size_t used=0,staged_bytes=0;
      for(size_t i=0;s==ASN1_OK&&i<count;++i){asn1_byte_writer_t* child=NULL;s=asn1_byte_writer_create(&child);if(s==ASN1_OK)s=asn1_schema_oer_encode(child,p,t->element(),data+i*l->element()->size,depth+1);if(s==ASN1_OK){size_t size=asn1_byte_writer_view(child).size;if(size>ASN1_MAX_PDU_ALLOCATION_BYTES-staged_bytes)s=ASN1_ERR_RESOURCE_LIMIT;else{staged_bytes+=size;s=asn1_byte_writer_take(child,&pieces[used].bytes,&pieces[used].size);if(s==ASN1_OK)++used;}}asn1_byte_writer_destroy(child);}
      if(s==ASN1_OK&&count)qsort(pieces,count,sizeof(*pieces),compare_pieces);for(size_t i=0;i<used;++i){if(s==ASN1_OK)s=asn1_byte_writer_append(w,asn1_byte_view(pieces[i].bytes,pieces[i].size));free(pieces[i].bytes);}free(pieces);return s;}
    default:return ASN1_ERR_UNSUPPORTED;
  }
}
static asn1_status_t decode_field(asn1_byte_reader_t* r,asn1_oer_policy_t p,const asn1_codec_schema_t* t,size_t i,void* v,asn1_schema_budget_t* b,size_t depth){
  const asn1_tlv_field_t* f=t->layout()->fields+i;void* dst=NULL;TRY(asn1_schema_field_destination(v,f,b,&dst));TRY(asn1_schema_oer_decode(r,p,t->fields[i].type(),dst,b,depth+1));if(f->presence!=SIZE_MAX)*(int*)((uint8_t*)v+f->presence)=1;
  if(p==ASN1_OER_CANONICAL&&f->is_default&&f->is_default(v))return ASN1_ERR_NON_CANONICAL;return ASN1_OK;
}
static asn1_status_t absent(const asn1_tlv_field_t* f,void* v,asn1_schema_budget_t* b){if(f->default_init){TRY(asn1_schema_charge(b,f->default_bytes,0));return f->default_init(v);}return ASN1_OK;}
static asn1_status_t preamble(asn1_byte_reader_t* r,size_t bits,asn1_byte_view_t* out){TRY(asn1_byte_reader_read(r,bits/8u+(bits%8u!=0),out));if(bits%8u&&(out->data[out->size-1]&((1u<<(8u-bits%8u))-1u)))return ASN1_ERR_INVALID_ARGUMENT;return ASN1_OK;}
static int bitmap(asn1_byte_view_t bytes,size_t bit){return (bytes.data[bit/8u]&(128u>>(bit%8u)))!=0;}
static asn1_status_t decode_group(asn1_byte_reader_t* r,asn1_oer_policy_t p,const asn1_codec_schema_t* t,size_t group,void* v,asn1_schema_budget_t* b,size_t depth){
  size_t bits=0;int nested=0;const asn1_tlv_schema_t* l=t->layout();for(size_t i=0;i<l->field_count;++i)if(t->fields[i].group==group){nested=t->fields[i].nested_group;if(field_optional(t,i))++bits;}
  asn1_byte_view_t presence=asn1_byte_view(NULL,0);if(nested)TRY(preamble(r,bits,&presence));size_t bit=0;
  for(size_t i=0;i<l->field_count;++i)if(t->fields[i].group==group){int present=1;if(nested&&field_optional(t,i))present=bitmap(presence,bit++);if(present)TRY(decode_field(r,p,t,i,v,b,depth));else TRY(absent(l->fields+i,v,b));}return ASN1_OK;
}
static asn1_status_t decode_record(asn1_byte_reader_t* r,asn1_oer_policy_t p,const asn1_codec_schema_t* t,void* v,asn1_schema_budget_t* b,size_t depth){
  const asn1_tlv_schema_t* l=t->layout();size_t bits=l->extensible?1:0;for(size_t j=0;j<t->root_count;++j)bits+=field_optional(t,t->root_order[j]);asn1_byte_view_t presence;TRY(preamble(r,bits,&presence));size_t bit=l->extensible?1:0;
  for(size_t j=0;j<t->root_count;++j){size_t i=t->root_order[j];int present=1;if(field_optional(t,i))present=bitmap(presence,bit++);if(present)TRY(decode_field(r,p,t,i,v,b,depth));else TRY(absent(l->fields+i,v,b));}
  if(!l->extensible||!bitmap(presence,0))return ASN1_OK;
  asn1_byte_view_t additions;TRY(asn1_oer_decode_open(r,p,&additions));if(!additions.size||additions.data[0]>7)return ASN1_ERR_INVALID_ARGUMENT;unsigned unused=additions.data[0];size_t bytes=additions.size-1;
  if((!bytes&&unused)||(bytes&&unused&&(additions.data[additions.size-1]&((1u<<unused)-1u))))return ASN1_ERR_INVALID_ARGUMENT;
  if(bytes>ASN1_MAX_PDU_ELEMENTS/8u+1u)return ASN1_ERR_RESOURCE_LIMIT;size_t count=bytes*8u-unused;TRY(asn1_schema_charge(b,0,count));asn1_byte_view_t mask=asn1_byte_view(additions.data+1,bytes);
  for(size_t g=0;g<count;++g)if(bitmap(mask,g)){asn1_byte_view_t wire;TRY(asn1_oer_decode_open(r,p,&wire));if(g<t->group_count){TRY(asn1_schema_charge(b,wire.size,0));asn1_byte_reader_t* child=NULL;TRY(asn1_byte_reader_create(&child,wire));asn1_status_t s=decode_group(child,p,t,g,v,b,depth+1);if(s==ASN1_OK&&asn1_byte_reader_remaining(child))s=ASN1_ERR_INVALID_ARGUMENT;asn1_byte_reader_destroy(child);if(s!=ASN1_OK)return s;}}
  return ASN1_OK;
}
asn1_status_t asn1_schema_oer_decode(asn1_byte_reader_t* r,asn1_oer_policy_t p,const asn1_codec_schema_t* t,void* v,asn1_schema_budget_t* b,size_t depth){
  if(depth>=ASN1_MAX_RECURSION_DEPTH)return ASN1_ERR_RESOURCE_LIMIT;const asn1_tlv_schema_t* l=t->layout();uint8_t* base=(uint8_t*)v;
  switch(l->kind){
    case ASN1_TLV_ALIAS:{void* child=base+l->value_offset;if(l->indirect){TRY(asn1_schema_allocate(b,l->element()->size,1,&child));l->element()->init(child);*(void**)(base+l->value_offset)=child;}TRY(asn1_schema_oer_decode(r,p,t->element(),child,b,depth+1));break;}
    case ASN1_TLV_BOOLEAN:TRY(asn1_oer_decode_boolean(r,p,(int*)(base+l->value_offset)));break;
    case ASN1_TLV_NULL:break;
    case ASN1_TLV_INT64:case ASN1_TLV_BIGINT:{if(t->enumerator_count){TRY(asn1_oer_decode_enumerated(r,p,(int64_t*)(base+l->value_offset)));break;}
      /* Budget the geometric limb growth conservatively from wire size. */
      size_t input=t->integer_form.width;
      if(!input){asn1_byte_reader_t* probe=NULL;TRY(asn1_byte_reader_create(&probe,asn1_byte_reader_view(r)));asn1_status_t status=asn1_oer_decode_length(probe,p,&input);asn1_byte_reader_destroy(probe);if(status!=ASN1_OK)return status;}
      if(input>ASN1_MAX_BIGINT_LIMBS*4u+1u)return ASN1_ERR_RESOURCE_LIMIT;
      size_t cap=4,total=0;while(1){total+=cap*4u;if(cap>=input/4u+1u||cap>=ASN1_MAX_BIGINT_LIMBS)break;cap*=2;}
      TRY(asn1_schema_charge(b,total,0));asn1_bigint_t number;asn1_bigint_init(&number);TRY(asn1_oer_decode_integer(r,p,t->integer_form,&number));if(l->kind==ASN1_TLV_BIGINT)*(asn1_bigint_t*)(base+l->value_offset)=number;else{asn1_status_t s=asn1_bigint_to_int64(&number,(int64_t*)(base+l->value_offset));asn1_bigint_free(&number);if(s!=ASN1_OK)return s;}break;}
    case ASN1_TLV_REAL:if(t->real_form){TRY(ieee_decode(r,t->real_form,(double*)(base+l->value_offset)));break;}
      /* fall through */
    case ASN1_TLV_OID:case ASN1_TLV_RELATIVE_OID:{asn1_byte_view_t bytes;TRY(asn1_oer_decode_open(r,p,&bytes));if(l->kind==ASN1_TLV_REAL){
        /* X.696 12.4 requires DER contents even in BASIC-OER. Decimal REAL
           needs an abstract base-10 model rather than a host-double recast. */
        if(bytes.size&&bytes.data[0]>=1&&bytes.data[0]<=3)return ASN1_ERR_UNSUPPORTED;
        double value;TRY(asn1_value_decode_real(bytes,&value));TRY(asn1_schema_charge(b,11,0));uint8_t* canonical=NULL;size_t size=0;TRY(asn1_value_encode_real(value,&canonical,&size));int equal=size==bytes.size&&(!size||!memcmp(canonical,bytes.data,size));free(canonical);if(!equal)return ASN1_ERR_NON_CANONICAL;*(double*)(base+l->value_offset)=value;}
      else{if(bytes.size>ASN1_MAX_STRING_BYTES)return ASN1_ERR_RESOURCE_LIMIT;size_t capacity=bytes.size+(l->kind==ASN1_TLV_OID?1u:0u);TRY(asn1_schema_charge(b,capacity*sizeof(uint64_t),capacity));uint64_t* arcs=NULL;size_t count;TRY(asn1_value_decode_oid(bytes,&arcs,&count,l->kind==ASN1_TLV_RELATIVE_OID));*(uint64_t**)(base+l->data_offset)=arcs;*(size_t*)(base+l->count_offset)=count;*(size_t*)(base+l->capacity_offset)=capacity;}break;}
    case ASN1_TLV_OCTETS:case ASN1_TLV_OPEN:case ASN1_TLV_BITS:{asn1_byte_view_t bytes;size_t bit_length=0;
      if(l->kind==ASN1_TLV_BITS){if(t->fixed_size!=SIZE_MAX){bit_length=t->fixed_size;TRY(asn1_byte_reader_read(r,bit_length/8u+(bit_length%8u!=0),&bytes));}
        else{asn1_byte_view_t content;TRY(asn1_oer_decode_open(r,p,&content));if(!content.size||content.data[0]>7||(content.size==1&&content.data[0]))return ASN1_ERR_INVALID_ARGUMENT;bytes=asn1_byte_view(content.data+1,content.size-1);bit_length=bytes.size*8u-content.data[0];}
        if(bytes.size&&bit_length%8u&&(bytes.data[bytes.size-1]&((1u<<(8u-bit_length%8u))-1u)))return ASN1_ERR_INVALID_ARGUMENT;
      }else if(t->fixed_size!=SIZE_MAX&&l->kind!=ASN1_TLV_OPEN&&l->universal_tag!=12)TRY(asn1_byte_reader_read(r,t->fixed_size,&bytes));else TRY(asn1_oer_decode_open(r,p,&bytes));
      if(bytes.size>ASN1_MAX_STRING_BYTES)return ASN1_ERR_RESOURCE_LIMIT;uint8_t* data=NULL;TRY(asn1_schema_allocate(b,bytes.size,1,(void**)&data));if(bytes.size)memcpy(data,bytes.data,bytes.size);
      if(l->kind==ASN1_TLV_BITS){asn1_bitstring_t* bits=(asn1_bitstring_t*)(base+l->value_offset);bits->bits=data;bits->bit_length=bit_length;bits->cap_bytes=bytes.size;}else{*(void**)(base+l->data_offset)=data;*(size_t*)(base+l->count_offset)=*(size_t*)(base+l->capacity_offset)=bytes.size;}break;}
    case ASN1_TLV_SEQUENCE:case ASN1_TLV_SET:TRY(decode_record(r,p,t,v,b,depth));break;
    case ASN1_TLV_CHOICE:{asn1_ber_tag_t tag;TRY(asn1_oer_decode_tag(r,&tag));size_t selected=l->field_count;for(size_t i=0;i<l->field_count;++i)if(t->fields[i].tag.cls!=255?tag_equal(t->fields[i].tag,tag):choice_matches(t->fields[i].type(),tag,depth+1)){selected=i;break;}
      if(selected==l->field_count)return ASN1_ERR_UNSUPPORTED;l->choice_set(v,selected);
      if(t->fields[selected].group==SIZE_MAX)TRY(decode_field(r,p,t,selected,v,b,depth));else{asn1_byte_view_t wire;TRY(asn1_oer_decode_open(r,p,&wire));TRY(asn1_schema_charge(b,wire.size,0));asn1_byte_reader_t* child=NULL;TRY(asn1_byte_reader_create(&child,wire));asn1_status_t s=decode_field(child,p,t,selected,v,b,depth);if(s==ASN1_OK&&asn1_byte_reader_remaining(child))s=ASN1_ERR_INVALID_ARGUMENT;asn1_byte_reader_destroy(child);if(s!=ASN1_OK)return s;}break;}
    case ASN1_TLV_SEQUENCE_OF:case ASN1_TLV_SET_OF:{asn1_byte_view_t quantity;TRY(asn1_oer_decode_open(r,p,&quantity));if(!quantity.size)return ASN1_ERR_INVALID_ARGUMENT;if(p==ASN1_OER_CANONICAL&&quantity.size>1&&quantity.data[0]==0)return ASN1_ERR_NON_CANONICAL;size_t count=0;
      for(size_t i=0;i<quantity.size;++i){if(count>(SIZE_MAX-quantity.data[i])/256u)return ASN1_ERR_LENGTH_OVERFLOW;count=count*256u+quantity.data[i];}
      uint8_t* data=NULL;TRY(asn1_schema_allocate(b,l->element()->size,count,(void**)&data));*(void**)(base+l->data_offset)=data;*(size_t*)(base+l->capacity_offset)=count;asn1_byte_view_t previous=asn1_byte_view(NULL,0);int have_previous=0;
      for(size_t i=0;i<count;++i){void* element=data+i*l->element()->size;l->element()->init(element);*(size_t*)(base+l->count_offset)=i+1;asn1_byte_view_t start=asn1_byte_reader_view(r);size_t before=asn1_byte_reader_position(r);TRY(asn1_schema_oer_decode(r,p,t->element(),element,b,depth+1));asn1_byte_view_t current=asn1_byte_view(start.data,asn1_byte_reader_position(r)-before);
        if(p==ASN1_OER_CANONICAL&&l->kind==ASN1_TLV_SET_OF&&have_previous){piece_t a={(uint8_t*)previous.data,previous.size},c={(uint8_t*)current.data,current.size};if(compare_pieces(&a,&c)>0)return ASN1_ERR_NON_CANONICAL;}previous=current;have_previous=1;}break;}
    default:return ASN1_ERR_UNSUPPORTED;
  }return l->validate(v);
}
