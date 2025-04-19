add_catch(test_stack test.cpp)
target_link_libraries(test_stack PRIVATE libhazard_ptr)

add_catch(bench_stack run.cpp)
target_link_libraries(bench_stack PRIVATE libhazard_ptr)
