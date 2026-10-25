# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 4.3.1-msvc1)

# Reject any attempt to use a toolchain file. We must not use one because
# we could be downloading it here. If the CMAKE_TOOLCHAIN_FILE environment
# variable is set, the cache variable will have been initialized from it.
unset(CMAKE_TOOLCHAIN_FILE CACHE)
unset(ENV{CMAKE_TOOLCHAIN_FILE})

# We name the project and the target for the ExternalProject_Add() call
# to something that will highlight to the user what we are working on if
# something goes wrong and an error message is produced.

project(sdl3-populate NONE)



include(ExternalProject)
ExternalProject_Add(sdl3-populate
                     "UPDATE_DISCONNECTED" "False" "DOWNLOAD_EXTRACT_TIMESTAMP" "TRUE" "EXTERNALPROJECT_INTERNAL_ARGUMENT_SEPARATOR" "URL" "C:/Users/espen/OneDrive/Dokumenter/ChatGPT/SLANG/build/downloads/SDL3-3.4.18.zip" "URL_HASH" "SHA256=9cd42377704398796071b8597cd7e21da254a43bad98c4199739647adc13fa6f"
                    SOURCE_DIR          "C:/Users/espen/OneDrive/Dokumenter/ChatGPT/SLANG/_deps/sdl3-src"
                    BINARY_DIR          "C:/Users/espen/OneDrive/Dokumenter/ChatGPT/SLANG/_deps/sdl3-build"
                    CONFIGURE_COMMAND   ""
                    BUILD_COMMAND       ""
                    INSTALL_COMMAND     ""
                    TEST_COMMAND        ""
                    USES_TERMINAL_DOWNLOAD  YES
                    USES_TERMINAL_UPDATE    YES
                    USES_TERMINAL_PATCH     YES
)


