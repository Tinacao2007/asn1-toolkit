/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/include/asn1/c_runtime/coer.h
**   $Version: 0.1
**   $Date:   2026-10-17
**   $Author: tina.cao
***************************************************************************/
/** File Description:

    Canonical OER (X.696 clause 31), implemented as OER plus policy.
    Length/integer/BOOLEAN/quantity/default and SET OF restrictions are
    selected per entry. Primitives use oer.h; generated structures use
    schema_codec.h. This legacy family header is not the generated API.

    See docs/changelog.md for the supported C value profile.
***************************************************************************/
#ifndef ASN1_C_COER_H
#define ASN1_C_COER_H

#include <asn1/c_runtime/oer.h>

#ifdef __cplusplus
extern "C" {
#endif

/* TODO sub-task 2: asn1_coer_encode_<Type> / asn1_coer_decode_<Type>
   per generated stub. */

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* ASN1_C_COER_H */
