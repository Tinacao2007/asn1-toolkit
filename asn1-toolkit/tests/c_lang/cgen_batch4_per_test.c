#include "generated.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x); return 1; } } while (0)

static int hex_value(char c) {
  if(c>='0'&&c<='9')return c-'0';
  if(c>='a'&&c<='f')return c-'a'+10;
  if(c>='A'&&c<='F')return c-'A'+10;
  return -1;
}
static int load_fragment_oracle(const char* path,int aper,uint8_t* out,size_t capacity,size_t* out_size) {
  FILE* file=fopen(path,"rb"); if(!file)return 0;
  char line[20000]; int found=0;
  while(fgets(line,sizeof(line),file)) if(strncmp(line,"frag_boundary ",14)==0) {
    char* token=strtok(line," \t\r\n");
    token=strtok(NULL," \t\r\n");
    if(aper)token=strtok(NULL," \t\r\n");
    if(!token)break;
    const size_t length=strlen(token); if((length&1u)||length/2u>capacity)break;
    for(size_t i=0;i<length/2u;++i) {
      const int hi=hex_value(token[2u*i]),lo=hex_value(token[2u*i+1u]);
      if(hi<0||lo<0){fclose(file);return 0;}
      out[i]=(uint8_t)((hi<<4)|lo);
    }
    *out_size=length/2u;found=1;break;
  }
  fclose(file);return found;
}

