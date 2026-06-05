cmake_minimum_required (VERSION 3.8)
project("larpsaver" LANGUAGES C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

file(GLOB sources "src/*.c")

add_library(${PROJECT_NAME} STATIC ${sources} )

include_directories(${PROJECT_NAME} "include/")
add_compile_definitions(${PROJECT_NAME} "_LARPSAVER")

if(WIN32)
  target_sources(${PROJECT_NAME} PRIVATE "src/platform/win32.c")
  target_link_libraries(${PROJECT_NAME} PRIVATE comctl32)
endif()

function(add_example example)
  find_package(OpenGL)
  add_executable("${example}" WIN32 examples/${example}.c )
  include_directories(${example} "include/")
  target_link_libraries(${example} PRIVATE larpsaver ${OPENGL_LIBRARIES})
  # set_target_properties(${example} PROPERTIES SUFFIX ".scr")
endfunction()

add_example("opengl_example")
