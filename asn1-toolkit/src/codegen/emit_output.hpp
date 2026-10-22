/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/src/codegen/emit_output.hpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   Implementation source for codegen / emit_output.hpp.
**
** Specification: Generated code layout follows ITU type rules; emission
**                 logic is toolchain-internal.
** Design Spec:   asn1-toolkit/docs/architecture.md
**                 asn1-toolkit/README.md
***************************************************************************/
#pragma once
#include <asn1/codegen/emit.hpp>
namespace asn1 { namespace codegen {
bool portable_output_name(const std::string& name);
std::string portable_output_key(std::string name);
bool validate_emit_options(const EmitOptions&, Diagnostics&);
void emit_directory(const ir::Model&, EmitOptions, Diagnostics&,
                    const std::filesystem::path&, bool split);
}}
