include(FetchContent)

# Offline override: point this at a local GoogleTest source tree to build
# without network access, e.g.
#   cmake -S . -B build -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=/path/to/googletest
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

add_executable(asn1_common_tests common/result_span_test.cpp)
target_link_libraries(asn1_common_tests PRIVATE asn1::common GTest::gtest_main)
gtest_discover_tests(asn1_common_tests)

add_executable(asn1_lexer_tests lexer/lexer_test.cpp)
target_link_libraries(asn1_lexer_tests PRIVATE asn1::frontend GTest::gtest_main)
gtest_discover_tests(asn1_lexer_tests)

add_executable(asn1_parser_tests parser/parser_test.cpp)
target_link_libraries(asn1_parser_tests PRIVATE asn1::frontend GTest::gtest_main)
gtest_discover_tests(asn1_parser_tests)

add_executable(asn1_semantic_tests semantic/semantic_test.cpp)
target_link_libraries(asn1_semantic_tests PRIVATE asn1::frontend asn1::semantic GTest::gtest_main)
gtest_discover_tests(asn1_semantic_tests)

# P3-1 parallel-frontend unit tests. Covers Diagnostics::merge byte-order,
# SymbolTable::merge, and the parallel-vs-serial diagnostic equivalence gate.
add_executable(asn1_parallel_analyzer_tests
               semantic/parallel_analyzer_test.cpp)
target_link_libraries(asn1_parallel_analyzer_tests PRIVATE asn1::frontend
                      asn1::semantic GTest::gtest_main)
gtest_discover_tests(asn1_parallel_analyzer_tests)

# P2-4 (--lang c) sub-task 1: C runtime header skeleton sanity check.
# Sub-task 1 only validates that the headers parse cleanly under
# `gcc -std=c11 -fsyntax-only -Wall -Wextra`. No runtime body is needed
# (sub-task 2 fills them in), so the gate is a build-time compile-only
# check rather than a runnable executable.
add_custom_command(
  OUTPUT asn1_c_header_syntax_test.stamp
  COMMAND ${CMAKE_C_COMPILER} -std=c11 -Wall -Wextra -fsyntax-only
          -I${CMAKE_CURRENT_SOURCE_DIR}/../include
          -c ${CMAKE_CURRENT_SOURCE_DIR}/c_lang/header_syntax_test.c
          -o ${CMAKE_CURRENT_BINARY_DIR}/header_syntax_test.c.o
  COMMENT "P2-4 sub-task 1: syntax-checking c_lang/header_syntax_test.c"
  VERBATIM)
add_custom_target(asn1_c_header_syntax_test DEPENDS
                   asn1_c_header_syntax_test.stamp)
add_test(NAME Asn1CHeaderSyntax COMMAND ${CMAKE_COMMAND} -E echo
         "P2-4 sub-task 1 c runtime headers pass fsyntax-only gate"
         "(see asn1_c_header_syntax_test target build output)")

add_executable(asn1_constraints_tests constraints/constraints_test.cpp)
target_link_libraries(asn1_constraints_tests PRIVATE asn1::frontend asn1::constraints asn1::semantic GTest::gtest_main)
gtest_discover_tests(asn1_constraints_tests)

add_executable(asn1_ber_tests ber/ber_test.cpp)
target_link_libraries(asn1_ber_tests PRIVATE asn1::ber GTest::gtest_main)
gtest_discover_tests(asn1_ber_tests)

add_executable(asn1_bigint_tests runtime/bigint_codec_test.cpp)
target_link_libraries(asn1_bigint_tests PRIVATE asn1::ber asn1::der asn1::per asn1::oer
                      asn1::coer asn1::xer asn1::jer GTest::gtest_main)
gtest_discover_tests(asn1_bigint_tests)

add_executable(asn1_der_tests der/der_test.cpp)
target_link_libraries(asn1_der_tests PRIVATE asn1::der GTest::gtest_main)
gtest_discover_tests(asn1_der_tests)

add_executable(asn1_per_tests per/per_primitives_test.cpp)
target_link_libraries(asn1_per_tests PRIVATE asn1::per GTest::gtest_main)
gtest_discover_tests(asn1_per_tests)

