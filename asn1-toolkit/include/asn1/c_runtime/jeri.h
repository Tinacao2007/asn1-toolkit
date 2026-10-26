/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/include/asn1/c_runtime/jeri.h
**   $Version: 0.1
**   $Date:   2026-10-17
**   $Author: tina.cao
***************************************************************************/
/** File Description:

    Project JER instruction entry using the shared X.697 JSON syntax.
    It preserves structural wrappers and is not a canonical-JSON format.
    This legacy helper header forwards to JER; generated JER/JERI value
    encode/decode APIs use schema_codec.h and the complete JSON parser.

    See docs/cgen-batch6-validation.md for the supported C value profile.
***************************************************************************/
#ifndef ASN1_C_JERI_H
#define ASN1_C_JERI_H

#include <asn1/c_runtime/jer.h>

#ifdef __cplusplus
extern "C" {
#endif

/* TODO sub-task 2: asn1_jeri_encode_<Type> / asn1_jeri_decode_<Type>
   per generated stub. */

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* ASN1_C_JERI_H */