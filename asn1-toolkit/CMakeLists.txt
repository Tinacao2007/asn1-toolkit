cmake_minimum_required(VERSION 3.16)

project(asn1-toolkit
  VERSION 0.1.0
  DESCRIPTION "ASN.1 compiler and codec runtime"
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ASN1_BUILD_TESTS "Build unit and integration tests" ON)

add_library(asn1_common INTERFACE)
add_library(asn1::common ALIAS asn1_common)
target_include_directories(asn1_common INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

add_library(asn1_support STATIC
  src/support/diagnostics.cpp
  src/support/source_file.cpp
)
add_library(asn1::support ALIAS asn1_support)
target_include_directories(asn1_support PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(asn1_support PUBLIC asn1::common)

add_library(asn1_ast STATIC
  src/ast/print.cpp
)
add_library(asn1::ast ALIAS asn1_ast)
target_include_directories(asn1_ast PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(asn1_ast PUBLIC asn1::support)

add_library(asn1_frontend STATIC
  src/frontend/token.cpp
  src/frontend/lexer.cpp
  src/frontend/parser.cpp
  src/frontend/parser_ioc.cpp
)
add_library(asn1::frontend ALIAS asn1_frontend)
target_include_directories(asn1_frontend PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(asn1_frontend PUBLIC asn1::ast)

add_library(asn1_ir STATIC
  src/ir/type.cpp
  src/ir/print.cpp
)
add_library(asn1::ir ALIAS asn1_ir)
target_include_directories(asn1_ir PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(asn1_ir PUBLIC asn1::support)

add_library(asn1_constraints STATIC
  src/constraints/normalize.cpp
)
add_library(asn1::constraints ALIAS asn1_constraints)
target_include_directories(asn1_constraints PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(asn1_constraints PUBLIC asn1::ast asn1::ir)

add_library(asn1_semantic STATIC
  src/semantic/symbol_table.cpp
  src/semantic/analyzer.cpp
)
add_library(asn1::semantic ALIAS asn1_semantic)
target_include_directories(asn1_semantic PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(asn1_semantic PUBLIC asn1::ast asn1::ir asn1::constraints)

add_library(asn1_runtime STATIC
  src/runtime/byte_io.cpp
  src/runtime/bit_io.cpp
  src/runtime/bigint.cpp
)
add_library(asn1::runtime ALIAS asn1_runtime)
target_include_directories(asn1_runtime PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(asn1_runtime PUBLIC asn1::common)

add_library(asn1_ber STATIC
  src/runtime/ber/tlv.cpp
  src/runtime/ber/codec.cpp
)
add_library(asn1::ber ALIAS asn1_ber)
target_include_directories(asn1_ber PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(asn1_ber PUBLIC asn1::runtime)

add_library(asn1_der STATIC
  src/runtime/der/tlv.cpp
  src/runtime/der/codec.cpp
)
add_library(asn1::der ALIAS asn1_der)
target_include_directories(asn1_der PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(asn1_der PUBLIC asn1::ber)

add_library(asn1_per STATIC
  src/runtime/per/primitives.cpp
  src/runtime/per/codec.cpp
)
add_library(asn1::per ALIAS asn1_per)
target_include_directories(asn1_per PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(asn1_per PUBLIC asn1::runtime asn1::ber)

add_library(asn1_oer STATIC
  src/runtime/oer/codec.cpp
)
add_library(asn1::oer ALIAS asn1_oer)
target_include_directories(asn1_oer PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(asn1_oer PUBLIC asn1::runtime asn1::ber)

add_library(asn1_coer STATIC
  src/runtime/coer/codec.cpp
)
add_library(asn1::coer ALIAS asn1_coer)
target_include_directories(asn1_coer PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(asn1_coer PUBLIC asn1::oer)

add_library(asn1_xer STATIC
  src/runtime/xer/xml.cpp
  src/runtime/xer/codec.cpp
)
add_library(asn1::xer ALIAS asn1_xer)
target_include_directories(asn1_xer PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(asn1_xer PUBLIC asn1::runtime)

add_library(asn1_cxer STATIC
  src/runtime/cxer/codec.cpp
)
add_library(asn1::cxer ALIAS asn1_cxer)
target_include_directories(asn1_cxer PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(asn1_cxer PUBLIC asn1::xer)

add_library(asn1_exer STATIC
  src/runtime/exer/codec.cpp
)
add_library(asn1::exer ALIAS asn1_exer)
target_include_directories(asn1_exer PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(asn1_exer PUBLIC asn1::xer)

add_library(asn1_jer STATIC
  src/runtime/jer/json.cpp
  src/runtime/jer/codec.cpp
)
add_library(asn1::jer ALIAS asn1_jer)
target_include_directories(asn1_jer PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(asn1_jer PUBLIC asn1::runtime)

add_library(asn1_jeri STATIC
  src/runtime/jeri/codec.cpp
)
add_library(asn1::jeri ALIAS asn1_jeri)
target_include_directories(asn1_jeri PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(asn1_jeri PUBLIC asn1::jer)

add_library(asn1_codegen STATIC
  src/codegen/emit.cpp
)
add_library(asn1::codegen ALIAS asn1_codegen)
target_include_directories(asn1_codegen PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(asn1_codegen PUBLIC asn1::ir asn1::support)

add_executable(asn1cxx tools/asn1cxx/main.cpp)
target_link_libraries(asn1cxx PRIVATE asn1::frontend asn1::semantic asn1::codegen)

if(ASN1_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
