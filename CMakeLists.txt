# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.8)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("larpsaver")

file(GLOB sources "src/*.c")

add_library(${PROJECT_NAME} STATIC ${sources} )

include_directories(${PROJECT_NAME} "include/")

if(WIN32)
	target_sources(${PROJECT_NAME} PRIVATE "src/platform/win32.c")
	target_link_libraries(${PROJECT_NAME} PRIVATE scrnsave comctl32)
endif()

function(add_example example)
	add_executable("${example}" examples/${example}.c )
	include_directories(${example} "include/")
	target_link_libraries(${example} PRIVATE larpsaver)
	# set_target_properties(${example} PROPERTIES SUFFIX ".scr")
endfunction()

add_example("opengl_example")