int main(int argc,char** argv) {
  /* X.691 fragmented collection length is interleaved with each fragment's
     elements; exact multiples require a final zero length determinant. */
  for (int aper=0;aper!=2;++aper) for (size_t count=16384;count<=16385;++count) {
    cgen4_mM_tLargeFlags input; cgen4_mM_tLargeFlags_init(&input);
    input.data=calloc(count,sizeof(*input.data));
    CHECK(input.data!=NULL); input.count=input.capacity=count;
    for(size_t i=0;i<count;++i) input.data[i].value=(i&1u)==0u;
    asn1_bit_writer_t* writer=NULL; CHECK(asn1_bit_writer_create(&writer)==ASN1_OK);
    CHECK((aper?cgen4_mM_tLargeFlags_encode_aper(writer,&input):cgen4_mM_tLargeFlags_encode_uper(writer,&input))==ASN1_OK);
    uint8_t* wire=NULL; size_t wire_size=0; CHECK(asn1_bit_writer_take(writer,&wire,&wire_size)==ASN1_OK);
    if(count==16385u) {
      CHECK(argc>=2);
      uint8_t oracle[8192];size_t oracle_size=0;
      CHECK(load_fragment_oracle(argv[1],aper,oracle,sizeof(oracle),&oracle_size));
      CHECK(wire_size==oracle_size && memcmp(wire,oracle,wire_size)==0);
    }
    asn1_bit_reader_t* reader=NULL; CHECK(asn1_bit_reader_create(&reader,asn1_byte_view(wire,wire_size))==ASN1_OK);
    cgen4_mM_tLargeFlags decoded; cgen4_mM_tLargeFlags_init(&decoded);
    CHECK((aper?cgen4_mM_tLargeFlags_decode_aper(reader,&decoded):cgen4_mM_tLargeFlags_decode_uper(reader,&decoded))==ASN1_OK);
    CHECK(decoded.count==count);
    for(size_t i=0;i<count;++i) CHECK(decoded.data[i].value==input.data[i].value);
    cgen4_mM_tLargeFlags_free(&decoded); cgen4_mM_tLargeFlags_free(&input);
    asn1_bit_reader_destroy(reader); asn1_bit_writer_destroy(writer); free(wire);
  }
  const uint8_t expected[] = {0xB6};
  for (int aper = 0; aper != 2; ++aper) {
    cgen4_mM_tPdu value;
    cgen4_mM_tPdu_init(&value);
    value.fflag.value = 1;
    value.fcount.value = 5;
    value.has_foptionalValue = 1;
    value.foptionalValue.value = 2;
    /* DEFAULT FALSE is omitted from the presence bitmap. */

    /* Presence storage is a public C field and must be validated, not coerced. */
    asn1_bit_writer_t* invalid_writer = NULL;
    CHECK(asn1_bit_writer_create(&invalid_writer) == ASN1_OK);
    value.has_foptionalValue = 2;
    CHECK((aper ? cgen4_mM_tPdu_encode_aper(invalid_writer, &value)
                : cgen4_mM_tPdu_encode_uper(invalid_writer, &value)) == ASN1_ERR_INVALID_ARGUMENT);
    value.has_foptionalValue = 1;
    asn1_bit_writer_destroy(invalid_writer);

    CHECK(asn1_bit_writer_create(&invalid_writer) == ASN1_OK);
    value.fcount.value = 8;
    CHECK((aper ? cgen4_mM_tPdu_encode_aper(invalid_writer, &value)
                : cgen4_mM_tPdu_encode_uper(invalid_writer, &value)) == ASN1_ERR_CONSTRAINT_VIOLATION);
    value.fcount.value = 5;
    asn1_bit_writer_destroy(invalid_writer);

    CHECK(asn1_bit_writer_create(&invalid_writer) == ASN1_OK);
    value.fflag.value = 2;
    CHECK((aper ? cgen4_mM_tPdu_encode_aper(invalid_writer, &value)
                : cgen4_mM_tPdu_encode_uper(invalid_writer, &value)) == ASN1_ERR_INVALID_ARGUMENT);
    value.fflag.value = 1;
    asn1_bit_writer_destroy(invalid_writer);

    asn1_bit_writer_t* writer = NULL;
    CHECK(asn1_bit_writer_create(&writer) == ASN1_OK);
    asn1_status_t status = aper
        ? cgen4_mM_tPdu_encode_aper(writer, &value)
        : cgen4_mM_tPdu_encode_uper(writer, &value);
    CHECK(status == ASN1_OK);
    uint8_t* wire = NULL; size_t size = 0;
    CHECK(asn1_bit_writer_take(writer, &wire, &size) == ASN1_OK);
    CHECK(size == sizeof(expected) && memcmp(wire, expected, size) == 0);

    asn1_bit_reader_t* reader = NULL;
    CHECK(asn1_bit_reader_create(&reader, asn1_byte_view(wire, size)) == ASN1_OK);
    cgen4_mM_tPdu decoded;
    cgen4_mM_tPdu_init(&decoded);
    status = aper ? cgen4_mM_tPdu_decode_aper(reader, &decoded)
                  : cgen4_mM_tPdu_decode_uper(reader, &decoded);
    CHECK(status == ASN1_OK);
    CHECK(decoded.fflag.value == 1 && decoded.fcount.value == 5 &&
          decoded.has_foptionalValue == 1 && decoded.foptionalValue.value == 2 &&
          decoded.fdefaultFlag.value == 0);
    cgen4_mM_tPdu_free(&decoded);
    asn1_bit_reader_destroy(reader);

    /* Public decode is transactional for the value, even when input truncates. */
    CHECK(asn1_bit_reader_create(&reader, asn1_byte_view(wire, 0)) == ASN1_OK);
    cgen4_mM_tPdu_init(&decoded);
    decoded.fflag.value = 0;
    decoded.fcount.value = 3;
    status = aper ? cgen4_mM_tPdu_decode_aper(reader, &decoded)
                  : cgen4_mM_tPdu_decode_uper(reader, &decoded);
    CHECK(status == ASN1_ERR_TRUNCATED);
    CHECK(decoded.fflag.value == 0 && decoded.fcount.value == 3);
    cgen4_mM_tPdu_free(&decoded);
    asn1_bit_reader_destroy(reader);

    free(wire); asn1_bit_writer_destroy(writer); cgen4_mM_tPdu_free(&value);
  }
  {
    const uint8_t expected[] = {0x80};
    for (int aper = 0; aper != 2; ++aper) {
      cgen4_mM_tMode mode;
      cgen4_mM_tMode_init(&mode);
      mode.value = 2;
      asn1_bit_writer_t* writer = NULL;
      CHECK(asn1_bit_writer_create(&writer) == ASN1_OK);
      CHECK((aper ? cgen4_mM_tMode_encode_aper(writer, &mode)
                  : cgen4_mM_tMode_encode_uper(writer, &mode)) == ASN1_OK);
      uint8_t* wire = NULL; size_t size = 0;
      CHECK(asn1_bit_writer_take(writer, &wire, &size) == ASN1_OK);
      CHECK(size == sizeof(expected) && memcmp(wire, expected, size) == 0);
      cgen4_mM_tMode decoded;
      cgen4_mM_tMode_init(&decoded);
      asn1_bit_reader_t* reader = NULL;
      CHECK(asn1_bit_reader_create(&reader, asn1_byte_view(wire, size)) == ASN1_OK);
      CHECK((aper ? cgen4_mM_tMode_decode_aper(reader, &decoded)
                  : cgen4_mM_tMode_decode_uper(reader, &decoded)) == ASN1_OK);
      CHECK(decoded.value == 2);
      cgen4_mM_tMode_free(&decoded);
      asn1_bit_reader_destroy(reader);
      free(wire); asn1_bit_writer_destroy(writer);
    }
  }
  for (int aper = 0; aper != 2; ++aper) {
    cgen4_mM_tExtensible value; cgen4_mM_tExtensible_init(&value);
    value.froot.value=5; value.has_fextra=1; value.fextra.value=1;
    asn1_bit_writer_t* writer=NULL; CHECK(asn1_bit_writer_create(&writer)==ASN1_OK);
    CHECK((aper?cgen4_mM_tExtensible_encode_aper(writer,&value):cgen4_mM_tExtensible_encode_uper(writer,&value))==ASN1_OK);
    uint8_t* wire=NULL; size_t size=0; CHECK(asn1_bit_writer_take(writer,&wire,&size)==ASN1_OK);
    asn1_bit_reader_t* reader=NULL; CHECK(asn1_bit_reader_create(&reader,asn1_byte_view(wire,size))==ASN1_OK);
    cgen4_mM_tExtensible decoded; cgen4_mM_tExtensible_init(&decoded);
    CHECK((aper?cgen4_mM_tExtensible_decode_aper(reader,&decoded):cgen4_mM_tExtensible_decode_uper(reader,&decoded))==ASN1_OK);
    CHECK(decoded.froot.value==5 && decoded.has_fextra==1 && decoded.fextra.value==1);
    cgen4_mM_tExtensible_free(&decoded);cgen4_mM_tExtensible_free(&value);free(wire);asn1_bit_reader_destroy(reader);asn1_bit_writer_destroy(writer);
  }
  for (int aper = 0; aper != 2; ++aper) {
    asn1_bit_writer_t* writer=NULL;CHECK(asn1_bit_writer_create(&writer)==ASN1_OK);
    CHECK(asn1_per_encode_sequence_preamble(writer,1,1,NULL,0)==ASN1_OK);
    const asn1_per_int_range_t range={0,1,7,1,0};
    CHECK(asn1_per_encode_int_ex(writer,aper?ASN1_PER_ALIGNED:ASN1_PER_UNALIGNED,&range,3)==ASN1_OK);
    const uint8_t presence[]={0,1}, unknown[]={0xA5};
    const asn1_byte_view_t values[]={{NULL,0},{unknown,sizeof(unknown)}};
    CHECK(asn1_per_encode_extension_additions(writer,aper?ASN1_PER_ALIGNED:ASN1_PER_UNALIGNED,presence,2,values)==ASN1_OK);
    uint8_t* wire=NULL;size_t size=0;CHECK(asn1_bit_writer_take(writer,&wire,&size)==ASN1_OK);
    asn1_bit_reader_t* reader=NULL;CHECK(asn1_bit_reader_create(&reader,asn1_byte_view(wire,size))==ASN1_OK);
    cgen4_mM_tExtensible decoded;cgen4_mM_tExtensible_init(&decoded);
    CHECK((aper?cgen4_mM_tExtensible_decode_aper(reader,&decoded):cgen4_mM_tExtensible_decode_uper(reader,&decoded))==ASN1_OK);
    CHECK(decoded.froot.value==3 && decoded.has_fextra==0);
    cgen4_mM_tExtensible_free(&decoded);free(wire);asn1_bit_reader_destroy(reader);asn1_bit_writer_destroy(writer);
  }
  for (int aper = 0; aper != 2; ++aper) {
    cgen4_mM_tVersioned value;cgen4_mM_tVersioned_init(&value);value.fbase.value=2;
    value.has_fadded=1;value.fadded.value=11;value.has_fnote=1;
    const uint8_t note[]="v2";value.fnote.data=(uint8_t*)malloc(sizeof(note)-1);CHECK(value.fnote.data!=NULL);memcpy(value.fnote.data,note,sizeof(note)-1);value.fnote.count=value.fnote.capacity=sizeof(note)-1;
    asn1_bit_writer_t* writer=NULL;CHECK(asn1_bit_writer_create(&writer)==ASN1_OK);
    CHECK((aper?cgen4_mM_tVersioned_encode_aper(writer,&value):cgen4_mM_tVersioned_encode_uper(writer,&value))==ASN1_OK);
    uint8_t* wire=NULL;size_t size=0;CHECK(asn1_bit_writer_take(writer,&wire,&size)==ASN1_OK);
    asn1_bit_reader_t* reader=NULL;CHECK(asn1_bit_reader_create(&reader,asn1_byte_view(wire,size))==ASN1_OK);
    cgen4_mM_tVersioned decoded;cgen4_mM_tVersioned_init(&decoded);
    CHECK((aper?cgen4_mM_tVersioned_decode_aper(reader,&decoded):cgen4_mM_tVersioned_decode_uper(reader,&decoded))==ASN1_OK);
    CHECK(decoded.fbase.value==2 && decoded.has_fadded==1 && decoded.fadded.value==11 && decoded.has_fnote==1 && decoded.fnote.count==sizeof(note)-1 && memcmp(decoded.fnote.data,note,sizeof(note)-1)==0);
    cgen4_mM_tVersioned_free(&decoded);cgen4_mM_tVersioned_free(&value);free(wire);asn1_bit_reader_destroy(reader);asn1_bit_writer_destroy(writer);
  }
  {
    for (int aper = 0; aper != 2; ++aper) {
      cgen4_mM_tCounts value;
      cgen4_mM_tCounts_init(&value);
      value.data = calloc(3, sizeof(*value.data));
      CHECK(value.data != NULL);
      value.count = value.capacity = 3;
      value.data[0].value = 1;
      value.data[1].value = 4;
      value.data[2].value = 7;
      asn1_bit_writer_t* writer = NULL;
      CHECK(asn1_bit_writer_create(&writer) == ASN1_OK);
      CHECK((aper ? cgen4_mM_tCounts_encode_aper(writer, &value)
                  : cgen4_mM_tCounts_encode_uper(writer, &value)) == ASN1_OK);
      uint8_t* wire = NULL; size_t size = 0;
      CHECK(asn1_bit_writer_take(writer, &wire, &size) == ASN1_OK);
      cgen4_mM_tCounts decoded;
      cgen4_mM_tCounts_init(&decoded);
      asn1_bit_reader_t* reader = NULL;
      CHECK(asn1_bit_reader_create(&reader, asn1_byte_view(wire, size)) == ASN1_OK);
      CHECK((aper ? cgen4_mM_tCounts_decode_aper(reader, &decoded)
                  : cgen4_mM_tCounts_decode_uper(reader, &decoded)) == ASN1_OK);
      CHECK(decoded.count == 3 && decoded.data[0].value == 1 &&
            decoded.data[1].value == 4 && decoded.data[2].value == 7);
      cgen4_mM_tCounts_free(&decoded);
      cgen4_mM_tCounts_free(&value);
      asn1_bit_reader_destroy(reader);
      asn1_bit_writer_destroy(writer);
      free(wire);
    }
  }
  {
    for (int aper = 0; aper != 2; ++aper) {
      cgen4_mM_tPayload value;
      cgen4_mM_tPayload_init(&value);
      value.kind = cgen4_mM_tPayload_arm_fsmall;
      value.value.fsmall.value = 5;
      asn1_bit_writer_t* writer = NULL;
      CHECK(asn1_bit_writer_create(&writer) == ASN1_OK);
      CHECK((aper ? cgen4_mM_tPayload_encode_aper(writer, &value)
                  : cgen4_mM_tPayload_encode_uper(writer, &value)) == ASN1_OK);
      uint8_t* wire = NULL; size_t size = 0;
      CHECK(asn1_bit_writer_take(writer, &wire, &size) == ASN1_OK);
      cgen4_mM_tPayload decoded;
      cgen4_mM_tPayload_init(&decoded);
      asn1_bit_reader_t* reader = NULL;
      CHECK(asn1_bit_reader_create(&reader, asn1_byte_view(wire, size)) == ASN1_OK);
      CHECK((aper ? cgen4_mM_tPayload_decode_aper(reader, &decoded)
                  : cgen4_mM_tPayload_decode_uper(reader, &decoded)) == ASN1_OK);
      CHECK(decoded.kind == cgen4_mM_tPayload_arm_fsmall && decoded.value.fsmall.value == 5);
      cgen4_mM_tPayload_free(&decoded);
      cgen4_mM_tPayload_free(&value);
      asn1_bit_reader_destroy(reader);
      asn1_bit_writer_destroy(writer);
      free(wire);
    }
  }
  {
    const uint64_t expected[] = {1, 2, 840, 113549};
    for (int aper = 0; aper != 2; ++aper) {
      cgen4_mM_tOid value;
      cgen4_mM_tOid_init(&value);
      value.data = calloc(4, sizeof(*value.data));
      CHECK(value.data != NULL);
      memcpy(value.data, expected, sizeof(expected));
      value.count = value.capacity = 4;
      asn1_bit_writer_t* writer = NULL;
      CHECK(asn1_bit_writer_create(&writer) == ASN1_OK);
      CHECK((aper ? cgen4_mM_tOid_encode_aper(writer, &value)
                  : cgen4_mM_tOid_encode_uper(writer, &value)) == ASN1_OK);
      uint8_t* wire = NULL; size_t size = 0;
      CHECK(asn1_bit_writer_take(writer, &wire, &size) == ASN1_OK);
      cgen4_mM_tOid decoded;
      cgen4_mM_tOid_init(&decoded);
      asn1_bit_reader_t* reader = NULL;
      CHECK(asn1_bit_reader_create(&reader, asn1_byte_view(wire, size)) == ASN1_OK);
      CHECK((aper ? cgen4_mM_tOid_decode_aper(reader, &decoded)
                  : cgen4_mM_tOid_decode_uper(reader, &decoded)) == ASN1_OK);
      CHECK(decoded.count == 4 && memcmp(decoded.data, expected, sizeof(expected)) == 0);
      cgen4_mM_tOid_free(&decoded);
      cgen4_mM_tOid_free(&value);
      asn1_bit_reader_destroy(reader);
      asn1_bit_writer_destroy(writer);
      free(wire);
    }
  }
  {
    for (int aper = 0; aper != 2; ++aper) {
      cgen4_mM_tBits value;
      cgen4_mM_tBits_init(&value);
      value.value.bits = malloc(1);
      CHECK(value.value.bits != NULL);
      value.value.bits[0] = 0xA8;
      value.value.bit_length = 5;
      value.value.cap_bytes = 1;
      asn1_bit_writer_t* writer = NULL;
      CHECK(asn1_bit_writer_create(&writer) == ASN1_OK);
      CHECK((aper ? cgen4_mM_tBits_encode_aper(writer, &value)
                  : cgen4_mM_tBits_encode_uper(writer, &value)) == ASN1_OK);
      uint8_t* wire = NULL; size_t size = 0;
      CHECK(asn1_bit_writer_take(writer, &wire, &size) == ASN1_OK);
      cgen4_mM_tBits decoded;
      cgen4_mM_tBits_init(&decoded);
      asn1_bit_reader_t* reader = NULL;
      CHECK(asn1_bit_reader_create(&reader, asn1_byte_view(wire, size)) == ASN1_OK);
      CHECK((aper ? cgen4_mM_tBits_decode_aper(reader, &decoded)
                  : cgen4_mM_tBits_decode_uper(reader, &decoded)) == ASN1_OK);
      CHECK(decoded.value.bit_length == 5 && decoded.value.bits[0] == 0xA8);
      cgen4_mM_tBits_free(&decoded);
      cgen4_mM_tBits_free(&value);
      asn1_bit_reader_destroy(reader);
      asn1_bit_writer_destroy(writer);
      free(wire);
    }
  }
  {
    const uint8_t expected[] = {0x12, 0x34};
    for (int aper = 0; aper != 2; ++aper) {
      cgen4_mM_tOctets value;
      cgen4_mM_tOctets_init(&value);
      value.data = malloc(sizeof(expected));
      CHECK(value.data != NULL);
      memcpy(value.data, expected, sizeof(expected));
      value.count = value.capacity = sizeof(expected);
      asn1_bit_writer_t* writer = NULL;
      CHECK(asn1_bit_writer_create(&writer) == ASN1_OK);
      CHECK((aper ? cgen4_mM_tOctets_encode_aper(writer, &value)
                  : cgen4_mM_tOctets_encode_uper(writer, &value)) == ASN1_OK);
      uint8_t* wire = NULL; size_t size = 0;
      CHECK(asn1_bit_writer_take(writer, &wire, &size) == ASN1_OK);
      cgen4_mM_tOctets decoded;
      cgen4_mM_tOctets_init(&decoded);
      asn1_bit_reader_t* reader = NULL;
      CHECK(asn1_bit_reader_create(&reader, asn1_byte_view(wire, size)) == ASN1_OK);
      CHECK((aper ? cgen4_mM_tOctets_decode_aper(reader, &decoded)
                  : cgen4_mM_tOctets_decode_uper(reader, &decoded)) == ASN1_OK);
      CHECK(decoded.count == sizeof(expected) && memcmp(decoded.data, expected, sizeof(expected)) == 0);
      cgen4_mM_tOctets_free(&decoded);
      cgen4_mM_tOctets_free(&value);
      asn1_bit_reader_destroy(reader);
      asn1_bit_writer_destroy(writer);
      free(wire);
    }
  }
  {
    for (int aper = 0; aper != 2; ++aper) {
      cgen4_mM_tPair value;
      cgen4_mM_tPair_init(&value);
      value.fleft.value = 6;
      value.fright.value = 1;
      asn1_bit_writer_t* writer = NULL;
      CHECK(asn1_bit_writer_create(&writer) == ASN1_OK);
      CHECK((aper ? cgen4_mM_tPair_encode_aper(writer, &value)
                  : cgen4_mM_tPair_encode_uper(writer, &value)) == ASN1_OK);
      uint8_t* wire = NULL; size_t size = 0;
      CHECK(asn1_bit_writer_take(writer, &wire, &size) == ASN1_OK);
      cgen4_mM_tPair decoded;
      cgen4_mM_tPair_init(&decoded);
      asn1_bit_reader_t* reader = NULL;
      CHECK(asn1_bit_reader_create(&reader, asn1_byte_view(wire, size)) == ASN1_OK);
      CHECK((aper ? cgen4_mM_tPair_decode_aper(reader, &decoded)
                  : cgen4_mM_tPair_decode_uper(reader, &decoded)) == ASN1_OK);
      CHECK(decoded.fleft.value == 6 && decoded.fright.value == 1);
      cgen4_mM_tPair_free(&decoded);
      cgen4_mM_tPair_free(&value);
      asn1_bit_reader_destroy(reader);
      asn1_bit_writer_destroy(writer);
      free(wire);
    }
  }
  for (int aper = 0; aper != 2; ++aper) {
    const uint8_t digits[] = " 907";
    cgen4_mM_tDigits d; cgen4_mM_tDigits_init(&d);
    d.data=(uint8_t*)malloc(sizeof(digits)-1); CHECK(d.data!=NULL);
    memcpy(d.data,digits,sizeof(digits)-1); d.count=d.capacity=sizeof(digits)-1;
    asn1_bit_writer_t* w=NULL; CHECK(asn1_bit_writer_create(&w)==ASN1_OK);
    CHECK((aper?cgen4_mM_tDigits_encode_aper(w,&d):cgen4_mM_tDigits_encode_uper(w,&d))==ASN1_OK);
    uint8_t* wire=NULL; size_t wire_size=0; CHECK(asn1_bit_writer_take(w,&wire,&wire_size)==ASN1_OK);
    asn1_bit_reader_t* r=NULL; CHECK(asn1_bit_reader_create(&r,asn1_byte_view(wire,wire_size))==ASN1_OK);
    cgen4_mM_tDigits decoded; cgen4_mM_tDigits_init(&decoded);
    CHECK((aper?cgen4_mM_tDigits_decode_aper(r,&decoded):cgen4_mM_tDigits_decode_uper(r,&decoded))==ASN1_OK);
    CHECK(decoded.count==sizeof(digits)-1 && memcmp(decoded.data,digits,decoded.count)==0);
    cgen4_mM_tDigits_free(&decoded); cgen4_mM_tDigits_free(&d); free(wire); asn1_bit_reader_destroy(r); asn1_bit_writer_destroy(w);
    cgen4_mM_tDigits invalid_digits;cgen4_mM_tDigits_init(&invalid_digits);invalid_digits.data=(uint8_t*)"A";invalid_digits.count=invalid_digits.capacity=1;
    CHECK(asn1_bit_writer_create(&w)==ASN1_OK);
    CHECK((aper?cgen4_mM_tDigits_encode_aper(w,&invalid_digits):cgen4_mM_tDigits_encode_uper(w,&invalid_digits))==ASN1_ERR_INVALID_ARGUMENT);
    asn1_bit_writer_destroy(w);

    const uint8_t label[] = "RRC-42";
    cgen4_mM_tLabel text; cgen4_mM_tLabel_init(&text);
    text.data=(uint8_t*)malloc(sizeof(label)-1); CHECK(text.data!=NULL);
    memcpy(text.data,label,sizeof(label)-1); text.count=text.capacity=sizeof(label)-1;
    CHECK(asn1_bit_writer_create(&w)==ASN1_OK);
    CHECK((aper?cgen4_mM_tLabel_encode_aper(w,&text):cgen4_mM_tLabel_encode_uper(w,&text))==ASN1_OK);
    CHECK(asn1_bit_writer_take(w,&wire,&wire_size)==ASN1_OK);
    CHECK(asn1_bit_reader_create(&r,asn1_byte_view(wire,wire_size))==ASN1_OK);
    cgen4_mM_tLabel text_out; cgen4_mM_tLabel_init(&text_out);
    CHECK((aper?cgen4_mM_tLabel_decode_aper(r,&text_out):cgen4_mM_tLabel_decode_uper(r,&text_out))==ASN1_OK);
    CHECK(text_out.count==sizeof(label)-1 && memcmp(text_out.data,label,text_out.count)==0);
    cgen4_mM_tLabel_free(&text_out); cgen4_mM_tLabel_free(&text); free(wire); asn1_bit_reader_destroy(r); asn1_bit_writer_destroy(w);

    const uint8_t printable[]="A.B";
    cgen4_mM_tPrinted shown;cgen4_mM_tPrinted_init(&shown);shown.data=(uint8_t*)malloc(sizeof(printable)-1);CHECK(shown.data!=NULL);
    memcpy(shown.data,printable,sizeof(printable)-1);shown.count=shown.capacity=sizeof(printable)-1;
    CHECK(asn1_bit_writer_create(&w)==ASN1_OK);
    CHECK((aper?cgen4_mM_tPrinted_encode_aper(w,&shown):cgen4_mM_tPrinted_encode_uper(w,&shown))==ASN1_OK);
    CHECK(asn1_bit_writer_take(w,&wire,&wire_size)==ASN1_OK);
    CHECK(asn1_bit_reader_create(&r,asn1_byte_view(wire,wire_size))==ASN1_OK);
    cgen4_mM_tPrinted shown_out;cgen4_mM_tPrinted_init(&shown_out);
    CHECK((aper?cgen4_mM_tPrinted_decode_aper(r,&shown_out):cgen4_mM_tPrinted_decode_uper(r,&shown_out))==ASN1_OK);
    CHECK(shown_out.count==sizeof(printable)-1&&memcmp(shown_out.data,printable,shown_out.count)==0);
    cgen4_mM_tPrinted_free(&shown_out);cgen4_mM_tPrinted_free(&shown);free(wire);asn1_bit_reader_destroy(r);asn1_bit_writer_destroy(w);
  }
  for (int aper = 0; aper != 2; ++aper) {
    cgen4_mM_tTree root; cgen4_mM_tTree_init(&root); root.fvalue.value=7;
    root.fchildren=calloc(1,sizeof(*root.fchildren)); CHECK(root.fchildren!=NULL);
    root.fchildren->data=(cgen4_mM_tTree*)calloc(1,sizeof(*root.fchildren->data)); CHECK(root.fchildren->data!=NULL);
    root.fchildren->count=root.fchildren->capacity=1; cgen4_mM_tTree_init(&root.fchildren->data[0]);
    root.fchildren->data[0].fvalue.value=9;
    root.fchildren->data[0].fchildren=calloc(1,sizeof(*root.fchildren->data[0].fchildren)); CHECK(root.fchildren->data[0].fchildren!=NULL);
    asn1_bit_writer_t* writer=NULL; CHECK(asn1_bit_writer_create(&writer)==ASN1_OK);
    CHECK((aper?cgen4_mM_tTree_encode_aper(writer,&root):cgen4_mM_tTree_encode_uper(writer,&root))==ASN1_OK);
    uint8_t* wire=NULL; size_t size=0; CHECK(asn1_bit_writer_take(writer,&wire,&size)==ASN1_OK);
    asn1_bit_reader_t* reader=NULL; CHECK(asn1_bit_reader_create(&reader,asn1_byte_view(wire,size))==ASN1_OK);
    cgen4_mM_tTree decoded; cgen4_mM_tTree_init(&decoded);
    CHECK((aper?cgen4_mM_tTree_decode_aper(reader,&decoded):cgen4_mM_tTree_decode_uper(reader,&decoded))==ASN1_OK);
    CHECK(decoded.fvalue.value==7 && decoded.fchildren && decoded.fchildren->count==1 && decoded.fchildren->data[0].fvalue.value==9);
    cgen4_mM_tTree_free(&decoded); cgen4_mM_tTree_free(&root); free(wire); asn1_bit_reader_destroy(reader); asn1_bit_writer_destroy(writer);
  }
  for (int aper = 0; aper != 2; ++aper) {
    const uint8_t decimal_real[]={0x05,0x02,'1','.','2','5'};
    asn1_bit_reader_t* real_reader=NULL;CHECK(asn1_bit_reader_create(&real_reader,asn1_byte_view(decimal_real,sizeof(decimal_real)))==ASN1_OK);
    cgen4_mM_tMeasurement decimal_value;cgen4_mM_tMeasurement_init(&decimal_value);
    CHECK((aper?cgen4_mM_tMeasurement_decode_aper(real_reader,&decimal_value):cgen4_mM_tMeasurement_decode_uper(real_reader,&decimal_value))==ASN1_OK);
    CHECK(decimal_value.value==1.25);cgen4_mM_tMeasurement_free(&decimal_value);asn1_bit_reader_destroy(real_reader);
    const double values[] = {1.5, -0.0, INFINITY, -INFINITY, NAN};
    for(size_t i=0;i<sizeof(values)/sizeof(values[0]);++i){
      cgen4_mM_tMeasurement value; cgen4_mM_tMeasurement_init(&value); value.value=values[i];
      asn1_bit_writer_t* writer=NULL; CHECK(asn1_bit_writer_create(&writer)==ASN1_OK);
      CHECK((aper?cgen4_mM_tMeasurement_encode_aper(writer,&value):cgen4_mM_tMeasurement_encode_uper(writer,&value))==ASN1_OK);
      uint8_t* wire=NULL;size_t size=0;CHECK(asn1_bit_writer_take(writer,&wire,&size)==ASN1_OK);
      if(i==0){const uint8_t expected_real[]={0x03,0x80,0xff,0x03};CHECK(size==sizeof(expected_real)&&memcmp(wire,expected_real,size)==0);}
      asn1_bit_reader_t* reader=NULL;CHECK(asn1_bit_reader_create(&reader,asn1_byte_view(wire,size))==ASN1_OK);
      cgen4_mM_tMeasurement decoded;cgen4_mM_tMeasurement_init(&decoded);
      CHECK((aper?cgen4_mM_tMeasurement_decode_aper(reader,&decoded):cgen4_mM_tMeasurement_decode_uper(reader,&decoded))==ASN1_OK);
      if(isnan(values[i]))CHECK(isnan(decoded.value));else CHECK(decoded.value==values[i]);
      cgen4_mM_tMeasurement_free(&decoded);free(wire);asn1_bit_reader_destroy(reader);asn1_bit_writer_destroy(writer);
    }
  }
  for (int aper = 0; aper != 2; ++aper) {
    cgen4_mM_tExtendedChoice value; cgen4_mM_tExtendedChoice_init(&value);
    value.kind=cgen4_mM_tExtendedChoice_arm_fextensionValue;
    value.value.fextensionValue.value=42;
    asn1_bit_writer_t* writer=NULL;CHECK(asn1_bit_writer_create(&writer)==ASN1_OK);
    CHECK((aper?cgen4_mM_tExtendedChoice_encode_aper(writer,&value):cgen4_mM_tExtendedChoice_encode_uper(writer,&value))==ASN1_OK);
    uint8_t* wire=NULL;size_t size=0;CHECK(asn1_bit_writer_take(writer,&wire,&size)==ASN1_OK);
    if(!aper){const uint8_t oracle[]={0x80,0x01,0x2a};CHECK(size==sizeof(oracle)&&memcmp(wire,oracle,size)==0);}
    asn1_bit_reader_t* reader=NULL;CHECK(asn1_bit_reader_create(&reader,asn1_byte_view(wire,size))==ASN1_OK);
    cgen4_mM_tExtendedChoice decoded;cgen4_mM_tExtendedChoice_init(&decoded);
    CHECK((aper?cgen4_mM_tExtendedChoice_decode_aper(reader,&decoded):cgen4_mM_tExtendedChoice_decode_uper(reader,&decoded))==ASN1_OK);
    CHECK(decoded.kind==cgen4_mM_tExtendedChoice_arm_fextensionValue && decoded.value.fextensionValue.value==42);
    cgen4_mM_tExtendedChoice_free(&decoded);cgen4_mM_tExtendedChoice_free(&value);
    asn1_bit_reader_destroy(reader);asn1_bit_writer_destroy(writer);free(wire);

    const uint8_t text[]={ 'A' };
    cgen4_mM_tExtendedChoice_init(&value);value.kind=cgen4_mM_tExtendedChoice_arm_fextensionText;
    value.value.fextensionText.data=(uint8_t*)malloc(sizeof(text));CHECK(value.value.fextensionText.data!=NULL);
    memcpy(value.value.fextensionText.data,text,sizeof(text));value.value.fextensionText.count=value.value.fextensionText.capacity=sizeof(text);
    CHECK(asn1_bit_writer_create(&writer)==ASN1_OK);
    CHECK((aper?cgen4_mM_tExtendedChoice_encode_aper(writer,&value):cgen4_mM_tExtendedChoice_encode_uper(writer,&value))==ASN1_OK);
    CHECK(asn1_bit_writer_take(writer,&wire,&size)==ASN1_OK);
    if(!aper){const uint8_t oracle[]={0x81,0x02,0x04,0x10};CHECK(size==sizeof(oracle)&&memcmp(wire,oracle,size)==0);}
    CHECK(asn1_bit_reader_create(&reader,asn1_byte_view(wire,size))==ASN1_OK);
    cgen4_mM_tExtendedChoice_init(&decoded);
    CHECK((aper?cgen4_mM_tExtendedChoice_decode_aper(reader,&decoded):cgen4_mM_tExtendedChoice_decode_uper(reader,&decoded))==ASN1_OK);
    CHECK(decoded.kind==cgen4_mM_tExtendedChoice_arm_fextensionText&&decoded.value.fextensionText.count==sizeof(text));
    CHECK(memcmp(decoded.value.fextensionText.data,text,sizeof(text))==0);
    cgen4_mM_tExtendedChoice_free(&decoded);cgen4_mM_tExtendedChoice_free(&value);
    asn1_bit_reader_destroy(reader);asn1_bit_writer_destroy(writer);free(wire);
  }
  for (int aper = 0; aper != 2; ++aper) {
    cgen4_mM_tTrailingRoot value;cgen4_mM_tTrailingRoot_init(&value);
    value.fhead.value=1;value.has_fextensionValue=1;value.fextensionValue.value=5;
    value.has_ftail=1;value.ftail.value=1;
    asn1_bit_writer_t* writer=NULL;CHECK(asn1_bit_writer_create(&writer)==ASN1_OK);
    CHECK((aper?cgen4_mM_tTrailingRoot_encode_aper(writer,&value):cgen4_mM_tTrailingRoot_encode_uper(writer,&value))==ASN1_OK);
    uint8_t* wire=NULL;size_t size=0;CHECK(asn1_bit_writer_take(writer,&wire,&size)==ASN1_OK);
    if(!aper){const uint8_t oracle[]={0xf0,0x10,0x15,0x00};CHECK(size==sizeof(oracle)&&memcmp(wire,oracle,size)==0);}
    asn1_bit_reader_t* reader=NULL;CHECK(asn1_bit_reader_create(&reader,asn1_byte_view(wire,size))==ASN1_OK);
    cgen4_mM_tTrailingRoot decoded;cgen4_mM_tTrailingRoot_init(&decoded);
    CHECK((aper?cgen4_mM_tTrailingRoot_decode_aper(reader,&decoded):cgen4_mM_tTrailingRoot_decode_uper(reader,&decoded))==ASN1_OK);
    CHECK(decoded.fhead.value==1&&decoded.has_fextensionValue&&decoded.fextensionValue.value==5&&decoded.has_ftail&&decoded.ftail.value==1);
    cgen4_mM_tTrailingRoot_free(&decoded);cgen4_mM_tTrailingRoot_free(&value);
    asn1_bit_reader_destroy(reader);asn1_bit_writer_destroy(writer);free(wire);

    cgen4_mM_tTrailingRoot_init(&value);value.fhead.value=1;value.has_ftail=1;value.ftail.value=1;
    CHECK(asn1_bit_writer_create(&writer)==ASN1_OK);
    CHECK((aper?cgen4_mM_tTrailingRoot_encode_aper(writer,&value):cgen4_mM_tTrailingRoot_encode_uper(writer,&value))==ASN1_OK);
    CHECK(asn1_bit_writer_take(writer,&wire,&size)==ASN1_OK);
    if(!aper){const uint8_t oracle[]={0x70};CHECK(size==sizeof(oracle)&&memcmp(wire,oracle,size)==0);}
    CHECK(asn1_bit_reader_create(&reader,asn1_byte_view(wire,size))==ASN1_OK);
    cgen4_mM_tTrailingRoot_init(&decoded);
    CHECK((aper?cgen4_mM_tTrailingRoot_decode_aper(reader,&decoded):cgen4_mM_tTrailingRoot_decode_uper(reader,&decoded))==ASN1_OK);
    CHECK(decoded.fhead.value==1&&!decoded.has_fextensionValue&&decoded.has_ftail&&decoded.ftail.value==1);
    cgen4_mM_tTrailingRoot_free(&decoded);cgen4_mM_tTrailingRoot_free(&value);
    asn1_bit_reader_destroy(reader);asn1_bit_writer_destroy(writer);free(wire);
  }
  for(int aper=0;aper!=2;++aper) {
    asn1_bit_writer_t* writer=NULL;CHECK(asn1_bit_writer_create(&writer)==ASN1_OK);
    CHECK(asn1_per_encode_enumerated_ex(writer,aper?ASN1_PER_ALIGNED:ASN1_PER_UNALIGNED,0,1,1,1,9)==ASN1_OK);
    const uint8_t unknown_payload[]={0x00};
    CHECK(asn1_per_encode_open_type(writer,aper?ASN1_PER_ALIGNED:ASN1_PER_UNALIGNED,asn1_byte_view(unknown_payload,sizeof(unknown_payload)))==ASN1_OK);
    uint8_t* wire=NULL;size_t size=0;CHECK(asn1_bit_writer_take(writer,&wire,&size)==ASN1_OK);
    asn1_bit_reader_t* reader=NULL;CHECK(asn1_bit_reader_create(&reader,asn1_byte_view(wire,size))==ASN1_OK);
    cgen4_mM_tExtendedChoice decoded;cgen4_mM_tExtendedChoice_init(&decoded);
    decoded.kind=cgen4_mM_tExtendedChoice_arm_frootFlag;decoded.value.frootFlag.value=1;
    CHECK((aper?cgen4_mM_tExtendedChoice_decode_aper(reader,&decoded):cgen4_mM_tExtendedChoice_decode_uper(reader,&decoded))==ASN1_ERR_UNSUPPORTED);
    CHECK(decoded.kind==cgen4_mM_tExtendedChoice_arm_frootFlag&&decoded.value.frootFlag.value==1);
    cgen4_mM_tExtendedChoice_free(&decoded);asn1_bit_reader_destroy(reader);asn1_bit_writer_destroy(writer);free(wire);
  }
  puts("C generated PER PDU: roundtrip, canonical vector and transactional truncation checks PASS");
  return 0;
}
