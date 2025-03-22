add_shad_library(libhazard_ptr hazard_ptr.cpp)

add_catch(test_hazard_ptr test.cpp)
target_link_libraries(test_hazard_ptr PRIVATE libhazard_ptr)

add_catch(bench_hazard_ptr run.cpp)
target_link_libraries(bench_hazard_ptr PRIVATE libhazard_ptr)
