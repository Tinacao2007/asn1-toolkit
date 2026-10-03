# Telecom ASN.1 fixtures (Phase 14 / 38 / 39)

Copies of 3GPP ASN.1 modules used as **parse / analyze / emit-gap gates**,
plus a slim RRC slice for **codec round-trips**. They are **not** normative
specifications.

| Fixture | Source | Role |
|---------|--------|------|
| `rrc_8_6_0/EUTRA-RRC-Definitions.asn` | `Python_asn1tools/tests/files/3gpp/rrc_8_6_0.asn` (first module) | Parse/analyze; emit reports anonymous nesting |
| `lpp_14_3_0/LPP-PDU-Definitions.asn` | `Python_asn1tools/tests/files/3gpp/lpp_14_3_0.asn` | Parse/analyze; emit reports anonymous nesting |
| `s1ap_14_4_0/s1ap_14_4_0.asn` | `Python_asn1tools/tests/files/3gpp/s1ap_14_4_0.asn` | Multi-module IOC; emit reports unresolved ProtocolIE fields |
| `rrc_slice/rrc_slice.asn` | Adapted from RRC Rel-8 BCCH-BCH / PCCH / Paging | UPER/OER emit + UPER round-trips |

License / copyright: original 3GPP ASN.1; redistributed here only as compiler test input.
