#include <asn1/c_runtime/text_tree.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"text test line %d\n",__LINE__);return 1;}}while(0)
static asn1_byte_view_t input(const char* s){return asn1_byte_view((const uint8_t*)s,strlen(s));}
int main(void){
  asn1_text_node_t* n=NULL;size_t error=0;
  CHECK(asn1_json_parse(input("{\"n\":340282366920938463463374607431768211456,\"s\":\"\\uD83D\\uDE00\\u0000\",\"v\":[true,null,-1.25e+2]}"),&n,&error)==ASN1_OK);
  CHECK(n->children->kind==ASN1_TEXT_NUMBER&&!strcmp(n->children->text,"340282366920938463463374607431768211456"));
  CHECK(n->children->next->text_size==5&&n->children->next->text[4]==0);
  asn1_text_tree_free(n);n=NULL;
  const char* bad[]={"{\"a\":1,\"a\":2}","[01]","[1,]","{\"x\":true false}","\"\\ud800\"","\"\\uDC00\"","1e","--1","{]","\"\xc0\x80\""};
  for(size_t i=0;i<sizeof(bad)/sizeof(*bad);++i){n=(asn1_text_node_t*)1;CHECK(asn1_json_parse(input(bad[i]),&n,&error)!=ASN1_OK&&n==(asn1_text_node_t*)1);}
  const char* good="<?xml version='1.0' encoding='UTF-8'?><!--pre--><Pdu a='a&amp;b&#10;c'><x><![CDATA[<&]]></x><y/>&#x1F600;</Pdu>";n=NULL;
  CHECK(asn1_xml_parse(input(good),&n,&error)==ASN1_OK);CHECK(!strcmp(n->name,"Pdu")&&n->attributes->text_size==5&&!memcmp(n->attributes->text,"a&b\nc",5));
  CHECK(!strcmp(n->children->text,"<&")&&n->text_size==4);asn1_text_tree_free(n);n=NULL;
  const char* bad_xml[]={"<x a='1' a='2'/>","<x><y/></z>","<x>&evil;</x>","<!DOCTYPE x><x/>","<x>&#0;</x>","<x>]]></x>","<x/ a='b'>","<?xml encoding='UTF-8'?><x/>","<?xml version='1.0' encoding='UTF-16'?><x/>","<x/><y/>","<x a='<'/>","<x>\xef\xbf\xbe</x>"};
  for(size_t i=0;i<sizeof(bad_xml)/sizeof(*bad_xml);++i){n=(asn1_text_node_t*)1;CHECK(asn1_xml_parse(input(bad_xml[i]),&n,&error)!=ASN1_OK&&n==(asn1_text_node_t*)1);}
  const char* documents[]={"{\"x\":[\"a\\n\",42]}","<x a='b'><y>c</y></x>"};
  for(size_t d=0;d<2;++d)for(size_t i=0;i<strlen(documents[d]);++i){n=NULL;asn1_status_t s=d?asn1_xml_parse(asn1_byte_view((const uint8_t*)documents[d],i),&n,&error):asn1_json_parse(asn1_byte_view((const uint8_t*)documents[d],i),&n,&error);CHECK(s!=ASN1_OK&&n==NULL);}
  asn1_byte_writer_t* w=NULL;CHECK(asn1_byte_writer_create(&w)==ASN1_OK);
  const uint8_t content[]={0,'"','\\',10};CHECK(asn1_json_quote(w,asn1_byte_view(content,sizeof(content)))==ASN1_OK);
  CHECK(asn1_json_parse(asn1_byte_writer_view(w),&n,&error)==ASN1_OK&&n->text_size==sizeof(content)&&!memcmp(n->text,content,sizeof(content)));
  asn1_text_tree_free(n);asn1_byte_writer_destroy(w);puts("C JSON/XML lossless syntax, escaping, negatives and truncations PASS");return 0;
}
