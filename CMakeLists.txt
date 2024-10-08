cmake_minimum_required (VERSION 3.8)

project(DinoJump)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -flto")

option(BITTBOY "Build for BittBoy/PocketGo compatibility" OFF)
option(MIYOO "Build for Miyoo Mini compatibility" OFF)
option(MIYOOA30 "Build for Miyoo A30 compatibility" OFF)
option(RG35XX "Build for RG35XX compatibility" OFF)
option(RG35XX22 "Build for RG35XX22 compatibility" OFF)
option(RG35XX22B "Build for RG35XX22B (Batocera) compatibility" OFF)
option(WEB "Build for web (Emscripten)" OFF)

message("Compiling for: ${CMAKE_SYSTEM_NAME}")

if(NOT BITTBOY AND NOT MIYOO AND NOT MIYOOA30 AND NOT RG35XX AND NOT RG35XX22 AND NOT RG35XX22B)
  # Web is desktop
  message(STATUS "Desktop mode")
  add_definitions(-DDESKTOP)
  set(DESKTOP ON)
else()
  set(DESKTOP OFF)
endif()


if (NOT ${CMAKE_SYSTEM_NAME} STREQUAL "Emscripten")
  find_package(SDL REQUIRED)
  include_directories(${SDL_INCLUDE_DIR})
endif()

if(BITTBOY)
  message(STATUS "BittBoy/PocketGo mode")
  add_definitions(-DBITTBOY)
endif()

if(MIYOO)
  message(STATUS "Miyoo Mini mode")
  add_definitions(-DMIYOO -DMIYOO_AUDIO)
endif()

if(MIYOOA30)
  message(STATUS "Miyoo A30 mode")
  add_definitions(-DMIYOOA30)
endif()

if(RG35XX)
  message(STATUS "RG35XX mode")
  add_definitions(-DRG35XX)
endif()

if(RG35XX22)
  message(STATUS "RG35XX22 mode")
  add_definitions(-DRG35XX -DRG35XX22)
endif()

if(RG35XX22B)
  message(STATUS "RG35XX22B mode")
  add_definitions(-DRG35XX -DRG35XX22B)
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

file(GLOB_RECURSE sources src/*.cc src/*.c)


if (NOT ${CMAKE_SYSTEM_NAME} STREQUAL "Emscripten")
  file(GLOB_RECURSE test_sources test/*.cc)
  file(GLOB_RECURSE tool_sources tool/*.cc)

  set(tests ${sources})
  get_filename_component(dino_jump_main src/dino_jump.cc ABSOLUTE)
  list(REMOVE_ITEM tests ${dino_jump_main})
  list(APPEND tests ${test_sources})

  if(DESKTOP)
    set(tools ${sources})
    list(REMOVE_ITEM tools ${dino_jump_main})
    list(APPEND tools ${tool_sources})

    add_executable(dino_jump_test ${tests})
    set_target_properties(dino_jump_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${tool_test_target_dir})
    target_compile_options(dino_jump_test PRIVATE "-g" "-DTEST")
    target_link_libraries(dino_jump_test ${SDL_LIBRARY})

    add_executable(gentool ${tools})
    set_target_properties(gentool PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${tool_test_target_dir})
    target_compile_options(gentool PRIVATE "-O3" "-DTEST")
    target_link_libraries(gentool ${SDL_LIBRARY})
  endif()
endif()

add_executable(dino_jump ${sources})

if (NOT ${CMAKE_SYSTEM_NAME} STREQUAL "Emscripten")

  set_target_properties(dino_jump PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${main_target_dir})
  target_compile_options(dino_jump PRIVATE
      $<$<CONFIG:Debug>:-g>
      $<$<CONFIG:Release>:-O3 -flto>)
  target_link_libraries(dino_jump ${SDL_LIBRARY} pthread)
  if(MIYOO)
    target_link_options(dino_jump PRIVATE -lmi_ao -lcam_os_wrapper -lmi_sys)
  endif()

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
