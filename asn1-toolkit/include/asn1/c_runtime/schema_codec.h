#ifndef ASN1_C_SCHEMA_CODEC_H
#define ASN1_C_SCHEMA_CODEC_H
#include <asn1/c_runtime/tlv_schema.h>
#include <asn1/c_runtime/oer.h>
#include <asn1/c_runtime/text_tree.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Target-independent storage comes from the existing runtime layout schema.
   These additional descriptors carry wire planning and names, not compiler IR. */
typedef struct asn1_codec_schema asn1_codec_schema_t;
typedef const asn1_codec_schema_t* (*asn1_codec_schema_fn)(void);
typedef struct {
  asn1_codec_schema_fn type;
  const char *name,*instruction_name;
  size_t group; /* SIZE_MAX=root, otherwise shared extension-group ordinal */
  int nested_group,optional;
  unsigned flags;
  asn1_ber_tag_t tag;
} asn1_codec_field_t;
typedef struct {int64_t value;const char *name,*xml_name;} asn1_codec_enum_t;
enum { ASN1_SCHEMA_ARRAY=1, ASN1_SCHEMA_BASE64=2, ASN1_SCHEMA_OBJECT=4,
  ASN1_SCHEMA_UNWRAPPED=8,
  ASN1_SCHEMA_ATTRIBUTE=256, ASN1_SCHEMA_XML_BASE64=512, ASN1_SCHEMA_TEXT=1024,
  ASN1_SCHEMA_NUMBER=2048, ASN1_SCHEMA_LIST=4096, ASN1_SCHEMA_UNTAGGED=8192,
  ASN1_SCHEMA_NIL=16384 };
struct asn1_codec_schema {
  asn1_tlv_schema_fn layout;
  const char* name;
  asn1_codec_schema_fn element;
  const asn1_codec_field_t* fields;
  const asn1_codec_enum_t* enumerators;
  size_t enumerator_count;
  asn1_oer_integer_form_t integer_form;
  size_t fixed_size; /* SIZE_MAX=variable; bits/bytes/elements as appropriate */
  unsigned flags;
  const size_t* root_order;
  size_t root_count,group_count;
  unsigned real_form; /* 0=BER contents, 1=IEEE binary32, 2=IEEE binary64 */
};
typedef enum {ASN1_CODEC_OER,ASN1_CODEC_COER,ASN1_CODEC_JER,ASN1_CODEC_JERI,
  ASN1_CODEC_XER,ASN1_CODEC_CXER,ASN1_CODEC_EXER} asn1_schema_codec_t;
/* Byte writer also carries UTF-8 text. Public calls are transactional and
   decoders accept one binary PDU or an entire JSON/XML document. */
asn1_status_t asn1_schema_encode(asn1_byte_writer_t*,asn1_schema_codec_t,const asn1_codec_schema_t*,const void*);
asn1_status_t asn1_schema_decode(asn1_byte_reader_t*,asn1_schema_codec_t,const asn1_codec_schema_t*,void*);
#ifdef __cplusplus
}
#endif
#endif
