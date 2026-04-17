set(TESTS
    json_load_success
    json_load_missing
    json_load_parse_error
    json_merge_basic
    json_validate_success
    json_validate_failure
)

find_package(Qt6 6.4 REQUIRED COMPONENTS
    Test
)

foreach(TEST ${TESTS})
    add_executable(${TEST}_ctest ${TEST}_ctest.cpp)
    target_link_libraries(${TEST}_ctest PRIVATE Qt6::Test mmolch_qtutil_json)
    add_test(${TEST} "${CMAKE_BINARY_DIR}/tests/${TEST}_ctest")
    # Run tests from the source root
    set_tests_properties(${TEST}
        PROPERTIES
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
endforeach()
