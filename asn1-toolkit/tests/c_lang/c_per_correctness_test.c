/* Pure C11 standard vectors, frozen independent oracle vectors, and bit model. */
#include <asn1/c_runtime/per.h>
#include <asn1/c_runtime/jer.h>
#include <asn1/c_runtime/xer.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

static unsigned checks;
#define CHECK(c) do { ++checks; if (!(c)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#c); exit(1); } } while (0)
#define OK(c) CHECK((c)==ASN1_OK)
struct oracle_vector { int kind, variant; int64_t p0,p1,p2,p3,p4; size_t nbits; const char* input; const char* wire; };
#include "golden/per_oracle_vectors.h"
static unsigned digit(char c) { return c <= '9' ? (unsigned)(c-'0') : (unsigned)(c-'a'+10); }
static uint8_t* unhex(const char* s, size_t* n) {
  *n=strlen(s)/2u; uint8_t* data=(uint8_t*)malloc(*n ? *n : 1u); CHECK(data!=NULL);
  for (size_t i=0;i<*n;++i) data[i]=(uint8_t)(digit(s[2*i])*16u+digit(s[2*i+1]));
  return data;
}
static void frozen_oracle(void) {
  for (size_t i=0;i<sizeof(oracle_vectors)/sizeof(*oracle_vectors);++i) {
    const struct oracle_vector* t=&oracle_vectors[i];
    asn1_per_variant_t v=(asn1_per_variant_t)t->variant;
    asn1_per_int_range_t ir={t->p0,(int)t->p1,t->p2,(int)t->p3,(int)t->p4};
    asn1_per_length_constraint_t sc={(asn1_per_length_kind_t)t->p0,(size_t)t->p1,(size_t)t->p2,(int)t->p3};
    asn1_bigint_t b; asn1_bigint_init(&b);
    uint8_t* input=NULL; size_t size=0; int64_t integer=0;
    asn1_bitstring_t bits; asn1_bitstring_init(&bits);
    if (t->kind==0) OK(asn1_bigint_init_from_decimal(&b,t->input,0));
    else if (t->kind==1) { char* end=NULL; errno=0; integer=(int64_t)strtoll(t->input,&end,10); CHECK(!errno && *end==0); }
    else { input=unhex(t->input,&size); bits.bits=input; bits.cap_bytes=size; bits.bit_length=t->nbits; }
    asn1_bit_writer_t* w=NULL; OK(asn1_bit_writer_create(&w)); OK(asn1_per_encode_bool(w,1));
    if (t->kind==0) OK(asn1_per_encode_unbounded_int_ex(w,v,&b));
    else if(t->kind==1) OK(asn1_per_encode_int_ex(w,v,&ir,integer));
    else if(t->kind==2) OK(asn1_per_encode_bitstring_ex(w,v,&sc,&bits));
    else OK(asn1_per_encode_octetstring_ex(w,v,&sc,asn1_byte_view(input,size)));
    OK(asn1_per_encode_bool(w,1)); uint8_t* bytes=NULL; size_t n=0;
    OK(asn1_bit_writer_take(w,&bytes,&n));
    size_t expected_size=0; uint8_t* expected=unhex(t->wire,&expected_size);
    if (n!=expected_size || memcmp(bytes,expected,n)) { fprintf(stderr,"oracle vector %zu\n",i); CHECK(0); }
    asn1_bit_writer_destroy(w); free(bytes);
    asn1_bit_reader_t* r=NULL; OK(asn1_bit_reader_create(&r,asn1_byte_view(expected,expected_size)));
    int bit=0; OK(asn1_per_decode_bool(r,&bit)); CHECK(bit==1);
    if(t->kind==0) { asn1_bigint_t out; asn1_bigint_init(&out); OK(asn1_per_decode_unbounded_int_ex(r,v,&out)); CHECK(asn1_bigint_cmp(&b,&out)==0); asn1_bigint_free(&out); }
    else if(t->kind==1) { int64_t out=0; OK(asn1_per_decode_int_ex(r,v,&ir,&out)); CHECK(out==integer); }
    else if(t->kind==2) { asn1_bitstring_t out; asn1_bitstring_init(&out); OK(asn1_per_decode_bitstring_ex(r,v,&sc,&out)); CHECK(out.bit_length==t->nbits && !memcmp(out.bits,input,size)); asn1_bitstring_free(&out); }
    else { uint8_t* out=NULL; size_t length=0; OK(asn1_per_decode_octetstring_ex(r,v,&sc,&out,&length)); CHECK(length==size && !memcmp(out,input,size)); free(out); }
    OK(asn1_per_decode_bool(r,&bit)); CHECK(bit==1 && asn1_bit_reader_bit_remaining(r)<8);
    asn1_bit_reader_destroy(r); asn1_bigint_free(&b); free(input); free(expected);
  }
}
static void setbit(uint8_t* p,size_t pos,int bit) { if(bit) p[pos/8]|=(uint8_t)(1u<<(7-pos%8)); }
static void bit_model(void) {
  const uint8_t data[]={0xA0,0x5E,0xC3};
  for(size_t start=0;start<8;++start) for(size_t length=0;length<=17;++length) {
    uint8_t expected[4]={0};
    for(size_t j=0;j<start;++j) setbit(expected,j,(int)((0x55u>>(start-j-1u))&1u));
    for(size_t j=0;j<length;++j) setbit(expected,start+j,(data[j/8]>>(7-j%8))&1);
    asn1_bit_writer_t* w=NULL; OK(asn1_bit_writer_create(&w));
    OK(asn1_bit_writer_put_bits(w,0x55,start));
    OK(asn1_bit_writer_put_bits_from_bytes(w,asn1_byte_view(data,sizeof(data)),length));
    CHECK(asn1_bit_writer_bit_size(w)==start+length);
    uint8_t* out=NULL; size_t n=0; OK(asn1_bit_writer_take(w,&out,&n));
    CHECK(n==(start+length+7)/8 && !memcmp(out,expected,n)); free(out);
    CHECK(asn1_bit_writer_put_bits(w,0,65)==ASN1_ERR_INVALID_ARGUMENT && asn1_bit_writer_bit_size(w)==0);
    CHECK(asn1_bit_writer_put_bits_from_bytes(w,asn1_byte_view(data,1),9)==ASN1_ERR_INVALID_ARGUMENT);
    CHECK(asn1_bit_writer_put_bits_from_bytes(w,asn1_byte_view(NULL,1),1)==ASN1_ERR_INVALID_ARGUMENT);
    asn1_bit_writer_destroy(w);
  }
}
/* Explicit X.691 11.7 offset and 11.9 zero-length rules; excluded from
 * asn1tools 0.166.0 comparison because its code demonstrably deviates. */
