#include "schema_internal.h"
#include <asn1/c_runtime/bigint.h>
#include <asn1/c_runtime/bit_string.h>
#include <asn1/c_runtime/value_content.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <locale.h>
#include <inttypes.h>
#include "alloc_internal.h"
#define TRY(x) do{asn1_status_t s_=(x);if(s_!=ASN1_OK)return s_;}while(0)
static asn1_status_t put(asn1_byte_writer_t* w,const char* s){return asn1_byte_writer_append(w,asn1_byte_view((const uint8_t*)s,strlen(s)));}
static asn1_status_t quote(asn1_byte_writer_t* w,const char* s){return asn1_json_quote(w,asn1_byte_view((const uint8_t*)s,strlen(s)));}
static const char alphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
asn1_status_t asn1_schema_binary_encode(asn1_byte_writer_t* w,asn1_byte_view_t bytes,int base64){
  if(!base64){static const char hex[]="0123456789ABCDEF";for(size_t i=0;i<bytes.size;++i){uint8_t b[2]={(uint8_t)hex[bytes.data[i]>>4],(uint8_t)hex[bytes.data[i]&15]};TRY(asn1_byte_writer_append(w,asn1_byte_view(b,2)));}return ASN1_OK;}
  for(size_t i=0;i<bytes.size;i+=3){size_t left=bytes.size-i;uint32_t v=(uint32_t)bytes.data[i]<<16;if(left>1)v|=(uint32_t)bytes.data[i+1]<<8;if(left>2)v|=bytes.data[i+2];
    uint8_t b[]={(uint8_t)alphabet[v>>18],(uint8_t)alphabet[(v>>12)&63],(uint8_t)(left>1?alphabet[(v>>6)&63]:'='),(uint8_t)(left>2?alphabet[v&63]:'=')};TRY(asn1_byte_writer_append(w,asn1_byte_view(b,4)));}return ASN1_OK;
}
static int hex(unsigned c){return c>='0'&&c<='9'?(int)c-'0':c>='a'&&c<='f'?(int)c-'a'+10:c>='A'&&c<='F'?(int)c-'A'+10:-1;}
static int b64(unsigned c){if(!c)return -1;const char* p=c<128?strchr(alphabet,(int)c):NULL;return p?(int)(p-alphabet):-1;}
asn1_status_t asn1_schema_binary_decode(asn1_byte_view_t text,int base64,uint8_t** out,size_t* size,asn1_schema_budget_t* budget){
  if(text.size%(base64?4u:2u))return ASN1_ERR_INVALID_ARGUMENT;
  size_t n=base64?text.size/4u*3u:text.size/2u;
  if(base64&&text.size){if(text.data[text.size-1]=='=')--n;if(text.data[text.size-2]=='=')--n;}
  if(n>ASN1_MAX_STRING_BYTES)return ASN1_ERR_RESOURCE_LIMIT;
  uint8_t* bytes=NULL;TRY(asn1_schema_allocate(budget,n,1,(void**)&bytes));asn1_status_t s=ASN1_OK;size_t used=0;
  if(!base64){for(size_t i=0;i<text.size;i+=2){int a=hex(text.data[i]),b=hex(text.data[i+1]);if(a<0||b<0){s=ASN1_ERR_INVALID_ARGUMENT;break;}bytes[used++]=(uint8_t)((a<<4)|b);}}
  else for(size_t i=0;i<text.size;i+=4){int a=b64(text.data[i]),b=b64(text.data[i+1]),c=text.data[i+2]=='='?0:b64(text.data[i+2]),d=text.data[i+3]=='='?0:b64(text.data[i+3]);
    int pad2=text.data[i+2]=='=',pad1=text.data[i+3]=='=';
    if(a<0||b<0||c<0||d<0||(pad2&&!pad1)||((pad1||pad2)&&i+4!=text.size)||(pad2&&(b&15))||(!pad2&&pad1&&(c&3))){s=ASN1_ERR_INVALID_ARGUMENT;break;}
    uint32_t v=((unsigned)a<<18)|((unsigned)b<<12)|((unsigned)c<<6)|(unsigned)d;if(used<n)bytes[used++]=(uint8_t)(v>>16);if(used<n)bytes[used++]=(uint8_t)(v>>8);if(used<n)bytes[used++]=(uint8_t)v;
  }
  if(s!=ASN1_OK){free(bytes);return s;}*out=bytes;*size=n;return ASN1_OK;
}
static asn1_status_t number(asn1_byte_writer_t* w,int64_t value){char s[32];int n=snprintf(s,sizeof(s),"%" PRId64,value);return n>0&&(size_t)n<sizeof(s)?put(w,s):ASN1_ERR_INVALID_ARGUMENT;}
static asn1_status_t real_number(asn1_byte_writer_t* w,double value){
  if(isnan(value))return quote(w,"NaN");if(isinf(value))return quote(w,value<0?"-INF":"INF");
  char s[128];int n=snprintf(s,sizeof(s),"%.17g",value);if(n<1||(size_t)n>=sizeof(s))return ASN1_ERR_INVALID_ARGUMENT;
  /* Do not change the process locale. Normalize only printf's radix token. */
  const char* radix=localeconv()->decimal_point;if(radix&&*radix&&strcmp(radix,".")){char* at=strstr(s,radix);if(at){size_t bytes=strlen(radix);*at='.';memmove(at+1,at+bytes,strlen(at+bytes)+1);}}
  return put(w,s);
}
static asn1_status_t json_at(asn1_byte_writer_t*,const asn1_codec_schema_t*,const void*,size_t,unsigned);
static asn1_status_t record_value(const asn1_codec_schema_t** schema,const void** value){
  for(size_t depth=0;depth<ASN1_MAX_RECURSION_DEPTH;++depth){const asn1_tlv_schema_t* l=(*schema)->layout();if(l->kind!=ASN1_TLV_ALIAS)return l->kind==ASN1_TLV_SEQUENCE||l->kind==ASN1_TLV_SET?ASN1_OK:ASN1_ERR_UNSUPPORTED;
    const void* child=(const uint8_t*)*value+l->value_offset;if(l->indirect)child=*(void*const*)child;if(!child)return ASN1_ERR_CONSTRAINT_VIOLATION;*value=child;*schema=(*schema)->element();
  }return ASN1_ERR_RESOURCE_LIMIT;
}
static const asn1_codec_schema_t* record_schema(const asn1_codec_schema_t* schema,unsigned* flags){*flags=0;for(size_t depth=0;depth<ASN1_MAX_RECURSION_DEPTH;++depth){*flags|=schema->flags;if(schema->layout()->kind!=ASN1_TLV_ALIAS)return schema;schema=schema->element();}return NULL;}
static int nullable(const asn1_codec_schema_t* t,size_t depth,unsigned inherited){
  if(depth>=ASN1_MAX_RECURSION_DEPTH)return 1;
  unsigned flags=inherited|t->flags;
  if(t->layout()->kind==ASN1_TLV_ALIAS)return nullable(t->element(),depth+1,flags);
  if(t->layout()->kind==ASN1_TLV_NULL||t->layout()->kind==ASN1_TLV_OPEN)return 1;
  if(t->layout()->kind==ASN1_TLV_CHOICE&&(flags&ASN1_SCHEMA_UNWRAPPED)){
    if(t->layout()->extensible)return 1;
    for(size_t i=0;i<t->layout()->field_count;++i)if(nullable(t->fields[i].type(),depth+1,0))return 1;
  }return 0;
}
/* X.697 ARRAY places trailing root components before extension additions. */
static size_t array_field(const asn1_codec_schema_t* t,size_t ordinal){
  if(ordinal<t->root_count)return t->root_order[ordinal];
  ordinal-=t->root_count;
  for(size_t i=0;i<t->layout()->field_count;++i)if(t->fields[i].group!=SIZE_MAX){if(!ordinal)return i;--ordinal;}
  return t->layout()->field_count;
}
typedef struct {const uint8_t* data;size_t size;} key_t;
static int key_order(const void* a,const void* b){const key_t* x=(const key_t*)a;const key_t* y=(const key_t*)b;size_t n=x->size<y->size?x->size:y->size;int c=n?memcmp(x->data,y->data,n):0;return c?c:(x->size>y->size)-(x->size<y->size);}
static asn1_status_t object_key(const asn1_codec_schema_t* t,const void* value,key_t* key){
  const asn1_codec_enum_t* names=t->enumerators;size_t names_count=t->enumerator_count;
  for(size_t depth=0;depth<ASN1_MAX_RECURSION_DEPTH;++depth){const asn1_tlv_schema_t* l=t->layout();if(!names_count&&t->enumerator_count){names=t->enumerators;names_count=t->enumerator_count;}if(l->kind==ASN1_TLV_ALIAS){const void* child=(const uint8_t*)value+l->value_offset;if(l->indirect)child=*(void*const*)child;if(!child)return ASN1_ERR_CONSTRAINT_VIOLATION;value=child;t=t->element();continue;}
    TRY(l->validate(value));
    if(l->kind==ASN1_TLV_INT64&&t->enumerator_count){if(!names_count){names=t->enumerators;names_count=t->enumerator_count;}int64_t n=*(const int64_t*)((const uint8_t*)value+l->value_offset);for(size_t i=0;i<names_count;++i)if(names[i].value==n){key->data=(const uint8_t*)names[i].name;key->size=strlen(names[i].name);return ASN1_OK;}return ASN1_ERR_CONSTRAINT_VIOLATION;}
    if(l->kind!=ASN1_TLV_OCTETS||l->universal_tag==4)return ASN1_ERR_UNSUPPORTED;key->data=*(uint8_t*const*)((const uint8_t*)value+l->data_offset);key->size=*(const size_t*)((const uint8_t*)value+l->count_offset);return ASN1_OK;
  }return ASN1_ERR_RESOURCE_LIMIT;
}
static asn1_status_t object_encode(asn1_byte_writer_t* w,const asn1_codec_schema_t* t,const void* value,size_t depth){
  const asn1_tlv_schema_t* l=t->layout();const uint8_t* base=(const uint8_t*)value;size_t count=*(const size_t*)(base+l->count_offset);if(count>ASN1_MAX_PDU_ELEMENTS)return ASN1_ERR_RESOURCE_LIMIT;
  const uint8_t* data=*(uint8_t*const*)(base+l->data_offset);key_t* keys=count?(key_t*)calloc(count,sizeof(*keys)):NULL;if(count&&!keys)return ASN1_ERR_OUT_OF_MEMORY;asn1_status_t s=ASN1_OK;
  for(size_t i=0;s==ASN1_OK&&i<count;++i){const asn1_codec_schema_t* pair=t->element();const void* item=data+i*l->element()->size;s=record_value(&pair,&item);if(s!=ASN1_OK)break;const asn1_tlv_schema_t* record=pair->layout();if(record->field_count!=2){s=ASN1_ERR_UNSUPPORTED;break;}s=record->validate(item);const void* key=asn1_schema_field_value(item,record->fields);if(!key){s=ASN1_ERR_CONSTRAINT_VIOLATION;break;}if(s==ASN1_OK)s=object_key(pair->fields[0].type(),key,keys+i);}
  if(s==ASN1_OK&&count){qsort(keys,count,sizeof(*keys),key_order);for(size_t i=1;i<count;++i)if(!key_order(keys+i-1,keys+i)){s=ASN1_ERR_INVALID_ARGUMENT;break;}}free(keys);if(s!=ASN1_OK)return s;
  TRY(put(w,"{"));for(size_t i=0;i<count;++i){const asn1_codec_schema_t* pair=t->element();const void* item=data+i*l->element()->size;TRY(record_value(&pair,&item));const asn1_tlv_schema_t* record=pair->layout();const void* key=asn1_schema_field_value(item,record->fields);const void* child=asn1_schema_field_value(item,record->fields+1);if(!key||!child)return ASN1_ERR_CONSTRAINT_VIOLATION;
    if(i)TRY(put(w,","));TRY(json_at(w,pair->fields[0].type(),key,depth+1,0));TRY(put(w,":"));TRY(json_at(w,pair->fields[1].type(),child,depth+1,0));}return put(w,"}");
}
static asn1_status_t json_binary(asn1_byte_writer_t* w,asn1_byte_view_t bytes,int base64){TRY(put(w,"\""));TRY(asn1_schema_binary_encode(w,bytes,base64));return put(w,"\"");}
static asn1_status_t json_at(asn1_byte_writer_t* w,const asn1_codec_schema_t* t,const void* value,size_t depth,unsigned inherited){
  if(depth>=ASN1_MAX_RECURSION_DEPTH)return ASN1_ERR_RESOURCE_LIMIT;
  const asn1_tlv_schema_t* l=t->layout();TRY(l->validate(value));const uint8_t* base=(const uint8_t*)value;unsigned flags=t->flags|inherited;
  if(l->kind==ASN1_TLV_OPEN)return ASN1_ERR_UNSUPPORTED;
  if(l->kind==ASN1_TLV_ALIAS){const void* child=base+l->value_offset;if(l->indirect)child=*(void*const*)child;if(!child)return ASN1_ERR_CONSTRAINT_VIOLATION;asn1_codec_schema_t target=*t->element();if(t->enumerator_count){target.enumerators=t->enumerators;target.enumerator_count=t->enumerator_count;}return json_at(w,&target,child,depth+1,flags);}
  switch(l->kind){
    case ASN1_TLV_BOOLEAN:return put(w,*(const int*)(base+l->value_offset)?"true":"false");
    case ASN1_TLV_NULL:return put(w,"null");
    case ASN1_TLV_INT64:{int64_t n=*(const int64_t*)(base+l->value_offset);if(t->enumerator_count){for(size_t i=0;i<t->enumerator_count;++i)if(t->enumerators[i].value==n)return quote(w,t->enumerators[i].name);return ASN1_ERR_CONSTRAINT_VIOLATION;}return number(w,n);}
    case ASN1_TLV_BIGINT:{char* text=NULL;size_t n;TRY(asn1_bigint_to_decimal((const asn1_bigint_t*)(base+l->value_offset),&text,&n));asn1_status_t s=asn1_byte_writer_append(w,asn1_byte_view((const uint8_t*)text,n));free(text);return s;}
    case ASN1_TLV_REAL:return real_number(w,*(const double*)(base+l->value_offset));
    case ASN1_TLV_OID:case ASN1_TLV_RELATIVE_OID:{const uint64_t* data=*(uint64_t*const*)(base+l->data_offset);size_t n=*(const size_t*)(base+l->count_offset);TRY(put(w,"\""));for(size_t i=0;i<n;++i){if(i)TRY(put(w,"."));char s[32];int count=snprintf(s,sizeof(s),"%" PRIu64,data[i]);if(count<=0||(size_t)count>=sizeof(s))return ASN1_ERR_INVALID_ARGUMENT;TRY(put(w,s));}return put(w,"\"");}
    case ASN1_TLV_OCTETS:case ASN1_TLV_OPEN:{asn1_byte_view_t bytes=asn1_byte_view(*(uint8_t*const*)(base+l->data_offset),*(const size_t*)(base+l->count_offset));if(l->universal_tag!=4&&l->kind!=ASN1_TLV_OPEN)return asn1_json_quote(w,bytes);return json_binary(w,bytes,(flags&ASN1_SCHEMA_BASE64)!=0);}
    case ASN1_TLV_BITS:{const asn1_bitstring_t* b=(const asn1_bitstring_t*)(base+l->value_offset);asn1_byte_view_t bytes=asn1_byte_view(b->bits,b->bit_length/8+(b->bit_length%8!=0));
      if(t->fixed_size!=SIZE_MAX)return json_binary(w,bytes,0);TRY(put(w,"{\"value\":"));TRY(json_binary(w,bytes,0));TRY(put(w,",\"length\":"));TRY(number(w,(int64_t)b->bit_length));return put(w,"}");}
    case ASN1_TLV_CHOICE:{size_t selected=l->choice_get(value);if(selected>=l->field_count)return ASN1_ERR_CONSTRAINT_VIOLATION;const void* child=asn1_schema_field_value(value,l->fields+selected);if(!child)return ASN1_ERR_CONSTRAINT_VIOLATION;
      size_t start=asn1_byte_writer_view(w).size;
      if(!(flags&ASN1_SCHEMA_UNWRAPPED)){TRY(put(w,"{"));TRY(quote(w,t->fields[selected].instruction_name));TRY(put(w,":"));}
      TRY(json_at(w,t->fields[selected].type(),child,depth+1,0));
      if(flags&ASN1_SCHEMA_UNWRAPPED){asn1_byte_view_t wire=asn1_byte_writer_view(w);asn1_text_node_t* tree=NULL;size_t error=0;TRY(asn1_json_parse(asn1_byte_view(wire.data+start,wire.size-start),&tree,&error));
        asn1_schema_budget_t budget={0,0};void* check=NULL;asn1_status_t status=asn1_schema_allocate(&budget,l->size,1,&check);if(status==ASN1_OK){l->init(check);asn1_codec_schema_t copy=*t;copy.flags=flags;status=asn1_schema_json_decode(tree,&copy,check,&budget,depth+1);if(status==ASN1_OK&&l->choice_get(check)!=selected)status=ASN1_ERR_INVALID_ARGUMENT;l->clear(check);}free(check);asn1_text_tree_free(tree);return status;}
      return put(w,"}");}
    case ASN1_TLV_SEQUENCE:case ASN1_TLV_SET:{int array=(flags&ASN1_SCHEMA_ARRAY)!=0;TRY(put(w,array?"[":"{"));int first=1;
      for(size_t ordinal=0;ordinal<l->field_count;++ordinal){size_t i=array?array_field(t,ordinal):ordinal;const asn1_tlv_field_t* f=l->fields+i;if(!asn1_schema_field_present(value,f)){if(array){if(!first)TRY(put(w,","));TRY(put(w,"null"));first=0;}continue;}
        const void* child=asn1_schema_field_value(value,f);if(!child)return ASN1_ERR_CONSTRAINT_VIOLATION;if(!first)TRY(put(w,","));first=0;if(!array){TRY(quote(w,t->fields[i].instruction_name));TRY(put(w,":"));}TRY(json_at(w,t->fields[i].type(),child,depth+1,0));}
      return put(w,array?"]":"}");}
    case ASN1_TLV_SEQUENCE_OF:case ASN1_TLV_SET_OF:{if(flags&ASN1_SCHEMA_OBJECT)return object_encode(w,t,value,depth);size_t count=*(const size_t*)(base+l->count_offset);if(count>ASN1_MAX_PDU_ELEMENTS)return ASN1_ERR_RESOURCE_LIMIT;
      const uint8_t* data=*(uint8_t*const*)(base+l->data_offset);TRY(put(w,"["));for(size_t i=0;i<count;++i){if(i)TRY(put(w,","));TRY(json_at(w,t->element(),data+i*l->element()->size,depth+1,0));}return put(w,"]");}
    default:return ASN1_ERR_UNSUPPORTED;
  }
}
asn1_status_t asn1_schema_json_encode(asn1_byte_writer_t* w,const asn1_codec_schema_t* t,const void* v,size_t d){return json_at(w,t,v,d,0);}
static int equals(const char* s,size_t n,const char* expected){return n==strlen(expected)&&(!n||!memcmp(s,expected,n));}
static const asn1_text_node_t* member(const asn1_text_node_t* n,const char* key){for(n=n->children;n;n=n->next)if(equals(n->name,n->name_size,key))return n;return NULL;}
static asn1_status_t integer(const asn1_text_node_t* n,asn1_bigint_t* value,asn1_schema_budget_t* b){
  if(n->kind!=ASN1_TEXT_NUMBER||!n->text_size)return ASN1_ERR_INVALID_ARGUMENT;
  for(size_t i=n->text[0]=='-'?1u:0u;i<n->text_size;++i)if(n->text[i]<'0'||n->text[i]>'9')return ASN1_ERR_INVALID_ARGUMENT;
  if(n->text_size>ASN1_MAX_BIGINT_LIMBS*10u)return ASN1_ERR_RESOURCE_LIMIT;
  /* Conservative bound covers every geometric limb allocation. */
  size_t limbs=n->text_size/9u+1u,capacity=4,bytes=0;while(1){bytes+=capacity*4u;if(capacity>=limbs||capacity>=ASN1_MAX_BIGINT_LIMBS)break;capacity*=2u;}
  TRY(asn1_schema_charge(b,bytes,0));return asn1_bigint_init_from_decimal(value,n->text,0);
}
static asn1_status_t json_decode_at(const asn1_text_node_t*,const asn1_codec_schema_t*,void*,asn1_schema_budget_t*,size_t,unsigned);
static asn1_status_t json_decode_body(const asn1_text_node_t* n,const asn1_codec_schema_t* t,void* value,asn1_schema_budget_t* budget,size_t depth,unsigned inherited){
  const asn1_tlv_schema_t* l=t->layout();uint8_t* base=(uint8_t*)value;unsigned flags=t->flags|inherited;
  if(l->kind==ASN1_TLV_OPEN)return ASN1_ERR_UNSUPPORTED;
  if(l->kind==ASN1_TLV_ALIAS){void* child=base+l->value_offset;if(l->indirect){TRY(asn1_schema_allocate(budget,l->element()->size,1,&child));*(void**)(base+l->value_offset)=child;l->element()->init(child);}asn1_codec_schema_t target=*t->element();if(t->enumerator_count){target.enumerators=t->enumerators;target.enumerator_count=t->enumerator_count;}return json_decode_at(n,&target,child,budget,depth+1,flags);}
  switch(l->kind){
    case ASN1_TLV_BOOLEAN:if(n->kind!=ASN1_TEXT_BOOL)return ASN1_ERR_INVALID_ARGUMENT;*(int*)(base+l->value_offset)=equals(n->text,n->text_size,"true");return ASN1_OK;
    case ASN1_TLV_NULL:return n->kind==ASN1_TEXT_NULL?ASN1_OK:ASN1_ERR_INVALID_ARGUMENT;
    case ASN1_TLV_INT64:case ASN1_TLV_BIGINT:{if(t->enumerator_count){if(n->kind!=ASN1_TEXT_STRING)return ASN1_ERR_INVALID_ARGUMENT;for(size_t i=0;i<t->enumerator_count;++i)if(equals(n->text,n->text_size,t->enumerators[i].name)){*(int64_t*)(base+l->value_offset)=t->enumerators[i].value;return ASN1_OK;}return ASN1_ERR_CONSTRAINT_VIOLATION;}
      asn1_bigint_t number;asn1_bigint_init(&number);TRY(integer(n,&number,budget));if(l->kind==ASN1_TLV_BIGINT){*(asn1_bigint_t*)(base+l->value_offset)=number;return ASN1_OK;}asn1_status_t s=asn1_bigint_to_int64(&number,(int64_t*)(base+l->value_offset));asn1_bigint_free(&number);return s;}
    case ASN1_TLV_REAL:{double real;if(n->kind==ASN1_TEXT_STRING){if(equals(n->text,n->text_size,"NaN"))real=NAN;else if(equals(n->text,n->text_size,"INF"))real=INFINITY;else if(equals(n->text,n->text_size,"-INF"))real=-INFINITY;else return ASN1_ERR_INVALID_ARGUMENT;}
      else if(n->kind==ASN1_TEXT_NUMBER){uint8_t* text=NULL;TRY(asn1_schema_allocate(budget,n->text_size+3u,1,(void**)&text));text[0]=strchr(n->text,'e')||strchr(n->text,'E')?3:strchr(n->text,'.')?2:1;memcpy(text+1,n->text,n->text_size);asn1_status_t s=asn1_value_decode_real(asn1_byte_view(text,n->text_size+1),&real);free(text);if(s!=ASN1_OK)return s;}else return ASN1_ERR_INVALID_ARGUMENT;*(double*)(base+l->value_offset)=real;return ASN1_OK;}
    case ASN1_TLV_OCTETS:case ASN1_TLV_OPEN:{if(n->kind!=ASN1_TEXT_STRING)return ASN1_ERR_INVALID_ARGUMENT;uint8_t* data=NULL;size_t size=n->text_size;
      if(l->universal_tag==4||l->kind==ASN1_TLV_OPEN)TRY(asn1_schema_binary_decode(asn1_byte_view((const uint8_t*)n->text,n->text_size),(flags&ASN1_SCHEMA_BASE64)!=0,&data,&size,budget));
      else{TRY(asn1_schema_allocate(budget,size,1,(void**)&data));if(size)memcpy(data,n->text,size);}
      *(uint8_t**)(base+l->data_offset)=data;*(size_t*)(base+l->count_offset)=*(size_t*)(base+l->capacity_offset)=size;return ASN1_OK;}
    case ASN1_TLV_BITS:{const asn1_text_node_t* text=n;size_t bit_length=t->fixed_size;
      if(bit_length==SIZE_MAX){if(n->kind!=ASN1_TEXT_OBJECT)return ASN1_ERR_INVALID_ARGUMENT;const asn1_text_node_t* len=member(n,"length");text=member(n,"value");size_t fields=0;for(const asn1_text_node_t* f=n->children;f;f=f->next)++fields;
        if(!len||!text||fields!=2)return ASN1_ERR_INVALID_ARGUMENT;asn1_bigint_t size;asn1_bigint_init(&size);TRY(integer(len,&size,budget));int64_t count=0;asn1_status_t s=asn1_bigint_to_int64(&size,&count);asn1_bigint_free(&size);if(s!=ASN1_OK)return s;if(count<0||(uint64_t)count>ASN1_MAX_STRING_BYTES*8u)return ASN1_ERR_RESOURCE_LIMIT;bit_length=(size_t)count;}
      if(text->kind!=ASN1_TEXT_STRING)return ASN1_ERR_INVALID_ARGUMENT;uint8_t* data=NULL;size_t bytes=0;TRY(asn1_schema_binary_decode(asn1_byte_view((const uint8_t*)text->text,text->text_size),0,&data,&bytes,budget));
      if(bytes!=bit_length/8u+(bit_length%8u!=0)||(bytes&&bit_length%8u&&(data[bytes-1]&((1u<<(8u-bit_length%8u))-1u)))){free(data);return ASN1_ERR_INVALID_ARGUMENT;}
      asn1_bitstring_t* bits=(asn1_bitstring_t*)(base+l->value_offset);bits->bits=data;bits->bit_length=bit_length;bits->cap_bytes=bytes;return ASN1_OK;}
    case ASN1_TLV_OID:case ASN1_TLV_RELATIVE_OID:{if(n->kind!=ASN1_TEXT_STRING||!n->text_size)return ASN1_ERR_INVALID_ARGUMENT;size_t count=1;for(size_t i=0;i<n->text_size;++i)count+=n->text[i]=='.';uint64_t* arcs=NULL;TRY(asn1_schema_allocate(budget,sizeof(*arcs),count,(void**)&arcs));size_t at=0;asn1_status_t s=ASN1_OK;
      for(size_t i=0;i<count;++i){size_t start=at;uint64_t arc=0;while(at<n->text_size&&n->text[at]!='.'){unsigned c=(unsigned char)n->text[at++];if(c<'0'||c>'9'||arc>(UINT64_MAX-(c-'0'))/10u){s=ASN1_ERR_INVALID_ARGUMENT;break;}arc=arc*10u+c-'0';}if(s!=ASN1_OK||at==start||(at-start>1&&n->text[start]=='0')){s=ASN1_ERR_INVALID_ARGUMENT;break;}arcs[i]=arc;++at;}
      if(s==ASN1_OK&&l->kind==ASN1_TLV_OID&&(count<2||arcs[0]>2||(arcs[0]<2&&arcs[1]>39)))s=ASN1_ERR_CONSTRAINT_VIOLATION;
      if(s!=ASN1_OK){free(arcs);return s;}*(uint64_t**)(base+l->data_offset)=arcs;*(size_t*)(base+l->count_offset)=*(size_t*)(base+l->capacity_offset)=count;return ASN1_OK;}
    case ASN1_TLV_CHOICE:{size_t selected=l->field_count;const asn1_text_node_t* child=n;
      if(flags&ASN1_SCHEMA_UNWRAPPED){
        size_t matches=0;
        for(size_t i=0;i<l->field_count;++i){void* candidate=NULL;TRY(asn1_schema_allocate(budget,l->size,1,&candidate));l->init(candidate);l->choice_set(candidate,i);void* dst=NULL;asn1_status_t s=asn1_schema_field_destination(candidate,l->fields+i,budget,&dst);
          if(s==ASN1_OK)s=json_decode_at(n,t->fields[i].type(),dst,budget,depth+1,0);
          if(s==ASN1_OK){if(matches){l->clear(candidate);free(candidate);return ASN1_ERR_INVALID_ARGUMENT;}memcpy(value,candidate,l->size);l->init(candidate);++matches;}
          l->clear(candidate);free(candidate);if(s==ASN1_ERR_OUT_OF_MEMORY||s==ASN1_ERR_RESOURCE_LIMIT)return s;
        }return matches?ASN1_OK:ASN1_ERR_CONSTRAINT_VIOLATION;
      }
      if(n->kind!=ASN1_TEXT_OBJECT||!n->children||n->children->next)return ASN1_ERR_INVALID_ARGUMENT;child=n->children;
      for(size_t i=0;i<l->field_count;++i)if(equals(child->name,child->name_size,t->fields[i].instruction_name)){selected=i;break;}
      if(selected==l->field_count)return ASN1_ERR_TAG_MISMATCH;l->choice_set(value,selected);void* dst=NULL;TRY(asn1_schema_field_destination(value,l->fields+selected,budget,&dst));return json_decode_at(child,t->fields[selected].type(),dst,budget,depth+1,0);}
    case ASN1_TLV_SEQUENCE_OF:case ASN1_TLV_SET_OF:{int object=(flags&ASN1_SCHEMA_OBJECT)!=0;if(n->kind!=(object?ASN1_TEXT_OBJECT:ASN1_TEXT_ARRAY))return ASN1_ERR_INVALID_ARGUMENT;size_t count=0;for(const asn1_text_node_t* f=n->children;f;f=f->next)++count;uint8_t* data=NULL;TRY(asn1_schema_allocate(budget,l->element()->size,count,(void**)&data));*(void**)(base+l->data_offset)=data;*(size_t*)(base+l->capacity_offset)=count;size_t i=0;
      for(const asn1_text_node_t* f=n->children;f;f=f->next){void* child=data+i*l->element()->size;l->element()->init(child);*(size_t*)(base+l->count_offset)=++i;
        if(!object)TRY(json_decode_at(f,t->element(),child,budget,depth+1,0));
        else{unsigned pair_flags=0;const asn1_codec_schema_t* pair=record_schema(t->element(),&pair_flags);if(!pair||pair->layout()->field_count!=2)return ASN1_ERR_UNSUPPORTED;
          asn1_text_node_t key={0},entry=*f,record={0};key.kind=ASN1_TEXT_STRING;key.text=f->name;key.text_size=f->name_size;key.name=(char*)pair->fields[0].instruction_name;key.name_size=strlen(key.name);key.next=&entry;entry.name=(char*)pair->fields[1].instruction_name;entry.name_size=strlen(entry.name);entry.next=NULL;record.kind=(pair_flags&ASN1_SCHEMA_ARRAY)?ASN1_TEXT_ARRAY:ASN1_TEXT_OBJECT;record.children=&key;
          TRY(json_decode_at(&record,t->element(),child,budget,depth+1,0));
        }}return ASN1_OK;}
    case ASN1_TLV_SEQUENCE:case ASN1_TLV_SET:{int array=(flags&ASN1_SCHEMA_ARRAY)!=0;if(n->kind!=(array?ASN1_TEXT_ARRAY:ASN1_TEXT_OBJECT))return ASN1_ERR_INVALID_ARGUMENT;
      uint8_t* seen=NULL;TRY(asn1_schema_allocate(budget,l->field_count,1,(void**)&seen));asn1_status_t s=ASN1_OK;size_t index=0;
      for(const asn1_text_node_t* f=n->children;f;f=f->next){size_t selected=l->field_count;if(array)selected=array_field(t,index++);else for(size_t i=0;i<l->field_count;++i)if(equals(f->name,f->name_size,t->fields[i].instruction_name)){selected=i;break;}
        if(selected>=l->field_count){if(!l->extensible){s=ASN1_ERR_TAG_MISMATCH;break;}continue;}
        const asn1_tlv_field_t* field=l->fields+selected;
        if(f->kind==ASN1_TEXT_NULL&&(field->is_default||field->presence!=SIZE_MAX)&&!nullable(t->fields[selected].type(),0,0))continue;
        if(seen[selected]){s=ASN1_ERR_INVALID_ARGUMENT;break;}seen[selected]=1;void* dst=NULL;s=asn1_schema_field_destination(value,field,budget,&dst);if(s!=ASN1_OK)break;
        s=json_decode_at(f,t->fields[selected].type(),dst,budget,depth+1,0);if(s!=ASN1_OK)break;if(field->presence!=SIZE_MAX)*(int*)(base+field->presence)=1;
      }
      for(size_t i=0;s==ASN1_OK&&i<l->field_count;++i)if(!seen[i]){const asn1_tlv_field_t* f=l->fields+i;if(f->presence==SIZE_MAX&&!f->is_default)s=ASN1_ERR_CONSTRAINT_VIOLATION;else if(f->default_init){s=asn1_schema_charge(budget,f->default_bytes,0);if(s==ASN1_OK)s=f->default_init(value);}}
      free(seen);return s;}
    default:return ASN1_ERR_UNSUPPORTED;
  }
}
static asn1_status_t json_decode_at(const asn1_text_node_t* n,const asn1_codec_schema_t* t,void* v,asn1_schema_budget_t* b,size_t d,unsigned flags){
  if(d>=ASN1_MAX_RECURSION_DEPTH)return ASN1_ERR_RESOURCE_LIMIT;TRY(json_decode_body(n,t,v,b,d,flags));return t->layout()->validate(v);
}
asn1_status_t asn1_schema_json_decode(const asn1_text_node_t* n,const asn1_codec_schema_t* t,void* v,asn1_schema_budget_t* b,size_t d){return json_decode_at(n,t,v,b,d,0);}
