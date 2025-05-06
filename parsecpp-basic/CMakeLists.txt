function(add_target NAME FILE)
    add_catch(${NAME} ${FILE})
    target_include_directories(${NAME} PRIVATE ../parsecpp/lib)
    target_include_directories(${NAME} PRIVATE ../parsecpp-constants)
endfunction()

add_target(test_basic_parsers test_basic_parsers.cpp)
