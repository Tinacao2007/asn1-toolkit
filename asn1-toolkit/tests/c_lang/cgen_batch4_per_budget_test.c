#include "generated.h"
#include <stdio.h>
#include <stdlib.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x); return 1; } } while (0)

int main(void) {
#if ASN1_MAX_PDU_ELEMENTS <= 32
  const size_t count=20; /* each list fits; their combined element count does not */
#else
  const size_t count=12; /* each list fits 64 bytes; their combined allocation does not */
#endif
  cgen4_mM_tTwoLists input; cgen4_mM_tTwoLists_init(&input);
  input.ffirst.data=calloc(count,sizeof(*input.ffirst.data)); CHECK(input.ffirst.data!=NULL);
  input.ffirst.count=input.ffirst.capacity=count;
  input.fsecond.data=calloc(count,sizeof(*input.fsecond.data)); CHECK(input.fsecond.data!=NULL);
  input.fsecond.count=input.fsecond.capacity=count;
  asn1_bit_writer_t* writer=NULL; CHECK(asn1_bit_writer_create(&writer)==ASN1_OK);
  CHECK(cgen4_mM_tTwoLists_encode_uper(writer,&input)==ASN1_OK);
  uint8_t* wire=NULL; size_t wire_size=0; CHECK(asn1_bit_writer_take(writer,&wire,&wire_size)==ASN1_OK);
  asn1_bit_reader_t* reader=NULL; CHECK(asn1_bit_reader_create(&reader,asn1_byte_view(wire,wire_size))==ASN1_OK);
  cgen4_mM_tTwoLists output; cgen4_mM_tTwoLists_init(&output);
  output.ffirst.data=calloc(1,sizeof(*output.ffirst.data)); CHECK(output.ffirst.data!=NULL);
  output.ffirst.count=output.ffirst.capacity=1; output.ffirst.data[0].value=1;
  CHECK(cgen4_mM_tTwoLists_decode_uper(reader,&output)==ASN1_ERR_RESOURCE_LIMIT);
  CHECK(output.ffirst.count==1 && output.ffirst.data[0].value==1 && output.fsecond.count==0);
  cgen4_mM_tTwoLists_free(&input); cgen4_mM_tTwoLists_free(&output);
  asn1_bit_reader_destroy(reader); asn1_bit_writer_destroy(writer); free(wire);
  puts("Generated C PDU: cumulative byte/element budget rejects without changing output PASS");
  return 0;
}
