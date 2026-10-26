#ifndef ASN1_C_TLV_H
#define ASN1_C_TLV_H
#include <asn1/c_runtime/ber_tlv.h>
#include <asn1/c_runtime/byte_io.h>
#ifdef __cplusplus
extern "C" {
#endif
/* DER and CER are restrictions of the same BER implementation. */
typedef enum { ASN1_TLV_BER, ASN1_TLV_DER, ASN1_TLV_CER } asn1_tlv_policy_t;
typedef struct {
  asn1_ber_tag_t tag;
  int indefinite;
  asn1_byte_view_t content;  /* excludes the terminating EOC */
  asn1_byte_view_t encoding; /* includes header and EOC */
} asn1_tlv_t;
/* Parse one complete envelope. No allocations or caller mutations on failure.
   Reject standalone EOC, primitive indefinite length and malformed headers.
   Indefinite children are bounded by ASN1_MAX_RECURSION_DEPTH. */
asn1_status_t asn1_tlv_parse(asn1_byte_view_t input, asn1_tlv_policy_t policy, asn1_tlv_t* out);
asn1_status_t asn1_tlv_read(asn1_byte_reader_t* reader, asn1_tlv_policy_t policy, asn1_tlv_t* out);
asn1_status_t asn1_tlv_write(asn1_byte_writer_t* writer, asn1_tlv_policy_t policy,
                            asn1_ber_tag_t tag, asn1_byte_view_t content);
/* Complete primitive value codecs. Decoder outputs are transactional.
   Integer decode requires an initialized, empty BigInt destination; octet
   decode transfers a new malloc buffer to *data on success (caller frees it). */
asn1_status_t asn1_tlv_encode_boolean(asn1_byte_writer_t*,asn1_tlv_policy_t,int);
asn1_status_t asn1_tlv_decode_boolean(const asn1_tlv_t*,asn1_tlv_policy_t,int*);
asn1_status_t asn1_tlv_encode_integer(asn1_byte_writer_t*,asn1_tlv_policy_t,const asn1_bigint_t*);
asn1_status_t asn1_tlv_decode_integer(const asn1_tlv_t*,asn1_bigint_t*);
/* OCTET STRING content; canonical CER splits at 1000 octets. */
asn1_status_t asn1_tlv_encode_octets(asn1_byte_writer_t*,asn1_tlv_policy_t,asn1_byte_view_t);
asn1_status_t asn1_tlv_decode_octets(const asn1_tlv_t*,asn1_tlv_policy_t,uint8_t**,size_t*);
#ifdef __cplusplus
}
#endif
#endif
