file(GLOB_RECURSE SOLUTION_SRC CONFIGURE_DEPENDS "lib/*.cpp")
add_library(parsecpp-lib ${SOLUTION_SRC})

target_link_libraries(parsecpp-lib PRIVATE ../parsecpp-constants)
target_include_directories(parsecpp-lib PUBLIC ../parsecpp-constants)

add_catch(test_parsecpp tests/test_parsecpp.cpp)
target_include_directories(test_parsecpp PUBLIC lib ../parsecpp-constants)
