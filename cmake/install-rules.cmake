install(
    TARGETS galluzlang_exe
    RUNTIME COMPONENT galluzlang_Runtime
)

if(PROJECT_IS_TOP_LEVEL)
  include(CPack)
endif()
