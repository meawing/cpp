file(GLOB SOLUTION_SRC CONFIGURE_DEPENDS "*.cpp")
add_shad_shared_library(check_names ${SOLUTION_SRC})

target_include_directories(check_names SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
target_compile_definitions(check_names PRIVATE ${LLVM_DEFINITIONS})
target_link_directories(check_names PRIVATE ${LLVM_LIBRARY_DIRS})

target_link_libraries(check_names PRIVATE
  clangAnalysis
  clangAST
  clangASTMatchers
  clangBasic
  clangCodeGen
  clangDriver
  clangEdit
  clangFrontend
  clangLex
  clangParse
  clangRewrite
  clangSema
  clangSerialization
  clangTooling)
