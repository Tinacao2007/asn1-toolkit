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

add_executable(asn1_codegen_tests codegen/codegen_test.cpp)
target_link_libraries(asn1_codegen_tests PRIVATE asn1::frontend asn1::semantic asn1::codegen
                      GTest::gtest_main)
gtest_discover_tests(asn1_codegen_tests)

set(ASN1_CODEGEN_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated_person")
add_custom_command(
  OUTPUT "${ASN1_CODEGEN_OUT_DIR}/generated.hpp" "${ASN1_CODEGEN_OUT_DIR}/generated.cpp"
  COMMAND asn1cxx --emit-dir "${ASN1_CODEGEN_OUT_DIR}" --codec uper
          "${CMAKE_SOURCE_DIR}/examples/person.asn"
  DEPENDS asn1cxx "${CMAKE_SOURCE_DIR}/examples/person.asn"
  COMMENT "Generating UPER C++ for examples/person.asn"
  VERBATIM
)
add_executable(asn1_codegen_roundtrip_tests
  codegen/person_roundtrip_test.cpp
  "${ASN1_CODEGEN_OUT_DIR}/generated.cpp"
)
target_include_directories(asn1_codegen_roundtrip_tests PRIVATE "${ASN1_CODEGEN_OUT_DIR}")
target_link_libraries(asn1_codegen_roundtrip_tests PRIVATE asn1::per GTest::gtest_main)
gtest_discover_tests(asn1_codegen_roundtrip_tests)
