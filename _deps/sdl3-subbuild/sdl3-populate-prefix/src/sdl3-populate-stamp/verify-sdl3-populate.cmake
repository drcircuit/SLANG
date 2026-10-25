# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

if("C:/Users/espen/OneDrive/Dokumenter/ChatGPT/SLANG/build/downloads/SDL3-3.4.18.zip" STREQUAL "")
  message(FATAL_ERROR "LOCAL can't be empty")
endif()

if(NOT EXISTS "C:/Users/espen/OneDrive/Dokumenter/ChatGPT/SLANG/build/downloads/SDL3-3.4.18.zip")
  message(FATAL_ERROR "File not found: C:/Users/espen/OneDrive/Dokumenter/ChatGPT/SLANG/build/downloads/SDL3-3.4.18.zip")
endif()

if("SHA256" STREQUAL "")
  message(WARNING "File cannot be verified since no URL_HASH specified")
  return()
endif()

if("9cd42377704398796071b8597cd7e21da254a43bad98c4199739647adc13fa6f" STREQUAL "")
  message(FATAL_ERROR "EXPECT_VALUE can't be empty")
endif()

message(VERBOSE "verifying file...
     file='C:/Users/espen/OneDrive/Dokumenter/ChatGPT/SLANG/build/downloads/SDL3-3.4.18.zip'")

file("SHA256" "C:/Users/espen/OneDrive/Dokumenter/ChatGPT/SLANG/build/downloads/SDL3-3.4.18.zip" actual_value)

if(NOT "${actual_value}" STREQUAL "9cd42377704398796071b8597cd7e21da254a43bad98c4199739647adc13fa6f")
  message(FATAL_ERROR "error: SHA256 hash of
  C:/Users/espen/OneDrive/Dokumenter/ChatGPT/SLANG/build/downloads/SDL3-3.4.18.zip
does not match expected value
  expected: '9cd42377704398796071b8597cd7e21da254a43bad98c4199739647adc13fa6f'
    actual: '${actual_value}'
")
endif()

message(VERBOSE "verifying file... done")
