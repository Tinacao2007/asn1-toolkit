#include "generated.h"
#include <stdlib.h>
int second(void);
int main(void) {
  asn1_gen_mM_tRecord value, decoded;
  asn1_gen_mM_tRecord_init(&value);
  asn1_gen_mM_tRecord_init(&decoded);
  value.fcount.value=42;
  asn1_byte_writer_t* writer=NULL;
  asn1_byte_reader_t* reader=NULL;
  int failed=asn1_byte_writer_create(&writer)!=ASN1_OK;
  if(!failed) failed=asn1_gen_mM_tRecord_encode_ber(writer,&value)!=ASN1_OK;
  if(!failed) failed=asn1_byte_reader_create(&reader,asn1_byte_writer_view(writer))!=ASN1_OK;
  if(!failed) failed=asn1_gen_mM_tRecord_decode_ber(reader,&decoded)!=ASN1_OK || decoded.fcount.value!=42;
  asn1_byte_reader_destroy(reader);
  asn1_byte_writer_destroy(writer);
  asn1_gen_mM_tRecord_free(&value);
  asn1_gen_mM_tRecord_free(&decoded);
  return failed || second();
}
