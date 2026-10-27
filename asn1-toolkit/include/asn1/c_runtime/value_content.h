#ifndef ASN1_C_VALUE_CONTENT_H
#define ASN1_C_VALUE_CONTENT_H
#include <asn1/c_runtime/asn1_c_common.h>
#ifdef __cplusplus
extern "C" {
#endif
/* UTCTime X.680 47 / X.690 11.8. Exact Gregorian date validation,
   +/-hhmm offsets and omitted seconds. Canonical output is 13 bytes YYMMDDhhmmssZ.
   No locale, timezone database, allocation or process-global state. */
asn1_status_t asn1_utctime_validate(asn1_byte_view_t);
asn1_status_t asn1_utctime_normalize(asn1_byte_view_t,uint8_t out[13]);
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
