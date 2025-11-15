find_package(Boost REQUIRED COMPONENTS program_options)
find_package(Drogon CONFIG REQUIRED)

configure_file(./config.hpp.in ./config.hpp)

set(TARGET accio)
set(SOURCES
    main.cpp
    core.hpp
    core.cpp
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
endif()

target_include_directories(${TARGET} PRIVATE ${GENERATED_INCLUDE_DIR})

target_link_libraries(${TARGET} PRIVATE Boost::program_options Drogon::Drogon)

if(WIN32)
    target_link_libraries(${TARGET} PRIVATE Shell32 Ole32)
endif()