static void standard_vectors(void) {
  for(int variant=0;variant<2;++variant) {
    asn1_bit_writer_t* w=NULL; OK(asn1_bit_writer_create(&w));
    asn1_per_int_range_t semi={-100,1,0,0,0};
    OK(asn1_per_encode_int_ex(w,(asn1_per_variant_t)variant,&semi,-100));
    uint8_t* out=NULL; size_t n=0; OK(asn1_bit_writer_take(w,&out,&n));
    CHECK(n==2 && out[0]==1 && out[1]==0);
    asn1_bit_reader_t* r=NULL; OK(asn1_bit_reader_create(&r,asn1_byte_view(out,n))); free(out);
    int64_t value=0; OK(asn1_per_decode_int_ex(r,(asn1_per_variant_t)variant,&semi,&value)); CHECK(value==-100);
    asn1_bit_reader_destroy(r);
    OK(asn1_per_encode_int_ex(w,(asn1_per_variant_t)variant,&semi,28));
    OK(asn1_bit_writer_take(w,&out,&n)); CHECK(n==2 && out[0]==1 && out[1]==128); free(out);
    asn1_per_length_constraint_t zero={ASN1_PER_LENGTH_CONSTRAINED,0,16,0};
    OK(asn1_bit_writer_put_bit(w,1));
    OK(asn1_per_encode_octetstring_ex(w,(asn1_per_variant_t)variant,&zero,asn1_byte_view(NULL,0)));
    OK(asn1_bit_writer_put_bit(w,1)); OK(asn1_bit_writer_take(w,&out,&n));
    CHECK(n==1 && out[0]==0x82); free(out);
    asn1_bit_writer_destroy(w);
  }
}
static void aligned_extensions(void) {
  const uint8_t data[]={0xA5,0x5E,0x80};
  for(int v=0;v<2;++v) for(int which=0;which<3;++which) {
    size_t length=which==0?0:which==1?17:8;
    uint8_t expected[8]={0}; size_t pos=0;
    setbit(expected,pos++,1); /* preceding BOOLEAN */
    if(which<2) setbit(expected,pos++,1); /* extension bit */
    if(v) pos=(pos+7u)/8u*8u;
    size_t determinant=which<2?length:1;
    for(size_t j=0;j<8;++j) setbit(expected,pos++,(int)((determinant>>(7u-j))&1u));
    for(size_t j=0;j<length;++j) setbit(expected,pos++,(data[j/8]>>(7u-j%8))&1u);
    setbit(expected,pos++,1);
    asn1_bit_writer_t* w=NULL; OK(asn1_bit_writer_create(&w)); OK(asn1_bit_writer_put_bit(w,1));
    asn1_per_length_constraint_t ext={ASN1_PER_LENGTH_CONSTRAINED,1,16,1};
    asn1_bitstring_t bits={(uint8_t*)data,length,sizeof(data)};
    if(which<2) OK(asn1_per_encode_bitstring_ex(w,(asn1_per_variant_t)v,&ext,&bits));
    else OK(asn1_per_encode_open_type(w,(asn1_per_variant_t)v,asn1_byte_view(data,1)));
    OK(asn1_bit_writer_put_bit(w,1)); uint8_t* wire=NULL; size_t n=0;
    OK(asn1_bit_writer_take(w,&wire,&n)); CHECK(n==(pos+7u)/8u && !memcmp(wire,expected,n));
    if(which==2) {
      asn1_bit_reader_t* r=NULL; OK(asn1_bit_reader_create(&r,asn1_byte_view(expected,n)));
      int bit=0; OK(asn1_bit_reader_get_bit(r,&bit)); uint8_t* value=NULL; size_t count=0;
      OK(asn1_per_decode_open_type(r,(asn1_per_variant_t)v,&value,&count)); CHECK(count==1 && value[0]==0xA5);
      OK(asn1_bit_reader_get_bit(r,&bit)); CHECK(bit==1); free(value); asn1_bit_reader_destroy(r);
    }
    free(wire); asn1_bit_writer_destroy(w);
  }
}
static void fragmentation(void) {
  const size_t lengths[]={0,127,128,16383,16384,16385,32768,49152,65536,65537,131072};
  uint8_t* data=(uint8_t*)malloc(131072); CHECK(data!=NULL);
  for(size_t i=0;i<131072;++i) data[i]=(uint8_t)(i*37+165);
  asn1_per_length_constraint_t unbound={ASN1_PER_LENGTH_UNCONSTRAINED,0,0,0};
  for(int variant=0;variant<2;++variant) for(size_t i=0;i<sizeof(lengths)/sizeof(*lengths);++i) {
    size_t length=lengths[i]; asn1_bit_writer_t* w=NULL; OK(asn1_bit_writer_create(&w));
    if(length) OK(asn1_per_encode_open_type(w,(asn1_per_variant_t)variant,asn1_byte_view(data,length)));
    else OK(asn1_per_encode_octetstring_ex(w,(asn1_per_variant_t)variant,&unbound,asn1_byte_view(NULL,0)));
    uint8_t* wire=NULL; size_t n=0; OK(asn1_bit_writer_take(w,&wire,&n));
    if(length>=16384 && length<=65536 && length%16384==0) CHECK(wire[0]==(uint8_t)(0xC0+length/16384) && wire[n-1]==0 && n==length+2);
    if(length==65537) CHECK(wire[0]==0xC4 && wire[65537]==1 && n==65539);
    if(length==131072) CHECK(wire[0]==0xC4 && wire[65537]==0xC4 && wire[n-1]==0);
    asn1_bit_reader_t* r=NULL; OK(asn1_bit_reader_create(&r,asn1_byte_view(wire,n)));
    uint8_t* out=NULL; size_t count=77;
    asn1_status_t s=length?asn1_per_decode_open_type(r,(asn1_per_variant_t)variant,&out,&count)
                        :asn1_per_decode_octetstring_ex(r,(asn1_per_variant_t)variant,&unbound,&out,&count);
    if(length>ASN1_MAX_STRING_BYTES) CHECK(s==ASN1_ERR_RESOURCE_LIMIT && out==NULL && count==77);
    else { CHECK(s==ASN1_OK && count==length && !memcmp(out,data,length)); free(out); }
    asn1_bit_reader_destroy(r);
    if(length>=16384 && length%16384==0) {
      OK(asn1_bit_reader_create(&r,asn1_byte_view(wire,n-1)));
      out=NULL; count=77;
      s=asn1_per_decode_open_type(r,(asn1_per_variant_t)variant,&out,&count);
      CHECK(s!=ASN1_OK && out==NULL && count==77); asn1_bit_reader_destroy(r);
    }
    free(wire); asn1_bit_writer_destroy(w);
    /* BIT STRING uses bit units and preserves a non-octet tail. */
    asn1_bitstring_t bits={data,length,(length+7)/8};
    asn1_per_length_constraint_t ext={ASN1_PER_LENGTH_CONSTRAINED,1,16,1};
    OK(asn1_bit_writer_create(&w)); OK(asn1_bit_writer_put_bit(w,1));
    OK(asn1_per_encode_bitstring_ex(w,(asn1_per_variant_t)variant,&ext,&bits));
    OK(asn1_bit_writer_put_bit(w,1)); OK(asn1_bit_writer_take(w,&wire,&n));
    OK(asn1_bit_reader_create(&r,asn1_byte_view(wire,n))); int flag=0; OK(asn1_bit_reader_get_bit(r,&flag));
    asn1_bitstring_t decoded; asn1_bitstring_init(&decoded);
    OK(asn1_per_decode_bitstring_ex(r,(asn1_per_variant_t)variant,&ext,&decoded));
    CHECK(decoded.bit_length==length && !memcmp(decoded.bits,data,length/8));
    if(length%8) CHECK(decoded.bits[length/8]==(uint8_t)(data[length/8]&(255u<<(8-length%8))));
    OK(asn1_bit_reader_get_bit(r,&flag)); CHECK(flag==1);
    asn1_bitstring_free(&decoded); asn1_bit_reader_destroy(r); asn1_bit_writer_destroy(w); free(wire);
  }
  free(data);
  (void)unbound;
}
static void invalid_inputs(void) {
  asn1_bit_writer_t* w=NULL; OK(asn1_bit_writer_create(&w));
  asn1_per_length_constraint_t c={ASN1_PER_LENGTH_CONSTRAINED,9,1,0};
  CHECK(asn1_per_encode_size_ex(w,ASN1_PER_UNALIGNED,&c,2)==ASN1_ERR_INVALID_ARGUMENT);
  asn1_per_int_range_t bad={9,1,1,1,0};
  CHECK(asn1_per_encode_int_ex(w,ASN1_PER_UNALIGNED,&bad,2)==ASN1_ERR_INVALID_ARGUMENT);
  c.kind=ASN1_PER_LENGTH_UNCONSTRAINED;
  CHECK(asn1_per_encode_size_ex(w,ASN1_PER_UNALIGNED,&c,16384)==ASN1_ERR_UNSUPPORTED);
  CHECK(asn1_per_encode_length_fragment(w,ASN1_PER_UNALIGNED,16385,1)==ASN1_ERR_INVALID_ARGUMENT);
  CHECK(asn1_per_encode_length_fragment(w,(asn1_per_variant_t)7,0,0)==ASN1_ERR_INVALID_ARGUMENT);
  OK(asn1_per_encode_open_type(w,ASN1_PER_UNALIGNED,asn1_byte_view(NULL,0)));
  asn1_per_size_range_t legacy={0,0}; CHECK(asn1_per_encode_size(w,&legacy,16384)==ASN1_ERR_UNSUPPORTED);
  CHECK(asn1_bit_writer_bit_size(w)==8); asn1_bit_writer_destroy(w);
  const uint8_t invalid[]={0xC0,0xC5,0xFF};
  for(size_t i=0;i<sizeof(invalid);++i) {
    asn1_bit_reader_t* r=NULL; OK(asn1_bit_reader_create(&r,asn1_byte_view(invalid+i,1)));
    size_t n=77; int more=77; CHECK(asn1_per_decode_length_fragment(r,ASN1_PER_UNALIGNED,&n,&more)==ASN1_ERR_INVALID_ARGUMENT && n==77 && more==77);
    asn1_bit_reader_destroy(r);
  }
  const uint8_t empty[]={0}, redundant[]={2,0,1};
  const asn1_byte_view_t inputs[]={{empty,1},{redundant,3}};
  for(size_t i=0;i<2;++i) {
    asn1_bit_reader_t* r=NULL; OK(asn1_bit_reader_create(&r,inputs[i]));
    asn1_bigint_t out; asn1_bigint_init(&out);
    CHECK(asn1_per_decode_unbounded_int(r,&out)==(i?ASN1_ERR_NON_CANONICAL:ASN1_ERR_INVALID_ARGUMENT));
    CHECK(out.limbs==NULL); asn1_bigint_free(&out); asn1_bit_reader_destroy(r);
  }
}
static void length_helpers(void) {
  const asn1_per_length_constraint_t cases[]={
    {ASN1_PER_LENGTH_CONSTRAINED,3,3,0},
    {ASN1_PER_LENGTH_CONSTRAINED,1,16,0},
    {ASN1_PER_LENGTH_SEMI_CONSTRAINED,3,0,0},
    {ASN1_PER_LENGTH_UNCONSTRAINED,0,0,0},
    {ASN1_PER_LENGTH_CONSTRAINED,1,16,1}};
  const size_t values[]={3,9,5,128,17};
  for(int v=0;v<2;++v) for(size_t i=0;i<sizeof(values)/sizeof(*values);++i) {
    asn1_bit_writer_t* w=NULL; OK(asn1_bit_writer_create(&w)); OK(asn1_bit_writer_put_bit(w,1));
    OK(asn1_per_encode_size_ex(w,(asn1_per_variant_t)v,&cases[i],values[i]));
    uint8_t* wire=NULL; size_t count=0; OK(asn1_bit_writer_take(w,&wire,&count));
    asn1_bit_reader_t* r=NULL; OK(asn1_bit_reader_create(&r,asn1_byte_view(wire,count)));
    int bit=0; OK(asn1_bit_reader_get_bit(r,&bit)); size_t value=77;
    OK(asn1_per_decode_size_ex(r,(asn1_per_variant_t)v,&cases[i],&value)); CHECK(value==values[i]);
    asn1_bit_reader_destroy(r); asn1_bit_writer_destroy(w); free(wire);
  }
  const uint8_t fragment=0xC1;
  asn1_bit_reader_t* r=NULL; OK(asn1_bit_reader_create(&r,asn1_byte_view(&fragment,1)));
  size_t length=77; int more=0; OK(asn1_per_decode_length_fragment(r,ASN1_PER_UNALIGNED,&length,&more));
  CHECK(length==16384 && more==1); asn1_bit_reader_destroy(r);
  OK(asn1_bit_reader_create(&r,asn1_byte_view(&fragment,1)));
  asn1_per_size_range_t legacy={0,0}; length=77;
  CHECK(asn1_per_decode_size(r,&legacy,&length)==ASN1_ERR_UNSUPPORTED && length==77);
  asn1_bit_reader_destroy(r);
}
static void normally_small_and_enumerated(void) {
  const uint64_t values[]={0,1,63,64,127,255,256,UINT64_C(4294967296),UINT64_MAX};
  for(int v=0;v<2;++v) for(size_t i=0;i<sizeof(values)/sizeof(*values);++i) {
    asn1_bit_writer_t* w=NULL; OK(asn1_bit_writer_create(&w));
    OK(asn1_bit_writer_put_bit(w,1));
    OK(asn1_per_encode_normally_small_uint_ex(w,(asn1_per_variant_t)v,values[i]));
    OK(asn1_bit_writer_put_bit(w,1));
    uint8_t* bytes=NULL; size_t n=0; OK(asn1_bit_writer_take(w,&bytes,&n));
    asn1_bit_reader_t* r=NULL; OK(asn1_bit_reader_create(&r,asn1_byte_view(bytes,n)));
    int bit=0; uint64_t decoded=0; OK(asn1_bit_reader_get_bit(r,&bit)); CHECK(bit==1);
    OK(asn1_per_decode_normally_small_uint_ex(r,(asn1_per_variant_t)v,&decoded)); CHECK(decoded==values[i]);
    OK(asn1_bit_reader_get_bit(r,&bit)); CHECK(bit==1);
    free(bytes); asn1_bit_reader_destroy(r); asn1_bit_writer_destroy(w);
  }
  for(int v=0;v<2;++v) {
    const size_t roots=5;
    for(size_t i=0;i<roots;++i) {
      asn1_bit_writer_t* w=NULL; OK(asn1_bit_writer_create(&w));
      OK(asn1_per_encode_enumerated_ex(w,(asn1_per_variant_t)v,i,roots,1,0,0));
      uint8_t* bytes=NULL; size_t n=0; OK(asn1_bit_writer_take(w,&bytes,&n));
      asn1_bit_reader_t* r=NULL; OK(asn1_bit_reader_create(&r,asn1_byte_view(bytes,n)));
      int ext=1; uint64_t ordinal=99;
      OK(asn1_per_decode_enumerated_ex(r,(asn1_per_variant_t)v,roots,1,&ext,&ordinal));
      CHECK(!ext && ordinal==i); free(bytes); asn1_bit_reader_destroy(r); asn1_bit_writer_destroy(w);
    }
    const uint64_t ext_values[]={0,63,64,500};
    for(size_t i=0;i<sizeof(ext_values)/sizeof(*ext_values);++i) {
      asn1_bit_writer_t* w=NULL; OK(asn1_bit_writer_create(&w));
      OK(asn1_per_encode_enumerated_ex(w,(asn1_per_variant_t)v,0,roots,1,1,ext_values[i]));
      uint8_t* bytes=NULL; size_t n=0; OK(asn1_bit_writer_take(w,&bytes,&n));
      asn1_bit_reader_t* r=NULL; OK(asn1_bit_reader_create(&r,asn1_byte_view(bytes,n)));
      int ext=0; uint64_t ordinal=99;
      OK(asn1_per_decode_enumerated_ex(r,(asn1_per_variant_t)v,roots,1,&ext,&ordinal));
      CHECK(ext && ordinal==ext_values[i]); free(bytes); asn1_bit_reader_destroy(r); asn1_bit_writer_destroy(w);
    }
  }
  asn1_bit_writer_t* w=NULL; OK(asn1_bit_writer_create(&w));
  CHECK(asn1_per_encode_enumerated_ex(w,ASN1_PER_UNALIGNED,5,5,0,0,0)==ASN1_ERR_INVALID_ARGUMENT);
  CHECK(asn1_per_encode_enumerated_ex(w,ASN1_PER_UNALIGNED,0,5,0,1,0)==ASN1_ERR_INVALID_ARGUMENT);
  CHECK(asn1_bit_writer_bit_size(w)==0); asn1_bit_writer_destroy(w);
  /* X.691 10.6/10.7/10.9: the >=64 form contains an octet-aligned
     semi-constrained value in APER, while UPER appends the fields tightly. */
  const uint8_t expected_uper[]={0x80,0xA0,0x00};
  const uint8_t expected_aper[]={0x80,0x01,0x40};
  for(int v=0;v<2;++v) {
    OK(asn1_bit_writer_create(&w));
    OK(asn1_per_encode_normally_small_uint_ex(w,(asn1_per_variant_t)v,64));
    uint8_t* actual=NULL; size_t size=0; OK(asn1_bit_writer_take(w,&actual,&size));
    const uint8_t* expected=v?expected_aper:expected_uper;
    const size_t expected_size=v?sizeof(expected_aper):sizeof(expected_uper);
    if(size!=expected_size || memcmp(actual,expected,size)!=0) {
      fprintf(stderr,"normally-small frozen vector variant=%d size=%zu actual=",v,size);
      for(size_t i=0;i<size;++i) fprintf(stderr,"%02x",actual[i]);
      fprintf(stderr," expected=");
      for(size_t i=0;i<expected_size;++i) fprintf(stderr,"%02x",expected[i]);
      fputc('\n',stderr); CHECK(0);
    }
    free(actual); asn1_bit_writer_destroy(w);
  }
}
static void oid_contents(void) {
  const uint64_t arcs[]={1,2,840,113549};
  const uint8_t expected[]={6,0x2A,0x86,0x48,0x86,0xF7,0x0D};
  for(int v=0;v<2;++v) {
    asn1_bit_writer_t* w=NULL; OK(asn1_bit_writer_create(&w));
    OK(asn1_per_encode_oid_ex(w,(asn1_per_variant_t)v,arcs,4,0));
    uint8_t* wire=NULL; size_t n=0; OK(asn1_bit_writer_take(w,&wire,&n));
    CHECK(n==sizeof(expected) && memcmp(wire,expected,n)==0);
    asn1_bit_reader_t* r=NULL; OK(asn1_bit_reader_create(&r,asn1_byte_view(wire,n)));
    uint64_t* decoded=NULL; size_t count=0;
    OK(asn1_per_decode_oid_ex(r,(asn1_per_variant_t)v,&decoded,&count,0));
    CHECK(count==4 && memcmp(decoded,arcs,sizeof(arcs))==0);
    free(decoded); free(wire); asn1_bit_reader_destroy(r); asn1_bit_writer_destroy(w);
  }
  asn1_bit_writer_t* w=NULL; OK(asn1_bit_writer_create(&w));
  const uint64_t invalid[]={3,1};
  CHECK(asn1_per_encode_oid_ex(w,ASN1_PER_UNALIGNED,invalid,2,0)==ASN1_ERR_CONSTRAINT_VIOLATION);
  CHECK(asn1_per_encode_oid_ex(w,ASN1_PER_UNALIGNED,arcs,1,0)==ASN1_ERR_INVALID_ARGUMENT);
  CHECK(asn1_bit_writer_bit_size(w)==0); asn1_bit_writer_destroy(w);
  const uint8_t malformed[]={1,0x81,0}; asn1_bit_reader_t* r=NULL;
  OK(asn1_bit_reader_create(&r,asn1_byte_view(malformed,sizeof(malformed))));
  uint64_t* untouched=(uint64_t*)(uintptr_t)1; size_t untouched_count=77;
  CHECK(asn1_per_decode_oid_ex(r,ASN1_PER_UNALIGNED,&untouched,&untouched_count,0)==ASN1_ERR_TRUNCATED);
  CHECK(untouched==(uint64_t*)(uintptr_t)1 && untouched_count==77);
  asn1_bit_reader_destroy(r);
  const uint8_t redundant[]={1,0x80,0};
  OK(asn1_bit_reader_create(&r,asn1_byte_view(redundant,sizeof(redundant))));
  CHECK(asn1_per_decode_oid_ex(r,ASN1_PER_UNALIGNED,&untouched,&untouched_count,0)==ASN1_ERR_NON_CANONICAL);
  CHECK(untouched==(uint64_t*)(uintptr_t)1 && untouched_count==77);
  asn1_bit_reader_destroy(r);
}
static void sequence_preamble(void) {
  const uint8_t optional[]={1,0,1};
  for(int ext=0;ext<2;++ext) {
    asn1_bit_writer_t* w=NULL; OK(asn1_bit_writer_create(&w));
    OK(asn1_per_encode_sequence_preamble(w,ext,ext,optional,3));
    uint8_t* wire=NULL; size_t n=0; OK(asn1_bit_writer_take(w,&wire,&n));
    CHECK(n==1 && wire[0]==(ext?0xD0:0xA0));
    asn1_bit_reader_t* r=NULL; OK(asn1_bit_reader_create(&r,asn1_byte_view(wire,n)));
    asn1_per_sequence_preamble_t decoded={0};
    OK(asn1_per_decode_sequence_preamble(r,ext,3,&decoded));
    CHECK(decoded.extensions_present==ext && decoded.optional_count==3 &&
          decoded.optional_bits[0]==1 && decoded.optional_bits[1]==0 && decoded.optional_bits[2]==1);
    asn1_per_sequence_preamble_free(&decoded); free(wire); asn1_bit_reader_destroy(r);
    asn1_bit_writer_destroy(w);
  }
  asn1_bit_writer_t* w=NULL; OK(asn1_bit_writer_create(&w));
  const uint8_t invalid[]={0,2};
  CHECK(asn1_per_encode_sequence_preamble(w,1,1,invalid,2)==ASN1_ERR_INVALID_ARGUMENT);
  CHECK(asn1_bit_writer_bit_size(w)==0); asn1_bit_writer_destroy(w);
  const uint8_t empty[]={0}; asn1_bit_reader_t* r=NULL;
  OK(asn1_bit_reader_create(&r,asn1_byte_view(empty,0)));
  asn1_per_sequence_preamble_t untouched={1,(uint8_t*)(uintptr_t)1,77};
  CHECK(asn1_per_decode_sequence_preamble(r,1,3,&untouched)==ASN1_ERR_TRUNCATED);
  CHECK(untouched.extensions_present==1 && untouched.optional_bits==(uint8_t*)(uintptr_t)1 && untouched.optional_count==77);
  asn1_bit_reader_destroy(r);
}
static void restricted_strings(void) {
  const asn1_per_length_constraint_t unconstrained={ASN1_PER_LENGTH_UNCONSTRAINED,0,0,0};
  const uint8_t bmp_a[]={0x41};
  const uint8_t supplementary[]={0xF0,0x9F,0x98,0x80};
  for(int v=0;v<2;++v) {
    asn1_bit_writer_t* w=NULL; OK(asn1_bit_writer_create(&w));
    OK(asn1_per_encode_restricted_string_ex(w,(asn1_per_variant_t)v,&unconstrained,ASN1_PER_STRING_BMP,asn1_byte_view(bmp_a,sizeof(bmp_a))));
    uint8_t* wire=NULL; size_t n=0; OK(asn1_bit_writer_take(w,&wire,&n));
    asn1_bit_reader_t* r=NULL; OK(asn1_bit_reader_create(&r,asn1_byte_view(wire,n)));
    uint8_t* decoded=NULL; size_t decoded_size=0;
    OK(asn1_per_decode_restricted_string_ex(r,(asn1_per_variant_t)v,&unconstrained,ASN1_PER_STRING_BMP,&decoded,&decoded_size));
    CHECK(decoded_size==sizeof(bmp_a) && memcmp(decoded,bmp_a,decoded_size)==0);
    free(decoded); free(wire); asn1_bit_reader_destroy(r); asn1_bit_writer_destroy(w);

    OK(asn1_bit_writer_create(&w));
    CHECK(asn1_per_encode_restricted_string_ex(w,(asn1_per_variant_t)v,&unconstrained,ASN1_PER_STRING_BMP,asn1_byte_view(supplementary,sizeof(supplementary)))==ASN1_ERR_CONSTRAINT_VIOLATION);
    CHECK(asn1_bit_writer_bit_size(w)==0); asn1_bit_writer_destroy(w);

    /* A BMPString wire code unit in the surrogate range is invalid. */
    OK(asn1_bit_writer_create(&w)); OK(asn1_per_encode_size_ex(w,(asn1_per_variant_t)v,&unconstrained,1));
    if(v==ASN1_PER_ALIGNED) OK(asn1_bit_writer_align_to_octet(w));
    OK(asn1_bit_writer_put_bits(w,0xD800u,16)); OK(asn1_bit_writer_take(w,&wire,&n));
    OK(asn1_bit_reader_create(&r,asn1_byte_view(wire,n)));
    decoded=(uint8_t*)(uintptr_t)1; decoded_size=77;
    CHECK(asn1_per_decode_restricted_string_ex(r,(asn1_per_variant_t)v,&unconstrained,ASN1_PER_STRING_BMP,&decoded,&decoded_size)==ASN1_ERR_CONSTRAINT_VIOLATION);
    CHECK(decoded==(uint8_t*)(uintptr_t)1 && decoded_size==77);
    free(wire); asn1_bit_reader_destroy(r); asn1_bit_writer_destroy(w);
  }
}
static void lifecycle_and_legacy(void) {
  const uint8_t source[]={0x80};
  asn1_byte_reader_t* bytes=NULL; OK(asn1_byte_reader_create(&bytes,asn1_byte_view(source,1)));
  asn1_byte_reader_destroy(bytes); asn1_byte_reader_destroy(NULL);
  asn1_bit_writer_t* w=NULL; OK(asn1_bit_writer_create(&w));
  CHECK(asn1_bit_writer_octet_aligned(w)); OK(asn1_bit_writer_put_bit(w,1));
  CHECK(!asn1_bit_writer_octet_aligned(w)); asn1_bit_writer_clear(w);
  CHECK(asn1_bit_writer_bit_size(w)==0 && asn1_bit_writer_octet_aligned(w));
  asn1_bit_reader_t* r=NULL; OK(asn1_bit_reader_create(&r,asn1_byte_view(source,1)));
  CHECK(asn1_bit_reader_octet_aligned(r)); int bit=0; OK(asn1_bit_reader_get_bit(r,&bit));
  CHECK(!asn1_bit_reader_octet_aligned(r)); OK(asn1_bit_reader_align_to_octet(r));
  CHECK(asn1_bit_reader_octet_aligned(r) && asn1_bit_reader_eof(r)); asn1_bit_reader_destroy(r);
  asn1_bigint_t value; asn1_bigint_init(&value); OK(asn1_bigint_init_from_decimal(&value,"128",0));
  OK(asn1_per_encode_unbounded_int(w,&value)); uint8_t* data=NULL; size_t n=0;
  OK(asn1_bit_writer_take(w,&data,&n)); CHECK(n==3 && data[0]==2 && data[1]==0 && data[2]==0x80);
  free(data); asn1_bigint_free(&value); asn1_bit_writer_destroy(w);
  const uint8_t minus_one[]={1,0xFF}; OK(asn1_bit_reader_create(&r,asn1_byte_view(minus_one,2)));
  OK(asn1_per_decode_unbounded_int(r,&value)); int64_t signed_value=0;
  OK(asn1_bigint_to_int64(&value,&signed_value)); CHECK(signed_value==-1);
  asn1_bigint_free(&value); asn1_bit_reader_destroy(r);
  asn1_jer_reader_t json; OK(asn1_jer_reader_init(&json,"{}",2)); CHECK(json.pos==0 && json.size==2);
  asn1_xer_reader_t xml; OK(asn1_xer_reader_init(&xml,"<x/>",4)); CHECK(xml.pos==0 && xml.size==4);
}
int main(void) {
  bit_model(); frozen_oracle(); standard_vectors(); aligned_extensions(); fragmentation(); invalid_inputs(); length_helpers(); normally_small_and_enumerated(); oid_contents(); sequence_preamble(); restricted_strings(); lifecycle_and_legacy();
  printf("C PER correctness: %u checks, %zu frozen oracle vectors PASS (limit=%u)\n",checks,sizeof(oracle_vectors)/sizeof(*oracle_vectors),(unsigned)ASN1_MAX_STRING_BYTES);
  return 0;
}
