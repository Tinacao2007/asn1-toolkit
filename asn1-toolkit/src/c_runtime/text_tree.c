#include <asn1/c_runtime/text_tree.h>
#include <asn1/c_runtime/tlv_schema.h>
#include <asn1/c_runtime/limits.h>
#include <stdlib.h>
#include <string.h>
#include "alloc_internal.h"

#define TRY(x) do { asn1_status_t s_=(x); if(s_!=ASN1_OK)return s_; } while(0)
typedef struct {
  asn1_byte_view_t input;
  size_t pos,bytes,nodes;
} parser_t;
static asn1_status_t charge(parser_t* p,size_t n) {
  if(n>ASN1_MAX_PDU_ALLOCATION_BYTES-p->bytes)return ASN1_ERR_RESOURCE_LIMIT;
  p->bytes+=n;return ASN1_OK;
}
static int ws(unsigned c){return c==' '||c=='\t'||c=='\r'||c=='\n';}
static void skip(parser_t* p){while(p->pos<p->input.size&&ws(p->input.data[p->pos]))++p->pos;}
static int take(parser_t* p,unsigned c){if(p->pos<p->input.size&&p->input.data[p->pos]==c){++p->pos;return 1;}return 0;}
static int match(parser_t* p,const char* s){size_t n=strlen(s);if(n>p->input.size-p->pos||memcmp(p->input.data+p->pos,s,n))return 0;p->pos+=n;return 1;}
static asn1_status_t copy_text(parser_t* p,const uint8_t* data,size_t n,char** out){
  if(n>ASN1_MAX_STRING_BYTES)return ASN1_ERR_RESOURCE_LIMIT;
  TRY(charge(p,n+1u));char* s=(char*)malloc(n+1u);if(!s)return ASN1_ERR_OUT_OF_MEMORY;
  if(n)memcpy(s,data,n);s[n]=0;*out=s;return ASN1_OK;
}
static asn1_status_t node(parser_t* p,asn1_text_kind_t k,asn1_text_node_t** out){
  if(p->nodes>=ASN1_MAX_PDU_ELEMENTS)return ASN1_ERR_RESOURCE_LIMIT;
  TRY(charge(p,sizeof(**out)));asn1_text_node_t* n=(asn1_text_node_t*)calloc(1,sizeof(*n));
  if(!n)return ASN1_ERR_OUT_OF_MEMORY;++p->nodes;n->kind=k;*out=n;return ASN1_OK;
}
void asn1_text_tree_free(asn1_text_node_t* n){
  while(n){asn1_text_node_t* next=n->next;asn1_text_tree_free(n->children);asn1_text_tree_free(n->attributes);free(n->name);free(n->text);free(n);n=next;}
}
static int hex(unsigned c){return c>='0'&&c<='9'?(int)(c-'0'):c>='a'&&c<='f'?(int)(c-'a'+10):c>='A'&&c<='F'?(int)(c-'A'+10):-1;}
static asn1_status_t codepoint(asn1_byte_writer_t* w,uint32_t cp){
  uint8_t b[4];size_t n;
  if(cp>0x10ffffu||(cp>=0xd800u&&cp<=0xdfffu))return ASN1_ERR_INVALID_ARGUMENT;
  if(cp<128u){b[0]=(uint8_t)cp;n=1;}
  else if(cp<2048u){b[0]=(uint8_t)(0xc0u|(cp>>6));b[1]=(uint8_t)(0x80u|(cp&63u));n=2;}
  else if(cp<65536u){b[0]=(uint8_t)(0xe0u|(cp>>12));b[1]=(uint8_t)(0x80u|((cp>>6)&63u));b[2]=(uint8_t)(0x80u|(cp&63u));n=3;}
  else{b[0]=(uint8_t)(0xf0u|(cp>>18));b[1]=(uint8_t)(0x80u|((cp>>12)&63u));b[2]=(uint8_t)(0x80u|((cp>>6)&63u));b[3]=(uint8_t)(0x80u|(cp&63u));n=4;}
  return asn1_byte_writer_append(w,asn1_byte_view(b,n));
}
static asn1_status_t u16(parser_t* p,uint32_t* cp){
  if(p->input.size-p->pos<4)return ASN1_ERR_TRUNCATED;uint32_t value=0;
  for(unsigned i=0;i<4;++i){int v=hex(p->input.data[p->pos++]);if(v<0)return ASN1_ERR_INVALID_ARGUMENT;value=(value<<4)|(unsigned)v;}
  *cp=value;return ASN1_OK;
}
/* Buffer is bounded before every append; the decoded string is charged once
 * when ownership moves to the tree. Scratch storage has its separate cap. */
