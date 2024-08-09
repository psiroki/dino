cmake_minimum_required (VERSION 3.8)

project(DinoJump)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -flto")

option(BITTBOY "Build for BittBoy/PocketGo compatibility" OFF)
option(MIYOO "Build for Miyoo Mini compatibility" OFF)
option(RG35XX "Build for RG35XX compatibility" OFF)

message("Compiling for: ${CMAKE_SYSTEM_NAME}")

if (NOT ${CMAKE_SYSTEM_NAME} STREQUAL "Emscripten")
  find_package(SDL REQUIRED)
  include_directories(${SDL_INCLUDE_DIR})
endif()

if(BITTBOY)
    message(STATUS "BittBoy/PocketGo mode")
    add_definitions(-DBITTBOY)
endif()

if(MIYOO)
    message(STATUS "Miyoo mode")
    add_definitions(-DMIYOO)
endif()

if(RG35XX)
    message(STATUS "RG35XX mode")
    add_definitions(-DRG35XX)
endif()

if(NOT RG35XX AND NOT MIYOO AND NOT BITTBOY)
    message(STATUS "Desktop mode")
    add_definitions(-DDESKTOP)
endif()


if (${CMAKE_SYSTEM_NAME} STREQUAL "Emscripten")
  set(main_target_dir ${CMAKE_CURRENT_BINARY_DIR}/web/)
  message(STATUS "WASM directory: ${main_target_dir}")
  set(CMAKE_EXECUTABLE_SUFFIX ".html")
  set(assets_dir "${CMAKE_SOURCE_DIR}/assets")
else()
  set(main_target_dir ${CMAKE_CURRENT_BINARY_DIR}/target/)
  message(STATUS "Target directory: ${main_target_dir}")
endif()

make_directory(${main_target_dir})

set(tool_test_target_dir ${CMAKE_CURRENT_BINARY_DIR}/tool/)
message(STATUS "Test & tool directory: ${tool_test_target_dir}")
make_directory(${tool_test_target_dir})

file(GLOB_RECURSE sources src/*.cc)


if (NOT ${CMAKE_SYSTEM_NAME} STREQUAL "Emscripten")
  file(GLOB_RECURSE test_sources test/*.cc)
  file(GLOB_RECURSE tool_sources tool/*.cc)

  set(tests ${sources})
  get_filename_component(dino_jump_main src/dino_jump.cc ABSOLUTE)
  list(REMOVE_ITEM tests ${dino_jump_main})
  list(APPEND tests ${test_sources})

  if(NOT MIYOO AND NOT RG35XX)
    set(tools ${sources})
    list(REMOVE_ITEM tools ${dino_jump_main})
    list(APPEND tools ${tool_sources})

    add_executable(dino_jump_test ${tests})
    set_target_properties(dino_jump_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${tool_test_target_dir})
    target_compile_options(dino_jump_test PUBLIC "-g")
    target_link_libraries(dino_jump_test ${SDL_LIBRARY})
  endif()
endif()

add_executable(dino_jump ${sources})

if (NOT ${CMAKE_SYSTEM_NAME} STREQUAL "Emscripten")

  set_target_properties(dino_jump PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${main_target_dir})
  target_compile_options(dino_jump PUBLIC "-g")
  target_link_libraries(dino_jump ${SDL_LIBRARY} pthread)

else()
  message("Assets: ${assets_dir}")
  target_link_libraries(dino_jump PUBLIC ${EMSCRIPTEN_LIBRARIES})
  target_compile_options(dino_jump PUBLIC "-O3")
  target_link_options(dino_jump PUBLIC
    -sUSE_SDL=1
    --use-preload-plugins
    --preload-file ${assets_dir}@assets
    -sALLOW_MEMORY_GROWTH=1
    -sINITIAL_MEMORY=256MB
    -sTOTAL_STACK=128MB
    -sWASM=2
    ${EMSCRIPTEN_CFLAGS})
  set_target_properties(dino_jump PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${main_target_dir})
  set_target_properties(dino_jump PROPERTIES
    OUTPUT_NAME "${PROJECT_NAME}"
    OPTIMIZATION "s" # Optimize for size
  )
endif()
