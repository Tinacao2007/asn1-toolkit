#include "schema_internal.h"
#include <stdlib.h>
#include "alloc_internal.h"
#define TRY(x) do{asn1_status_t s_=(x);if(s_!=ASN1_OK)return s_;}while(0)
asn1_status_t asn1_schema_charge(asn1_schema_budget_t* b,size_t bytes,size_t elements){
  if(bytes>ASN1_MAX_PDU_ALLOCATION_BYTES-b->bytes||elements>ASN1_MAX_PDU_ELEMENTS-b->elements)return ASN1_ERR_RESOURCE_LIMIT;
  b->bytes+=bytes;b->elements+=elements;return ASN1_OK;
}
asn1_status_t asn1_schema_allocate(asn1_schema_budget_t* b,size_t size,size_t count,void** out){
  if(count&&size>SIZE_MAX/count)return ASN1_ERR_LENGTH_OVERFLOW;
  size_t bytes=size*count;TRY(asn1_schema_charge(b,bytes,count));if(!bytes){*out=NULL;return ASN1_OK;}
  void* p=calloc(count,size);if(!p)return ASN1_ERR_OUT_OF_MEMORY;*out=p;return ASN1_OK;
}
const void* asn1_schema_field_value(const void* value,const asn1_tlv_field_t* f){const void* p=(const uint8_t*)value+f->offset;return f->indirect?*(void*const*)p:p;}
int asn1_schema_field_present(const void* value,const asn1_tlv_field_t* f){
  if(f->presence!=SIZE_MAX&&!*(const int*)((const uint8_t*)value+f->presence))return 0;
  return !f->is_default||!f->is_default(value);
}
asn1_status_t asn1_schema_field_destination(void* value,const asn1_tlv_field_t* f,asn1_schema_budget_t* b,void** out){
  uint8_t* place=(uint8_t*)value+f->offset;
  if(f->indirect){void* child=NULL;TRY(asn1_schema_allocate(b,f->type()->size,1,&child));f->type()->init(child);*(void**)place=child;place=(uint8_t*)child;}
  *out=place;return ASN1_OK;
}
static asn1_status_t tree_charge(const asn1_text_node_t* n,asn1_schema_budget_t* b){
  for(;n;n=n->next){TRY(asn1_schema_charge(b,sizeof(*n)+(n->name?n->name_size+1:0)+(n->text?n->text_size+1:0),1));TRY(tree_charge(n->children,b));TRY(tree_charge(n->attributes,b));}return ASN1_OK;
}
asn1_status_t asn1_schema_encode(asn1_byte_writer_t* w,asn1_schema_codec_t codec,const asn1_codec_schema_t* t,const void* v){
  if(!w||!t||!v||codec<ASN1_CODEC_OER||codec>ASN1_CODEC_EXER)return ASN1_ERR_INVALID_ARGUMENT;
  asn1_byte_writer_t* tmp=NULL;TRY(asn1_byte_writer_create(&tmp));asn1_status_t s;
  if(codec<=ASN1_CODEC_COER)s=asn1_schema_oer_encode(tmp,codec==ASN1_CODEC_COER?ASN1_OER_CANONICAL:ASN1_OER_BASIC,t,v,0);
  else if(codec<=ASN1_CODEC_JERI)s=asn1_schema_json_encode(tmp,t,v,0);
  else s=asn1_schema_xml_encode(tmp,codec,t,v,t->name,0,0);
  if(s==ASN1_OK)s=asn1_byte_writer_append(w,asn1_byte_writer_view(tmp));asn1_byte_writer_destroy(tmp);return s;
}
asn1_status_t asn1_schema_decode(asn1_byte_reader_t* r,asn1_schema_codec_t codec,const asn1_codec_schema_t* t,void* out){
  if(!r||!t||!out||codec<ASN1_CODEC_OER||codec>ASN1_CODEC_EXER)return ASN1_ERR_INVALID_ARGUMENT;
  asn1_schema_budget_t budget={0,0};void* value=NULL;asn1_text_node_t* tree=NULL;size_t used=0;asn1_byte_reader_t* shadow=NULL;
  asn1_byte_view_t input=asn1_byte_reader_view(r);asn1_status_t s;
  if(codec>=ASN1_CODEC_JER){size_t error=0;s=codec<=ASN1_CODEC_JERI?asn1_json_parse(input,&tree,&error):asn1_xml_parse(input,&tree,&error);if(s!=ASN1_OK)return s;s=tree_charge(tree,&budget);}
  else{s=asn1_schema_charge(&budget,input.size,0);if(s==ASN1_OK)s=asn1_byte_reader_create(&shadow,input);}
  if(s==ASN1_OK)s=asn1_schema_allocate(&budget,t->layout()->size,1,&value);
  if(s==ASN1_OK){t->layout()->init(value);
    if(codec<=ASN1_CODEC_COER){s=asn1_schema_oer_decode(shadow,codec==ASN1_CODEC_COER?ASN1_OER_CANONICAL:ASN1_OER_BASIC,t,value,&budget,0);used=asn1_byte_reader_position(shadow);}
    else if(codec<=ASN1_CODEC_JERI){s=asn1_schema_json_decode(tree,t,value,&budget,0);used=input.size;}
    else{if(tree->name_size!=strlen(t->name)||memcmp(tree->name,t->name,tree->name_size))s=ASN1_ERR_TAG_MISMATCH;else s=asn1_schema_xml_decode(tree,codec,t,value,&budget,0,0);used=input.size;}
  }
  if(s==ASN1_OK&&codec==ASN1_CODEC_CXER){asn1_byte_writer_t* canonical=NULL;s=asn1_byte_writer_create(&canonical);if(s==ASN1_OK)s=asn1_schema_xml_encode(canonical,codec,t,value,t->name,0,0);
    if(s==ASN1_OK){asn1_byte_view_t expected=asn1_byte_writer_view(canonical);if(expected.size!=input.size||(input.size&&memcmp(expected.data,input.data,input.size)))s=ASN1_ERR_NON_CANONICAL;}asn1_byte_writer_destroy(canonical);}
  if(s==ASN1_OK){t->layout()->clear(out);memcpy(out,value,t->layout()->size);s=asn1_byte_reader_skip(r,used);}
  else if(value)t->layout()->clear(value);
  free(value);asn1_text_tree_free(tree);asn1_byte_reader_destroy(shadow);return s;
}
