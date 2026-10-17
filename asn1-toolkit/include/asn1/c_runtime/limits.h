/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/include/asn1/c_runtime/limits.h
**   $Version: 0.1
**   $Date:   2026-10-17
**   $Author: tina.cao
***************************************************************************/
/**  File Description:

    P2-4 (--lang c) hard limits for the decoder.

    C11 port of asn1-toolkit/include/asn1/runtime/limits.hpp. The C++
    version stores its limits in `namespace asn1` as `constexpr` values;
    the C port surfaces them as #define constants so they can be used
    in static array declarations and preprocessor `#if` checks.

    Design Spec: asn1-toolkit/docs/todo.md#p2-4--c-语言代码生成器-lang-c
***************************************************************************/
#ifndef ASN1_C_LIMITS_H
#define ASN1_C_LIMITS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ASN1_MAX_RECURSION_DEPTH      256   /* ASN1_MAX_RECURSION_DEPTH */
#define ASN1_MAX_STRING_BYTES         (64u * 1024u) /* OCTET STRING cap */
#define ASN1_MAX_BIGINT_LIMBS         1024   /* INTEGER limb cap (32 bits each) */
#define ASN1_MAX_CHOICE_ALTS          256    /* CHOICE alternatives cap */

/* TODO sub-task 2: derive limits from per_codec REQUIREMENT (e.g. RRC may
   need higher OCTET STRING cap than LPP); until then, single global
   constant set is fine. */

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* ASN1_C_LIMITS_H */