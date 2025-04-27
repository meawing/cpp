add_shad_library(resp_reader resp_reader.cpp)
target_link_libraries(resp_reader PUBLIC cactus)

add_catch(test_resp_reader test.cpp)
target_link_libraries(test_resp_reader PRIVATE resp_reader)
