cmake_minimum_required(VERSION 3.14)
project(shad-cpp)

set(CMAKE_CXX_STANDARD             23)
set(CMAKE_MODULE_PATH              "${CMAKE_SOURCE_DIR}/tools/cmake")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
set(CMAKE_EXPORT_COMPILE_COMMANDS  ON)

find_package(Catch REQUIRED)
find_package(TBB REQUIRED)

find_package(PNG)
find_package(JPEG)
find_package(FFTW)

set(Clang_DIR "/usr/lib/llvm-19/lib/cmake/clang")
find_package(Clang CONFIG)

include_directories(tools/commons)
include(tools/cmake/TestSolution.cmake)
include(tools/cmake/BuildFlags.cmake)

function(add_if_exists name)
  if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${name})
    add_subdirectory(${name})
  endif()
endfunction()

add_if_exists(range-sum)

add_if_exists(is-prime)
add_if_exists(reduce)
add_if_exists(subset-sum)
add_if_exists(hash-table)

add_if_exists(semaphore)
add_if_exists(rw-lock)
add_if_exists(timerqueue)
add_if_exists(buffered-channel)
add_if_exists(unbuffered-channel)

if (PNG_FOUND AND JPEG_FOUND AND FFTW_FOUND)
  add_if_exists(jpeg-decoder)
elseif()
  message(STATUS "jpeg-decoder disabled. PNG:${PNG_FOUND} JPEG:${JPEG_FOUND} FFTW:${FFTW_FOUND}")
endif()

add_if_exists(spinlock)
add_if_exists(rw-spinlock)
add_if_exists(rw-counter)
add_if_exists(fast-queue)
add_if_exists(mpsc-queue)

add_if_exists(futex)
add_if_exists(hazard-ptr)
add_if_exists(lock-free-stack)
add_if_exists(sync-map)

add_if_exists(executor)

if (UNIX AND NOT APPLE)
  include(tools/cmake/Protobuf.cmake)
  find_package(glog)

  if (Protobuf_FOUND AND glog_FOUND)
    message("Networking tasks are enabled")

    add_if_exists(tools/cactus)

    add_if_exists(portscan)
    add_if_exists(portknock)
    add_if_exists(socks4)
    add_if_exists(fanout)

    add_if_exists(socksd)
    add_if_exists(balancer)

    add_if_exists(redis)
  else()
    message(WARNING "Networking tasks disabled. "
      "Protobuf:${Protobuf_FOUND} glog:${glog_FOUND}"
    )
  endif()

else()
  message(WARNING "For networking tasks only Linux is supported. "
    "See instructions for remote toolchain setup."
  )
endif()

if (LLVM_FOUND)
  message("clang-fun enabled")
  add_if_exists(clang-fun)
else()
  message(WARNING "clang-fun disabled")
endif()

add_if_exists(sfinae)
add_if_exists(compile-eval)
add_if_exists(constexpr-map)

add_if_exists(coroutine)
add_if_exists(concepts)
add_if_exists(ranges)

add_if_exists(matrix-2.0)
add_if_exists(matrix-3.0)

add_if_exists(parsecpp-constants)
add_if_exists(parsecpp-basic)
add_if_exists(parsecpp-basic-combinators)
add_if_exists(parsecpp-advanced)
add_if_exists(parsecpp)
if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/private)
  add_subdirectory(private)
endif()
