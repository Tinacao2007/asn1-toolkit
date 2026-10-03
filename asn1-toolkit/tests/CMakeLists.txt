include(FetchContent)

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

asn1_gen_person(both asn1_gen_per per)
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
