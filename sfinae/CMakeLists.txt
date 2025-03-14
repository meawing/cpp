add_catch(test_advance test_advance.cpp)
set_property(TARGET test_advance PROPERTY CXX_STANDARD)
target_compile_options(test_advance PRIVATE -std=c++14)

add_catch(test_set test_set.cpp)
set_property(TARGET test_set PROPERTY CXX_STANDARD)
target_compile_options(test_set PRIVATE -std=c++14)
