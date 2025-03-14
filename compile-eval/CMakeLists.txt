add_executable(test_templates test_templates.cpp)
set_property(TARGET test_templates PROPERTY CXX_STANDARD)
target_compile_options(test_templates PRIVATE -std=c++98)

add_executable(test_constexpr_simple test_constexpr_simple.cpp)
set_property(TARGET test_constexpr_simple PROPERTY CXX_STANDARD)
target_compile_options(test_constexpr_simple PRIVATE -std=c++11)

add_executable(test_constexpr test_constexpr.cpp)
set_property(TARGET test_constexpr PROPERTY CXX_STANDARD)
target_compile_options(test_constexpr PRIVATE -std=c++14)
