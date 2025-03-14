add_catch(test_reduce test.cpp)
add_catch(bench_reduce run.cpp)
target_link_libraries(bench_reduce PRIVATE TBB::tbb)
