message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
message(STATUS "LLVM include dir is ${LLVM_INCLUDE_DIR}")

add_subdirectory(checker)
add_subdirectory(tests)
