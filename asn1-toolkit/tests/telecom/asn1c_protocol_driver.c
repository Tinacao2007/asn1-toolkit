/* 仅与独立 asn1c 生成源链接；输入 XER 构造值，输出编码或解码后的 XER。 */
#include <asn_application.h>
#include <constraints.h>
#include <per_encoder.h>
#include <per_decoder.h>
#include <der_encoder.h>
#include <ber_decoder.h>
#include <xer_encoder.h>
#include <xer_decoder.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(ORACLE_LPP)
#include "LPP-Message.h"
#else
#include "BCCH-BCH-Message.h"
#include "UL-CCCH-Message.h"
#endif
static int output(const void* data,size_t size,void* stream){
 return fwrite(data,1,size,(FILE*)stream)==size?0:-1;
}
int main(int argc,char** argv){
 if(argc!=5)return 2;
 asn_TYPE_descriptor_t* type=NULL;
#if defined(ORACLE_LPP)
 if(!strcmp(argv[1],"LPP-Message"))type=&asn_DEF_LPP_Message;
#else
 if(!strcmp(argv[1],"BCCH-BCH-Message"))type=&asn_DEF_BCCH_BCH_Message;
 if(!strcmp(argv[1],"UL-CCCH-Message"))type=&asn_DEF_UL_CCCH_Message;
#endif
 if(!type)return 2;FILE* f=fopen(argv[4],"rb");if(!f)return 2;
 unsigned char data[65536],wire[65536];size_t n=fread(data,1,sizeof(data),f);int extra=fgetc(f);fclose(f);if(extra!=EOF)return 2;
 void* value=NULL;asn_dec_rval_t result;
 if(!strcmp(argv[3],"encode"))result=xer_decode(NULL,type,&value,data,n);
 else if(!strcmp(argv[2],"uper"))result=uper_decode_complete(NULL,type,&value,data,n);
 else result=ber_decode(NULL,type,&value,data,n);
 if(result.code!=RC_OK||!value){if(value)ASN_STRUCT_FREE(*type,value);return 1;}
 char error[512];size_t error_size=sizeof(error);
 if(asn_check_constraints(type,value,error,&error_size)){fprintf(stderr,"%s\n",error);ASN_STRUCT_FREE(*type,value);return 1;}
 int status=0;
 if(!strcmp(argv[3],"encode")){
  asn_enc_rval_t encoded=!strcmp(argv[2],"uper")?uper_encode_to_buffer(type,NULL,value,wire,sizeof(wire)):der_encode_to_buffer(type,value,wire,sizeof(wire));
  if(encoded.encoded<0)status=1;else{size_t size=!strcmp(argv[2],"uper")?((size_t)encoded.encoded+7u)/8u:(size_t)encoded.encoded;
   for(size_t i=0;i<size;++i)printf("%02x",wire[i]);puts("");}
 }else {
  /* Canonical XER avoids pretty-printer whitespace inside BIT STRING text;
   * no ASN.1 wire bytes or decoded abstract values are normalized here. */
  asn_enc_rval_t encoded=xer_encode(type,value,XER_F_CANONICAL,output,stdout);
  if(encoded.encoded<0)status=1;
 }
 ASN_STRUCT_FREE(*type,value);return status;
}
