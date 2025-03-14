function(add_shad_executable NAME)
  add_executable(${NAME} ${ARGN})
  set_target_properties(${NAME} PROPERTIES COMPILE_FLAGS "-Wall -Werror -Wextra -Wno-gnu-string-literal-operator-template -Wpedantic")
endfunction()

function(add_shad_library NAME)
  add_library(${NAME} ${ARGN})
  set_target_properties(${NAME} PROPERTIES COMPILE_FLAGS "-Wall -Werror -Wextra -Wno-gnu-string-literal-operator-template -Wpedantic")
endfunction()

function(add_shad_shared_library NAME)
  add_library(${NAME} SHARED ${ARGN})
  set_target_properties(${NAME} PROPERTIES COMPILE_FLAGS "-Wall -Werror -Wextra -Wno-gnu-string-literal-operator-template -Wpedantic")
endfunction()

function(add_catch TARGET)
  add_shad_executable(${TARGET} ${ARGN})
  target_link_libraries(${TARGET} PRIVATE catch_main)
endfunction()
