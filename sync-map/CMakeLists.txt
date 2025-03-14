add_catch(test_sync_map test.cpp)
target_link_libraries(test_sync_map PRIVATE libhazard_ptr)

add_catch(bench_sync_map run.cpp)
target_link_libraries(bench_sync_map PRIVATE libhazard_ptr)
