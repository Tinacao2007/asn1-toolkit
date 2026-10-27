/* 仅测试：消费完整模式的 C11 接口；S1AP 原始开放内容逐层独立校验。 */
#include "generated.h"
#include <asn1/c_runtime/text_tree.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
static void hex(const uint8_t* data,size_t n){for(size_t i=0;i<n;++i)printf("%02x",data[i]);}
static int nibble(char c){return c>='0'&&c<='9'?c-'0':c>='A'&&c<='F'?c-'A'+10:c>='a'&&c<='f'?c-'a'+10:-1;}
static asn1_status_t unhex(const char* text,uint8_t** out,size_t* size){
  size_t n=strlen(text);if(n%2||n>131072)return ASN1_ERR_INVALID_ARGUMENT;
  uint8_t* data=(uint8_t*)malloc(n/2+1);if(!data)return ASN1_ERR_OUT_OF_MEMORY;
  for(size_t i=0;i<n;i+=2){int a=nibble(text[i]),b=nibble(text[i+1]);if(a<0||b<0){free(data);return ASN1_ERR_INVALID_ARGUMENT;}data[i/2]=(uint8_t)((a<<4)|b);}
  *out=data;*size=n/2;return ASN1_OK;
}
#define JSON_LOAD(T) static asn1_status_t load_##T(const char* text,T* v){ \
 asn1_byte_reader_t* r=NULL;asn1_status_t s=asn1_byte_reader_create(&r,asn1_byte_view((const uint8_t*)text,strlen(text))); \
 if(!s)s=T##_decode_jer(r,v);asn1_byte_reader_destroy(r);return s; }
#define JSON_DUMP(T) static asn1_status_t dump_##T(const T* v){ \
 asn1_byte_writer_t* w=NULL;asn1_status_t s=asn1_byte_writer_create(&w);if(!s)s=T##_encode_jer(w,v); \
 if(!s){asn1_byte_view_t b=asn1_byte_writer_view(w);fwrite(b.data,1,b.size,stdout);puts("");}asn1_byte_writer_destroy(w);return s; }
#if defined(PROTOCOL_S1AP)
#define S(n) proto_mS1AP_x2dPDU_x2dDescriptions_t##n
#define B(n) proto_mS1AP_x2dPDU_x2dContents_t##n
static const asn1_text_node_t* member(const asn1_text_node_t* n,const char* key){
 for(const asn1_text_node_t* c=n?n->children:NULL;c;c=c->next)if(c->name&&!strcmp(c->name,key))return c;return NULL;
}
static int criticality(const asn1_text_node_t* n){if(!n||n->kind!=ASN1_TEXT_STRING)return -1;return !strcmp(n->text,"reject")?0:!strcmp(n->text,"ignore")?1:!strcmp(n->text,"notify")?2:-1;}
static const char* critical_name(int64_t n){return n==0?"reject":n==1?"ignore":"notify";}
static asn1_status_t raw(const asn1_text_node_t* n,uint8_t** data,size_t* count,size_t* capacity){
 if(!n||n->kind!=ASN1_TEXT_STRING)return ASN1_ERR_INVALID_ARGUMENT;asn1_status_t s=unhex(n->text,data,count);if(!s)*capacity=*count;return s;
}
static asn1_status_t load_proto_mS1AP_x2dPDU_x2dDescriptions_tS1AP_x2dPDU(const char* text,S(S1AP_x2dPDU)* v){
 asn1_text_node_t* tree=NULL;size_t at=0;asn1_status_t s=asn1_json_parse(asn1_byte_view((const uint8_t*)text,strlen(text)),&tree,&at);if(s)return s;
 const asn1_text_node_t* m=member(tree,"initiatingMessage");v->kind=1;if(!m){m=member(tree,"successfulOutcome");v->kind=2;}
 const asn1_text_node_t* code=member(m,"procedureCode");int crit=criticality(member(m,"criticality"));
 if(!code||code->kind!=ASN1_TEXT_NUMBER||crit<0){s=ASN1_ERR_INVALID_ARGUMENT;goto done;}
#define MESSAGE(x) do{(x).fprocedureCode.value.value=strtoll(code->text,NULL,10);(x).fcriticality.value.value=crit; \
 s=raw(member(m,"value"),&(x).fvalue.data,&(x).fvalue.count,&(x).fvalue.capacity);}while(0)
 if(v->kind==1)MESSAGE(v->value.finitiatingMessage);else MESSAGE(v->value.fsuccessfulOutcome);
