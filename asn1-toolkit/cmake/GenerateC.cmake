# Configure-time source discovery. The executable must run on the build host.
# Schema, generator and declared artifact changes cause CMake to reconfigure;
# adding/removing a type or module therefore updates the target's source list.
function(asn1_generate_c result)
  cmake_parse_arguments(ARG "" "GENERATOR;OUTPUT_DIRECTORY;CODECS" "SCHEMAS;OPTIONS" ${ARGN})
  if(ARG_UNPARSED_ARGUMENTS OR NOT ARG_GENERATOR OR NOT ARG_OUTPUT_DIRECTORY OR NOT ARG_SCHEMAS)
    message(FATAL_ERROR "asn1_generate_c requires GENERATOR, OUTPUT_DIRECTORY and SCHEMAS")
  endif()
  if(NOT EXISTS "${ARG_GENERATOR}" OR IS_DIRECTORY "${ARG_GENERATOR}")
    message(FATAL_ERROR "asn1_generate_c requires an already-built native host executable")
  endif()
  if(NOT ARG_CODECS)
    set(ARG_CODECS uper)
  endif()
  get_filename_component(_out "${ARG_OUTPUT_DIRECTORY}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_BINARY_DIR}")
  set(_schemas)
  foreach(_schema IN LISTS ARG_SCHEMAS)
    get_filename_component(_schema "${_schema}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    list(APPEND _schemas "${_schema}")
  endforeach()
  execute_process(COMMAND "${ARG_GENERATOR}" --lang c --codec "${ARG_CODECS}"
    ${ARG_OPTIONS} --emit-dir "${_out}" ${_schemas}
    RESULT_VARIABLE _status OUTPUT_VARIABLE _stdout ERROR_VARIABLE _stderr)
  if(NOT _status EQUAL 0)
    message(FATAL_ERROR "Native C generation failed (${_status}): ${_stdout}${_stderr}")
  endif()
  include("${_out}/generated-sources.cmake")
  if(NOT ASN1_GENERATED_SOURCES AND NOT ASN1_GENERATED_HEADERS)
    message(FATAL_ERROR "C generator returned an empty artifact manifest")
  endif()
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ${_schemas} "${ARG_GENERATOR}" "${_out}/generated-sources.cmake"
    ${ASN1_GENERATED_SOURCES} ${ASN1_GENERATED_HEADERS})
  set(${result} ${ASN1_GENERATED_SOURCES} PARENT_SCOPE)
  set(${result}_HEADERS ${ASN1_GENERATED_HEADERS} PARENT_SCOPE)
  set(${result}_INCLUDE_DIRECTORY "${_out}" PARENT_SCOPE)
endfunction()
