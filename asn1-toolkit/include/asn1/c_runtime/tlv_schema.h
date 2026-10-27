#ifndef ASN1_C_TLV_SCHEMA_H
#define ASN1_C_TLV_SCHEMA_H
#include <asn1/c_runtime/tlv.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Generated storage descriptors. These are runtime ABI metadata, never IR/AST
   objects; only generated C includes both schema decisions and runtime types. */
typedef struct asn1_tlv_schema asn1_tlv_schema_t;
typedef const asn1_tlv_schema_t* (*asn1_tlv_schema_fn)(void);
typedef enum {
  ASN1_TLV_ALIAS, ASN1_TLV_BOOLEAN, ASN1_TLV_INT64, ASN1_TLV_BIGINT,
  ASN1_TLV_NULL, ASN1_TLV_OCTETS, ASN1_TLV_BITS, ASN1_TLV_OPEN,
  ASN1_TLV_SEQUENCE, ASN1_TLV_SET, ASN1_TLV_CHOICE, ASN1_TLV_SEQUENCE_OF, ASN1_TLV_SET_OF,
  ASN1_TLV_REAL, ASN1_TLV_OID, ASN1_TLV_RELATIVE_OID
} asn1_tlv_kind_t;
typedef struct {
  asn1_tlv_schema_fn type;
  size_t offset,presence;
  int indirect,explicit_tag;
  asn1_ber_tag_t tag; /* cls=255 means no component override */
  int (*is_default)(const void*);
  asn1_status_t (*default_init)(void*);
  size_t default_bytes;
  int extension;
} asn1_tlv_field_t;
struct asn1_tlv_schema {
  asn1_tlv_kind_t kind;
  size_t size;
  uint64_t universal_tag;
  asn1_ber_tag_t tag;
  int explicit_tag,extensible,indirect;
  size_t value_offset,data_offset,count_offset,capacity_offset;
  asn1_tlv_schema_fn element;
  const asn1_tlv_field_t* fields;
  size_t field_count;
  void (*init)(void*);
  void (*clear)(void*);
  asn1_status_t (*validate)(const void*);
  size_t (*choice_get)(const void*);
  void (*choice_set)(void*,size_t);
  asn1_status_t (*named_length)(size_t,size_t*); /* NULL for ordinary BIT STRING */
  int (*has_selected)(const void*); /* typed views require a codec-aware adapter */
};
asn1_status_t asn1_tlv_schema_encode(asn1_byte_writer_t*,asn1_tlv_policy_t,
                                    const asn1_tlv_schema_t*,const void*);
/* Value and reader cursor are transactional. A successful decode consumes one
   TLV, allowing the caller to decode a concatenated stream explicitly. */
asn1_status_t asn1_tlv_schema_decode(asn1_byte_reader_t*,asn1_tlv_policy_t,
                                    const asn1_tlv_schema_t*,void*);
/* Validates UTF-8 / restricted ASCII repertoires and counts ASN.1 characters. */
asn1_status_t asn1_tlv_string_length(uint64_t tag,asn1_byte_view_t,size_t*);
#ifdef __cplusplus
}
#endif
#endif
