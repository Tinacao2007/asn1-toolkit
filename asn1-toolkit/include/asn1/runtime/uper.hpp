/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/include/asn1/runtime/uper.hpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   Thin include forwarding to unaligned PER (UPER) entry points.
**
** Specification: ITU-T X.691 — ASN.1 encoding rules: Packed Encoding
**                 Rules (PER); UPER/APER variants.
** Design Spec:   asn1-toolkit/docs/ARCHITECTURE.md
**                 asn1-toolkit/README.md
***************************************************************************/
#pragma once

/// Unaligned PER entry points. Implementation lives in asn1::per with
/// Variant::Unaligned; see asn1/runtime/per/codec.hpp.
#include <asn1/runtime/per/codec.hpp>
