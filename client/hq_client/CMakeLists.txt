cmake_minimum_required(VERSION 3.1)

set(CMAKE_CXX_FLAGS_DEBUG  "-g")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(BUILD_SHARED_LIBS OFF)
set(CMAKE_BUILD_TYPE Debug)

project(
  kamadan_trade_chat
  VERSION 1.0
  LANGUAGES C
)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../Dependencies/Headquarter Headquarter)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../Dependencies/curl-8.10.1 libcurl)


# Look for required libraries
add_library(
  kamadan_trade_chat

  SHARED

  main.c
)

target_link_libraries(kamadan_trade_chat PRIVATE libcurl common client)

set_target_properties(common
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/"
)

set_target_properties(portal
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/"
)

set_target_properties(client
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/"
)
set_target_properties(kamadan_trade_chat
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/"
)


target_compile_definitions(kamadan_trade_chat PRIVATE HEADQUARTER_BUILD_EXPORTS)

target_include_directories(
  kamadan_trade_chat

  PUBLIC
  Headquarter/include
  libcurl/include
)




if (MSVC)
  # We disable the following warnings:
  # - C4057: nonstandard extension used: nameless struct/union
  # - C4204: nonstandard extension used: non-constant aggregate initializer
  # - C4221: nonstandard extension used: '<var>': cannot be initialized using address of automatic variable
  # - C4459: declaration of '<var>' hides global declaration
  # - C4706: assignment within conditional expression
  target_compile_options(kamadan_trade_chat PRIVATE /WX /W4 /wd4201 /wd4204 /wd4221 /wd4459 /wd4100 /wd4706)
  set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT client)
endif()

