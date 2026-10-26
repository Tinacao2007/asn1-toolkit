/* X.690 value contents shared by PER and BER-derived policies.
 * Kept separate so BER-only linking does not pull the PER bitstream core. */
#include <asn1/c_runtime/value_content.h>
#include <asn1/c_runtime/limits.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include "alloc_internal.h"
asn1_status_t asn1_value_encode_real(double value,uint8_t** out,size_t* size) {
  if(!out||!size) return ASN1_ERR_INVALID_ARGUMENT;
  uint8_t content[11]; size_t n=0;
  if(isnan(value)) content[n++]=0x42u;
  else if(isinf(value)) content[n++]=signbit(value)?0x41u:0x40u;
  else if(value==0.0) { if(signbit(value))content[n++]=0x43u; }
  else {
    if(sizeof(double)!=sizeof(uint64_t)) return ASN1_ERR_UNSUPPORTED;
    uint64_t bits=0; memcpy(&bits,&value,sizeof(bits));
    const int negative=(int)(bits>>63u); const unsigned biased=(unsigned)((bits>>52u)&0x7ffu);
    uint64_t mant=bits&((UINT64_C(1)<<52u)-1u); int exponent;
    if(biased==0u) exponent=-1074; else {mant|=UINT64_C(1)<<52u; exponent=(int)biased-1023-52;}
    while((mant&1u)==0u){mant>>=1u;++exponent;}
    size_t en=exponent>=-128&&exponent<=127?1u:2u;
    content[n++]=(uint8_t)(0x80u|(negative?0x40u:0u)|(en==1u?0u:1u));
    if(en==1u) content[n++]=(uint8_t)exponent;
    else {content[n++]=(uint8_t)((uint16_t)exponent>>8u);content[n++]=(uint8_t)exponent;}
    uint8_t m[8];size_t mn=0;do{m[mn++]=(uint8_t)mant;mant>>=8u;}while(mant);
    while(mn)content[n++]=m[--mn];
  }
  uint8_t* bytes=NULL;if(n){bytes=(uint8_t*)malloc(n);if(!bytes)return ASN1_ERR_OUT_OF_MEMORY;memcpy(bytes,content,n);}
  *out=bytes;*size=n;return ASN1_OK;
}

static asn1_status_t parse_decimal_real(const uint8_t* p,size_t n,double* out) {
  if(n<2u||p[0]<1u||p[0]>3u)return ASN1_ERR_INVALID_ARGUMENT;
  const unsigned form=p[0];size_t i=1u;int negative=0;
  if(i<n&&(p[i]=='+'||p[i]=='-'))negative=p[i++]=='-';
  long double significand=0.0L;size_t digits=0,fraction=0;int decimal=0,has_exp=0,exp_negative=0,explicit_exp=0;
  while(i<n&&p[i]!='E'&&p[i]!='e'){
    const uint8_t c=p[i++];
    if(c>='0'&&c<='9'){if(digits>=20u)return ASN1_ERR_UNSUPPORTED;significand=significand*10.0L+(long double)(c-'0');++digits;if(decimal)++fraction;continue;}
    if((c=='.'||c==',')&&!decimal){decimal=1;continue;}
    return ASN1_ERR_INVALID_ARGUMENT;
  }
  if(i<n){if(form!=3u||!digits)return ASN1_ERR_INVALID_ARGUMENT;has_exp=1;++i;if(i<n&&(p[i]=='+'||p[i]=='-'))exp_negative=p[i++]=='-';if(i==n)return ASN1_ERR_INVALID_ARGUMENT;
    for(;i<n;++i){if(p[i]<'0'||p[i]>'9')return ASN1_ERR_INVALID_ARGUMENT;if(explicit_exp>1000)return ASN1_ERR_UNSUPPORTED;explicit_exp=explicit_exp*10+(int)(p[i]-'0');}}
  if(!digits||(form==1u&&(decimal||has_exp))||(form==2u&&(!decimal||has_exp))||(form==3u&&!has_exp))return ASN1_ERR_INVALID_ARGUMENT;
  int power=(exp_negative?-explicit_exp:explicit_exp)-(int)fraction;
  if(power < -5000 || power > 5000)return ASN1_ERR_UNSUPPORTED;
  long double magnitude=significand*powl(10.0L,(long double)power);
  if(!isfinite(magnitude)||magnitude>(long double)DBL_MAX)return ASN1_ERR_UNSUPPORTED;
  double result=(double)magnitude;
  if(significand!=0.0L&&result==0.0)return ASN1_ERR_UNSUPPORTED;
  *out=negative?-result:result;return ASN1_OK;
}

