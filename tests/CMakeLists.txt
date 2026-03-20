include(FetchContent)

FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.5.2
)

FetchContent_Declare(rapidcheck
    GIT_REPOSITORY https://github.com/emil-e/rapidcheck.git
    GIT_TAG master
)

FetchContent_MakeAvailable(Catch2 rapidcheck)

add_executable(parsecpp_tests
    test_pos.cpp
    test_error.cpp
    test_prim.cpp
    test_char.cpp
    test_combinator.cpp
    test_expr.cpp
    test_token.cpp
    test_integration.cpp
)

target_link_libraries(parsecpp_tests PRIVATE
    parsecpp
    Catch2::Catch2WithMain
    rapidcheck
)

include(Catch)
catch_discover_tests(parsecpp_tests)
