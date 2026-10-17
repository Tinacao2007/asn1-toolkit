/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/include/asn1/c_runtime/aper.h
**   $Version: 0.1
**   $Date:   2026-10-17
**   $Author: tina.cao
***************************************************************************/
/**  File Description:

    P2-4 (--lang c) Aligned PER entry point forwarding header.

    C11 port of asn1-toolkit/include/asn1/runtime/aper.hpp. APER differs
    from UPER only in length determinants and a few type-specific
    alignments (X.691 11.5.7); the C port mirrors that with an aper_
    prefix on each per_ primitive that needs the aligned form.

    Design Spec: asn1-toolkit/docs/todo.md#p2-4--c-语言代码生成器-lang-c
***************************************************************************/
#ifndef ASN1_C_APER_H
#define ASN1_C_APER_H

#include <asn1/c_runtime/per.h>

/* TODO sub-task 2: aper_-prefixed variants of the constrained-length
   determinant + open type + the alignment-sensitive cases (X.691
   11.5.7.4). */

/* APER entry — same surface as UPER for types where alignment does not
   affect the wire (most SEQUENCE/CHOICE/INTEGER); sub-task 2 will add
   aper-specific variants where required. */
#define asn1_aper_encode_bool asn1_per_encode_bool
#define asn1_aper_decode_bool asn1_per_decode_bool

#endif /* ASN1_C_APER_H */