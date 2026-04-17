cmake_minimum_required(VERSION 3.16)

set(PROJECT_NAME "mmolch_qtutil_json")
set(PROJECT_VERSION "1.0.0")

project(${PROJECT_NAME}
    VERSION ${PROJECT_VERSION}
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)

find_package(Qt6 6.4 REQUIRED COMPONENTS
    Core
)

qt_standard_project_setup()

add_library(${PROJECT_NAME} STATIC
    include/mmolch/qtutil_json.h
    include/mmolch/qtutil_json_error.h
    include/mmolch/qtutil_json_pointer.h
    src/qtutil_json.cpp
    src/qtutil_json_pointer.cpp
    src/qtutil_json_validate.cpp
)

target_link_libraries(${PROJECT_NAME} PUBLIC
    Qt6::Core
)

target_include_directories(${PROJECT_NAME} PUBLIC
    include
)

if(CMAKE_CURRENT_LIST_DIR STREQUAL ${CMAKE_SOURCE_DIR})
    if (CMAKE_BUILD_TYPE MATCHES "Debug")
        target_compile_options(${PROJECT_NAME} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
        )
    endif ()

    include(CTest)
    if(BUILD_TESTING)
        add_subdirectory(tests)
    endif()
endif()