#undef MESSAGE
 done:asn1_text_tree_free(tree);return s;
}
static asn1_status_t dump_proto_mS1AP_x2dPDU_x2dDescriptions_tS1AP_x2dPDU(const S(S1AP_x2dPDU)* v){
#define PRINT(x,name) do{printf("{\"%s\":{\"procedureCode\":%" PRId64 ",\"criticality\":\"%s\",\"value\":\"",name,(x).fprocedureCode.value.value,critical_name((x).fcriticality.value.value));hex((x).fvalue.data,(x).fvalue.count);puts("\"}}");}while(0)
 if(v->kind==1)PRINT(v->value.finitiatingMessage,"initiatingMessage");else if(v->kind==2)PRINT(v->value.fsuccessfulOutcome,"successfulOutcome");else return ASN1_ERR_INVALID_ARGUMENT;
#undef PRINT
 return ASN1_OK;
}
static asn1_status_t load_proto_mS1AP_x2dPDU_x2dContents_tErrorIndication(const char* text,B(ErrorIndication)* v){
 asn1_text_node_t* tree=NULL;size_t at=0;asn1_status_t s=asn1_json_parse(asn1_byte_view((const uint8_t*)text,strlen(text)),&tree,&at);if(s)return s;
 const asn1_text_node_t* list=member(tree,"protocolIEs");if(!list||list->kind!=ASN1_TEXT_ARRAY){s=ASN1_ERR_INVALID_ARGUMENT;goto done;}
 size_t count=0;for(const asn1_text_node_t* n=list->children;n;n=n->next)++count;if(count>16){s=ASN1_ERR_RESOURCE_LIMIT;goto done;}
 v->fprotocolIEs.data=calloc(count?count:1,sizeof(*v->fprotocolIEs.data));if(!v->fprotocolIEs.data){s=ASN1_ERR_OUT_OF_MEMORY;goto done;}v->fprotocolIEs.capacity=count;
 for(const asn1_text_node_t* n=list->children;n;n=n->next){size_t i=v->fprotocolIEs.count++;const asn1_text_node_t* id=member(n,"id");int c=criticality(member(n,"criticality"));
  if(!id||id->kind!=ASN1_TEXT_NUMBER||c<0){s=ASN1_ERR_INVALID_ARGUMENT;break;}
  v->fprotocolIEs.data[i].fid.value.value=strtoll(id->text,NULL,10);v->fprotocolIEs.data[i].fcriticality.value.value=c;
  s=raw(member(n,"value"),&v->fprotocolIEs.data[i].fvalue.data,&v->fprotocolIEs.data[i].fvalue.count,&v->fprotocolIEs.data[i].fvalue.capacity);if(s)break;
 }
 done:asn1_text_tree_free(tree);return s;
}
static asn1_status_t dump_proto_mS1AP_x2dPDU_x2dContents_tErrorIndication(const B(ErrorIndication)* v){
 printf("{\"protocolIEs\":[");for(size_t i=0;i<v->fprotocolIEs.count;++i){if(i)putchar(',');printf("{\"id\":%" PRId64 ",\"criticality\":\"%s\",\"value\":\"",v->fprotocolIEs.data[i].fid.value.value,critical_name(v->fprotocolIEs.data[i].fcriticality.value.value));hex(v->fprotocolIEs.data[i].fvalue.data,v->fprotocolIEs.data[i].fvalue.count);printf("\"}");}puts("]}");return ASN1_OK;
}
#endif
#define BIT(T,rule) if(!strcmp(codec,#rule)){ \
 if(!encoding){s=asn1_bit_reader_create(&pr,asn1_byte_view(bytes,n));if(s)goto done;s=T##_decode_##rule(pr,&v);if(s)goto done;if(asn1_bit_reader_bit_remaining(pr)>7){s=ASN1_ERR_INVALID_ARGUMENT;goto done;}s=dump_##T(&v);if(s)goto done;} \
 s=asn1_bit_writer_create(&pw);if(!s)s=T##_encode_##rule(pw,&v);if(s)goto done;s=asn1_bit_writer_take(pw,&owned,&size);if(!s){hex(owned,size);puts("");}goto done; }
#define DRIVER(T) static int run_##T(const char* codec,const char* mode,const char* input){ \
 T v;T##_init(&v);asn1_status_t s=ASN1_OK;int encoding=!strcmp(mode,"encode");uint8_t* bytes=NULL;size_t n=0;uint8_t* owned=NULL;size_t size=0; \
 asn1_bit_reader_t* pr=NULL;asn1_bit_writer_t* pw=NULL;asn1_byte_reader_t* br=NULL;asn1_byte_writer_t* bw=NULL; \
 if(encoding)s=load_##T(input,&v);else s=unhex(input,&bytes,&n);if(s)goto done;BIT(T,uper)BIT(T,aper) \
 if(!strcmp(codec,"ber")){if(!encoding){s=asn1_byte_reader_create(&br,asn1_byte_view(bytes,n));if(!s)s=T##_decode_ber(br,&v);if(s)goto done;if(asn1_byte_reader_remaining(br)){s=ASN1_ERR_INVALID_ARGUMENT;goto done;}s=dump_##T(&v);if(s)goto done;} \
 s=asn1_byte_writer_create(&bw);if(!s)s=T##_encode_ber(bw,&v);if(!s){asn1_byte_view_t b=asn1_byte_writer_view(bw);hex(b.data,b.size);puts("");}goto done;}s=ASN1_ERR_INVALID_ARGUMENT; \
 done:if(s)fprintf(stderr,"%s: %s\n",#T,asn1_status_name(s));T##_free(&v);free(bytes);free(owned);asn1_bit_reader_destroy(pr);asn1_bit_writer_destroy(pw);asn1_byte_reader_destroy(br);asn1_byte_writer_destroy(bw);return s?1:0; }
#define NORMAL(T) JSON_LOAD(T) JSON_DUMP(T) DRIVER(T)
#if defined(PROTOCOL_LPP)
NORMAL(proto_mLPP_x2dPDU_x2dDefinitions_tLPP_x2dMessage)
#elif defined(PROTOCOL_RRC)
NORMAL(proto_mEUTRA_x2dRRC_x2dDefinitions_tBCCH_x2dBCH_x2dMessage)
NORMAL(proto_mEUTRA_x2dRRC_x2dDefinitions_tUL_x2dCCCH_x2dMessage)
#elif defined(PROTOCOL_S1AP)
DRIVER(proto_mS1AP_x2dPDU_x2dDescriptions_tS1AP_x2dPDU)
DRIVER(proto_mS1AP_x2dPDU_x2dContents_tErrorIndication)
NORMAL(proto_mS1AP_x2dPDU_x2dContents_tMMEConfigurationUpdateAcknowledge)
NORMAL(proto_mS1AP_x2dIEs_tMME_x2dUE_x2dS1AP_x2dID)
NORMAL(proto_mS1AP_x2dIEs_tENB_x2dUE_x2dS1AP_x2dID)
NORMAL(proto_mS1AP_x2dIEs_tCause)
#endif
int main(int argc,char** argv){
 if(argc!=5||(strcmp(argv[3],"encode")&&strcmp(argv[3],"decode")))return 2;
#define TYPE(name,T) if(!strcmp(argv[1],name))return run_##T(argv[2],argv[3],argv[4]);
#if defined(PROTOCOL_LPP)
 TYPE("LPP-Message",proto_mLPP_x2dPDU_x2dDefinitions_tLPP_x2dMessage)
#elif defined(PROTOCOL_RRC)
 TYPE("BCCH-BCH-Message",proto_mEUTRA_x2dRRC_x2dDefinitions_tBCCH_x2dBCH_x2dMessage)
 TYPE("UL-CCCH-Message",proto_mEUTRA_x2dRRC_x2dDefinitions_tUL_x2dCCCH_x2dMessage)
#elif defined(PROTOCOL_S1AP)
 TYPE("S1AP-PDU",proto_mS1AP_x2dPDU_x2dDescriptions_tS1AP_x2dPDU)
 TYPE("ErrorIndication",proto_mS1AP_x2dPDU_x2dContents_tErrorIndication)
 TYPE("MMEConfigurationUpdateAcknowledge",proto_mS1AP_x2dPDU_x2dContents_tMMEConfigurationUpdateAcknowledge)
 TYPE("MME-UE-S1AP-ID",proto_mS1AP_x2dIEs_tMME_x2dUE_x2dS1AP_x2dID)
 TYPE("ENB-UE-S1AP-ID",proto_mS1AP_x2dIEs_tENB_x2dUE_x2dS1AP_x2dID)
 TYPE("Cause",proto_mS1AP_x2dIEs_tCause)
#endif
 return 2;
}
