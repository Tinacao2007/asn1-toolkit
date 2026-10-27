/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/include/asn1/c_runtime/cxer.h
**   $Version: 0.1
**   $Date:   2026-10-17
**   $Author: tina.cao
***************************************************************************/
/** File Description:

    Canonical XER (X.693 clause 9), implemented as XER plus policy.
    Canonical lexical form, REAL, DEFAULT presence and SET/SET OF order
    are enforced by schema_codec.h. ENUMERATED names remain names;
    USE-NUMBER belongs to E-XER instructions, not CXER.

    See docs/changelog.md for the supported C value profile.
***************************************************************************/
#ifndef ASN1_C_CXER_H
#define ASN1_C_CXER_H

#include <asn1/c_runtime/xer.h>

#ifdef __cplusplus
extern "C" {
#endif

/* TODO sub-task 2: asn1_cxer_encode_<Type> / asn1_cxer_decode_<Type>
   per generated stub. */

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* ASN1_C_CXER_H */
