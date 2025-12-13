find_package(Boost REQUIRED COMPONENTS program_options)

find_package(httplib CONFIG QUIET)

set(HTTPLIB_TARGETS "")

if(TARGET httplib::httplib)
    set(HTTPLIB_TARGETS httplib::httplib)
else()
    find_path(HTTPLIB_INCLUDE_DIR httplib.h
        PATHS /usr/include /usr/local/include
        PATH_SUFFIXES cpp-httplib include/cpp-httplib
    )

    if(NOT HTTPLIB_INCLUDE_DIR)
        message(FATAL_ERROR "cpp-httplib headers not found. Install libcpp-httplib-dev or add it via vcpkg.")
    endif()

    find_library(HTTPLIB_LIBRARY NAMES cpp-httplib)

    if(HTTPLIB_LIBRARY)
        list(APPEND HTTPLIB_TARGETS ${HTTPLIB_LIBRARY})
    else()
        message(STATUS "cpp-httplib library not found; assuming header-only distribution")
    endif()
endif()

configure_file(./config.hpp.in ./config.hpp)

set(TARGET accio)
set(SOURCES
    main.cpp
    core.hpp
    core.cpp
    utils/file.cpp
    utils/network.cpp
)

set(INDEX_HTML_FILE ${CMAKE_CURRENT_SOURCE_DIR}/index.html)
file(READ ${INDEX_HTML_FILE} INDEX_HTML_CONTENT)
set(GENERATED_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GENERATED_INCLUDE_DIR})
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/indexHtml.hpp.in ${GENERATED_INCLUDE_DIR}/indexHtml.hpp @ONLY)

if(ENABLE_TESTS)
    add_library(accioCore ${SOURCES})
endif()

add_executable(${TARGET} ${SOURCES})

if(ENABLE_TESTS)
    target_include_directories(accioCore PUBLIC ${GENERATED_INCLUDE_DIR})
    if(HTTPLIB_INCLUDE_DIR)
        target_include_directories(accioCore PUBLIC ${HTTPLIB_INCLUDE_DIR})
    endif()
    if(HTTPLIB_TARGETS)
        target_link_libraries(accioCore PUBLIC ${HTTPLIB_TARGETS})
    endif()
endif()

target_include_directories(${TARGET} PRIVATE ${GENERATED_INCLUDE_DIR})
if(HTTPLIB_INCLUDE_DIR)
    target_include_directories(${TARGET} PRIVATE ${HTTPLIB_INCLUDE_DIR})
endif()

target_link_libraries(${TARGET} PRIVATE Boost::program_options)
if(HTTPLIB_TARGETS)
    target_link_libraries(${TARGET} PRIVATE ${HTTPLIB_TARGETS})
endif()

if(WIN32)
    target_link_libraries(${TARGET} PRIVATE Shell32 Ole32)
endif()
