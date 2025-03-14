function(add_target NAME FILE)
    add_catch(${NAME} ${FILE})
    target_include_directories(${NAME} PRIVATE ../parsecpp/lib)
    target_include_directories(${NAME} PRIVATE ../parsecpp-constants)
endfunction()

add_target(test_advanced_combinators test_advanced_combinators.cpp)
add_target(test_parser_operators test_parser_operators.cpp)