asn1_status_t asn1_value_decode_real(asn1_byte_view_t input,double* out) {
  if(!out || (input.size&&!input.data)) return ASN1_ERR_INVALID_ARGUMENT;
  const uint8_t* data=input.data;const size_t n=input.size;asn1_status_t s=ASN1_OK;
  double value=0.0;
  if(n==0) value=0.0;
  else if(n==1&&data[0]>=0x40u&&data[0]<=0x43u){if(data[0]==0x40u)value=INFINITY;else if(data[0]==0x41u)value=-INFINITY;else if(data[0]==0x42u)value=NAN;else value=-0.0;}
  else if((data[0]&0x80u)==0) s=parse_decimal_real(data,n,&value);
  else {
    const uint8_t control=data[0]; const unsigned base=(control>>4u)&3u,scale=(control>>2u)&3u,ee=control&3u;
    if(base!=0u||ee==3u) s=ASN1_ERR_UNSUPPORTED;
    else {
      const size_t en=ee+1u;
      if(n<=en) s=ASN1_ERR_INVALID_ARGUMENT;
      else if(n-en>8u) s=ASN1_ERR_UNSUPPORTED;
      else {
        int32_t exponent=0;
        for(size_t i=0;i<en;++i) exponent=(int32_t)(((uint32_t)exponent<<8u)|data[1u+i]);
        if(en<4u&&(data[1] & 0x80u)) exponent|=(int32_t)(UINT32_MAX<<(en*8u));
        uint64_t mant=0;for(size_t i=1u+en;i<n;++i)mant=(mant<<8u)|data[i];
        if(!mant) s=ASN1_ERR_INVALID_ARGUMENT;
        else {
          unsigned trailing=0;while((mant&1u)==0u){mant>>=1u;++trailing;}
          if(mant>((UINT64_C(1)<<53u)-1u)) s=ASN1_ERR_UNSUPPORTED;
          else { const int power=exponent+(int)scale+(int)trailing; value=ldexp((double)mant,power);
            if(!isfinite(value)||value==0.0||ldexp(value,-power)!=(double)mant)s=ASN1_ERR_UNSUPPORTED;
            else if(control&0x40u)value=-value;
          }
        }
      }
    }
  }
  if(s==ASN1_OK)*out=value;return s;
}

static size_t oid_subidentifier_octets(uint64_t value) {
  size_t n = 1;
  while (value >= 128u) { value >>= 7u; ++n; }
  return n;
}

static void oid_write_subidentifier(uint8_t* out, size_t* position, uint64_t value) {
  uint8_t groups[10]; size_t n=0;
  do { groups[n++]=(uint8_t)(value&0x7fu); value>>=7u; } while(value);
  while(n) { --n; out[(*position)++]=(uint8_t)(groups[n] | (n ? 0x80u : 0u)); }
}

asn1_status_t asn1_value_encode_oid(const uint64_t* arcs,size_t count,int relative,uint8_t** out,size_t* size) {
  if(!out || !size || (relative!=0 && relative!=1) || !arcs ||
     (relative ? count<1u : count<2u)) return ASN1_ERR_INVALID_ARGUMENT;
  if(!relative && (arcs[0]>2u || (arcs[0]<2u && arcs[1]>39u)))
    return ASN1_ERR_CONSTRAINT_VIOLATION;
  size_t n=0;
  for(size_t i=relative?0u:1u;i<count;++i) {
    uint64_t value=arcs[i];
    if(!relative && i==1u) {
      if(arcs[0]>(UINT64_MAX-value)/40u) return ASN1_ERR_LENGTH_OVERFLOW;
      value+=arcs[0]*40u;
    }
    const size_t part=oid_subidentifier_octets(value);
    if(part>ASN1_MAX_STRING_BYTES-n) return ASN1_ERR_RESOURCE_LIMIT;
    n+=part;
  }
  if(!n) return ASN1_ERR_INVALID_ARGUMENT;
  uint8_t* bytes=(uint8_t*)malloc(n);
  if(!bytes) return ASN1_ERR_OUT_OF_MEMORY;
  size_t position=0;
  for(size_t i=relative?0u:1u;i<count;++i) {
    uint64_t value=arcs[i];
    if(!relative && i==1u) value+=arcs[0]*40u;
    oid_write_subidentifier(bytes,&position,value);
  }
  *out=bytes;*size=n;return ASN1_OK;
}

asn1_status_t asn1_value_decode_oid(asn1_byte_view_t input,uint64_t** out_arcs,size_t* out_count,int relative) {
  if(!out_arcs || !out_count || (input.size&&!input.data) ||
     (relative!=0 && relative!=1)) return ASN1_ERR_INVALID_ARGUMENT;
  const uint8_t* bytes=input.data;const size_t n=input.size;
  if(!n)return ASN1_ERR_CONSTRAINT_VIOLATION;
  if(n>ASN1_MAX_STRING_BYTES)return ASN1_ERR_RESOURCE_LIMIT;
  size_t capacity=n+(relative?0u:1u);
  if(capacity>SIZE_MAX/sizeof(uint64_t))return ASN1_ERR_LENGTH_OVERFLOW;
  uint64_t* arcs=(uint64_t*)malloc(capacity*sizeof(*arcs));
  if(!arcs)return ASN1_ERR_OUT_OF_MEMORY;
  size_t used=0, count=0; asn1_status_t status=ASN1_OK;
  while(used<n) {
    uint64_t value=0; size_t start=used;
    for(;;) {
      if(used>=n) { status=ASN1_ERR_TRUNCATED; break; }
      const uint8_t byte=bytes[used++];
      if(used==start+1u && byte==0x80u) { status=ASN1_ERR_NON_CANONICAL; break; }
      if(value>(UINT64_MAX-(uint64_t)(byte&0x7fu))/128u) { status=ASN1_ERR_LENGTH_OVERFLOW; break; }
      value=value*128u+(uint64_t)(byte&0x7fu);
      if(!(byte&0x80u)) break;
    }
    if(status!=ASN1_OK) break;
    if(relative) arcs[count++]=value;
    else if(count==0u) {
      if(value<40u) { arcs[count++]=0; arcs[count++]=value; }
      else if(value<80u) { arcs[count++]=1; arcs[count++]=value-40u; }
      else { arcs[count++]=2; arcs[count++]=value-80u; }
    } else arcs[count++]=value;
  }
  if(status!=ASN1_OK || count<(relative?1u:2u)) { free(arcs); return status==ASN1_OK?ASN1_ERR_CONSTRAINT_VIOLATION:status; }
  *out_arcs=arcs; *out_count=count;
  return ASN1_OK;
}
