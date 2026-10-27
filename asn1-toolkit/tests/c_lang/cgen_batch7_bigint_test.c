#include <asn1/c_runtime/per.h>
#include <asn1/c_runtime/value_content.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if(!(x)){fprintf(stderr,"CGEN7 bigint line %d\n",__LINE__);return 1;} } while(0)
static unsigned digit(char c){return c<='9'?(unsigned)(c-'0'):(unsigned)(c-'a'+10);}
int main(void) {
  {
    const char* invalid[]={"230229120000Z","240230120000Z","240102240000Z","240102120060Z","240102120000+2460","240102120000", "240102120000Zx"};
    for(size_t i=0;i<sizeof(invalid)/sizeof(invalid[0]);++i){uint8_t out[13];memset(out,0xa5,sizeof(out));CHECK(asn1_utctime_normalize(asn1_byte_view((const uint8_t*)invalid[i],strlen(invalid[i])),out)!=ASN1_OK);for(size_t j=0;j<13;++j)CHECK(out[j]==0xa5);}
    uint8_t out[13];const char* value="2403010030+0100";CHECK(asn1_utctime_normalize(asn1_byte_view((const uint8_t*)value,15),out)==ASN1_OK&&!memcmp(out,"240229233000Z",13));
    asn1_bitstring_t bits={0},normalized={0};CHECK(asn1_bitstring_resize(&bits,8)==ASN1_OK);bits.bits[0]=128;CHECK(asn1_bitstring_normalize(&bits,16,&normalized)==ASN1_OK&&normalized.bit_length==16&&normalized.bits[0]==128&&normalized.bits[1]==0);asn1_bitstring_free(&normalized);CHECK(asn1_bitstring_normalize(&bits,0,&normalized)==ASN1_ERR_CONSTRAINT_VIOLATION&&!normalized.bits);asn1_bitstring_free(&bits);
  }
  const char* values[]={"0","15","16","-1","340282366920938463463374607431768211456","-340282366920938463463374607431768211456"};
  const asn1_per_int_range_t range={0,1,15,1,1};
  /* Independent asn1tools 0.169.0, INTEGER(0..15,...), UPER and PER (aligned).
     These constants pin all values, not just self-roundtrip behavior. */
  const char* expected[2][6]={
    {"00","78","808800","80ff80","88808000000000000000000000000000000000","88ff8000000000000000000000000000000000"},
    {"00","78","800110","8001ff","80110100000000000000000000000000000000","8011ff00000000000000000000000000000000"}
  };
  for(int variant=0;variant<2;++variant)for(size_t i=0;i<sizeof(values)/sizeof(values[0]);++i){
    asn1_bigint_t value,back;asn1_bigint_init(&value);asn1_bigint_init(&back);
    CHECK(asn1_bigint_init_from_decimal(&value,values[i],0)==ASN1_OK);
    asn1_bit_writer_t* w=NULL;CHECK(asn1_bit_writer_create(&w)==ASN1_OK);
    CHECK(asn1_per_encode_bigint_root_ex(w,(asn1_per_variant_t)variant,&range,&value)==ASN1_OK);
    uint8_t* data=NULL;size_t size=0;CHECK(asn1_bit_writer_take(w,&data,&size)==ASN1_OK);
    CHECK(size==strlen(expected[variant][i])/2);
    for(size_t n=0;n<size;++n)CHECK(data[n]==(uint8_t)((digit(expected[variant][i][2*n])<<4)|digit(expected[variant][i][2*n+1])));
    if(i<2)CHECK(size==1 && data[0]==(i?0x78:0));
    if(i==2){const uint8_t uper[]={0x80,0x88,0};const uint8_t aper[]={0x80,1,0x10};CHECK(size==3&&!memcmp(data,variant?aper:uper,3));}
    asn1_bit_reader_t* r=NULL;CHECK(asn1_bit_reader_create(&r,asn1_byte_view(data,size))==ASN1_OK);
    CHECK(asn1_per_decode_bigint_root_ex(r,(asn1_per_variant_t)variant,&range,&back)==ASN1_OK);
    CHECK(asn1_bigint_cmp(&value,&back)==0);
    asn1_bigint_free(&back);asn1_bit_reader_destroy(r);
    for(size_t n=0;n<size;++n){
      CHECK(asn1_bit_reader_create(&r,asn1_byte_view(data,n))==ASN1_OK);
      CHECK(asn1_per_decode_bigint_root_ex(r,(asn1_per_variant_t)variant,&range,&back)!=ASN1_OK);
      CHECK(!back.limbs&&!back.nlimbs&&!back.sign);
      asn1_bit_reader_destroy(r);
    }
    asn1_bigint_free(&value);free(data);asn1_bit_writer_destroy(w);
  }
  { /* An extension branch cannot carry a value in the root. */
    const uint8_t wire[]={0x80,1,0};asn1_bit_reader_t* r=NULL;asn1_bigint_t back;asn1_bigint_init(&back);
    CHECK(asn1_bit_reader_create(&r,asn1_byte_view(wire,sizeof(wire)))==ASN1_OK);
    CHECK(asn1_per_decode_bigint_root_ex(r,ASN1_PER_ALIGNED,&range,&back)==ASN1_ERR_CONSTRAINT_VIOLATION);
    CHECK(!back.limbs);asn1_bit_reader_destroy(r);
  }
  { /* asn1c 0.9.29, INTEGER(1..3|8|15,...): UPER roots use 1..15. */
    const asn1_per_int_range_t roots[]={{1,1,3,1,0},{8,1,8,1,0},{15,1,15,1,0}};
    const int numbers[]={1,3,4,8,14,15};const uint8_t expected[]={0,0x10,0x18,0x38,0x68,0x70};
    for(int variant=0;variant<2;++variant)for(size_t i=0;i<6;++i){
      char text[16];snprintf(text,sizeof(text),"%d",numbers[i]);asn1_bigint_t v,b;asn1_bigint_init(&v);asn1_bigint_init(&b);CHECK(asn1_bigint_init_from_decimal(&v,text,0)==ASN1_OK);
      asn1_bit_writer_t* w=NULL;CHECK(asn1_bit_writer_create(&w)==ASN1_OK);CHECK(asn1_per_encode_bigint_ranges_ex(w,(asn1_per_variant_t)variant,roots,3,1,&v)==ASN1_OK);
      uint8_t* data=NULL;size_t size=0;CHECK(asn1_bit_writer_take(w,&data,&size)==ASN1_OK);CHECK(size==1&&data[0]==expected[i]);
      asn1_bit_reader_t* r=NULL;CHECK(asn1_bit_reader_create(&r,asn1_byte_view(data,size))==ASN1_OK);CHECK(asn1_per_decode_bigint_ranges_ex(r,(asn1_per_variant_t)variant,roots,3,1,&b)==ASN1_OK&&!asn1_bigint_cmp(&v,&b));
      if(numbers[i]==4||numbers[i]==14)CHECK(asn1_per_encode_bigint_ranges_ex(w,(asn1_per_variant_t)variant,roots,3,0,&v)==ASN1_ERR_CONSTRAINT_VIOLATION);
      asn1_bigint_free(&v);asn1_bigint_free(&b);asn1_bit_reader_destroy(r);asn1_bit_writer_destroy(w);free(data);
    }
  }
  puts("CGEN7 bigint finite/disjoint root, arbitrary extension bounds and truncation PASS");return 0;
}
