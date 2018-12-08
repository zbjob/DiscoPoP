set(LLVM_USED_LIBS clangTooling)

add_clang_executable(FlowGraphTran FlowGraphTran.cpp)

install(TARGETS clang-check
      RUNTIME DESTINATION bin)      