static asn1_status_t json_string_body(parser_t* p,asn1_byte_writer_t* w){
  if(!take(p,'"'))return ASN1_ERR_INVALID_ARGUMENT;
  while(p->pos<p->input.size){
    unsigned c=p->input.data[p->pos++];if(c=='"')return ASN1_OK;
    if(c<32)return ASN1_ERR_INVALID_ARGUMENT;
    if(c=='\\'){
      if(p->pos==p->input.size)return ASN1_ERR_TRUNCATED;c=p->input.data[p->pos++];
      switch(c){case '"':case '\\':case '/':break;case 'b':c=8;break;case 'f':c=12;break;case 'n':c=10;break;case 'r':c=13;break;case 't':c=9;break;
        case 'u':{uint32_t cp;TRY(u16(p,&cp));if(cp>=0xd800u&&cp<=0xdbffu){uint32_t low;if(!match(p,"\\u"))return ASN1_ERR_INVALID_ARGUMENT;TRY(u16(p,&low));if(low<0xdc00u||low>0xdfffu)return ASN1_ERR_INVALID_ARGUMENT;cp=0x10000u+((cp-0xd800u)<<10)+(low-0xdc00u);}
          size_t encoded=cp<0x80u?1u:cp<0x800u?2u:cp<0x10000u?3u:4u;
          if(asn1_byte_writer_view(w).size>ASN1_MAX_STRING_BYTES-encoded)return ASN1_ERR_RESOURCE_LIMIT;
          TRY(codepoint(w,cp));continue;}
        default:return ASN1_ERR_INVALID_ARGUMENT;
      }
    }
    if(asn1_byte_writer_view(w).size>=ASN1_MAX_STRING_BYTES)return ASN1_ERR_RESOURCE_LIMIT;
    const uint8_t byte=(uint8_t)c;TRY(asn1_byte_writer_append(w,asn1_byte_view(&byte,1)));
  }return ASN1_ERR_TRUNCATED;
}
static asn1_status_t json_string(parser_t* p,char** text,size_t* size){
  asn1_byte_writer_t* w=NULL;TRY(asn1_byte_writer_create(&w));asn1_status_t s=json_string_body(p,w);
  asn1_byte_view_t bytes=asn1_byte_writer_view(w);size_t count=0;
  if(s==ASN1_OK)s=asn1_tlv_string_length(12,bytes,&count);
  if(s==ASN1_OK)s=copy_text(p,bytes.data,bytes.size,text);
  if(s==ASN1_OK)*size=bytes.size;asn1_byte_writer_destroy(w);return s;
}
static int duplicate(const asn1_text_node_t* list,const char* name,size_t n){
  for(;list;list=list->next)if(list->name_size==n&&(!n||!memcmp(list->name,name,n)))return 1;return 0;
}
static int decimal(unsigned c){return c>='0'&&c<='9';}
static asn1_status_t json_value(parser_t* p,size_t depth,asn1_text_node_t** out){
  if(depth>=ASN1_MAX_RECURSION_DEPTH)return ASN1_ERR_RESOURCE_LIMIT;
  skip(p);if(p->pos==p->input.size)return ASN1_ERR_TRUNCATED;
  unsigned c=p->input.data[p->pos];asn1_text_kind_t kind=c=='{'?ASN1_TEXT_OBJECT:c=='['?ASN1_TEXT_ARRAY:c=='"'?ASN1_TEXT_STRING:c=='t'||c=='f'?ASN1_TEXT_BOOL:c=='n'?ASN1_TEXT_NULL:ASN1_TEXT_NUMBER;
  TRY(node(p,kind,out));asn1_text_node_t* n=*out;
  if(kind==ASN1_TEXT_OBJECT||kind==ASN1_TEXT_ARRAY){
    ++p->pos;skip(p);unsigned end=kind==ASN1_TEXT_OBJECT?'}':']';if(take(p,end))return ASN1_OK;
    asn1_text_node_t** tail=&n->children;
    for(;;){char* name=NULL;size_t size=0;
      if(kind==ASN1_TEXT_OBJECT){TRY(json_string(p,&name,&size));skip(p);
        if(!take(p,':')||duplicate(n->children,name,size)){free(name);return ASN1_ERR_INVALID_ARGUMENT;}}
      asn1_status_t s=json_value(p,depth+1u,tail);
      if(*tail){(*tail)->name=name;(*tail)->name_size=size;}else free(name);
      if(s!=ASN1_OK)return s;tail=&(*tail)->next;skip(p);
      if(take(p,end))return ASN1_OK;if(!take(p,','))return ASN1_ERR_INVALID_ARGUMENT;skip(p);
    }
  }
  if(kind==ASN1_TEXT_STRING)return json_string(p,&n->text,&n->text_size);
  size_t start=p->pos;
  if(kind==ASN1_TEXT_NULL){if(!match(p,"null"))return ASN1_ERR_INVALID_ARGUMENT;}
  else if(kind==ASN1_TEXT_BOOL){if(!match(p,"true")&&!match(p,"false"))return ASN1_ERR_INVALID_ARGUMENT;}
  else{
    take(p,'-');if(!take(p,'0')){if(p->pos==p->input.size)return ASN1_ERR_TRUNCATED;if(p->input.data[p->pos]<'1'||p->input.data[p->pos]>'9')return ASN1_ERR_INVALID_ARGUMENT;while(p->pos<p->input.size&&decimal(p->input.data[p->pos]))++p->pos;}
    if(take(p,'.')){size_t first=p->pos;while(p->pos<p->input.size&&decimal(p->input.data[p->pos]))++p->pos;if(first==p->pos)return ASN1_ERR_INVALID_ARGUMENT;}
    if(take(p,'e')||take(p,'E')){if(!take(p,'+'))take(p,'-');size_t first=p->pos;while(p->pos<p->input.size&&decimal(p->input.data[p->pos]))++p->pos;if(first==p->pos)return ASN1_ERR_INVALID_ARGUMENT;}
  }
  n->text_size=p->pos-start;return copy_text(p,p->input.data+start,n->text_size,&n->text);
}
asn1_status_t asn1_json_parse(asn1_byte_view_t input,asn1_text_node_t** out,size_t* error_offset){
  if(!out||!error_offset||(input.size&&!input.data))return ASN1_ERR_INVALID_ARGUMENT;
  if(input.size>ASN1_MAX_PDU_ALLOCATION_BYTES)return ASN1_ERR_RESOURCE_LIMIT;
  parser_t p={input,0,0,0};asn1_text_node_t* result=NULL;
  asn1_status_t s=json_value(&p,0,&result);skip(&p);if(s==ASN1_OK&&p.pos!=input.size)s=ASN1_ERR_INVALID_ARGUMENT;
  if(s!=ASN1_OK){asn1_text_tree_free(result);*error_offset=p.pos;return s;}*out=result;*error_offset=p.pos;return ASN1_OK;
}
static asn1_status_t quote_body(asn1_byte_writer_t* out,asn1_byte_view_t text,int xml,int attribute){
  size_t count;TRY(asn1_tlv_string_length(12,text,&count));
  if(xml)for(size_t i=0;i+2<text.size;++i)if(text.data[i]==0xef&&text.data[i+1]==0xbf&&(text.data[i+2]==0xbe||text.data[i+2]==0xbf))return ASN1_ERR_INVALID_ARGUMENT;
  static const char digits[]="0123456789ABCDEF";
  if(!xml)TRY(asn1_byte_writer_append(out,asn1_byte_view((const uint8_t*)"\"",1)));
  for(size_t i=0;i<text.size;++i){const uint8_t c=text.data[i];const char* escape=NULL;
    if(xml){if(c=='&')escape="&amp;";else if(c=='<')escape="&lt;";else if(c=='>')escape="&gt;";else if(attribute&&c=='"')escape="&quot;";
      else if(c==13)escape="&#xD;";else if(attribute&&c==10)escape="&#xA;";else if(attribute&&c==9)escape="&#x9;";
      else if(c<32&&c!=9&&c!=10&&c!=13)return ASN1_ERR_INVALID_ARGUMENT;}
    else{if(c=='"')escape="\\\"";else if(c=='\\')escape="\\\\";}
    if(escape)TRY(asn1_byte_writer_append(out,asn1_byte_view((const uint8_t*)escape,strlen(escape))));
    else if(!xml&&c<32){uint8_t b[]={'\\','u','0','0',(uint8_t)digits[c>>4],(uint8_t)digits[c&15]};TRY(asn1_byte_writer_append(out,asn1_byte_view(b,6)));}
    else TRY(asn1_byte_writer_append(out,asn1_byte_view(&c,1)));
  }
  return xml?ASN1_OK:asn1_byte_writer_append(out,asn1_byte_view((const uint8_t*)"\"",1));
}
static asn1_status_t quote(asn1_byte_writer_t* out,asn1_byte_view_t text,int xml,int attr){
  if(!out||(text.size&&!text.data))return ASN1_ERR_INVALID_ARGUMENT;
  asn1_byte_writer_t* tmp=NULL;TRY(asn1_byte_writer_create(&tmp));asn1_status_t s=quote_body(tmp,text,xml,attr);
  if(s==ASN1_OK)s=asn1_byte_writer_append(out,asn1_byte_writer_view(tmp));asn1_byte_writer_destroy(tmp);return s;
}
asn1_status_t asn1_json_quote(asn1_byte_writer_t* out,asn1_byte_view_t text){return quote(out,text,0,0);}
asn1_status_t asn1_xml_escape(asn1_byte_writer_t* out,asn1_byte_view_t text,int attribute){return quote(out,text,1,attribute);}

