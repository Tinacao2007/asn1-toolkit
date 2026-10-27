#ifndef ASN1_SCHEMA_INTERNAL_H
#define ASN1_SCHEMA_INTERNAL_H
#include <asn1/c_runtime/schema_codec.h>
#include <asn1/c_runtime/limits.h>
#include <asn1/c_runtime/bit_string.h>
#include <string.h>
typedef struct {size_t bytes,elements;} asn1_schema_budget_t;
asn1_status_t asn1_schema_charge(asn1_schema_budget_t*,size_t,size_t);
asn1_status_t asn1_schema_allocate(asn1_schema_budget_t*,size_t,size_t,void**);
asn1_status_t asn1_schema_named_finish(const asn1_tlv_schema_t*,void*,asn1_schema_budget_t*);
asn1_status_t asn1_schema_named_bits(const asn1_tlv_schema_t*,const asn1_bitstring_t*,int,asn1_bitstring_t*);
asn1_status_t asn1_schema_selection(const asn1_codec_field_t*,const void*,const asn1_codec_selection_t**);
asn1_status_t asn1_schema_selected_value(const asn1_codec_field_t*,const void*,const asn1_codec_selection_t**,const void**);
asn1_status_t asn1_schema_selected_destination(const asn1_codec_field_t*,void*,const asn1_codec_selection_t*,asn1_schema_budget_t*,void**);
const void* asn1_schema_field_value(const void*,const asn1_tlv_field_t*);
int asn1_schema_field_present(const void*,const asn1_tlv_field_t*);
asn1_status_t asn1_schema_field_destination(void*,const asn1_tlv_field_t*,asn1_schema_budget_t*,void**);
asn1_status_t asn1_schema_binary_encode(asn1_byte_writer_t*,asn1_byte_view_t,int base64);
asn1_status_t asn1_schema_binary_decode(asn1_byte_view_t,int,uint8_t**,size_t*,asn1_schema_budget_t*);
asn1_status_t asn1_schema_json_encode(asn1_byte_writer_t*,const asn1_codec_schema_t*,const void*,size_t);
asn1_status_t asn1_schema_json_decode(const asn1_text_node_t*,const asn1_codec_schema_t*,void*,asn1_schema_budget_t*,size_t);
asn1_status_t asn1_schema_xml_encode(asn1_byte_writer_t*,asn1_schema_codec_t,const asn1_codec_schema_t*,const void*,const char*,size_t,unsigned);
asn1_status_t asn1_schema_xml_decode(const asn1_text_node_t*,asn1_schema_codec_t,const asn1_codec_schema_t*,void*,asn1_schema_budget_t*,size_t,unsigned);
asn1_status_t asn1_schema_oer_encode(asn1_byte_writer_t*,asn1_oer_policy_t,const asn1_codec_schema_t*,const void*,size_t);
asn1_status_t asn1_schema_oer_decode(asn1_byte_reader_t*,asn1_oer_policy_t,const asn1_codec_schema_t*,void*,asn1_schema_budget_t*,size_t);
#endif
