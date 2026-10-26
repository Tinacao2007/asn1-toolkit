/* Independent oracle harness, compiled only against asn1c-generated sources. */
#include "RRCConnectionRequest.h"
#include "per_encoder.h"
#include "per_decoder.h"
#include "der_encoder.h"
#include "ber_decoder.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void dump(const char* codec,const unsigned char* data,size_t n){printf("%s ",codec);for(size_t i=0;i<n;++i)printf("%02x",data[i]);puts("");}
static int valid(const RRCConnectionRequest_t* v){
  const RRCConnectionRequest_r8_IEs_t* ie=&v->criticalExtensions.choice.rrcConnectionRequest_r8;
  const unsigned char expected[]={1,2,3,4,5};
  return v->criticalExtensions.present==1 && ie->ue_Identity.present==2 && ie->ue_Identity.choice.randomValue.size==5 &&
    !memcmp(ie->ue_Identity.choice.randomValue.buf,expected,5) && ie->establishmentCause==3 && ie->spare.size==1 && ie->spare.bits_unused==7 && ie->spare.buf[0]==0;
}
int main(int argc,char** argv){
  if(argc!=3)return 1;RRCConnectionRequest_t v;memset(&v,0,sizeof(v));v.criticalExtensions.present=1;
  RRCConnectionRequest_r8_IEs_t* ie=&v.criticalExtensions.choice.rrcConnectionRequest_r8;ie->ue_Identity.present=2;ie->establishmentCause=3;
  ie->ue_Identity.choice.randomValue.buf=calloc(5,1);ie->ue_Identity.choice.randomValue.size=5;
  for(size_t i=0;i<5;++i)ie->ue_Identity.choice.randomValue.buf[i]=(unsigned char)(i+1);
  ie->spare.buf=calloc(1,1);ie->spare.size=1;ie->spare.bits_unused=7;
  unsigned char wire[1024];asn_enc_rval_t e=uper_encode_to_buffer(&asn_DEF_RRCConnectionRequest,NULL,&v,wire,sizeof(wire));if(e.encoded<0)return 2;dump("uper",wire,((size_t)e.encoded+7u)/8u);
  e=der_encode_to_buffer(&asn_DEF_RRCConnectionRequest,&v,wire,sizeof(wire));if(e.encoded<0)return 3;dump("ber",wire,(size_t)e.encoded);
  for(int i=1;i<3;++i){FILE* f=fopen(argv[i],"rb");if(!f)return 4;size_t n=fread(wire,1,sizeof(wire),f);fclose(f);RRCConnectionRequest_t* decoded=NULL;
    asn_dec_rval_t d=i==1?uper_decode_complete(NULL,&asn_DEF_RRCConnectionRequest,(void**)&decoded,wire,n):ber_decode(NULL,&asn_DEF_RRCConnectionRequest,(void**)&decoded,wire,n);
    if(d.code!=RC_OK || !decoded || !valid(decoded))return 5;ASN_STRUCT_FREE(asn_DEF_RRCConnectionRequest,decoded);
  }
  ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_RRCConnectionRequest,&v);return 0;
}
