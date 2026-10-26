include(FetchContent)

# Offline override: point this at a local GoogleTest source tree to build
# without network access, e.g.
#   cmake -S . -B build -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=/path/to/googletest
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "Do not install private test dependencies" FORCE)
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.24")
  set(_extract_timestamp DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
endif()
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
  URL_HASH SHA256=1f357c27ca988c3f7c6b4bf68a9395005ac6761f034046e9dde0896e3aba00e4
  ${_extract_timestamp}
)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

foreach(_mode IN ITEMS outline inline split)
  set(_dir "${CMAKE_CURRENT_BINARY_DIR}/standard_${_mode}")
  set(_flags "")
  set(_cpp "${_dir}/generated.cpp")
  if(_mode STREQUAL "inline")
    set(_flags --emit-inline)
  elseif(_mode STREQUAL "split")
    set(_flags --emit-split)
    set(_cpp "${_dir}/Standards.cpp")
  endif()
  add_custom_command(OUTPUT "${_dir}/generated.hpp" "${_cpp}"
    COMMAND "${ASN1_GENERATOR}" --no-emit-cache --codec ber,der,cer,uper,aper,oer,coer,xer,cxer,exer,jer,jeri
      ${_flags} --emit-dir "${_dir}" "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/standards.asn"
    DEPENDS ${ASN1_GENERATOR_DEPENDENCY} "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/standards.asn" VERBATIM)
  add_executable(asn1_standard_${_mode}_tests codegen/standard_generated_test.cpp codegen/standard_second_tu.cpp "${_cpp}")
  target_include_directories(asn1_standard_${_mode}_tests PRIVATE "${_dir}")
  target_link_libraries(asn1_standard_${_mode}_tests PRIVATE asn1::per asn1::cer asn1::coer asn1::exer asn1::cxer asn1::jeri GTest::gtest_main)
  if(_mode STREQUAL "split")
    target_compile_definitions(asn1_standard_${_mode}_tests PRIVATE STANDARD_SPLIT)
  endif()
  gtest_discover_tests(asn1_standard_${_mode}_tests TEST_PREFIX "${_mode}.")
endforeach()
add_executable(asn1_standard_oracle_driver codegen/standard_oracle_driver.cpp
  "${CMAKE_CURRENT_BINARY_DIR}/standard_outline/generated.cpp")
target_include_directories(asn1_standard_oracle_driver PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/standard_outline")
target_link_libraries(asn1_standard_oracle_driver PRIVATE asn1::per asn1::cer asn1::coer asn1::exer asn1::cxer asn1::jeri)

add_executable(asn1_standard_policy_tests runtime/standard_policy_test.cpp)
target_link_libraries(asn1_standard_policy_tests PRIVATE asn1::ber asn1::der asn1::cer asn1::coer asn1::jer GTest::gtest_main)
gtest_discover_tests(asn1_standard_policy_tests)

add_executable(asn1_decoder_fuzz_regression fuzz/decoder_fuzz.cpp)
target_compile_definitions(asn1_decoder_fuzz_regression PRIVATE ASN1_FUZZ_REGRESSION_MAIN
  ASN1_FUZZ_SEEDS="${CMAKE_CURRENT_SOURCE_DIR}/fuzz/seeds.txt")
target_link_libraries(asn1_decoder_fuzz_regression PRIVATE asn1::cer asn1::coer asn1::xer asn1::jer)
add_test(NAME DecoderFuzz.SeedRegression COMMAND asn1_decoder_fuzz_regression)
option(ASN1_BUILD_FUZZERS "Build Clang libFuzzer decoder harness (development only)" OFF)
if(ASN1_BUILD_FUZZERS)
  if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR MSVC)
    message(FATAL_ERROR "ASN1_BUILD_FUZZERS requires Clang with libFuzzer")
  endif()
  add_executable(asn1_decoder_fuzzer fuzz/decoder_fuzz.cpp)
  foreach(_runtime IN ITEMS asn1_runtime asn1_ber asn1_der asn1_cer asn1_oer asn1_coer asn1_xer asn1_jer)
    target_compile_options(${_runtime} PRIVATE -fsanitize=fuzzer-no-link)
    target_link_options(${_runtime} INTERFACE -fsanitize=fuzzer-no-link)
  endforeach()
  target_compile_options(asn1_decoder_fuzzer PRIVATE -fsanitize=fuzzer)
  target_link_options(asn1_decoder_fuzzer PRIVATE -fsanitize=fuzzer)
  target_link_libraries(asn1_decoder_fuzzer PRIVATE asn1::cer asn1::coer asn1::xer asn1::jer)
endif()

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

# P2-4 (--lang c) sub-task 2: link-and-run gate for the parallel C runtime.
# Sub-task 1 only verified that every header parses cleanly under
# `gcc -std=c11 -fsyntax-only`; sub-task 2 fills the bodies in
# src/c_runtime/*.c, so this gate now links the runtime lib and runs
# a smoke driver that calls every declared function with a sample input.
# A failure here means a C-runtime body returns the wrong status or
# produces bytes that don't match the canonical wire form.
add_executable(asn1_c_runtime_smoke c_lang/c_runtime_smoke.c)
target_link_libraries(asn1_c_runtime_smoke PRIVATE asn1::c_runtime)
add_test(NAME Asn1CRuntimeSmoke COMMAND asn1_c_runtime_smoke)

# Fault injection is confined to a separate executable/source compilation.
# Production asn1::c_runtime has no allocator configuration or test symbols.
get_target_property(_c_runtime_sources asn1_c_runtime SOURCES)
list(TRANSFORM _c_runtime_sources PREPEND "${PROJECT_SOURCE_DIR}/")
add_executable(asn1_c_runtime_failure_tests
  c_lang/c_runtime_failure_test.c ${_c_runtime_sources})
if(NOT MSVC)
  target_link_libraries(asn1_c_runtime_failure_tests PRIVATE m)
