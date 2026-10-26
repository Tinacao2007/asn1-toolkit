#ifndef ASN1_C_TEXT_TREE_H
#define ASN1_C_TEXT_TREE_H
#include <asn1/c_runtime/byte_io.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Lossless text syntax tree. Numbers retain their original decimal spelling.
 * Strings/names are UTF-8 with explicit lengths; embedded NUL is supported.
 * XML attributes and children have separate lists. No external entities. */
typedef enum { ASN1_TEXT_NULL, ASN1_TEXT_BOOL, ASN1_TEXT_NUMBER,
  ASN1_TEXT_STRING, ASN1_TEXT_ARRAY, ASN1_TEXT_OBJECT, ASN1_TEXT_ELEMENT } asn1_text_kind_t;
typedef struct asn1_text_node {
  asn1_text_kind_t kind;
  char *name, *text;
  size_t name_size, text_size;
  size_t text_offset; /* XML child: position in parent's concatenated text */
  struct asn1_text_node *children, *attributes, *next;
} asn1_text_node_t;
void asn1_text_tree_free(asn1_text_node_t*);
/* Parse an entire document. Success transfers ownership to *out; failure leaves
 * *out unchanged and reports the input byte offset through error_offset.
 * Depth, node, decoded string and aggregate allocation limits apply. */
asn1_status_t asn1_json_parse(asn1_byte_view_t,asn1_text_node_t**,size_t* error_offset);
asn1_status_t asn1_xml_parse(asn1_byte_view_t,asn1_text_node_t**,size_t* error_offset);
/* Escapes one UTF-8 string/attribute into an atomic append. JSON includes quotes.
 * XML escapes text, or attribute text when attribute is nonzero. */
asn1_status_t asn1_json_quote(asn1_byte_writer_t*,asn1_byte_view_t);
asn1_status_t asn1_xml_escape(asn1_byte_writer_t*,asn1_byte_view_t,int attribute);
#ifdef __cplusplus
}
#endif
#endif
