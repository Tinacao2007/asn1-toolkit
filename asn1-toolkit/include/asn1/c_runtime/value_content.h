#ifndef ASN1_C_VALUE_CONTENT_H
#define ASN1_C_VALUE_CONTENT_H
#include <asn1/c_runtime/asn1_c_common.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Shared X.690 contents used by the PER and TLV families. Returned buffers
   are owned by the caller. Failure preserves output parameters. */
asn1_status_t asn1_value_encode_real(double,uint8_t**,size_t*);
asn1_status_t asn1_value_decode_real(asn1_byte_view_t,double*);
asn1_status_t asn1_value_encode_oid(const uint64_t*,size_t,int,uint8_t**,size_t*);
asn1_status_t asn1_value_decode_oid(asn1_byte_view_t,uint64_t**,size_t*,int);
#ifdef __cplusplus
}
#endif
#endif