endif()
target_include_directories(asn1_c_runtime_failure_tests PRIVATE "${PROJECT_SOURCE_DIR}/include")
target_compile_definitions(asn1_c_runtime_failure_tests PRIVATE ASN1_C_RUNTIME_TEST_ALLOCATOR)
set_target_properties(asn1_c_runtime_failure_tests PROPERTIES
  C_STANDARD 11 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
add_test(NAME Asn1CRuntimeFailureContracts COMMAND asn1_c_runtime_failure_tests)

add_executable(asn1_c_per_correctness c_lang/c_per_correctness_test.c)
target_link_libraries(asn1_c_per_correctness PRIVATE asn1::c_runtime)
set_target_properties(asn1_c_per_correctness PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
add_test(NAME Asn1CPerCorrectness COMMAND asn1_c_per_correctness)
add_executable(asn1_cgen_batch5_tlv_tests c_lang/cgen_batch5_tlv_test.c)
target_link_libraries(asn1_cgen_batch5_tlv_tests PRIVATE asn1::c_runtime)
set_target_properties(asn1_cgen_batch5_tlv_tests PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
add_test(NAME CGenBatch5.TlvCore COMMAND asn1_cgen_batch5_tlv_tests)
add_executable(asn1_cgen_batch6_text_tests c_lang/cgen_batch6_text_test.c)
target_link_libraries(asn1_cgen_batch6_text_tests PRIVATE asn1::c_runtime)
set_target_properties(asn1_cgen_batch6_text_tests PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
add_test(NAME CGenBatch6.TextSyntax COMMAND asn1_cgen_batch6_text_tests)
foreach(_family IN ITEMS jer exer)
  set(_dir "${CMAKE_CURRENT_BINARY_DIR}/cgen6-${_family}")
  add_custom_command(OUTPUT "${_dir}/generation.stamp"
    BYPRODUCTS "${_dir}/generated.h" "${_dir}/generated.c" "${_dir}/generated_internal.h"
    COMMAND "${ASN1_GENERATOR}" --lang c --codec jer,exer --c-prefix ix
      --emit-dir "${_dir}" "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/cgen_batch6_${_family}.asn"
    COMMAND "${CMAKE_COMMAND}" -E touch "${_dir}/generation.stamp"
    DEPENDS ${ASN1_GENERATOR_DEPENDENCY} "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/cgen_batch6_${_family}.asn" VERBATIM)
  add_custom_target(asn1_cgen6_${_family}_generated DEPENDS "${_dir}/generation.stamp")
  add_executable(asn1_cgen_batch6_${_family}_instructions c_lang/cgen_batch6_instruction_test.c "${_dir}/generated.c")
  add_dependencies(asn1_cgen_batch6_${_family}_instructions asn1_cgen6_${_family}_generated)
  target_include_directories(asn1_cgen_batch6_${_family}_instructions PRIVATE "${_dir}")
  target_link_libraries(asn1_cgen_batch6_${_family}_instructions PRIVATE asn1::c_runtime)
  set_target_properties(asn1_cgen_batch6_${_family}_instructions PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
  if(_family STREQUAL "jer")
    target_compile_definitions(asn1_cgen_batch6_${_family}_instructions PRIVATE JER_TEST)
  endif()
  add_test(NAME CGenBatch6.${_family}Instructions COMMAND asn1_cgen_batch6_${_family}_instructions)
endforeach()
foreach(_cgen6_shape IN ITEMS outline inline split)
  set(_cgen6_dir "${CMAKE_CURRENT_BINARY_DIR}/cgen6-${_cgen6_shape}")
  set(_cgen6_flags)
  set(_cgen6_source "${_cgen6_dir}/generated.c")
  if(_cgen6_shape STREQUAL "inline")
    set(_cgen6_flags --emit-inline)
    set(_cgen6_source "${_cgen6_dir}/generated.h")
  elseif(_cgen6_shape STREQUAL "split")
    set(_cgen6_flags --emit-split)
    set(_cgen6_source "${_cgen6_dir}/c_mC6.c")
  endif()
  set(_cgen6_outputs "${_cgen6_dir}/generated.h" ${_cgen6_source})
  list(REMOVE_DUPLICATES _cgen6_outputs)
  add_custom_command(OUTPUT "${_cgen6_dir}/generation.stamp"
    BYPRODUCTS ${_cgen6_outputs}
    COMMAND "${ASN1_GENERATOR}" --lang c --codec oer,coer,jer,jeri,xer,cxer,exer --c-prefix cgen6 ${_cgen6_flags}
      --emit-dir "${_cgen6_dir}" "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/cgen_batch6.asn"
    COMMAND "${CMAKE_COMMAND}" -E touch "${_cgen6_dir}/generation.stamp"
    DEPENDS ${ASN1_GENERATOR_DEPENDENCY} "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/cgen_batch6.asn" VERBATIM)
  add_custom_target(asn1_cgen6_${_cgen6_shape}_generated DEPENDS "${_cgen6_dir}/generation.stamp")
  add_executable(asn1_cgen_batch6_${_cgen6_shape}_tests c_lang/cgen_batch6_pdu_test.c ${_cgen6_source})
  add_dependencies(asn1_cgen_batch6_${_cgen6_shape}_tests asn1_cgen6_${_cgen6_shape}_generated)
  target_include_directories(asn1_cgen_batch6_${_cgen6_shape}_tests PRIVATE "${_cgen6_dir}")
  target_link_libraries(asn1_cgen_batch6_${_cgen6_shape}_tests PRIVATE asn1::c_runtime)
  set_target_properties(asn1_cgen_batch6_${_cgen6_shape}_tests PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
  add_test(NAME CGenBatch6.Generated${_cgen6_shape} COMMAND asn1_cgen_batch6_${_cgen6_shape}_tests)
endforeach()
add_executable(asn1_cgen_batch6_failure_tests c_lang/cgen_batch6_failure_test.c
  "${CMAKE_CURRENT_BINARY_DIR}/cgen6-outline/generated.c" ${_c_runtime_sources})
target_include_directories(asn1_cgen_batch6_failure_tests PRIVATE
  "${CMAKE_CURRENT_BINARY_DIR}/cgen6-outline" "${PROJECT_SOURCE_DIR}/include")
target_compile_definitions(asn1_cgen_batch6_failure_tests PRIVATE ASN1_C_RUNTIME_TEST_ALLOCATOR)
if(NOT MSVC)
  target_link_libraries(asn1_cgen_batch6_failure_tests PRIVATE m)
endif()
set_target_properties(asn1_cgen_batch6_failure_tests PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
add_test(NAME CGenBatch6.AllocationFailures COMMAND asn1_cgen_batch6_failure_tests)
add_executable(asn1_cgen_batch6_oracle_driver c_lang/cgen_batch6_oracle_driver.c
  "${CMAKE_CURRENT_BINARY_DIR}/cgen6-outline/generated.c")
target_include_directories(asn1_cgen_batch6_oracle_driver PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/cgen6-outline")
target_link_libraries(asn1_cgen_batch6_oracle_driver PRIVATE asn1::c_runtime)
set_target_properties(asn1_cgen_batch6_oracle_driver PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
add_executable(asn1_cgen_batch6_boundary_tests c_lang/cgen_batch6_boundary_test.c
  "${CMAKE_CURRENT_BINARY_DIR}/cgen6-outline/generated.c")
target_include_directories(asn1_cgen_batch6_boundary_tests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/cgen6-outline")
target_link_libraries(asn1_cgen_batch6_boundary_tests PRIVATE asn1::c_runtime)
set_target_properties(asn1_cgen_batch6_boundary_tests PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
add_test(NAME CGenBatch6.StandardBoundaries COMMAND asn1_cgen_batch6_boundary_tests)
add_executable(asn1_cgen_batch6_limits_tests c_lang/cgen_batch6_limits_test.c
  "${CMAKE_CURRENT_BINARY_DIR}/cgen6-outline/generated.c" ${_c_runtime_sources})
target_include_directories(asn1_cgen_batch6_limits_tests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/cgen6-outline" "${PROJECT_SOURCE_DIR}/include")
target_compile_definitions(asn1_cgen_batch6_limits_tests PRIVATE ASN1_MAX_PDU_ALLOCATION_BYTES=1024 ASN1_MAX_PDU_ELEMENTS=8 ASN1_MAX_STRING_BYTES=64)
if(NOT MSVC)
  target_link_libraries(asn1_cgen_batch6_limits_tests PRIVATE m)
endif()
set_target_properties(asn1_cgen_batch6_limits_tests PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
add_test(NAME CGenBatch6.ResourceLimits COMMAND asn1_cgen_batch6_limits_tests)
foreach(_consumer IN ITEMS failure_tests oracle_driver boundary_tests limits_tests)
  add_dependencies(asn1_cgen_batch6_${_consumer} asn1_cgen6_outline_generated)
endforeach()
foreach(_cgen5_shape IN ITEMS outline inline split)
  set(_cgen5_dir "${CMAKE_CURRENT_BINARY_DIR}/cgen5-${_cgen5_shape}")
  set(_cgen5_flags)
  set(_cgen5_source "${_cgen5_dir}/generated.c")
  if(_cgen5_shape STREQUAL "inline")
    set(_cgen5_flags --emit-inline)
    set(_cgen5_source "${_cgen5_dir}/generated.h")
  elseif(_cgen5_shape STREQUAL "split")
    set(_cgen5_flags --emit-split)
    set(_cgen5_source "${_cgen5_dir}/c_mC5.c")
  endif()
  set(_cgen5_outputs "${_cgen5_dir}/generated.h" ${_cgen5_source})
  list(REMOVE_DUPLICATES _cgen5_outputs)
  add_custom_command(OUTPUT ${_cgen5_outputs}
    COMMAND "${ASN1_GENERATOR}" --lang c --codec uper,ber,der,cer --c-prefix cgen5 ${_cgen5_flags}
      --emit-dir "${_cgen5_dir}" "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/cgen_batch5.asn"
    DEPENDS ${ASN1_GENERATOR_DEPENDENCY} "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/cgen_batch5.asn" VERBATIM)
  add_executable(asn1_cgen_batch5_${_cgen5_shape}_tests c_lang/cgen_batch5_pdu_test.c ${_cgen5_source})
  target_include_directories(asn1_cgen_batch5_${_cgen5_shape}_tests PRIVATE "${_cgen5_dir}")
  target_link_libraries(asn1_cgen_batch5_${_cgen5_shape}_tests PRIVATE asn1::c_runtime)
  set_target_properties(asn1_cgen_batch5_${_cgen5_shape}_tests PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
  add_test(NAME CGenBatch5.Generated${_cgen5_shape} COMMAND asn1_cgen_batch5_${_cgen5_shape}_tests)
endforeach()
add_test(NAME CGenBatch4.PerPrimitiveCore COMMAND asn1_c_per_correctness)
add_executable(asn1_cgen_batch5_failure_tests c_lang/cgen_batch5_failure_test.c
  "${CMAKE_CURRENT_BINARY_DIR}/cgen5-outline/generated.c" ${_c_runtime_sources})
target_include_directories(asn1_cgen_batch5_failure_tests PRIVATE
  "${CMAKE_CURRENT_BINARY_DIR}/cgen5-outline" "${PROJECT_SOURCE_DIR}/include")
target_compile_definitions(asn1_cgen_batch5_failure_tests PRIVATE ASN1_C_RUNTIME_TEST_ALLOCATOR)
if(NOT MSVC)
  target_link_libraries(asn1_cgen_batch5_failure_tests PRIVATE m)
endif()
set_target_properties(asn1_cgen_batch5_failure_tests PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
add_test(NAME CGenBatch5.AllocationFailures COMMAND asn1_cgen_batch5_failure_tests)

add_executable(asn1_cgen_batch5_limits_tests c_lang/cgen_batch5_limits_test.c
  "${CMAKE_CURRENT_BINARY_DIR}/cgen5-outline/generated.c" ${_c_runtime_sources})
target_include_directories(asn1_cgen_batch5_limits_tests PRIVATE
  "${CMAKE_CURRENT_BINARY_DIR}/cgen5-outline" "${PROJECT_SOURCE_DIR}/include")
target_compile_definitions(asn1_cgen_batch5_limits_tests PRIVATE
  ASN1_MAX_PDU_ALLOCATION_BYTES=96 ASN1_MAX_PDU_ELEMENTS=8)
if(NOT MSVC)
  target_link_libraries(asn1_cgen_batch5_limits_tests PRIVATE m)
endif()
set_target_properties(asn1_cgen_batch5_limits_tests PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
add_test(NAME CGenBatch5.ResourceLimits COMMAND asn1_cgen_batch5_limits_tests)

# Actual generated-C PDU consumer: constrained scalars, ENUMERATED, BIT/OCTET
# STRING, OID, root CHOICE, SEQUENCE OF, SET, DEFAULT/OPTIONAL and UPER+APER.
set(_cgen4_per_dir "${CMAKE_CURRENT_BINARY_DIR}/cgen4-per")
add_custom_command(OUTPUT "${_cgen4_per_dir}/generated.h" "${_cgen4_per_dir}/generated.c"
  COMMAND "${ASN1_GENERATOR}" --no-emit-cache --lang c --codec uper,aper --c-prefix cgen4
    --emit-dir "${_cgen4_per_dir}" "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/cgen_batch4_basic.asn"
  DEPENDS ${ASN1_GENERATOR_DEPENDENCY} "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/cgen_batch4_basic.asn"
  VERBATIM)
add_executable(asn1_cgen_batch4_per_tests c_lang/cgen_batch4_per_test.c "${_cgen4_per_dir}/generated.c")
target_include_directories(asn1_cgen_batch4_per_tests PRIVATE "${_cgen4_per_dir}")
target_link_libraries(asn1_cgen_batch4_per_tests PRIVATE asn1::c_runtime)
set_target_properties(asn1_cgen_batch4_per_tests PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
add_test(NAME CGenBatch4.GeneratedPdu COMMAND asn1_cgen_batch4_per_tests
  "${CMAKE_SOURCE_DIR}/tests/per/golden/per_golden_vectors.txt")

# Table-constrained CLASS.&Value retains raw complete PER octets and also gets
# generated selector-driven typed decode/encode for known object-set entries.
set(_cgen4_open_dir "${CMAKE_CURRENT_BINARY_DIR}/cgen4-open-type")
add_custom_command(OUTPUT "${_cgen4_open_dir}/generated.h" "${_cgen4_open_dir}/generated.c"
  COMMAND "${ASN1_GENERATOR}" --no-emit-cache --lang c --codec uper,aper --emit-dir "${_cgen4_open_dir}"
    "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/telecom/table_constraint_ie.asn"
  DEPENDS ${ASN1_GENERATOR_DEPENDENCY} "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/telecom/table_constraint_ie.asn"
  VERBATIM)
add_executable(asn1_cgen_open_type_tests c_lang/cgen_open_type_test.c "${_cgen4_open_dir}/generated.c")
target_include_directories(asn1_cgen_open_type_tests PRIVATE "${_cgen4_open_dir}")
target_link_libraries(asn1_cgen_open_type_tests PRIVATE asn1::c_runtime)
set_target_properties(asn1_cgen_open_type_tests PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
add_test(NAME CGenBatch4.OpenTypeOpaquePayload COMMAND asn1_cgen_open_type_tests)

set(_cgen4_inline_dir "${CMAKE_CURRENT_BINARY_DIR}/cgen4-per-inline")
add_custom_command(OUTPUT "${_cgen4_inline_dir}/generated.h"
  COMMAND "${ASN1_GENERATOR}" --no-emit-cache --lang c --codec uper,aper --c-prefix cgen4 --emit-inline
    --emit-dir "${_cgen4_inline_dir}" "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/cgen_batch4_basic.asn"
  DEPENDS ${ASN1_GENERATOR_DEPENDENCY} "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/cgen_batch4_basic.asn" VERBATIM)
add_executable(asn1_cgen_batch4_per_inline_tests c_lang/cgen_batch4_per_test.c "${_cgen4_inline_dir}/generated.h")
target_include_directories(asn1_cgen_batch4_per_inline_tests PRIVATE "${_cgen4_inline_dir}")
target_link_libraries(asn1_cgen_batch4_per_inline_tests PRIVATE asn1::c_runtime)
set_target_properties(asn1_cgen_batch4_per_inline_tests PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
add_test(NAME CGenBatch4.GeneratedPduInline COMMAND asn1_cgen_batch4_per_inline_tests
  "${CMAKE_SOURCE_DIR}/tests/per/golden/per_golden_vectors.txt")

set(_cgen4_split_dir "${CMAKE_CURRENT_BINARY_DIR}/cgen4-per-split")
add_custom_command(OUTPUT "${_cgen4_split_dir}/generated.h" "${_cgen4_split_dir}/c_mM.c"
  COMMAND "${ASN1_GENERATOR}" --no-emit-cache --lang c --codec uper,aper --c-prefix cgen4 --emit-split
    --emit-dir "${_cgen4_split_dir}" "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/cgen_batch4_basic.asn"
  DEPENDS ${ASN1_GENERATOR_DEPENDENCY} "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/cgen_batch4_basic.asn" VERBATIM)
add_executable(asn1_cgen_batch4_per_split_tests c_lang/cgen_batch4_per_test.c "${_cgen4_split_dir}/c_mM.c")
target_include_directories(asn1_cgen_batch4_per_split_tests PRIVATE "${_cgen4_split_dir}")
target_link_libraries(asn1_cgen_batch4_per_split_tests PRIVATE asn1::c_runtime)
set_target_properties(asn1_cgen_batch4_per_split_tests PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
add_test(NAME CGenBatch4.GeneratedPduSplit COMMAND asn1_cgen_batch4_per_split_tests
  "${CMAKE_SOURCE_DIR}/tests/per/golden/per_golden_vectors.txt")

# Compile a private fault-injected C runtime copy and generated decoder together.
add_executable(asn1_cgen_batch4_per_failure_tests
  c_lang/cgen_batch4_per_failure_test.c "${_cgen4_per_dir}/generated.c" ${_c_runtime_sources})
if(NOT MSVC)
  target_link_libraries(asn1_cgen_batch4_per_failure_tests PRIVATE m)
endif()
target_include_directories(asn1_cgen_batch4_per_failure_tests PRIVATE "${_cgen4_per_dir}" "${PROJECT_SOURCE_DIR}/include")
target_compile_definitions(asn1_cgen_batch4_per_failure_tests PRIVATE ASN1_C_RUNTIME_TEST_ALLOCATOR)
set_target_properties(asn1_cgen_batch4_per_failure_tests PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
add_test(NAME CGenBatch4.GeneratedPduAllocationFailure COMMAND asn1_cgen_batch4_per_failure_tests)

# Build two private runtime variants to prove both cumulative PDU byte and
# element budgets reject a collection before publishing partial output.
foreach(_budget_kind IN ITEMS bytes elements)
  add_executable(asn1_cgen_batch4_per_${_budget_kind}_budget_tests
    c_lang/cgen_batch4_per_budget_test.c "${_cgen4_per_dir}/generated.c" ${_c_runtime_sources})
  target_include_directories(asn1_cgen_batch4_per_${_budget_kind}_budget_tests PRIVATE
    "${_cgen4_per_dir}" "${PROJECT_SOURCE_DIR}/include")
  if(_budget_kind STREQUAL "bytes")
    target_compile_definitions(asn1_cgen_batch4_per_${_budget_kind}_budget_tests PRIVATE
      ASN1_MAX_PDU_ALLOCATION_BYTES=64 ASN1_MAX_PDU_ELEMENTS=1000)
  else()
    target_compile_definitions(asn1_cgen_batch4_per_${_budget_kind}_budget_tests PRIVATE
      ASN1_MAX_PDU_ALLOCATION_BYTES=1048576 ASN1_MAX_PDU_ELEMENTS=32)
  endif()
  if(NOT MSVC)
    target_link_libraries(asn1_cgen_batch4_per_${_budget_kind}_budget_tests PRIVATE m)
  endif()
  set_target_properties(asn1_cgen_batch4_per_${_budget_kind}_budget_tests PROPERTIES
    C_STANDARD 11 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
  add_test(NAME CGenBatch4.Cumulative${_budget_kind}Budget
    COMMAND asn1_cgen_batch4_per_${_budget_kind}_budget_tests)
endforeach()

# Same tests above the production 64KiB decoder cap, without weakening it.
add_executable(asn1_c_per_large c_lang/c_per_correctness_test.c ${_c_runtime_sources})
if(NOT MSVC)
  target_link_libraries(asn1_c_per_large PRIVATE m)
endif()
target_include_directories(asn1_c_per_large PRIVATE "${PROJECT_SOURCE_DIR}/include")
target_compile_definitions(asn1_c_per_large PRIVATE ASN1_MAX_STRING_BYTES=131072)
set_target_properties(asn1_c_per_large PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
add_test(NAME Asn1CPerLargeValues COMMAND asn1_c_per_large)

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
target_link_libraries(asn1_per_tests PRIVATE asn1::per asn1::c_runtime GTest::gtest_main)
gtest_discover_tests(asn1_per_tests)

# Differential PER validation against externally-generated golden vectors
# (asn1tools via scripts/gen_per_golden.py). The generated.hpp comes from the
# per_golden.asn fixture; the vectors are checked in under tests/per/golden/.
set(ASN1_PER_GOLDEN_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated_per_golden")
add_custom_command(
  OUTPUT "${ASN1_PER_GOLDEN_GEN_DIR}/generated.hpp" "${ASN1_PER_GOLDEN_GEN_DIR}/generated.cpp"
  COMMAND "${ASN1_GENERATOR}" --no-emit-cache --emit-dir "${ASN1_PER_GOLDEN_GEN_DIR}" --codec per
          --namespace pg
          "${CMAKE_SOURCE_DIR}/tests/fixtures/telecom/per_golden.asn"
  DEPENDS ${ASN1_GENERATOR_DEPENDENCY} "${CMAKE_SOURCE_DIR}/tests/fixtures/telecom/per_golden.asn"
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
    COMMAND "${ASN1_GENERATOR}" --emit-dir "${_outdir}" --codec ${codec} --namespace ${ns}
            "${ASN1_PERSON_ASN}"
    DEPENDS ${ASN1_GENERATOR_DEPENDENCY} "${ASN1_PERSON_ASN}"
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
  COMMAND "${ASN1_GENERATOR}" --emit-dir "${ASN1_RRC_SLICE_OUT_DIR}" --codec uper
          "${CMAKE_SOURCE_DIR}/tests/fixtures/telecom/rrc_slice/rrc_slice.asn"
  DEPENDS ${ASN1_GENERATOR_DEPENDENCY} "${CMAKE_SOURCE_DIR}/tests/fixtures/telecom/rrc_slice/rrc_slice.asn"
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
  COMMAND "${ASN1_GENERATOR}" --emit-dir "${ASN1_TLV_OER_GEN_DIR}" --codec ber,der,oer,jer
          --namespace asn1_tlv_oer
          "${CMAKE_SOURCE_DIR}/tests/fixtures/telecom/tlv_oer_golden.asn"
  DEPENDS ${ASN1_GENERATOR_DEPENDENCY} "${CMAKE_SOURCE_DIR}/tests/fixtures/telecom/tlv_oer_golden.asn"
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
  COMMAND "${ASN1_GENERATOR}" --emit-dir "${ASN1_TLV_GOLDEN_GEN_DIR}" --codec ber,der,oer
          --namespace tg
          "${CMAKE_SOURCE_DIR}/tests/fixtures/telecom/tlv_golden.asn"
  DEPENDS ${ASN1_GENERATOR_DEPENDENCY} "${CMAKE_SOURCE_DIR}/tests/fixtures/telecom/tlv_golden.asn"
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
  COMMAND "${ASN1_GENERATOR}" --emit-dir "${ASN1_CER_GEN_DIR}" --codec cer,der
          --namespace asn1_cer
          "${CMAKE_SOURCE_DIR}/tests/fixtures/cer/cer_golden.asn"
  DEPENDS ${ASN1_GENERATOR_DEPENDENCY} "${CMAKE_SOURCE_DIR}/tests/fixtures/cer/cer_golden.asn"
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
  COMMAND "${ASN1_GENERATOR}" --emit-dir "${ASN1_IOC_JER_OUT_DIR}" --codec jer
          --namespace asn1_ioc_jer "${CMAKE_SOURCE_DIR}/examples/ioc.asn"
  DEPENDS ${ASN1_GENERATOR_DEPENDENCY} "${CMAKE_SOURCE_DIR}/examples/ioc.asn"
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
  COMMAND "${ASN1_GENERATOR}" --emit-dir "${ASN1_TC_IE_OUT_DIR}" --codec uper
          --namespace tc_test
          "${CMAKE_SOURCE_DIR}/tests/fixtures/telecom/table_constraint_ie.asn"
  DEPENDS ${ASN1_GENERATOR_DEPENDENCY} "${CMAKE_SOURCE_DIR}/tests/fixtures/telecom/table_constraint_ie.asn"
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
  COMMAND "${ASN1_GENERATOR}" --emit-dir "${ASN1_RRC_SLICE_OER_DIR}" --codec oer --namespace asn1_rrc_oer
          "${CMAKE_SOURCE_DIR}/tests/fixtures/telecom/rrc_slice/rrc_slice.asn"
  DEPENDS ${ASN1_GENERATOR_DEPENDENCY} "${CMAKE_SOURCE_DIR}/tests/fixtures/telecom/rrc_slice/rrc_slice.asn"
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
# Re-emit with the current generator, but preserve final file timestamps when
# bytes are unchanged. Ninja restat then reuses objects for identical output.
function(asn1_staged_copies stage destination result)
  set(_commands COMMAND "${CMAKE_COMMAND}" -E make_directory "${destination}")
  foreach(_file IN LISTS ARGN)
    list(APPEND _commands COMMAND "${CMAKE_COMMAND}" -E copy_if_different
      "${stage}/${_file}" "${destination}/${_file}")
  endforeach()
  set(${result} "${_commands}" PARENT_SCOPE)
endfunction()

macro(asn1_fixture_compile_gate name codec lib fixture)
  set(_dir "${CMAKE_CURRENT_BINARY_DIR}/generated_${name}")
  set(_stage "${_dir}.stage")
  asn1_staged_copies("${_stage}" "${_dir}" _copies generated.hpp generated.cpp)
  add_custom_command(
    OUTPUT "${_dir}/generated.hpp" "${_dir}/generated.cpp"
    COMMAND "${ASN1_GENERATOR}" --emit-dir "${_stage}" --codec ${codec} --namespace asn1_${name}
            "${CMAKE_SOURCE_DIR}/tests/fixtures/telecom/${fixture}"
    ${_copies}
    DEPENDS ${ASN1_GENERATOR_DEPENDENCY} "${CMAKE_SOURCE_DIR}/tests/fixtures/telecom/${fixture}"
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
  # -O1 keeps standard calling conventions. Sanitized compile-only gates
  # use -g1: retain line information without multi-GB full debug type graphs.
  target_compile_options(asn1_${name}_compile PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-O1>
    $<$<AND:$<BOOL:${ASN1_ENABLE_SANITIZERS}>,$<CXX_COMPILER_ID:GNU,Clang>>:-g1>)
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
    "uper per" "aper per" "jeri jeri"
    "ber ber" "der der" "cer cer"
    "oer oer" "coer coer"
    "jer jeri" "xer xer" "exer exer" "cxer cxer")
foreach(_pair IN LISTS _all_codec_pairs)
  string(REPLACE " " ";" _pair_split "${_pair}")
  list(GET _pair_split 0 _codec)
  list(GET _pair_split 1 _lib)
  set(_dir "${CMAKE_CURRENT_BINARY_DIR}/generated_lpp_single_${_codec}")
  set(_stage "${_dir}.stage")
  asn1_staged_copies("${_stage}" "${_dir}" _copies generated.hpp generated.cpp)
  add_custom_command(
    OUTPUT "${_dir}/generated.hpp" "${_dir}/generated.cpp"
    COMMAND "${ASN1_GENERATOR}" --emit-dir "${_stage}" --codec ${_codec} --namespace asn1_lpp_single_${_codec}
            "${CMAKE_SOURCE_DIR}/tests/fixtures/telecom/lpp_14_3_0/LPP-PDU-Definitions.asn"
    ${_copies}
    DEPENDS ${ASN1_GENERATOR_DEPENDENCY} "${CMAKE_SOURCE_DIR}/tests/fixtures/telecom/lpp_14_3_0/LPP-PDU-Definitions.asn"
    COMMENT "Generating single-${_codec} C++ for telecom LPP (single-codec gate)"
    VERBATIM
  )
  add_library(asn1_lpp_single_${_codec}_compile STATIC "${_dir}/generated.cpp")
  target_include_directories(asn1_lpp_single_${_codec}_compile PRIVATE "${_dir}")
  target_link_libraries(asn1_lpp_single_${_codec}_compile PRIVATE asn1::${_lib})
  # Compile gate only — see macro comment above. -O1 shaves ~half the
  # compile time across the 12 single-codec LPP gates.
  target_compile_options(asn1_lpp_single_${_codec}_compile PRIVATE $<$<CXX_COMPILER_ID:GNU,Clang>:-O1>
    $<$<AND:$<BOOL:${ASN1_ENABLE_SANITIZERS}>,$<CXX_COMPILER_ID:GNU,Clang>>:-g1>)
endforeach()

# Split emit compile gates (--emit-split): per-module <Module>.hpp/.cpp
# pairs + aggregate header must compile. S1AP exercises cross-module
# references AND a merged mutually-recursive module unit (IEs + Containers +
# PDU-Contents share one TU); LPP covers the single-module unit path.
macro(asn1_fixture_split_gate name codec lib fixture)
  set(_dir "${CMAKE_CURRENT_BINARY_DIR}/generated_split_${name}")
  set(_outs "${_dir}/generated.hpp")
  set(_srcs "")
  set(_relative_outputs generated.hpp)
  set(_stage "${_dir}.stage")
  foreach(_stem ${ARGN})
    list(APPEND _outs "${_dir}/${_stem}.hpp" "${_dir}/${_stem}.cpp")
    list(APPEND _relative_outputs "${_stem}.hpp" "${_stem}.cpp")
    list(APPEND _srcs "${_dir}/${_stem}.cpp")
  endforeach()
  asn1_staged_copies("${_stage}" "${_dir}" _copies ${_relative_outputs})
  add_custom_command(
    OUTPUT ${_outs}
    COMMAND "${ASN1_GENERATOR}" --emit-dir "${_stage}" --codec ${codec} --namespace asn1_split_${name}
            --emit-split "${CMAKE_SOURCE_DIR}/tests/fixtures/telecom/${fixture}"
    ${_copies}
    DEPENDS ${ASN1_GENERATOR_DEPENDENCY} "${CMAKE_SOURCE_DIR}/tests/fixtures/telecom/${fixture}"
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
  target_compile_options(asn1_split_${name} PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-O1>
    $<$<AND:$<BOOL:${ASN1_ENABLE_SANITIZERS}>,$<CXX_COMPILER_ID:GNU,Clang>>:-g1>)
endmacro()

# LPP: single module -> one unit file. S1AP: three units (CommonDataTypes,
# PDU-Descriptions, and the merged IEs/Containers/PDU-Contents cycle).
asn1_fixture_split_gate(lpp_split uper per lpp_14_3_0/LPP-PDU-Definitions.asn
  LPP_PDU_Definitions)
asn1_fixture_split_gate(s1ap_split uper per s1ap_14_4_0/s1ap_14_4_0.asn
  S1AP_CommonDataTypes S1AP_PDU_Descriptions S1AP_IEs)

# asn1cxx CLI end-to-end tests (spawn the built asn1cxx binary).
if(UNIX)
add_executable(asn1_cli_tests
  codegen/cli_test.cpp
)
target_compile_definitions(asn1_cli_tests PRIVATE
  ASN1CXX_BIN="$<TARGET_FILE:asn1cxx>"
  ASN1_EXAMPLES_DIR="${CMAKE_SOURCE_DIR}/examples")
target_link_libraries(asn1_cli_tests PRIVATE GTest::gtest_main)
add_dependencies(asn1_cli_tests asn1cxx)
gtest_discover_tests(asn1_cli_tests)
endif()

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
  codegen/cgen_batch2_test.cpp
  codegen/cgen_batch1_test.cpp
  codegen/emission_plan_test.cpp
  codegen/emit_cache_test.cpp
)
target_compile_definitions(asn1_emit_cache_tests PRIVATE
  ASN1CXX_FIXTURES_DIR="${CMAKE_SOURCE_DIR}/tests")
target_link_libraries(asn1_emit_cache_tests PRIVATE
  asn1::frontend asn1::semantic asn1::codegen GTest::gtest_main)
gtest_discover_tests(asn1_emit_cache_tests)

# REV-10: compile, link and run the same E-XER negative cases in every shape.
foreach(_mode IN ITEMS outline inline split)
  set(_dir "${CMAKE_CURRENT_BINARY_DIR}/generated_attributes_${_mode}")
  set(_flags "")
  set(_cpp "${_dir}/generated.cpp")
  if(_mode STREQUAL "inline")
    set(_flags --emit-inline)
  elseif(_mode STREQUAL "split")
    set(_flags --emit-split)
    set(_cpp "${_dir}/AttrTest.cpp")
  endif()
  add_custom_command(
    OUTPUT "${_dir}/generated.hpp" "${_cpp}"
    COMMAND "${ASN1_GENERATOR}" --no-emit-cache --codec exer ${_flags} --emit-dir "${_dir}"
            "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/exer/attributes.asn"
    DEPENDS ${ASN1_GENERATOR_DEPENDENCY} "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/exer/attributes.asn"
    VERBATIM)
  add_executable(asn1_attributes_${_mode}_tests codegen/exer_attributes_test.cpp "${_cpp}")
  target_include_directories(asn1_attributes_${_mode}_tests PRIVATE "${_dir}")
  if(_mode STREQUAL "split")
    target_compile_definitions(asn1_attributes_${_mode}_tests PRIVATE ATTR_SPLIT)
  endif()
  target_link_libraries(asn1_attributes_${_mode}_tests PRIVATE asn1::exer GTest::gtest_main)
  gtest_discover_tests(asn1_attributes_${_mode}_tests TEST_PREFIX "${_mode}.")
endforeach()

# Batch 2 type-only API acceptance; no generated codec is claimed here.
add_executable(asn1_cgen_batch2_generate codegen/cgen_batch2_generate.cpp)
target_link_libraries(asn1_cgen_batch2_generate PRIVATE asn1::frontend asn1::semantic asn1::codegen)
set(_cgen2_dir "${CMAKE_CURRENT_BINARY_DIR}/cgen_batch2")
file(MAKE_DIRECTORY "${_cgen2_dir}")
add_custom_command(OUTPUT "${_cgen2_dir}/generated.h" "${_cgen2_dir}/generated.c"
  COMMAND asn1_cgen_batch2_generate "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/cgen_batch2.asn"
    "${_cgen2_dir}/generated.h" "${_cgen2_dir}/generated.c"
  DEPENDS asn1_cgen_batch2_generate "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/cgen_batch2.asn" VERBATIM)
add_library(asn1_cgen_batch2_header OBJECT codegen/cgen_batch2_header.cpp "${_cgen2_dir}/generated.h")
target_include_directories(asn1_cgen_batch2_header PRIVATE "${_cgen2_dir}" "${PROJECT_SOURCE_DIR}/include")
add_executable(asn1_cgen_batch2_lifecycle codegen/cgen_batch2_lifecycle.c
  codegen/cgen_batch2_second.c codegen/cgen_batch2_instrumented.c "${_cgen2_dir}/generated.h")
target_include_directories(asn1_cgen_batch2_lifecycle PRIVATE "${_cgen2_dir}")
target_link_libraries(asn1_cgen_batch2_lifecycle PRIVATE asn1::c_runtime)
add_dependencies(asn1_cgen_batch2_lifecycle asn1_cgen_batch2_header)
add_test(NAME CGenBatch2.GeneratedLifecycle COMMAND asn1_cgen_batch2_lifecycle)
add_test(NAME CGenBatch2.GeneratedOwningProfiles COMMAND asn1_cgen_batch2_lifecycle --profiles)
add_test(NAME CGenBatch2.GeneratedPlanningDecisions COMMAND asn1_cgen_batch2_lifecycle --plans)
add_test(NAME CGenBatch2.GeneratedSizeMembership COMMAND asn1_cgen_batch2_lifecycle --size)

# REV-16: every product entry links against its declared runtime and calls it.
set(_gate_names "")
set(_gate_codecs "")
foreach(_pair IN LISTS _all_codec_pairs)
  string(REPLACE " " ";" _parts "${_pair}")
  list(GET _parts 0 _codec)
  list(APPEND _gate_names "${_codec}")
endforeach()
foreach(_entry IN LISTS _gate_names ITEMS combined combined_inline combined_split)
  set(_flags "")
  set(_codecs "${_entry}")
  set(_dir "${CMAKE_CURRENT_BINARY_DIR}/entry_${_entry}")
  set(_cpp "${_dir}/generated.cpp")
  set(GATE_NAMESPACE "gate")
  if(_entry MATCHES "^combined")
    set(_codecs "${_gate_names}")
  endif()
  if(_entry STREQUAL "combined_inline")
    set(_flags --emit-inline)
  elseif(_entry STREQUAL "combined_split")
    set(_flags --emit-split)
    set(_cpp "${_dir}/Entries.cpp")
    set(GATE_NAMESPACE "gate::Entries")
  endif()
  set(GATE_CALLS "")
  set(_libs "")
  foreach(_codec IN LISTS _codecs)
    if(_codec MATCHES "uper|aper")
      string(APPEND GATE_CALLS "  BINARY(${_codec}, BitWriter, BitReader);\n")
      list(APPEND _libs asn1::per)
    elseif(_codec MATCHES "ber|der|cer|oer|coer")
      string(APPEND GATE_CALLS "  BINARY(${_codec}, ByteWriter, ByteReader);\n")
      list(APPEND _libs asn1::${_codec})
    elseif(_codec MATCHES "jer|jeri")
      string(APPEND GATE_CALLS "  TREE(${_codec}, jer);\n")
      list(APPEND _libs asn1::jeri)
    else()
      if(_codec STREQUAL "cxer")
        string(APPEND GATE_CALLS "  TREE(${_codec}, cxer);\n")
      else()
        string(APPEND GATE_CALLS "  TREE(${_codec}, xer);\n")
      endif()
      list(APPEND _libs asn1::${_codec})
    endif()
  endforeach()
  string(REPLACE ";" "," _selection "${_codecs}")
  configure_file(codegen/codec_entry_driver.cpp.in "${_dir}/driver.cpp" @ONLY)
  add_custom_command(OUTPUT "${_cpp}" "${_dir}/generated.hpp"
    COMMAND "${ASN1_GENERATOR}" --no-emit-cache --namespace gate --codec "${_selection}"
            ${_flags} --emit-dir "${_dir}" "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/codec_entries.asn"
    DEPENDS ${ASN1_GENERATOR_DEPENDENCY} "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/codec_entries.asn" VERBATIM)
  add_executable(asn1_entry_${_entry} "${_cpp}" "${_dir}/driver.cpp")
  target_include_directories(asn1_entry_${_entry} PRIVATE "${_dir}")
  target_link_libraries(asn1_entry_${_entry} PRIVATE ${_libs})
  add_test(NAME CodecEntry.${_entry} COMMAND asn1_entry_${_entry})
  list(APPEND _entry_executables "$<TARGET_FILE:asn1_entry_${_entry}>")
endforeach()
find_package(Python3 COMPONENTS Interpreter REQUIRED)
if(Python3_Interpreter_FOUND)
  if(NOT MSVC)
    find_program(ASN1_CGEN5_ORACLE NAMES asn1c)
    set(_cgen5_oracle_args)
    if(ASN1_CGEN5_ORACLE)
      list(APPEND _cgen5_oracle_args --asn1c "${ASN1_CGEN5_ORACLE}")
    endif()
    set(_cgen5_sanitizer_args)
    if(ASN1_ENABLE_SANITIZERS)
      list(APPEND _cgen5_sanitizer_args --sanitizers)
    endif()
    add_test(NAME CGenBatch5.EntryShapeMatrix COMMAND "${Python3_EXECUTABLE}"
      "${CMAKE_CURRENT_SOURCE_DIR}/codegen/cgen_batch5_matrix.py"
      --compiler "${ASN1_GENERATOR}" --cc "${CMAKE_C_COMPILER}" --cxx "${CMAKE_CXX_COMPILER}"
      --root "${PROJECT_SOURCE_DIR}" --build "${PROJECT_BINARY_DIR}"
      --work "${CMAKE_CURRENT_BINARY_DIR}/cgen5-matrix" ${_cgen5_sanitizer_args})
    add_test(NAME CGenBatch6.EntryShapeMatrix COMMAND "${Python3_EXECUTABLE}"
      "${CMAKE_CURRENT_SOURCE_DIR}/codegen/cgen_batch6_matrix.py"
      --compiler "${ASN1_GENERATOR}" --cc "${CMAKE_C_COMPILER}"
      --runtime "$<TARGET_FILE:asn1_c_runtime>" --root "${PROJECT_SOURCE_DIR}"
      --work "${CMAKE_CURRENT_BINARY_DIR}/cgen6-matrix" ${_cgen5_sanitizer_args})
    add_test(NAME CGenBatch5.WholeRrc COMMAND "${Python3_EXECUTABLE}"
      "${CMAKE_CURRENT_SOURCE_DIR}/codegen/cgen_batch5_rrc.py"
      --compiler "${ASN1_GENERATOR}" --cc "${CMAKE_C_COMPILER}"
      --runtime "$<TARGET_FILE:asn1_c_runtime>" --root "${PROJECT_SOURCE_DIR}"
      --work "${CMAKE_CURRENT_BINARY_DIR}/cgen5-rrc-acceptance" ${_cgen5_sanitizer_args} ${_cgen5_oracle_args})
  endif()
  if(NOT MSVC)
    set(_cgen2_sanitizer_args)
    if(ASN1_ENABLE_SANITIZERS)
      list(APPEND _cgen2_sanitizer_args --sanitizers)
    endif()
    add_test(NAME CGenBatch2.CompiledNamingMatrix COMMAND "${Python3_EXECUTABLE}"
      "${CMAKE_CURRENT_SOURCE_DIR}/codegen/cgen_batch2_names.py"
      --cc "${CMAKE_C_COMPILER}" --cxx "${CMAKE_CXX_COMPILER}"
      --generator "$<TARGET_FILE:asn1_cgen_batch2_generate>"
      --include "${PROJECT_SOURCE_DIR}/include" --work "${_cgen2_dir}/names-check" ${_cgen2_sanitizer_args})
    add_test(NAME CGenBatch2.C11LinkAndDuplicateDefinition COMMAND "${Python3_EXECUTABLE}"
      "${CMAKE_CURRENT_SOURCE_DIR}/codegen/cgen_batch2_link.py"
      --cc "${CMAKE_C_COMPILER}" --include "${PROJECT_SOURCE_DIR}/include"
      --generated "${_cgen2_dir}" --work "${_cgen2_dir}/link-check" --runtime "$<TARGET_FILE:asn1_c_runtime>" ${_cgen2_sanitizer_args})
  endif()
  add_test(NAME Architecture.Layering COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/scripts/validate_layering.py" --cmake "${CMAKE_COMMAND}")
  if(NOT MSVC)
    add_test(NAME Architecture.PublicHeaders COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/scripts/validate_public_headers.py" --cxx "${CMAKE_CXX_COMPILER}" --cc "${CMAKE_C_COMPILER}")
  endif()
  add_test(NAME StandardOracle.FrozenReplay COMMAND "${Python3_EXECUTABLE}"
    "${PROJECT_SOURCE_DIR}/scripts/validate_standard_oracles.py"
    --driver "$<TARGET_FILE:asn1_standard_oracle_driver>")
  add_test(NAME StandardCapabilities.Probes COMMAND "${Python3_EXECUTABLE}"
    "${PROJECT_SOURCE_DIR}/scripts/validate_standard_capabilities.py"
    --compiler "${ASN1_GENERATOR}" --output "${CMAKE_CURRENT_BINARY_DIR}/standard_capabilities")
  add_test(NAME CodecEntry.CombinationEquality COMMAND "${Python3_EXECUTABLE}"
    "${PROJECT_SOURCE_DIR}/scripts/validate_codec_entries.py" "${ASN1_GENERATOR}" ${_entry_executables})
  add_custom_target(asn1_codec_matrix
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/scripts/validate_codec_matrix.py"
      --compiler "${ASN1_GENERATOR}" --cxx "${CMAKE_CXX_COMPILER}"
      --output "${CMAKE_CURRENT_BINARY_DIR}/codec_matrix"
    DEPENDS ${ASN1_GENERATOR_DEPENDENCY} USES_TERMINAL VERBATIM)
endif()

# REV-25: a module dependency SCC must be one linked emission unit.
set(_cycle_dir "${CMAKE_CURRENT_BINARY_DIR}/split_cycle")
add_custom_command(OUTPUT "${_cycle_dir}/generated.hpp" "${_cycle_dir}/A.cpp"
  COMMAND "${ASN1_GENERATOR}" --no-emit-cache --codec uper --emit-split
    --emit-dir "${_cycle_dir}" "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/split_cycle.asn"
  DEPENDS ${ASN1_GENERATOR_DEPENDENCY} "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/split_cycle.asn" VERBATIM)
add_executable(asn1_split_cycle_tests codegen/split_cycle_test.cpp "${_cycle_dir}/A.cpp")
target_include_directories(asn1_split_cycle_tests PRIVATE "${_cycle_dir}")
target_link_libraries(asn1_split_cycle_tests PRIVATE asn1::per GTest::gtest_main)
gtest_discover_tests(asn1_split_cycle_tests)

# Batch 1 acceptance exercises real CLI failures and existing linked codecs.
add_test(NAME CGenBatch6.UnsupportedInstructions COMMAND "${Python3_EXECUTABLE}"
  "${CMAKE_CURRENT_SOURCE_DIR}/codegen/cgen_batch6_rejections.py" "${ASN1_GENERATOR}")
add_test(NAME CGenBatch6.FrozenOracleReplay COMMAND "${Python3_EXECUTABLE}"
  "${CMAKE_CURRENT_SOURCE_DIR}/codegen/cgen_batch6_frozen.py"
  "$<TARGET_FILE:asn1_cgen_batch6_oracle_driver>" "${CMAKE_CURRENT_SOURCE_DIR}/standards/cgen6-asn1tools-vectors.json")
add_test(NAME CGenBatch1.CliDispatch COMMAND "${Python3_EXECUTABLE}"
  "${CMAKE_CURRENT_SOURCE_DIR}/codegen/cgen_batch1_cli.py" "${ASN1_GENERATOR}"
  "$<TARGET_FILE:asn1_entry_combined>" "$<TARGET_FILE:asn1_entry_combined_inline>"
  "$<TARGET_FILE:asn1_entry_combined_split>")
foreach(_shape IN ITEMS combined combined_inline combined_split)
  add_test(NAME CGenBatch1.Cpp.${_shape} COMMAND asn1_entry_${_shape})
endforeach()

# C batch 2 literal normalization must preserve the C++ emitter's behaviour.
foreach(_mode IN ITEMS outline inline split)
  set(_default_dir "${CMAKE_CURRENT_BINARY_DIR}/cgen2_cpp_${_mode}")
  set(_default_flag "")
  set(_default_cpp "${_default_dir}/generated.cpp")
  if(_mode STREQUAL "inline")
    set(_default_flag --emit-inline)
  elseif(_mode STREQUAL "split")
    set(_default_flag --emit-split)
    set(_default_cpp "${_default_dir}/CppDefaults.cpp")
  endif()
  add_custom_command(OUTPUT "${_default_dir}/generated.hpp" "${_default_cpp}"
    COMMAND "${ASN1_GENERATOR}" --no-emit-cache --codec uper ${_default_flag}
      --emit-dir "${_default_dir}" "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/cgen2_cpp_defaults.asn"
    DEPENDS ${ASN1_GENERATOR_DEPENDENCY} "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/cgen2_cpp_defaults.asn" VERBATIM)
  add_executable(asn1_cgen2_cpp_${_mode}_tests codegen/cgen2_cpp_defaults_test.cpp "${_default_cpp}")
  target_include_directories(asn1_cgen2_cpp_${_mode}_tests PRIVATE "${_default_dir}")
  target_link_libraries(asn1_cgen2_cpp_${_mode}_tests PRIVATE asn1::per GTest::gtest_main)
  if(_mode STREQUAL "split")
    target_compile_definitions(asn1_cgen2_cpp_${_mode}_tests PRIVATE CGEN2_CPP_SPLIT)
  endif()
  gtest_discover_tests(asn1_cgen2_cpp_${_mode}_tests TEST_PREFIX "${_mode}.")
endforeach()

# C batch 3 exercises actual generated artifacts through the unified CLI.
foreach(_case IN ITEMS shapes single_module rrc_types publication cache options cmake)
  if(MSVC AND (_case STREQUAL "shapes" OR _case STREQUAL "single_module" OR _case STREQUAL "rrc_types"))
    continue() # Strict GNU flags and GNU time metrics; native CMake consumer remains enabled.
  endif()
  set(_cgen3_san_args)
  if(ASN1_ENABLE_SANITIZERS)
    list(APPEND _cgen3_san_args --sanitizers)
  endif()
  add_test(NAME CGenBatch3.${_case} COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/codegen/cgen_batch3.py"
    --compiler "${ASN1_GENERATOR}" --cc "${CMAKE_C_COMPILER}" --cxx "${CMAKE_CXX_COMPILER}"
    --runtime "$<TARGET_FILE:asn1_c_runtime>" --root "${PROJECT_SOURCE_DIR}"
    --work "${CMAKE_CURRENT_BINARY_DIR}/cgen3-${_case}" --case "${_case}"
    --cmake "${CMAKE_COMMAND}" ${_cgen3_san_args})
endforeach()