static int xml_char(uint32_t cp){return cp==9||cp==10||cp==13||(cp>=32&&cp<=0xd7ffu)||(cp>=0xe000u&&cp<=0xfffdu)||(cp>=0x10000u&&cp<=0x10ffffu);}
static asn1_status_t xml_entity(parser_t* p,asn1_byte_writer_t* w){
  if(match(p,"amp;"))return codepoint(w,'&');if(match(p,"lt;"))return codepoint(w,'<');
  if(match(p,"gt;"))return codepoint(w,'>');if(match(p,"quot;"))return codepoint(w,'"');if(match(p,"apos;"))return codepoint(w,'\'');
  if(!take(p,'#'))return ASN1_ERR_INVALID_ARGUMENT;unsigned radix=take(p,'x')?16:10;uint32_t cp=0;size_t first=p->pos;
  while(p->pos<p->input.size&&p->input.data[p->pos]!=';'){
    int d=hex(p->input.data[p->pos++]);if(d<0||(unsigned)d>=radix||cp>(0x10ffffu-(unsigned)d)/radix)return ASN1_ERR_INVALID_ARGUMENT;cp=cp*radix+(unsigned)d;
  }
  if(first==p->pos||!take(p,';')||!xml_char(cp))return ASN1_ERR_INVALID_ARGUMENT;return codepoint(w,cp);
}
static asn1_status_t xml_text_body(parser_t* p,asn1_byte_writer_t* w,unsigned end,int attribute){
  while(p->pos<p->input.size&&p->input.data[p->pos]!=end){
    unsigned c=p->input.data[p->pos++];
    if(asn1_byte_writer_view(w).size>=ASN1_MAX_STRING_BYTES)return ASN1_ERR_RESOURCE_LIMIT;
    if(c=='&'){TRY(xml_entity(p,w));continue;}
    if(c=='<'||!xml_char(c))return ASN1_ERR_INVALID_ARGUMENT;
    if(c==']'&&p->input.size-p->pos>=2&&p->input.data[p->pos]==']'&&p->input.data[p->pos+1]=='>')return ASN1_ERR_INVALID_ARGUMENT;
    if(c==13){take(p,10);c=10;}
    if(attribute&&ws(c))c=' ';
    uint8_t b=(uint8_t)c;TRY(asn1_byte_writer_append(w,asn1_byte_view(&b,1)));
  }
  return ASN1_OK;
}
static int name_start(unsigned c){return c==':'||c=='_'||(c>='A'&&c<='Z')||(c>='a'&&c<='z')||c>=128;}
static int name_part(unsigned c){return name_start(c)||decimal(c)||c=='-'||c=='.';}
static int unicode_name_start(uint32_t c){return c==':'||c=='_'||(c>='A'&&c<='Z')||(c>='a'&&c<='z')||
  (c>=0xc0&&c<=0xd6)||(c>=0xd8&&c<=0xf6)||(c>=0xf8&&c<=0x2ff)||(c>=0x370&&c<=0x37d)||(c>=0x37f&&c<=0x1fff)||
  (c>=0x200c&&c<=0x200d)||(c>=0x2070&&c<=0x218f)||(c>=0x2c00&&c<=0x2fef)||(c>=0x3001&&c<=0xd7ff)||
  (c>=0xf900&&c<=0xfdcf)||(c>=0xfdf0&&c<=0xfffd)||(c>=0x10000&&c<=0xeffff);}
