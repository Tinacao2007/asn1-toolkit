/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/src/codegen/sha256.hpp
**   $Version: 0.1
**   $Date:   2026-10-16
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   Minimal SHA-256 used for the emit content-hash cache (P2-3). The cache
**   needs a stable content fingerprint; cryptographic strength is not a
**   concern (collisions only cost a stale cache miss). Self-contained:
**   no OpenSSL dependency, no extra runtime link, and a public API that
**   only depends on <cstdint>/<string>/<string_view>/<filesystem>.
**
** Specification: ITU-T X.680 / X.690 series; SHA-256 per FIPS 180-4.
** Design Spec:   asn1-toolkit/docs/architecture.md
**                 asn1-toolkit/README.md
***************************************************************************/
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace asn1 {
namespace codegen {

// 64-character lowercase hex digest (32 raw bytes).
std::string sha256_hex(std::string_view bytes);

// Hash the contents of an existing file; returns empty string if the file
// cannot be opened/read (caller decides how to react: usually treat as
// cache miss / force rebuild).
std::string sha256_file_hex(const std::filesystem::path& path);

}  // namespace codegen
}  // namespace asn1
