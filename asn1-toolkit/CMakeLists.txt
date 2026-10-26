cmake_minimum_required(VERSION 3.16)

project(asn1-toolkit
  VERSION 0.1.0
  DESCRIPTION "ASN.1 compiler and codec runtime"
  LANGUAGES C CXX
)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_CROSSCOMPILING)
  set(_tests_default OFF)
else()
  set(_tests_default ON)
endif()
option(ASN1_BUILD_TESTS "Build unit and integration tests" ${_tests_default})
option(ASN1_BUILD_COMPILER "Build compiler libraries and asn1cxx" ON)
option(ASN1_BUILD_RUNTIME "Build C++ codec runtime libraries" ON)
option(ASN1_BUILD_C_RUNTIME "Build pure C11 runtime" ON)
option(ASN1_ENABLE_SANITIZERS "Build with ASan+UBSan (GCC/Clang only)" OFF)
set(ASN1_HOST_COMPILER "" CACHE FILEPATH "Native asn1cxx for cross-build code generation")
if(ASN1_ENABLE_SANITIZERS AND (MSVC OR NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang"))
  message(FATAL_ERROR "Combined ASan+UBSan requires GCC/Clang; MSVC ASan is not UBSan")
endif()
if(ASN1_BUILD_TESTS AND CMAKE_CROSSCOMPILING)
  message(FATAL_ERROR "Target tests are native-only; use a native gate build and a separate target build")
endif()
if(ASN1_BUILD_TESTS AND NOT (ASN1_BUILD_COMPILER AND ASN1_BUILD_RUNTIME AND ASN1_BUILD_C_RUNTIME))
  message(FATAL_ERROR "Tests require compiler, C++ runtime and C runtime")
endif()
if(ASN1_HOST_COMPILER)
  if(NOT EXISTS "${ASN1_HOST_COMPILER}")
    message(FATAL_ERROR "ASN1_HOST_COMPILER does not exist")
  endif()
  set(ASN1_GENERATOR "${ASN1_HOST_COMPILER}")
  set(ASN1_GENERATOR_DEPENDENCY "${ASN1_HOST_COMPILER}")
else()
  set(ASN1_GENERATOR "$<TARGET_FILE:asn1cxx>")
  set(ASN1_GENERATOR_DEPENDENCY asn1cxx)
endif()

function(asn1_configure_target target)
  get_target_property(_type ${target} TYPE)
  if(_type STREQUAL "INTERFACE_LIBRARY")
    return()
  endif()
  set_target_properties(${target} PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
  if(MSVC)
    target_compile_options(${target} PRIVATE /utf-8)
  endif()
  if(ASN1_ENABLE_SANITIZERS)
    target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined)
    target_link_options(${target} PRIVATE -fsanitize=address,undefined)
  elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" AND UNIX)
    target_compile_options(${target} PRIVATE -pipe)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
      target_compile_options(${target} PRIVATE -fno-plt)
    endif()
  endif()
endfunction()

add_library(asn1_common INTERFACE)
add_library(asn1::common ALIAS asn1_common)
target_include_directories(asn1_common INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

if(ASN1_BUILD_COMPILER)
add_library(asn1_support STATIC
  src/support/diagnostics.cpp
  src/support/source_file.cpp
)
add_library(asn1::support ALIAS asn1_support)
target_include_directories(asn1_support PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(asn1_support PUBLIC asn1::common)

add_library(asn1_ast STATIC
  src/ast/print.cpp
)
add_library(asn1::ast ALIAS asn1_ast)
target_include_directories(asn1_ast PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
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
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(asn1_frontend PUBLIC asn1::ast)

add_library(asn1_ir STATIC
  src/ir/type.cpp
  src/ir/print.cpp
)
add_library(asn1::ir ALIAS asn1_ir)
target_include_directories(asn1_ir PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(asn1_ir PUBLIC asn1::support)

add_library(asn1_constraints STATIC
  src/constraints/normalize.cpp
)
add_library(asn1::constraints ALIAS asn1_constraints)
target_include_directories(asn1_constraints PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(asn1_constraints PUBLIC asn1::ast asn1::ir)

add_library(asn1_semantic STATIC
  src/semantic/symbol_table.cpp
  src/semantic/analyzer.cpp
)
add_library(asn1::semantic ALIAS asn1_semantic)
target_include_directories(asn1_semantic PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
find_package(Threads REQUIRED)
target_link_libraries(asn1_semantic PUBLIC asn1::ast asn1::ir asn1::constraints PRIVATE Threads::Threads)

endif()

if(ASN1_BUILD_RUNTIME)
add_library(asn1_runtime STATIC
  src/runtime/byte_io.cpp
  src/runtime/bit_io.cpp
  src/runtime/bigint.cpp
)
add_library(asn1::runtime ALIAS asn1_runtime)
target_include_directories(asn1_runtime PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(asn1_runtime PUBLIC asn1::common)

add_library(asn1_ber STATIC
  src/runtime/ber/tlv.cpp
  src/runtime/ber/codec.cpp
)
add_library(asn1::ber ALIAS asn1_ber)
target_include_directories(asn1_ber PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(asn1_ber PUBLIC asn1::runtime)

add_library(asn1_der STATIC
  src/runtime/der/tlv.cpp
  src/runtime/der/codec.cpp
)
add_library(asn1::der ALIAS asn1_der)
target_include_directories(asn1_der PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(asn1_der PUBLIC asn1::ber)

add_library(asn1_cer STATIC
  src/runtime/cer/codec.cpp
)
add_library(asn1::cer ALIAS asn1_cer)
target_include_directories(asn1_cer PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(asn1_cer PUBLIC asn1::der)

add_library(asn1_per STATIC
  src/runtime/per/primitives.cpp
  src/runtime/per/codec.cpp
)
add_library(asn1::per ALIAS asn1_per)
target_include_directories(asn1_per PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(asn1_per PUBLIC asn1::runtime asn1::ber)

add_library(asn1_oer STATIC
  src/runtime/oer/codec.cpp
)
add_library(asn1::oer ALIAS asn1_oer)
target_include_directories(asn1_oer PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(asn1_oer PUBLIC asn1::runtime asn1::ber)

add_library(asn1_coer STATIC
  src/runtime/coer/codec.cpp
)
add_library(asn1::coer ALIAS asn1_coer)
target_include_directories(asn1_coer PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(asn1_coer PUBLIC asn1::oer)

add_library(asn1_xer STATIC
  src/runtime/xer/xml.cpp
  src/runtime/xer/codec.cpp
)
add_library(asn1::xer ALIAS asn1_xer)
target_include_directories(asn1_xer PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(asn1_xer PUBLIC asn1::runtime)

add_library(asn1_cxer STATIC
  src/runtime/cxer/codec.cpp
)
add_library(asn1::cxer ALIAS asn1_cxer)
target_include_directories(asn1_cxer PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(asn1_cxer PUBLIC asn1::xer)

add_library(asn1_exer STATIC
  src/runtime/exer/codec.cpp
)
add_library(asn1::exer ALIAS asn1_exer)
target_include_directories(asn1_exer PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(asn1_exer PUBLIC asn1::xer)

add_library(asn1_jer STATIC
  src/runtime/jer/json.cpp
  src/runtime/jer/codec.cpp
)
add_library(asn1::jer ALIAS asn1_jer)
target_include_directories(asn1_jer PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(asn1_jer PUBLIC asn1::runtime)

add_library(asn1_jeri STATIC
  src/runtime/jeri/codec.cpp
)
add_library(asn1::jeri ALIAS asn1_jeri)
target_include_directories(asn1_jeri PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(asn1_jeri PUBLIC asn1::jer)

endif()

if(ASN1_BUILD_COMPILER)
# Content identity changes automatically for compiler source/header edits.
set(_generator_inputs "${PROJECT_SOURCE_DIR}/CMakeLists.txt"
                      "${PROJECT_SOURCE_DIR}/cmake/generator_revision.hpp.in")
foreach(_module IN ITEMS common support ast frontend ir constraints semantic codegen)
  file(GLOB_RECURSE _module_inputs CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/src/${_module}/*.cpp" "${PROJECT_SOURCE_DIR}/src/${_module}/*.hpp"
    "${PROJECT_SOURCE_DIR}/include/asn1/${_module}/*.hpp")
  list(APPEND _generator_inputs ${_module_inputs})
endforeach()
file(GLOB_RECURSE _tool_inputs CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/tools/asn1cxx/*.cpp")
list(APPEND _generator_inputs ${_tool_inputs})
list(SORT _generator_inputs)
set(_generator_identity "cache-format-2|${CMAKE_CXX_COMPILER_ID}|${CMAKE_CXX_COMPILER_VERSION}|${CMAKE_SYSTEM_NAME}|${CMAKE_CXX_FLAGS}")
foreach(_file IN LISTS _generator_inputs)
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_file}")
  file(SHA256 "${_file}" _hash)
  file(RELATIVE_PATH _relative "${PROJECT_SOURCE_DIR}" "${_file}")
  string(APPEND _generator_identity "${_relative}:${_hash}\n")
endforeach()
string(SHA256 ASN1_GENERATOR_REVISION "${_generator_identity}")
configure_file(cmake/generator_revision.hpp.in generated/generator_revision.hpp @ONLY)

add_library(asn1_codegen STATIC
  src/codegen/c_types.cpp
  src/codegen/emit_output.cpp
  src/codegen/emit_core.cpp
  src/codegen/emit_per.cpp
  src/codegen/emit_jer.cpp
  src/codegen/emit_xer.cpp
  src/codegen/emit_oer.cpp
  src/codegen/emit_tlv.cpp
  src/codegen/emit_cache.cpp
  src/codegen/sha256.cpp)
add_library(asn1::codegen ALIAS asn1_codegen)
target_include_directories(asn1_codegen PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_include_directories(asn1_codegen PRIVATE "${PROJECT_BINARY_DIR}/generated")
target_link_libraries(asn1_codegen PUBLIC asn1::ir asn1::support)

add_executable(asn1cxx tools/asn1cxx/main.cpp)
target_link_libraries(asn1cxx PRIVATE asn1::frontend asn1::semantic asn1::codegen)

endif()

if(ASN1_BUILD_C_RUNTIME)
# P2-4 sub-task 2: parallel pure-C runtime. The C++ asn1cxx emits code that
# links against this library for `--lang c`. Header surface was locked in
# sub-task 1 (include/asn1/c_runtime/*); this static lib fills the bodies
# so the headers compile, link, and run. Sub-task 3+ wires the emitter.
add_library(asn1_c_runtime STATIC
  src/c_runtime/c_common.c
  src/c_runtime/bigint.c
  src/c_runtime/bit_io.c
  src/c_runtime/bit_string.c
  src/c_runtime/byte_io.c
  src/c_runtime/ber_tlv.c
  src/c_runtime/per.c
  src/c_runtime/per_core.c
  src/c_runtime/jer.c
  src/c_runtime/xer.c
)
add_library(asn1::c_runtime ALIAS asn1_c_runtime)
target_include_directories(asn1_c_runtime PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
if(NOT MSVC)
  target_link_libraries(asn1_c_runtime PUBLIC m)
endif()
# The pure C target has no C++ link dependencies. C11 properties are
# applied by asn1_configure_target; no per-source GNU flags are required.
endif()

if(ASN1_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

get_property(_asn1_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
foreach(_target IN LISTS _asn1_targets)
  asn1_configure_target(${_target})
endforeach()
if(ASN1_BUILD_TESTS)
  get_property(_test_targets DIRECTORY tests PROPERTY BUILDSYSTEM_TARGETS)
  foreach(_target IN LISTS _test_targets)
    get_target_property(_type ${_target} TYPE)
    if(_type MATCHES "EXECUTABLE|STATIC_LIBRARY|OBJECT_LIBRARY|SHARED_LIBRARY")
      asn1_configure_target(${_target})
    endif()
  endforeach()
endif()
include(cmake/Layering.cmake)
include(cmake/Install.cmake)
