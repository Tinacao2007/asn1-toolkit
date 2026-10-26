#include "schema_internal.h"
#include <asn1/c_runtime/bigint.h>
#include <asn1/c_runtime/bit_string.h>
#include <asn1/c_runtime/value_content.h>
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <math.h>
#include <locale.h>
#include "alloc_internal.h"
#define TRY(x) do{asn1_status_t s_=(x);if(s_!=ASN1_OK)return s_;}while(0)
static asn1_status_t put(asn1_byte_writer_t* w,const char* s){return asn1_byte_writer_append(w,asn1_byte_view((const uint8_t*)s,strlen(s)));}
static int ws(unsigned c){return c==' '||c==9||c==10||c==13;}
static int space_only(const char* s,size_t n){for(size_t i=0;i<n;++i)if(!ws((unsigned char)s[i]))return 0;return 1;}
static int equals(const char* s,size_t n,const char* v){return n==strlen(v)&&(!n||!memcmp(s,v,n));}
static const char* field_name(const asn1_codec_schema_t* t,size_t i,asn1_schema_codec_t codec){return codec==ASN1_CODEC_EXER?t->fields[i].instruction_name:t->fields[i].name;}
static asn1_codec_schema_t alias_target(const asn1_codec_schema_t* t,asn1_schema_codec_t codec){asn1_codec_schema_t target=*t->element();if(codec==ASN1_CODEC_EXER&&t->enumerator_count){target.enumerators=t->enumerators;target.enumerator_count=t->enumerator_count;}return target;}
static int compact_item(const asn1_codec_schema_t* t,asn1_schema_codec_t codec){unsigned flags=0;for(size_t depth=0;depth<ASN1_MAX_RECURSION_DEPTH;++depth){flags|=t->flags;const asn1_tlv_schema_t* l=t->layout();if(l->kind==ASN1_TLV_ALIAS){t=t->element();continue;}return !(codec==ASN1_CODEC_EXER&&(flags&(ASN1_SCHEMA_TEXT|ASN1_SCHEMA_NUMBER)))&&(l->kind==ASN1_TLV_BOOLEAN||(l->kind==ASN1_TLV_INT64&&t->enumerator_count));}return 0;}
static const char* controls[]={"nul","soh","stx","etx","eot","enq","ack","bel","bs","ht","lf","vt","ff","cr","so","si","dle","dc1","dc2","dc3","dc4","nak","syn","etb","can","em","sub","esc","is4","is3","is2","is1"};
static asn1_status_t escaped(asn1_byte_writer_t* w,const char* s,size_t n,int attr){return asn1_xml_escape(w,asn1_byte_view((const uint8_t*)s,n),attr);}
static asn1_status_t empty(asn1_byte_writer_t* w,const char* name,asn1_schema_codec_t codec){TRY(put(w,"<"));TRY(put(w,name));return put(w,codec==ASN1_CODEC_CXER?"/>":" />");}
static asn1_status_t character_string(asn1_byte_writer_t* w,asn1_byte_view_t bytes,asn1_schema_codec_t codec,int attr){
  if(!bytes.size)return ASN1_OK;
  size_t first=0;for(size_t i=0;i<bytes.size;++i)if(bytes.data[i]<32&&!ws(bytes.data[i])){if(attr)return ASN1_ERR_UNSUPPORTED;TRY(escaped(w,(const char*)bytes.data+first,i-first,0));TRY(empty(w,controls[bytes.data[i]],codec));first=i+1;}return escaped(w,(const char*)bytes.data+first,bytes.size-first,attr);
}
static asn1_status_t canonical_real(asn1_byte_writer_t* w,double value){
  if(value==0)return put(w,signbit(value)?"-0":"0");
  char text[128];int n=snprintf(text,sizeof(text),"%.16e",value);if(n<1||(size_t)n>=sizeof(text))return ASN1_ERR_INVALID_ARGUMENT;
  const char* radix=localeconv()->decimal_point;if(radix&&*radix&&strcmp(radix,".")){char* at=strstr(text,radix);if(at){size_t size=strlen(radix);*at='.';memmove(at+1,at+size,strlen(at+size)+1);}}
  char* exponent=strchr(text,'e');char* point=strchr(text,'.');if(!exponent||!point)return ASN1_ERR_INVALID_ARGUMENT;
  long power=strtol(exponent+1,NULL,10);char* end=exponent;while(end>point+2&&end[-1]=='0')--end;*end=0;
  TRY(put(w,text));char suffix[32];n=snprintf(suffix,sizeof(suffix),"E%ld",power);return n>0&&(size_t)n<sizeof(suffix)?put(w,suffix):ASN1_ERR_INVALID_ARGUMENT;
}
static asn1_status_t scalar(asn1_byte_writer_t* w,asn1_schema_codec_t codec,const asn1_codec_schema_t* t,const void* v,unsigned inherited,int attribute,size_t depth){
  if(depth>=ASN1_MAX_RECURSION_DEPTH)return ASN1_ERR_RESOURCE_LIMIT;const asn1_tlv_schema_t* l=t->layout();const uint8_t* base=(const uint8_t*)v;unsigned flags=codec==ASN1_CODEC_EXER?(t->flags|inherited):0;TRY(l->validate(v));
  if(l->kind==ASN1_TLV_OPEN)return ASN1_ERR_UNSUPPORTED;
  if(l->kind==ASN1_TLV_ALIAS){const void* child=base+l->value_offset;if(l->indirect)child=*(void*const*)child;asn1_codec_schema_t target=alias_target(t,codec);return child?scalar(w,codec,&target,child,flags,attribute,depth+1):ASN1_ERR_CONSTRAINT_VIOLATION;}
  switch(l->kind){
    case ASN1_TLV_BOOLEAN:{const char* text=*(const int*)(base+l->value_offset)?"true":"false";return attribute||(flags&ASN1_SCHEMA_TEXT)?put(w,text):empty(w,text,codec);}
    case ASN1_TLV_NULL:return ASN1_OK;
    case ASN1_TLV_INT64:{int64_t number=*(const int64_t*)(base+l->value_offset);if(t->enumerator_count&&!(flags&ASN1_SCHEMA_NUMBER)){for(size_t i=0;i<t->enumerator_count;++i)if(t->enumerators[i].value==number){const char* text=codec==ASN1_CODEC_EXER&&(flags&ASN1_SCHEMA_TEXT)?t->enumerators[i].name:t->enumerators[i].xml_name;return attribute||(flags&ASN1_SCHEMA_TEXT)?escaped(w,text,strlen(text),attribute):empty(w,text,codec);}return ASN1_ERR_CONSTRAINT_VIOLATION;}
      char text[32];int n=snprintf(text,sizeof(text),"%" PRId64,number);return n>0&&(size_t)n<sizeof(text)?put(w,text):ASN1_ERR_INVALID_ARGUMENT;}
    case ASN1_TLV_BIGINT:{char* text=NULL;size_t n;TRY(asn1_bigint_to_decimal((const asn1_bigint_t*)(base+l->value_offset),&text,&n));asn1_status_t s=asn1_byte_writer_append(w,asn1_byte_view((const uint8_t*)text,n));free(text);return s;}
    case ASN1_TLV_REAL:{double number=*(const double*)(base+l->value_offset);const char* special=isnan(number)?"NOT-A-NUMBER":isinf(number)?number<0?"MINUS-INFINITY":"PLUS-INFINITY":NULL;
      if(special)return attribute?put(w,special):empty(w,special,codec);return codec==ASN1_CODEC_CXER?canonical_real(w,number):asn1_schema_json_encode(w,t,v,depth+1);}
    case ASN1_TLV_OCTETS:case ASN1_TLV_OPEN:{asn1_byte_view_t bytes=asn1_byte_view(*(uint8_t*const*)(base+l->data_offset),*(const size_t*)(base+l->count_offset));if(l->universal_tag!=4&&l->kind!=ASN1_TLV_OPEN)return character_string(w,bytes,codec,attribute);return asn1_schema_binary_encode(w,bytes,(flags&ASN1_SCHEMA_XML_BASE64)!=0);}
    case ASN1_TLV_BITS:{const asn1_bitstring_t* bits=(const asn1_bitstring_t*)(base+l->value_offset);for(size_t i=0;i<bits->bit_length;++i)TRY(put(w,bits->bits[i/8u]&(128u>>(i%8u))?"1":"0"));return ASN1_OK;}
    case ASN1_TLV_OID:case ASN1_TLV_RELATIVE_OID:{const uint64_t* data=*(uint64_t*const*)(base+l->data_offset);size_t count=*(const size_t*)(base+l->count_offset);for(size_t i=0;i<count;++i){if(i)TRY(put(w,"."));char text[32];int n=snprintf(text,sizeof(text),"%" PRIu64,data[i]);if(n<1||(size_t)n>=sizeof(text))return ASN1_ERR_INVALID_ARGUMENT;TRY(put(w,text));}return ASN1_OK;}
    default:return ASN1_ERR_UNSUPPORTED;
  }
}
typedef struct{uint8_t* bytes;size_t size;} piece_t;
static int order(const void* a,const void* b){const piece_t* x=(const piece_t*)a;const piece_t* y=(const piece_t*)b;size_t n=x->size<y->size?x->size:y->size;int c=n?memcmp(x->bytes,y->bytes,n):0;return c?c:(x->size>y->size)-(x->size<y->size);}
static asn1_status_t body(asn1_byte_writer_t* w,asn1_schema_codec_t codec,const asn1_codec_schema_t* t,const void* v,size_t depth,unsigned inherited){
  const asn1_tlv_schema_t* l=t->layout();const uint8_t* base=(const uint8_t*)v;unsigned flags=codec==ASN1_CODEC_EXER?t->flags|inherited:0;
  if(l->kind==ASN1_TLV_ALIAS){const void* child=base+l->value_offset;if(l->indirect)child=*(void*const*)child;asn1_codec_schema_t target=alias_target(t,codec);return child?body(w,codec,&target,child,depth+1,flags):ASN1_ERR_CONSTRAINT_VIOLATION;}
  if(l->kind==ASN1_TLV_SEQUENCE||l->kind==ASN1_TLV_SET){for(size_t i=0;i<l->field_count;++i){const asn1_tlv_field_t* f=l->fields+i;unsigned ff=codec==ASN1_CODEC_EXER?t->fields[i].flags:0;if(ff&ASN1_SCHEMA_ATTRIBUTE)continue;
      if(!asn1_schema_field_present(v,f)&&!(codec==ASN1_CODEC_CXER&&f->is_default)){if(ff&ASN1_SCHEMA_NIL){TRY(put(w,"<"));TRY(put(w,field_name(t,i,codec)));TRY(put(w," xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xsi:nil=\"true\" />"));}continue;}
      const void* child=asn1_schema_field_value(v,f);if(!child)return ASN1_ERR_CONSTRAINT_VIOLATION;TRY(asn1_schema_xml_encode(w,codec,t->fields[i].type(),child,field_name(t,i,codec),depth+1,ff));}return ASN1_OK;}
  if(l->kind==ASN1_TLV_CHOICE){size_t i=l->choice_get(v);if(i>=l->field_count)return ASN1_ERR_CONSTRAINT_VIOLATION;const void* child=asn1_schema_field_value(v,l->fields+i);return child?asn1_schema_xml_encode(w,codec,t->fields[i].type(),child,field_name(t,i,codec),depth+1,codec==ASN1_CODEC_EXER?t->fields[i].flags:0):ASN1_ERR_CONSTRAINT_VIOLATION;}
  if(l->kind==ASN1_TLV_SEQUENCE_OF||l->kind==ASN1_TLV_SET_OF){size_t count=*(const size_t*)(base+l->count_offset);if(count>ASN1_MAX_PDU_ELEMENTS)return ASN1_ERR_RESOURCE_LIMIT;const uint8_t* data=*(uint8_t*const*)(base+l->data_offset);
    if(flags&ASN1_SCHEMA_LIST){for(size_t i=0;i<count;++i){if(i)TRY(put(w," "));asn1_byte_writer_t* token=NULL;TRY(asn1_byte_writer_create(&token));asn1_status_t s=scalar(token,codec,t->element(),data+i*l->element()->size,ASN1_SCHEMA_TEXT|ASN1_SCHEMA_NUMBER,1,depth+1);asn1_byte_view_t bytes=asn1_byte_writer_view(token);
        if(s==ASN1_OK){for(size_t j=0;j<bytes.size;++j)if(ws(bytes.data[j])){s=ASN1_ERR_UNSUPPORTED;break;}if(!bytes.size)s=ASN1_ERR_UNSUPPORTED;}if(s==ASN1_OK)s=asn1_byte_writer_append(w,bytes);asn1_byte_writer_destroy(token);if(s!=ASN1_OK)return s;}return ASN1_OK;}
    piece_t* pieces=count?(piece_t*)calloc(count,sizeof(*pieces)):NULL;if(count&&!pieces)return ASN1_ERR_OUT_OF_MEMORY;size_t used=0,staged_bytes=0;asn1_status_t s=ASN1_OK;
    for(size_t i=0;s==ASN1_OK&&i<count;++i){asn1_byte_writer_t* child=NULL;s=asn1_byte_writer_create(&child);if(s==ASN1_OK)s=compact_item(t->element(),codec)?scalar(child,codec,t->element(),data+i*l->element()->size,0,0,depth+1):asn1_schema_xml_encode(child,codec,t->element(),data+i*l->element()->size,t->element()->name,depth+1,0);if(s==ASN1_OK){size_t size=asn1_byte_writer_view(child).size;if(size>ASN1_MAX_PDU_ALLOCATION_BYTES-staged_bytes)s=ASN1_ERR_RESOURCE_LIMIT;else{staged_bytes+=size;s=asn1_byte_writer_take(child,&pieces[used].bytes,&pieces[used].size);if(s==ASN1_OK)++used;}}asn1_byte_writer_destroy(child);}
    if(s==ASN1_OK&&codec==ASN1_CODEC_CXER&&l->kind==ASN1_TLV_SET_OF&&count)qsort(pieces,count,sizeof(*pieces),order);
    for(size_t i=0;i<used;++i){if(s==ASN1_OK)s=asn1_byte_writer_append(w,asn1_byte_view(pieces[i].bytes,pieces[i].size));free(pieces[i].bytes);}free(pieces);return s;
  }
  return scalar(w,codec,t,v,flags,0,depth);
}
static asn1_status_t attributes(asn1_byte_writer_t* w,const asn1_codec_schema_t* t,const void* v,size_t depth){
  if(depth>=ASN1_MAX_RECURSION_DEPTH)return ASN1_ERR_RESOURCE_LIMIT;const asn1_tlv_schema_t* l=t->layout();
  if(l->kind==ASN1_TLV_ALIAS){const void* child=(const uint8_t*)v+l->value_offset;if(l->indirect)child=*(void*const*)child;return child?attributes(w,t->element(),child,depth+1):ASN1_ERR_CONSTRAINT_VIOLATION;}
  if(l->kind!=ASN1_TLV_SEQUENCE&&l->kind!=ASN1_TLV_SET)return ASN1_OK;
  for(size_t i=0;i<l->field_count;++i)if((t->fields[i].flags&ASN1_SCHEMA_ATTRIBUTE)&&asn1_schema_field_present(v,l->fields+i)){const void* child=asn1_schema_field_value(v,l->fields+i);if(!child)return ASN1_ERR_CONSTRAINT_VIOLATION;TRY(put(w," "));TRY(put(w,field_name(t,i,ASN1_CODEC_EXER)));TRY(put(w,"=\""));TRY(scalar(w,ASN1_CODEC_EXER,t->fields[i].type(),child,t->fields[i].flags,1,depth+1));TRY(put(w,"\""));}return ASN1_OK;
}
asn1_status_t asn1_schema_xml_encode(asn1_byte_writer_t* w,asn1_schema_codec_t codec,const asn1_codec_schema_t* t,const void* v,const char* name,size_t depth,unsigned inherited){
  if(depth>=ASN1_MAX_RECURSION_DEPTH)return ASN1_ERR_RESOURCE_LIMIT;TRY(t->layout()->validate(v));
  unsigned flags=codec==ASN1_CODEC_EXER?t->flags|inherited:0;if(flags&ASN1_SCHEMA_UNTAGGED)return body(w,codec,t,v,depth+1,flags);
  asn1_byte_writer_t* contents=NULL;TRY(asn1_byte_writer_create(&contents));asn1_status_t s=body(contents,codec,t,v,depth+1,flags);
  if(s==ASN1_OK)s=put(w,"<");if(s==ASN1_OK)s=put(w,name);if(s==ASN1_OK&&codec==ASN1_CODEC_EXER)s=attributes(w,t,v,depth+1);
  asn1_byte_view_t bytes=asn1_byte_writer_view(contents);
  if(s==ASN1_OK&&!bytes.size)s=put(w,codec==ASN1_CODEC_CXER?"/>":" />");
  else if(s==ASN1_OK){s=put(w,">");if(s==ASN1_OK)s=asn1_byte_writer_append(w,bytes);if(s==ASN1_OK)s=put(w,"</");if(s==ASN1_OK)s=put(w,name);if(s==ASN1_OK)s=put(w,">");}
  asn1_byte_writer_destroy(contents);return s;
}
static asn1_status_t primitive_decode(const asn1_text_node_t* n,asn1_schema_codec_t codec,const asn1_codec_schema_t* t,void* v,asn1_schema_budget_t* b,size_t depth,unsigned inherited){
  const asn1_tlv_schema_t* l=t->layout();uint8_t* base=(uint8_t*)v;unsigned flags=codec==ASN1_CODEC_EXER?t->flags|inherited:0;int attribute=(flags&ASN1_SCHEMA_ATTRIBUTE)!=0;
  if(l->kind==ASN1_TLV_OPEN)return ASN1_ERR_UNSUPPORTED;
  if(n->attributes)return ASN1_ERR_INVALID_ARGUMENT;
  asn1_text_node_t fake=*n;fake.children=NULL;fake.attributes=NULL;fake.next=NULL;
  asn1_codec_schema_t copy=*t;
  size_t first=0,last=n->text_size;int string=l->kind==ASN1_TLV_OCTETS&&l->universal_tag!=4;
  if(!string){while(first<last&&ws((unsigned char)n->text[first]))++first;while(last>first&&ws((unsigned char)n->text[last-1]))--last;}
  fake.text=n->text?n->text+first:NULL;fake.text_size=last-first;
  if(l->kind==ASN1_TLV_BITS){if(n->children)return ASN1_ERR_INVALID_ARGUMENT;size_t count=0;for(size_t i=first;i<last;++i)if(!ws((unsigned char)n->text[i]))++count;
    if(count>ASN1_MAX_STRING_BYTES*8u)return ASN1_ERR_RESOURCE_LIMIT;size_t size=count/8u+(count%8u!=0);uint8_t* data=NULL;TRY(asn1_schema_allocate(b,size,1,(void**)&data));size_t bit=0;
    for(size_t i=first;i<last;++i)if(!ws((unsigned char)n->text[i])){if(n->text[i]!='0'&&n->text[i]!='1'){free(data);return ASN1_ERR_INVALID_ARGUMENT;}if(n->text[i]=='1')data[bit/8u]|=(uint8_t)(128u>>(bit%8u));++bit;}
    asn1_bitstring_t* bits=(asn1_bitstring_t*)(base+l->value_offset);bits->bits=data;bits->bit_length=count;bits->cap_bytes=size;return ASN1_OK;}
  if(l->kind==ASN1_TLV_NULL)return !n->children&&space_only(n->text,n->text_size)?ASN1_OK:ASN1_ERR_INVALID_ARGUMENT;
  if(l->kind==ASN1_TLV_BOOLEAN){fake.kind=ASN1_TEXT_BOOL;
    if(!attribute&&!(flags&ASN1_SCHEMA_TEXT)){if(!space_only(n->text,n->text_size)||!n->children||n->children->next||n->children->children||n->children->attributes||n->children->text_size)return ASN1_ERR_INVALID_ARGUMENT;fake.text=n->children->name;fake.text_size=n->children->name_size;}
    if(!equals(fake.text,fake.text_size,"true")&&!equals(fake.text,fake.text_size,"false"))return ASN1_ERR_INVALID_ARGUMENT;
  }else if(l->kind==ASN1_TLV_INT64&&t->enumerator_count&&!(flags&ASN1_SCHEMA_NUMBER)){
    fake.kind=ASN1_TEXT_STRING;if(!attribute&&!(flags&ASN1_SCHEMA_TEXT)){if(!space_only(n->text,n->text_size)||!n->children||n->children->next||n->children->children||n->children->attributes||n->children->text_size)return ASN1_ERR_INVALID_ARGUMENT;fake.text=n->children->name;fake.text_size=n->children->name_size;}
    for(size_t i=0;i<t->enumerator_count;++i){const char* text=codec==ASN1_CODEC_EXER&&(flags&ASN1_SCHEMA_TEXT)?t->enumerators[i].name:t->enumerators[i].xml_name;if(equals(fake.text,fake.text_size,text)){fake.text=(char*)t->enumerators[i].name;fake.text_size=strlen(fake.text);break;}}
  }else if(l->kind==ASN1_TLV_INT64||l->kind==ASN1_TLV_BIGINT){if(n->children)return ASN1_ERR_INVALID_ARGUMENT;fake.kind=ASN1_TEXT_NUMBER;if(fake.text_size&&fake.text[0]=='+'){++fake.text;--fake.text_size;}copy.enumerator_count=0;
  }else if(l->kind==ASN1_TLV_REAL){fake.kind=ASN1_TEXT_NUMBER;if(n->children){if(n->children->next||n->children->children||n->children->attributes||n->children->text_size||!space_only(n->text,n->text_size))return ASN1_ERR_INVALID_ARGUMENT;fake.text=n->children->name;fake.text_size=n->children->name_size;}
    if(equals(fake.text,fake.text_size,"PLUS-INFINITY")){fake.text="INF";fake.text_size=3;fake.kind=ASN1_TEXT_STRING;}else if(equals(fake.text,fake.text_size,"MINUS-INFINITY")){fake.text="-INF";fake.text_size=4;fake.kind=ASN1_TEXT_STRING;}else if(equals(fake.text,fake.text_size,"NOT-A-NUMBER")){fake.text="NaN";fake.text_size=3;fake.kind=ASN1_TEXT_STRING;}
  }else fake.kind=ASN1_TEXT_STRING;
  if(string){
    if(n->children){size_t size=n->text_size;for(const asn1_text_node_t* c=n->children;c;c=c->next)++size;if(size>ASN1_MAX_STRING_BYTES)return ASN1_ERR_RESOURCE_LIMIT;uint8_t* data=NULL;TRY(asn1_schema_allocate(b,size,1,(void**)&data));size_t used=0,at=0;asn1_status_t s=ASN1_OK;
      for(const asn1_text_node_t* c=n->children;c;c=c->next){if(c->children||c->attributes||c->text_size||c->text_offset<at||c->text_offset>n->text_size){s=ASN1_ERR_INVALID_ARGUMENT;break;}size_t i;for(i=0;i<32;++i)if(equals(c->name,c->name_size,controls[i]))break;if(i==32){s=ASN1_ERR_INVALID_ARGUMENT;break;}size_t bytes=c->text_offset-at;if(bytes)memcpy(data+used,n->text+at,bytes);used+=bytes;at=c->text_offset;data[used++]=(uint8_t)i;}
      if(s!=ASN1_OK){free(data);return s;}if(n->text_size>at)memcpy(data+used,n->text+at,n->text_size-at);*(uint8_t**)(base+l->data_offset)=data;*(size_t*)(base+l->count_offset)=*(size_t*)(base+l->capacity_offset)=size;return ASN1_OK;
    }
  }else if(n->children&&l->kind!=ASN1_TLV_BOOLEAN&&l->kind!=ASN1_TLV_REAL&&!t->enumerator_count)return ASN1_ERR_INVALID_ARGUMENT;
  if(l->kind==ASN1_TLV_OPEN||l->kind==ASN1_TLV_OCTETS){copy.flags&=~ASN1_SCHEMA_BASE64;if(flags&ASN1_SCHEMA_XML_BASE64)copy.flags|=ASN1_SCHEMA_BASE64;
    if(!string){char* compact=NULL;TRY(asn1_schema_allocate(b,fake.text_size+1u,1,(void**)&compact));size_t used=0;for(size_t i=0;i<fake.text_size;++i)if(!ws((unsigned char)fake.text[i]))compact[used++]=fake.text[i];fake.text=compact;fake.text_size=used;asn1_status_t s=asn1_schema_json_decode(&fake,&copy,v,b,depth+1);free(compact);return s;}}
  /* The JSON numeric helper requires NUL-terminated tokens. XML trimming may
     leave trailing whitespace in the backing string. */
  char* token=NULL;TRY(asn1_schema_allocate(b,fake.text_size+1u,1,(void**)&token));if(fake.text_size)memcpy(token,fake.text,fake.text_size);fake.text=token;
  asn1_status_t s=asn1_schema_json_decode(&fake,&copy,v,b,depth+1);free(token);return s;
}
static asn1_status_t field_decode(const asn1_text_node_t* n,asn1_schema_codec_t codec,const asn1_codec_schema_t* t,size_t i,void* value,asn1_schema_budget_t* b,size_t depth){
  unsigned flags=codec==ASN1_CODEC_EXER?t->fields[i].flags:0;const asn1_tlv_field_t* f=t->layout()->fields+i;
  if(flags&ASN1_SCHEMA_NIL){int nil=0,xmlns=0;for(const asn1_text_node_t* a=n->attributes;a;a=a->next){if(equals(a->name,a->name_size,"xsi:nil")){nil=equals(a->text,a->text_size,"true")||equals(a->text,a->text_size,"1");if(!nil)return ASN1_ERR_INVALID_ARGUMENT;}
      else if(equals(a->name,a->name_size,"xmlns:xsi")&&equals(a->text,a->text_size,"http://www.w3.org/2001/XMLSchema-instance"))xmlns=1;else return ASN1_ERR_INVALID_ARGUMENT;}
    if(nil){if(!xmlns||n->children||!space_only(n->text,n->text_size)||f->presence==SIZE_MAX)return ASN1_ERR_INVALID_ARGUMENT;return ASN1_OK;}}
  void* dst=NULL;TRY(asn1_schema_field_destination(value,f,b,&dst));TRY(asn1_schema_xml_decode(n,codec,t->fields[i].type(),dst,b,depth+1,flags));if(f->presence!=SIZE_MAX)*(int*)((uint8_t*)value+f->presence)=1;
  return ASN1_OK;
}
asn1_status_t asn1_schema_xml_decode(const asn1_text_node_t* n,asn1_schema_codec_t codec,const asn1_codec_schema_t* t,void* value,asn1_schema_budget_t* b,size_t depth,unsigned inherited){
  if(depth>=ASN1_MAX_RECURSION_DEPTH)return ASN1_ERR_RESOURCE_LIMIT;const asn1_tlv_schema_t* l=t->layout();uint8_t* base=(uint8_t*)value;unsigned flags=codec==ASN1_CODEC_EXER?t->flags|inherited:0;
  if(l->kind==ASN1_TLV_ALIAS){void* child=base+l->value_offset;if(l->indirect){TRY(asn1_schema_allocate(b,l->element()->size,1,&child));*(void**)(base+l->value_offset)=child;l->element()->init(child);}asn1_codec_schema_t target=alias_target(t,codec);TRY(asn1_schema_xml_decode(n,codec,&target,child,b,depth+1,flags));return l->validate(value);}
  if(l->kind==ASN1_TLV_SEQUENCE||l->kind==ASN1_TLV_SET){
    if(!space_only(n->text,n->text_size))return ASN1_ERR_INVALID_ARGUMENT;uint8_t* seen=NULL;TRY(asn1_schema_allocate(b,l->field_count,1,(void**)&seen));asn1_status_t s=ASN1_OK;size_t previous=0;int has_previous=0;
    for(unsigned attributes=0;attributes<2&&s==ASN1_OK;++attributes)for(const asn1_text_node_t* child=attributes?n->attributes:n->children;child;child=child->next){size_t selected=l->field_count;
      for(size_t i=0;i<l->field_count;++i)if(equals(child->name,child->name_size,field_name(t,i,codec))){selected=i;break;}
      if(selected==l->field_count){if(attributes||!l->extensible)s=ASN1_ERR_TAG_MISMATCH;else if(codec==ASN1_CODEC_CXER)s=ASN1_ERR_UNSUPPORTED;continue;}
      if(!attributes&&l->kind==ASN1_TLV_SEQUENCE){if(has_previous&&selected<previous){s=ASN1_ERR_INVALID_ARGUMENT;break;}previous=selected;has_previous=1;}
      unsigned f=codec==ASN1_CODEC_EXER?t->fields[selected].flags:0;if(!!(f&ASN1_SCHEMA_ATTRIBUTE)!=(int)attributes||seen[selected]){s=ASN1_ERR_INVALID_ARGUMENT;break;}seen[selected]=1;s=field_decode(child,codec,t,selected,value,b,depth);if(s!=ASN1_OK)break;
    }
    for(size_t i=0;s==ASN1_OK&&i<l->field_count;++i)if(!seen[i]){const asn1_tlv_field_t* f=l->fields+i;if(codec==ASN1_CODEC_CXER&&f->is_default)s=ASN1_ERR_NON_CANONICAL;else if(f->presence==SIZE_MAX&&!f->is_default)s=ASN1_ERR_CONSTRAINT_VIOLATION;else if(f->default_init){s=asn1_schema_charge(b,f->default_bytes,0);if(s==ASN1_OK)s=f->default_init(value);}}
    free(seen);if(s!=ASN1_OK)return s;
  }else if(l->kind==ASN1_TLV_CHOICE){if(n->attributes||!space_only(n->text,n->text_size)||!n->children||n->children->next)return ASN1_ERR_INVALID_ARGUMENT;size_t selected=l->field_count;
    for(size_t i=0;i<l->field_count;++i)if(equals(n->children->name,n->children->name_size,field_name(t,i,codec))){selected=i;break;}if(selected==l->field_count)return ASN1_ERR_TAG_MISMATCH;l->choice_set(value,selected);TRY(field_decode(n->children,codec,t,selected,value,b,depth));
  }else if(l->kind==ASN1_TLV_SEQUENCE_OF||l->kind==ASN1_TLV_SET_OF){if(n->attributes)return ASN1_ERR_INVALID_ARGUMENT;size_t count=0;
    if(flags&ASN1_SCHEMA_LIST){if(n->children)return ASN1_ERR_INVALID_ARGUMENT;int token=0;for(size_t i=0;i<n->text_size;++i){if(ws((unsigned char)n->text[i]))token=0;else if(!token){++count;token=1;}}}
    else{if(!space_only(n->text,n->text_size))return ASN1_ERR_INVALID_ARGUMENT;for(const asn1_text_node_t* c=n->children;c;c=c->next)++count;}
    uint8_t* data=NULL;TRY(asn1_schema_allocate(b,l->element()->size,count,(void**)&data));*(void**)(base+l->data_offset)=data;*(size_t*)(base+l->capacity_offset)=count;const asn1_text_node_t* child=n->children;size_t at=0;
    for(size_t i=0;i<count;++i){void* dst=data+i*l->element()->size;l->element()->init(dst);*(size_t*)(base+l->count_offset)=i+1;
      if(flags&ASN1_SCHEMA_LIST){while(at<n->text_size&&ws((unsigned char)n->text[at]))++at;size_t start=at;while(at<n->text_size&&!ws((unsigned char)n->text[at]))++at;asn1_text_node_t token=*n;token.text=n->text+start;token.text_size=at-start;token.children=NULL;token.attributes=NULL;TRY(asn1_schema_xml_decode(&token,codec,t->element(),dst,b,depth+1,ASN1_SCHEMA_TEXT|ASN1_SCHEMA_NUMBER));}
      else{if(compact_item(t->element(),codec)){asn1_text_node_t item=*child,wrapper={0};item.next=NULL;wrapper.kind=ASN1_TEXT_ELEMENT;wrapper.children=&item;TRY(asn1_schema_xml_decode(&wrapper,codec,t->element(),dst,b,depth+1,0));}
        else{if(!equals(child->name,child->name_size,t->element()->name))return ASN1_ERR_TAG_MISMATCH;TRY(asn1_schema_xml_decode(child,codec,t->element(),dst,b,depth+1,0));}child=child->next;}}
  }else TRY(primitive_decode(n,codec,t,value,b,depth,flags));
  return l->validate(value);
}
