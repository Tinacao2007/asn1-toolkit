#include "generated.h"
#include <stdlib.h>
int second(void) {
  asn1_gen_mM_tRecord value,decoded;
  asn1_gen_mM_tRecord_init(&value);
  asn1_gen_mM_tRecord_init(&decoded);
  value.fcount.value=17;
  asn1_bit_writer_t* writer=NULL;
  asn1_bit_reader_t* reader=NULL;
  uint8_t* bytes=NULL;
  size_t size=0;
  int failed=asn1_bit_writer_create(&writer)!=ASN1_OK;
  if(!failed) failed=asn1_gen_mM_tRecord_encode_uper(writer,&value)!=ASN1_OK;
  if(!failed) failed=asn1_bit_writer_take(writer,&bytes,&size)!=ASN1_OK;
  if(!failed) failed=asn1_bit_reader_create(&reader,asn1_byte_view(bytes,size))!=ASN1_OK;
  if(!failed) failed=asn1_gen_mM_tRecord_decode_uper(reader,&decoded)!=ASN1_OK || decoded.fcount.value!=17;
  asn1_bit_reader_destroy(reader);
  asn1_bit_writer_destroy(writer);
  free(bytes);
  asn1_gen_mM_tRecord_free(&value);
  asn1_gen_mM_tRecord_free(&decoded);
  return failed;
}
