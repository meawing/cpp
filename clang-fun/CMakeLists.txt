file(GLOB_RECURSE TESTS_LIST CONFIGURE_DEPENDS "**/*.cpp")

foreach (test ${TESTS_LIST})
  get_filename_component(result ${test} NAME_WE)
  set(TARGET "clang_test_${result}")
  add_executable(${TARGET} ${test})
  target_compile_options(${TARGET} PRIVATE "-w")
endforeach(test)

add_catch(test_check_names_no_dict common.cpp test_no_dict.cpp)
target_link_libraries(test_check_names_no_dict PRIVATE check_names)

add_catch(test_check_names_checker common.cpp test_checker.cpp)
target_link_libraries(test_check_names_checker PRIVATE check_names)

add_catch(test_check_names_dict common.cpp test_dict.cpp)
target_link_libraries(test_check_names_dict PRIVATE check_names)