static uint32_t name_codepoint(const parser_t* p,size_t* bytes){
  unsigned c=p->input.data[p->pos];*bytes=c<128?1:c<224?2:c<240?3:4;uint32_t cp=c&(*bytes==1?127u:*bytes==2?31u:*bytes==3?15u:7u);
  for(size_t i=1;i<*bytes;++i)cp=(cp<<6)|(p->input.data[p->pos+i]&63u);return cp;
}
static asn1_status_t xml_name(parser_t* p,char** name,size_t* size){
  if(p->pos==p->input.size)return ASN1_ERR_TRUNCATED;
  size_t bytes=0,at=p->pos;uint32_t cp=name_codepoint(p,&bytes);
  if(!unicode_name_start(cp))return ASN1_ERR_INVALID_ARGUMENT;p->pos+=bytes;
  while(p->pos<p->input.size){cp=name_codepoint(p,&bytes);if(!unicode_name_start(cp)&&!decimal(cp)&&cp!='-'&&cp!='.'&&cp!=0xb7&&!(cp>=0x300&&cp<=0x36f)&&!(cp>=0x203f&&cp<=0x2040))break;p->pos+=bytes;}
  *size=p->pos-at;return copy_text(p,p->input.data+at,*size,name);
}
static asn1_status_t comment(parser_t* p){
  while(p->input.size-p->pos>=3){if(match(p,"-->"))return ASN1_OK;if(p->input.data[p->pos]=='-'&&p->input.data[p->pos+1]=='-')return ASN1_ERR_INVALID_ARGUMENT;++p->pos;}
  return ASN1_ERR_TRUNCATED;
}
static asn1_status_t xml_element(parser_t* p,size_t depth,asn1_text_node_t** out);
static asn1_status_t xml_contents(parser_t* p,size_t depth,asn1_text_node_t* n,asn1_byte_writer_t* text){
  asn1_text_node_t** tail=&n->children;
  for(;;){
    TRY(xml_text_body(p,text,'<',0));if(!take(p,'<'))return ASN1_ERR_TRUNCATED;
    if(take(p,'/')){
      char* close=NULL;size_t size=0;TRY(xml_name(p,&close,&size));int equal=size==n->name_size&&!memcmp(close,n->name,size);free(close);skip(p);
      return equal&&take(p,'>')?ASN1_OK:ASN1_ERR_INVALID_ARGUMENT;
    }
    if(match(p,"!--")){TRY(comment(p));continue;}
    if(match(p,"![CDATA[")){
      while(p->input.size-p->pos>=3&&!match(p,"]]>")){
        if(asn1_byte_writer_view(text).size>=ASN1_MAX_STRING_BYTES)return ASN1_ERR_RESOURCE_LIMIT;
        unsigned c=p->input.data[p->pos++];if(!xml_char(c))return ASN1_ERR_INVALID_ARGUMENT;
        if(c==13){take(p,10);c=10;}uint8_t b=(uint8_t)c;TRY(asn1_byte_writer_append(text,asn1_byte_view(&b,1)));
      }
      if(p->pos<3||memcmp(p->input.data+p->pos-3,"]]>",3))return ASN1_ERR_TRUNCATED;continue;
    }
    --p->pos;size_t offset=asn1_byte_writer_view(text).size;TRY(xml_element(p,depth+1u,tail));(*tail)->text_offset=offset;tail=&(*tail)->next;
  }
}
static asn1_status_t xml_element(parser_t* p,size_t depth,asn1_text_node_t** out){
  if(depth>=ASN1_MAX_RECURSION_DEPTH)return ASN1_ERR_RESOURCE_LIMIT;
  if(!take(p,'<'))return ASN1_ERR_INVALID_ARGUMENT;
  TRY(node(p,ASN1_TEXT_ELEMENT,out));asn1_text_node_t* n=*out;TRY(xml_name(p,&n->name,&n->name_size));
  asn1_text_node_t** tail=&n->attributes;
  for(;;){
    size_t at=p->pos;skip(p);if(match(p,"/>"))return ASN1_OK;if(take(p,'>'))break;
    if(p->pos==at)return ASN1_ERR_INVALID_ARGUMENT;
    TRY(node(p,ASN1_TEXT_STRING,tail));asn1_text_node_t* a=*tail;TRY(xml_name(p,&a->name,&a->name_size));
    for(asn1_text_node_t* prev=n->attributes;prev!=a;prev=prev->next)if(prev->name_size==a->name_size&&!memcmp(prev->name,a->name,a->name_size))return ASN1_ERR_INVALID_ARGUMENT;
    skip(p);if(!take(p,'='))return ASN1_ERR_INVALID_ARGUMENT;skip(p);
    unsigned q=take(p,'"')?'"':take(p,'\'')?'\'':0;if(!q)return ASN1_ERR_INVALID_ARGUMENT;
    asn1_byte_writer_t* w=NULL;TRY(asn1_byte_writer_create(&w));asn1_status_t s=xml_text_body(p,w,q,1);
    if(s==ASN1_OK&&!take(p,q))s=ASN1_ERR_TRUNCATED;asn1_byte_view_t bytes=asn1_byte_writer_view(w);
    if(s==ASN1_OK)s=copy_text(p,bytes.data,bytes.size,&a->text);if(s==ASN1_OK)a->text_size=bytes.size;asn1_byte_writer_destroy(w);if(s!=ASN1_OK)return s;
    tail=&a->next;
  }
  asn1_byte_writer_t* w=NULL;TRY(asn1_byte_writer_create(&w));asn1_status_t s=xml_contents(p,depth,n,w);
  asn1_byte_view_t bytes=asn1_byte_writer_view(w);if(s==ASN1_OK)s=copy_text(p,bytes.data,bytes.size,&n->text);if(s==ASN1_OK)n->text_size=bytes.size;asn1_byte_writer_destroy(w);return s;
}
static asn1_status_t declaration(parser_t* p){
  unsigned fields=0;
  for(;;){size_t before=p->pos;skip(p);if(match(p,"?>"))return (fields&1u)?ASN1_OK:ASN1_ERR_INVALID_ARGUMENT;
    if(p->pos==before)return ASN1_ERR_INVALID_ARGUMENT;
    char key[16],value[16];size_t k=0,n=0;
    while(p->pos<p->input.size&&name_part(p->input.data[p->pos])){if(k+1>=sizeof(key))return ASN1_ERR_INVALID_ARGUMENT;key[k++]=(char)p->input.data[p->pos++];}key[k]=0;
    skip(p);if(!take(p,'='))return ASN1_ERR_INVALID_ARGUMENT;skip(p);unsigned q=take(p,'"')?'"':take(p,'\'')?'\'':0;if(!q)return ASN1_ERR_INVALID_ARGUMENT;
    while(p->pos<p->input.size&&p->input.data[p->pos]!=q){if(n+1>=sizeof(value))return ASN1_ERR_INVALID_ARGUMENT;value[n++]=(char)p->input.data[p->pos++];}value[n]=0;
    if(!take(p,q))return ASN1_ERR_TRUNCATED;
    if(!strcmp(key,"version")){if(fields||strcmp(value,"1.0"))return ASN1_ERR_UNSUPPORTED;fields|=1u;}
    else if(!strcmp(key,"encoding")){if(!(fields&1u)||(fields&6u))return ASN1_ERR_INVALID_ARGUMENT;for(size_t i=0;i<n;++i)if(value[i]>='a'&&value[i]<='z')value[i]=(char)(value[i]-'a'+'A');if(strcmp(value,"UTF-8"))return ASN1_ERR_UNSUPPORTED;fields|=2u;}
    else if(!strcmp(key,"standalone")){if(!(fields&1u)||(fields&4u)||(strcmp(value,"yes")&&strcmp(value,"no")))return ASN1_ERR_INVALID_ARGUMENT;fields|=4u;}
    else return ASN1_ERR_INVALID_ARGUMENT;
  }
}
asn1_status_t asn1_xml_parse(asn1_byte_view_t input,asn1_text_node_t** out,size_t* error_offset){
  if(!out||!error_offset||(input.size&&!input.data))return ASN1_ERR_INVALID_ARGUMENT;
  if(input.size>ASN1_MAX_PDU_ALLOCATION_BYTES)return ASN1_ERR_RESOURCE_LIMIT;
  size_t characters;asn1_status_t s=asn1_tlv_string_length(12,input,&characters);if(s!=ASN1_OK){*error_offset=0;return s;}
  for(size_t i=0;i<input.size;){uint32_t cp=input.data[i++];if(cp>=128){unsigned need=cp<0xe0?1u:cp<0xf0?2u:3u;cp&=(1u<<(6u-need))-1u;while(need--)cp=(cp<<6)|(input.data[i++]&63u);}if(!xml_char(cp)){*error_offset=i;return ASN1_ERR_INVALID_ARGUMENT;}}
  parser_t p={input,0,0,0};asn1_text_node_t* result=NULL;
  if(input.size>=3&&!memcmp(input.data,"\xef\xbb\xbf",3))p.pos=3;
  /* Declaration is optional. Its supported encoding is UTF-8 only. */
  if(match(&p,"<?xml"))s=declaration(&p);
  skip(&p);while(s==ASN1_OK&&match(&p,"<!--")){s=comment(&p);skip(&p);}
  if(s==ASN1_OK)s=xml_element(&p,0,&result);
  skip(&p);while(s==ASN1_OK&&match(&p,"<!--")){s=comment(&p);skip(&p);}
  if(s==ASN1_OK&&p.pos!=input.size)s=ASN1_ERR_INVALID_ARGUMENT;
  if(s!=ASN1_OK){asn1_text_tree_free(result);*error_offset=p.pos;return s;}*out=result;*error_offset=p.pos;return ASN1_OK;
}
