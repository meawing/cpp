add_shad_library(resp_writer resp_writer.cpp)
target_link_libraries(resp_writer PUBLIC cactus)

add_catch(test_resp_writer test.cpp)
target_link_libraries(test_resp_writer PRIVATE resp_writer)
