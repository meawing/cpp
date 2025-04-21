enable_language(ASM)

add_library(cactus
  core/fiber.cpp
  core/scheduler.cpp
  core/timer.cpp
  core/debug.cpp
  core/group.cpp
  internal/context.cpp
  internal/context.S
  internal/wait_queue.cpp
  io/reader.cpp
  io/writer.cpp
  sync/mutex.cpp
  net/net.cpp
  net/poller.cpp)

target_link_libraries(cactus PUBLIC glog)
target_include_directories(cactus PUBLIC ..)

add_catch(test_cactus
  internal/context_test.cpp
  internal/intrusive_list_test.cpp
  internal/wait_queue_test.cpp
  io/io_test.cpp
  io/view_test.cpp
  core/scheduler_test.cpp
  core/group_test.cpp
  sync/mutex_test.cpp)
target_link_libraries(test_cactus PRIVATE cactus)

add_shad_executable(test_cactus_fiber_death core/fiber_death_test.cpp)
target_link_libraries(test_cactus_fiber_death PRIVATE cactus)

add_catch(test_cactus_net net/net_test.cpp)
target_link_libraries(test_cactus_net PRIVATE cactus)

add_catch(bench_cactus_context internal/context_benchmark.cpp)
target_link_libraries(bench_cactus_context PRIVATE cactus)

# add_catch(bench_cactus_scheduler core/scheduler_benchmark.cpp)
# target_link_libraries(bench_cactus_scheduler PRIVATE cactus)

# add_catch(bench_cactus_net_server net/net_server_benchmark.cpp)
# target_link_libraries(bench_cactus_net_server PRIVATE cactus)

# add_catch(bench_cactus_net_ping_pong net/net_ping_pong_benchmark.cpp)
# target_link_libraries(bench_cactus_net_ping_pong PRIVATE cactus)

add_subdirectory(examples)
add_subdirectory(rpc)