# Differential PER validation against externally-generated golden vectors
# (asn1tools via scripts/gen_per_golden.py). The generated.hpp comes from the
# per_golden.asn fixture; the vectors are checked in under tests/per/golden/.
set(ASN1_PER_GOLDEN_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated_per_golden")
add_custom_command(
  OUTPUT "${ASN1_PER_GOLDEN_GEN_DIR}/generated.hpp" "${ASN1_PER_GOLDEN_GEN_DIR}/generated.cpp"
  COMMAND asn1cxx --emit-dir "${ASN1_PER_GOLDEN_GEN_DIR}" --codec per
          --namespace pg
          "${CMAKE_SOURCE_DIR}/tests/fixtures/telecom/per_golden.asn"
  DEPENDS asn1cxx "${CMAKE_SOURCE_DIR}/tests/fixtures/telecom/per_golden.asn"
  COMMENT "Generating UPER/APER C++ for telecom per_golden.asn"
  VERBATIM
)
add_executable(asn1_per_golden_tests
  per/per_golden_vectors_test.cpp
  "${ASN1_PER_GOLDEN_GEN_DIR}/generated.cpp"
)
target_include_directories(asn1_per_golden_tests PRIVATE "${ASN1_PER_GOLDEN_GEN_DIR}")
target_compile_definitions(asn1_per_golden_tests PRIVATE
  ASN1_PER_GOLDEN_FILE="${CMAKE_SOURCE_DIR}/tests/per/golden/per_golden_vectors.txt")
target_link_libraries(asn1_per_golden_tests PRIVATE asn1::per GTest::gtest_main)
gtest_discover_tests(asn1_per_golden_tests)

add_executable(asn1_uper_tests uper/uper_test.cpp)
target_link_libraries(asn1_uper_tests PRIVATE asn1::per GTest::gtest_main)
gtest_discover_tests(asn1_uper_tests)

add_executable(asn1_aper_tests aper/aper_test.cpp)
target_link_libraries(asn1_aper_tests PRIVATE asn1::per GTest::gtest_main)
gtest_discover_tests(asn1_aper_tests)

add_executable(asn1_oer_tests oer/oer_test.cpp)
target_link_libraries(asn1_oer_tests PRIVATE asn1::oer GTest::gtest_main)
gtest_discover_tests(asn1_oer_tests)

add_executable(asn1_coer_tests coer/coer_test.cpp)
target_link_libraries(asn1_coer_tests PRIVATE asn1::coer GTest::gtest_main)
gtest_discover_tests(asn1_coer_tests)

add_executable(asn1_xer_tests xer/xer_test.cpp)
target_link_libraries(asn1_xer_tests PRIVATE asn1::xer GTest::gtest_main)
gtest_discover_tests(asn1_xer_tests)

add_executable(asn1_cxer_tests cxer/cxer_test.cpp)
target_link_libraries(asn1_cxer_tests PRIVATE asn1::cxer GTest::gtest_main)
gtest_discover_tests(asn1_cxer_tests)

add_executable(asn1_exer_tests exer/exer_test.cpp)
target_link_libraries(asn1_exer_tests PRIVATE asn1::exer GTest::gtest_main)
gtest_discover_tests(asn1_exer_tests)

add_executable(asn1_jer_tests jer/jer_test.cpp)
target_link_libraries(asn1_jer_tests PRIVATE asn1::jer GTest::gtest_main)
gtest_discover_tests(asn1_jer_tests)

add_executable(asn1_jeri_tests jeri/jeri_test.cpp)
target_link_libraries(asn1_jeri_tests PRIVATE asn1::jeri GTest::gtest_main)
gtest_discover_tests(asn1_jeri_tests)

add_executable(asn1_codegen_tests codegen/codegen_test.cpp)
target_link_libraries(asn1_codegen_tests PRIVATE asn1::frontend asn1::semantic asn1::codegen
                      GTest::gtest_main)
gtest_discover_tests(asn1_codegen_tests)

add_executable(asn1_telecom_tests telecom/telecom_test.cpp)
target_compile_definitions(asn1_telecom_tests PRIVATE
  ASN1_TELECOM_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures/telecom")
target_link_libraries(asn1_telecom_tests PRIVATE asn1::frontend asn1::semantic asn1::codegen
                      GTest::gtest_main)
gtest_discover_tests(asn1_telecom_tests)

# Phase 39: Person multi-codec validation round-trips.
set(ASN1_PERSON_GEN_ROOT "${CMAKE_CURRENT_BINARY_DIR}/generated_person")
set(ASN1_PERSON_ASN "${CMAKE_SOURCE_DIR}/examples/person.asn")

set(ASN1_PERSON_GEN_OUTPUTS "")
macro(asn1_gen_person codec ns subdir)
  set(_outdir "${ASN1_PERSON_GEN_ROOT}/${subdir}")
  add_custom_command(
    OUTPUT "${_outdir}/generated.hpp" "${_outdir}/generated.cpp"
    COMMAND asn1cxx --emit-dir "${_outdir}" --codec ${codec} --namespace ${ns}
            "${ASN1_PERSON_ASN}"
    DEPENDS asn1cxx "${ASN1_PERSON_ASN}"
    COMMENT "Generating ${codec} C++ for examples/person.asn (${ns})"
    VERBATIM
  )
  list(APPEND ASN1_PERSON_GEN_OUTPUTS "${_outdir}/generated.cpp")
endmacro()

asn1_gen_person(per asn1_gen_per per)
asn1_gen_person(oer asn1_gen_oer oer)
asn1_gen_person(coer asn1_gen_coer coer)
asn1_gen_person(ber asn1_gen_ber ber)
asn1_gen_person(der asn1_gen_der der)
asn1_gen_person(jer asn1_gen_jer jer)

add_executable(asn1_codegen_roundtrip_tests
  codegen/person_roundtrip_test.cpp
  ${ASN1_PERSON_GEN_OUTPUTS}
)
target_include_directories(asn1_codegen_roundtrip_tests PRIVATE "${ASN1_PERSON_GEN_ROOT}")
target_link_libraries(asn1_codegen_roundtrip_tests PRIVATE
  asn1::per asn1::oer asn1::coer asn1::ber asn1::der asn1::jer GTest::gtest_main)
gtest_discover_tests(asn1_codegen_roundtrip_tests)

set(ASN1_RRC_SLICE_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated_rrc_slice")
add_custom_command(
  OUTPUT "${ASN1_RRC_SLICE_OUT_DIR}/generated.hpp" "${ASN1_RRC_SLICE_OUT_DIR}/generated.cpp"
  COMMAND asn1cxx --emit-dir "${ASN1_RRC_SLICE_OUT_DIR}" --codec uper
          "${CMAKE_SOURCE_DIR}/tests/fixtures/telecom/rrc_slice/rrc_slice.asn"
  DEPENDS asn1cxx "${CMAKE_SOURCE_DIR}/tests/fixtures/telecom/rrc_slice/rrc_slice.asn"
  COMMENT "Generating UPER C++ for telecom rrc_slice.asn"
  VERBATIM
)
add_executable(asn1_telecom_roundtrip_tests
  telecom/rrc_slice_roundtrip_test.cpp
  "${ASN1_RRC_SLICE_OUT_DIR}/generated.cpp"
)
target_include_directories(asn1_telecom_roundtrip_tests PRIVATE "${ASN1_RRC_SLICE_OUT_DIR}")
target_link_libraries(asn1_telecom_roundtrip_tests PRIVATE asn1::per GTest::gtest_main)
gtest_discover_tests(asn1_telecom_roundtrip_tests)

# tlv_oer_golden fixture: mixed tags / DEFAULT / SET / nested CHOICE across
# BER, DER, OER and JER (byte-level assertions on tag and DEFAULT behaviour).
set(ASN1_TLV_OER_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated_tlv_oer_golden")
add_custom_command(
  OUTPUT "${ASN1_TLV_OER_GEN_DIR}/generated.hpp" "${ASN1_TLV_OER_GEN_DIR}/generated.cpp"
  COMMAND asn1cxx --emit-dir "${ASN1_TLV_OER_GEN_DIR}" --codec ber,der,oer,jer
          --namespace asn1_tlv_oer
          "${CMAKE_SOURCE_DIR}/tests/fixtures/telecom/tlv_oer_golden.asn"
  DEPENDS asn1cxx "${CMAKE_SOURCE_DIR}/tests/fixtures/telecom/tlv_oer_golden.asn"
  COMMENT "Generating BER/DER/OER/JER C++ for telecom tlv_oer_golden.asn"
  VERBATIM
)
add_executable(asn1_tlv_oer_golden_tests
  codegen/tlv_oer_golden_roundtrip_test.cpp
  "${ASN1_TLV_OER_GEN_DIR}/generated.cpp"
)
target_include_directories(asn1_tlv_oer_golden_tests PRIVATE "${ASN1_TLV_OER_GEN_DIR}")
target_link_libraries(asn1_tlv_oer_golden_tests PRIVATE
  asn1::ber asn1::der asn1::oer asn1::jer GTest::gtest_main)
gtest_discover_tests(asn1_tlv_oer_golden_tests)

# tlv_golden fixture: differential BER/DER/OER golden vectors (P1-1).
# The vectors in tests/tlv/golden/tlv_golden_vectors.txt are produced by
# independent oracles (asn1tools + asn1c) via scripts/gen_tlv_golden.py —
# asn1c arbitrates DER (SET/SET OF ordering, X.690 11.6), asn1tools is the
# BER/OER source (asn1c's oer_encode segfaults on SET values). BER has no
# canonical SET order (X.690 8.14), so SET cases carry '-' there.
set(ASN1_TLV_GOLDEN_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated_tlv_golden")
add_custom_command(
  OUTPUT "${ASN1_TLV_GOLDEN_GEN_DIR}/generated.hpp" "${ASN1_TLV_GOLDEN_GEN_DIR}/generated.cpp"
  COMMAND asn1cxx --emit-dir "${ASN1_TLV_GOLDEN_GEN_DIR}" --codec ber,der,oer
          --namespace tg
          "${CMAKE_SOURCE_DIR}/tests/fixtures/telecom/tlv_golden.asn"
  DEPENDS asn1cxx "${CMAKE_SOURCE_DIR}/tests/fixtures/telecom/tlv_golden.asn"
  COMMENT "Generating BER/DER/OER C++ for telecom tlv_golden.asn"
  VERBATIM
)
add_executable(asn1_tlv_golden_tests
  tlv/tlv_golden_vectors_test.cpp
  "${ASN1_TLV_GOLDEN_GEN_DIR}/generated.cpp"
)
target_include_directories(asn1_tlv_golden_tests PRIVATE "${ASN1_TLV_GOLDEN_GEN_DIR}")
target_compile_definitions(asn1_tlv_golden_tests PRIVATE
  ASN1_TLV_GOLDEN_FILE="${CMAKE_SOURCE_DIR}/tests/tlv/golden/tlv_golden_vectors.txt")
target_link_libraries(asn1_tlv_golden_tests PRIVATE
  asn1::ber asn1::der asn1::oer GTest::gtest_main)
gtest_discover_tests(asn1_tlv_golden_tests)

# CER (X.690) byte-level round-trips: indefinite wrappers (9.1), string
# fragmentation (9.2), BIT STRING fragment initial octets (8.6.4), SET tag
# ordering (9.3), unused-bit zeroing (11.2), canonical strictness, and the
# CER/DER value-equivalence cross-check.
set(ASN1_CER_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated_cer_golden")
add_custom_command(
  OUTPUT "${ASN1_CER_GEN_DIR}/generated.hpp" "${ASN1_CER_GEN_DIR}/generated.cpp"
  COMMAND asn1cxx --emit-dir "${ASN1_CER_GEN_DIR}" --codec cer,der
          --namespace asn1_cer
          "${CMAKE_SOURCE_DIR}/tests/fixtures/cer/cer_golden.asn"
  DEPENDS asn1cxx "${CMAKE_SOURCE_DIR}/tests/fixtures/cer/cer_golden.asn"
  COMMENT "Generating CER/DER C++ for cer_golden.asn"
  VERBATIM
)
add_executable(asn1_cer_roundtrip_tests
  cer/cer_roundtrip_test.cpp
  "${ASN1_CER_GEN_DIR}/generated.cpp"
)
target_include_directories(asn1_cer_roundtrip_tests PRIVATE "${ASN1_CER_GEN_DIR}")
target_link_libraries(asn1_cer_roundtrip_tests PRIVATE asn1::cer asn1::der
                      GTest::gtest_main)
gtest_discover_tests(asn1_cer_roundtrip_tests)

# JER open-type round-trips for information-object-class fields (&Value).
set(ASN1_IOC_JER_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated_ioc_jer")
add_custom_command(
  OUTPUT "${ASN1_IOC_JER_OUT_DIR}/generated.hpp" "${ASN1_IOC_JER_OUT_DIR}/generated.cpp"
  COMMAND asn1cxx --emit-dir "${ASN1_IOC_JER_OUT_DIR}" --codec jer
          --namespace asn1_ioc_jer "${CMAKE_SOURCE_DIR}/examples/ioc.asn"
  DEPENDS asn1cxx "${CMAKE_SOURCE_DIR}/examples/ioc.asn"
  COMMENT "Generating JER C++ for examples/ioc.asn (object-class open types)"
  VERBATIM
)
add_executable(asn1_ioc_jer_roundtrip_tests
  codegen/ioc_jer_roundtrip_test.cpp
  "${ASN1_IOC_JER_OUT_DIR}/generated.cpp"
)
target_include_directories(asn1_ioc_jer_roundtrip_tests PRIVATE "${ASN1_IOC_JER_OUT_DIR}")
target_link_libraries(asn1_ioc_jer_roundtrip_tests PRIVATE asn1::jer asn1::jeri
                      asn1::runtime GTest::gtest_main)
gtest_discover_tests(asn1_ioc_jer_roundtrip_tests)

# Table-constraint typed IE open types: {ObjectSet}{@id} dispatch into the
# per-entry specialized type on decode; typed view fills the open-type bytes
# and governor id on encode. Separate binary: the generated header name
# (`generated.hpp`) is shared with the other fixture gates.
set(ASN1_TC_IE_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated_tc_ie")
add_custom_command(
  OUTPUT "${ASN1_TC_IE_OUT_DIR}/generated.hpp" "${ASN1_TC_IE_OUT_DIR}/generated.cpp"
  COMMAND asn1cxx --emit-dir "${ASN1_TC_IE_OUT_DIR}" --codec uper
          --namespace tc_test
          "${CMAKE_SOURCE_DIR}/tests/fixtures/telecom/table_constraint_ie.asn"
  DEPENDS asn1cxx "${CMAKE_SOURCE_DIR}/tests/fixtures/telecom/table_constraint_ie.asn"
  COMMENT "Generating UPER C++ for telecom table_constraint_ie.asn"
  VERBATIM
)
add_executable(asn1_tc_ie_roundtrip_tests
  telecom/table_constraint_ie_roundtrip_test.cpp
  "${ASN1_TC_IE_OUT_DIR}/generated.cpp"
)
target_include_directories(asn1_tc_ie_roundtrip_tests PRIVATE "${ASN1_TC_IE_OUT_DIR}")
target_link_libraries(asn1_tc_ie_roundtrip_tests PRIVATE asn1::per GTest::gtest_main)
gtest_discover_tests(asn1_tc_ie_roundtrip_tests)

# Phase 41: compile-gate OER emit of rrc_slice (exercises shared BitStringValue).
set(ASN1_RRC_SLICE_OER_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated_rrc_slice_oer")
add_custom_command(
  OUTPUT "${ASN1_RRC_SLICE_OER_DIR}/generated.hpp" "${ASN1_RRC_SLICE_OER_DIR}/generated.cpp"
  COMMAND asn1cxx --emit-dir "${ASN1_RRC_SLICE_OER_DIR}" --codec oer --namespace asn1_rrc_oer
          "${CMAKE_SOURCE_DIR}/tests/fixtures/telecom/rrc_slice/rrc_slice.asn"
  DEPENDS asn1cxx "${CMAKE_SOURCE_DIR}/tests/fixtures/telecom/rrc_slice/rrc_slice.asn"
  COMMENT "Generating OER C++ for telecom rrc_slice.asn"
  VERBATIM
)
add_library(asn1_rrc_slice_oer_compile STATIC
  "${ASN1_RRC_SLICE_OER_DIR}/generated.cpp"
)
target_include_directories(asn1_rrc_slice_oer_compile PRIVATE "${ASN1_RRC_SLICE_OER_DIR}")
target_link_libraries(asn1_rrc_slice_oer_compile PRIVATE asn1::oer)

# Full 3GPP module compile gates: emit must produce C++ that actually
# compiles (emit success != compilable output; these fixtures exercise
# parameterized IE instantiations, table-constraint typed open types,
# nested names, and extension enumerations).
macro(asn1_fixture_compile_gate name codec lib fixture)
  set(_dir "${CMAKE_CURRENT_BINARY_DIR}/generated_${name}")
  add_custom_command(
    OUTPUT "${_dir}/generated.hpp" "${_dir}/generated.cpp"
    COMMAND asn1cxx --emit-dir "${_dir}" --codec ${codec} --namespace asn1_${name}
            "${CMAKE_SOURCE_DIR}/tests/fixtures/telecom/${fixture}"
    DEPENDS asn1cxx "${CMAKE_SOURCE_DIR}/tests/fixtures/telecom/${fixture}"
    COMMENT "Generating ${codec} C++ for telecom ${fixture} (compile gate)"
    VERBATIM
  )
  add_library(asn1_${name}_compile STATIC "${_dir}/generated.cpp")
  target_include_directories(asn1_${name}_compile PRIVATE "${_dir}")
  target_link_libraries(asn1_${name}_compile PRIVATE asn1::${lib})
  # Compile gate only — we don't ship this static lib, we just want it to
  # build clean. Drop optimization to -O1 (no -O3 inline expansion across
  # the multi-MB generated .cpp files) for ~2x faster compile on the
  # 3GPP fixtures. Still ABI-compatible with the runtime libs because
  # -O1 keeps standard calling conventions.
  target_compile_options(asn1_${name}_compile PRIVATE -O1)
endmacro()

asn1_fixture_compile_gate(rrc_full uper per rrc_8_6_0/EUTRA-RRC-Definitions.asn)
asn1_fixture_compile_gate(lpp_full uper per lpp_14_3_0/LPP-PDU-Definitions.asn)
asn1_fixture_compile_gate(s1ap_full aper per s1ap_14_4_0/s1ap_14_4_0.asn)

# Single-codec × whole-family compile gate (LPP, every codec in isolation).
# The all-12-codec combined emit gate above hides per-family include gaps
# (each family drags its runtime headers, transitively satisfying the
# next family's free functions): the previous P2-2 residual was exactly this
# shape — XML chose alone produced "make_error is not a member of asn1"
# because xer/codec.hpp does not pull in byte_io.hpp. Exercising every
# single codec independently catches single-family regressions the combined
# gate cannot. LPP is a representative multi-kilobyte ASN.1 fixture with
# CHOICE/SEQUENCE/enumerated/SIZE constraints, table-constraint-typed open
# types and IE references — enough to detect missing includes, link
# errors, or namespace regressions in any single family.
set(_all_codec_pairs
    "uper per" "aper per" "per per"
    "ber ber" "der per" "cer ber"
    "oer oer" "coer oer"
    "jer jer" "xer xer" "exer xer" "cxer xer")
foreach(_pair IN LISTS _all_codec_pairs)
  string(REPLACE " " ";" _pair_split "${_pair}")
  list(GET _pair_split 0 _codec)
  list(GET _pair_split 1 _lib)
  set(_dir "${CMAKE_CURRENT_BINARY_DIR}/generated_lpp_single_${_codec}")
  add_custom_command(
    OUTPUT "${_dir}/generated.hpp" "${_dir}/generated.cpp"
    COMMAND asn1cxx --emit-dir "${_dir}" --codec ${_codec} --namespace asn1_lpp_single_${_codec}
            "${CMAKE_SOURCE_DIR}/tests/fixtures/telecom/lpp_14_3_0/LPP-PDU-Definitions.asn"
    DEPENDS asn1cxx "${CMAKE_SOURCE_DIR}/tests/fixtures/telecom/lpp_14_3_0/LPP-PDU-Definitions.asn"
    COMMENT "Generating single-${_codec} C++ for telecom LPP (single-codec gate)"
    VERBATIM
  )
  add_library(asn1_lpp_single_${_codec}_compile STATIC "${_dir}/generated.cpp")
  target_include_directories(asn1_lpp_single_${_codec}_compile PRIVATE "${_dir}")
  target_link_libraries(asn1_lpp_single_${_codec}_compile PRIVATE asn1::${_lib})
  # Compile gate only — see macro comment above. -O1 shaves ~half the
  # compile time across the 12 single-codec LPP gates.
  target_compile_options(asn1_lpp_single_${_codec}_compile PRIVATE -O1)
endforeach()

# Split emit compile gates (--emit-split): per-module <Module>.hpp/.cpp
# pairs + aggregate header must compile. S1AP exercises cross-module
# references AND a merged mutually-recursive module unit (IEs + Containers +
# PDU-Contents share one TU); LPP covers the single-module unit path.
macro(asn1_fixture_split_gate name codec lib fixture)
  set(_dir "${CMAKE_CURRENT_BINARY_DIR}/generated_split_${name}")
  set(_outs "${_dir}/generated.hpp")
  set(_srcs "")
  foreach(_stem ${ARGN})
    list(APPEND _outs "${_dir}/${_stem}.hpp" "${_dir}/${_stem}.cpp")
    list(APPEND _srcs "${_dir}/${_stem}.cpp")
  endforeach()
  add_custom_command(
    OUTPUT ${_outs}
    COMMAND asn1cxx --emit-dir "${_dir}" --codec ${codec} --namespace asn1_split_${name}
            --emit-split "${CMAKE_SOURCE_DIR}/tests/fixtures/telecom/${fixture}"
    DEPENDS asn1cxx "${CMAKE_SOURCE_DIR}/tests/fixtures/telecom/${fixture}"
    COMMENT "Generating split ${codec} C++ for telecom ${fixture} (split compile gate)"
    VERBATIM
  )
  # One library per fixture holding every emitted unit .cpp (each unit is an
  # independent TU: it includes only its own header + dependency headers).
  add_library(asn1_split_${name} STATIC ${_srcs})
  target_include_directories(asn1_split_${name} PRIVATE "${_dir}")
  target_link_libraries(asn1_split_${name} PRIVATE asn1::${lib})
  # Same compile-only -O1 optimisation as the flat fixture gate (the
  # aggregate unit .cpp files for S1AP exceed 1 MB and the bottleneck is
  # -O3's inliner/vectoriser pass).
  target_compile_options(asn1_split_${name} PRIVATE -O1)
endmacro()

# LPP: single module -> one unit file. S1AP: three units (CommonDataTypes,
# PDU-Descriptions, and the merged IEs/Containers/PDU-Contents cycle).
asn1_fixture_split_gate(lpp_split uper per lpp_14_3_0/LPP-PDU-Definitions.asn
  LPP_PDU_Definitions)
asn1_fixture_split_gate(s1ap_split uper per s1ap_14_4_0/s1ap_14_4_0.asn
  S1AP_CommonDataTypes S1AP_PDU_Descriptions S1AP_IEs)

# asn1cxx CLI end-to-end tests (spawn the built asn1cxx binary).
add_executable(asn1_cli_tests
  codegen/cli_test.cpp
)
target_compile_definitions(asn1_cli_tests PRIVATE
  ASN1CXX_BIN="$<TARGET_FILE:asn1cxx>"
  ASN1_EXAMPLES_DIR="${CMAKE_SOURCE_DIR}/examples")
target_link_libraries(asn1_cli_tests PRIVATE GTest::gtest_main)
add_dependencies(asn1_cli_tests asn1cxx)
gtest_discover_tests(asn1_cli_tests)

# Cross-codec consistency: same value round-trips under every codec.
add_executable(asn1_cross_codec_tests
  codegen/cross_codec_consistency_test.cpp
  ${ASN1_PERSON_GEN_OUTPUTS}
)
target_include_directories(asn1_cross_codec_tests PRIVATE "${ASN1_PERSON_GEN_ROOT}")
target_link_libraries(asn1_cross_codec_tests PRIVATE
  asn1::per asn1::oer asn1::coer asn1::ber asn1::der asn1::jer GTest::gtest_main)
gtest_discover_tests(asn1_cross_codec_tests)

# Codec throughput micro-benchmark (smoke-level regression gate).
add_executable(asn1_codec_benchmark_tests
  codegen/codec_benchmark_test.cpp
  ${ASN1_PERSON_GEN_OUTPUTS}
)
target_include_directories(asn1_codec_benchmark_tests PRIVATE "${ASN1_PERSON_GEN_ROOT}")
target_link_libraries(asn1_codec_benchmark_tests PRIVATE
  asn1::per asn1::oer asn1::coer asn1::ber asn1::der asn1::jer GTest::gtest_main)
gtest_discover_tests(asn1_codec_benchmark_tests)

# P2-3: content-hash cache unit tests. The fixture root is exposed so the
# split-mode tests can locate `tests/fixtures/telecom/{lpp,s1ap}_*/...`
# without baking an absolute path into the source.
add_executable(asn1_emit_cache_tests
  codegen/emit_cache_test.cpp
)
target_compile_definitions(asn1_emit_cache_tests PRIVATE
  ASN1CXX_FIXTURES_DIR="${CMAKE_SOURCE_DIR}/tests")
target_link_libraries(asn1_emit_cache_tests PRIVATE
  asn1::frontend asn1::semantic asn1::codegen GTest::gtest_main)
gtest_discover_tests(asn1_emit_cache_tests)
