add_executable(calculator calculator.cpp)
target_link_libraries(calculator PRIVATE parsecpp)

add_executable(json_parser json_parser.cpp)
target_link_libraries(json_parser PRIVATE parsecpp)
