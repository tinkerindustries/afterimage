cmake_minimum_required(VERSION 3.20)
project(milk_experimental_effects CXX C)
if(APPLE)
  enable_language(OBJCXX)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

include(FetchContent)

set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_VENDORED ON CACHE BOOL "" FORCE)
set(SDLIMAGE_AVIF OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_JXL OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_TIF OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_WEBP OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_SAMPLES OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_TESTS OFF CACHE BOOL "" FORCE)
set(SDLTTF_VENDORED ON CACHE BOOL "" FORCE)
set(SDLTTF_HARFBUZZ OFF CACHE BOOL "" FORCE)
set(SDLTTF_PLUTOSVG OFF CACHE BOOL "" FORCE)
set(SDLTTF_SAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_Declare(SDL3
  GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  GIT_TAG release-3.4.16
  GIT_SHALLOW TRUE)
FetchContent_Declare(SDL3_image
  GIT_REPOSITORY https://github.com/libsdl-org/SDL_image.git
  GIT_TAG release-3.4.6
  GIT_SHALLOW TRUE)
FetchContent_Declare(SDL3_ttf
  GIT_REPOSITORY https://github.com/libsdl-org/SDL_ttf.git
  GIT_TAG release-3.2.2
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(SDL3 SDL3_image SDL3_ttf)

set(ENABLE_OPT OFF CACHE BOOL "" FORCE)
set(ENABLE_GLSLANG_BINARIES OFF CACHE BOOL "" FORCE)
set(ENABLE_SPVREMAPPER OFF CACHE BOOL "" FORCE)
set(ENABLE_HLSL ON CACHE BOOL "" FORCE)
set(GLSLANG_TESTS OFF CACHE BOOL "" FORCE)
set(GLSLANG_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(BUILD_EXTERNAL OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_CLI OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_HLSL OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_CPP OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_REFLECT OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_C_API OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_UTIL OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_SHARED OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_SKIP_INSTALL ON CACHE BOOL "" FORCE)
FetchContent_Declare(glslang
  GIT_REPOSITORY https://github.com/KhronosGroup/glslang.git
  GIT_TAG 16.6.0
  GIT_SHALLOW TRUE)
FetchContent_Declare(spirv_cross
  GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Cross.git
  GIT_TAG vulkan-sdk-1.4.363.0
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(glslang spirv_cross)

add_executable(milk_effect
  src/main.cpp
  src/audio_synth.cpp
  src/feedback.cpp
  src/preset.cpp)
target_include_directories(milk_effect PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/harness ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(milk_effect PRIVATE SDL3::SDL3 SDL3_image::SDL3_image SDL3_ttf::SDL3_ttf)
if(APPLE)
  target_link_libraries(milk_effect PRIVATE "-framework CoreGraphics" objc)
endif()

add_executable(milk_preset_check
  tools/preset_check.cpp
  src/audio_synth.cpp
  src/feedback.cpp
  src/preset.cpp
  src/milk_expr.cpp
  src/milk_preset.cpp
  src/milk_custom.cpp
  src/milk_audio.cpp)
target_include_directories(milk_preset_check PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_library(milk_shader STATIC src/milk_shader.cpp)
target_include_directories(milk_shader PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
# glslang hides its HLSL options unless the including code defines this too.
target_compile_definitions(milk_shader PRIVATE ENABLE_HLSL)
target_link_libraries(milk_shader PUBLIC glslang SPIRV glslang-default-resource-limits spirv-cross-msl)

if(APPLE)
  add_executable(milk_shader_check
    tools/shader_check.mm
    src/audio_synth.cpp
    src/feedback.cpp
    src/preset.cpp
    src/milk_expr.cpp
    src/milk_preset.cpp
  src/milk_custom.cpp
  src/milk_audio.cpp)
  target_compile_options(milk_shader_check PRIVATE $<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>)
  target_link_libraries(milk_shader_check PRIVATE milk_shader "-framework Foundation" "-framework Metal")
  target_compile_definitions(milk_shader_check PRIVATE MILK_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

  set(MILK_METAL_SHADERS mac/shaders.metal)
  set_source_files_properties(${MILK_METAL_SHADERS} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
  add_library(milk_mac STATIC
    mac/renderer.mm
    mac/system_audio.mm
    mac/band_levels.cpp
    mac/texture_bank.mm
    mac/preset_gpu.mm
    mac/custom_geometry.cpp
    src/audio_synth.cpp
    src/feedback.cpp
    src/preset.cpp
    src/milk_expr.cpp
    src/milk_preset.cpp
    src/milk_custom.cpp
    src/milk_audio.cpp
    src/milk_wave.cpp)
  target_include_directories(milk_mac PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/mac)
  target_compile_options(milk_mac PRIVATE $<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>)
  target_compile_definitions(milk_mac PRIVATE MILK_PRESET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/presets")
  target_link_libraries(milk_mac PUBLIC milk_shader
    "-framework Cocoa" "-framework Metal" "-framework MetalKit" "-framework QuartzCore" "-framework CoreAudio")

  add_executable(milk_render tools/milk_render.mm)
  target_compile_options(milk_render PRIVATE $<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>)
  target_compile_definitions(milk_render PRIVATE MILK_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
  target_link_libraries(milk_render PRIVATE milk_mac "-framework AudioToolbox" "-framework CoreText" "-framework ImageIO" "-framework UniformTypeIdentifiers")

  add_executable(MilkMetal MACOSX_BUNDLE
    mac/main.mm
    ${MILK_METAL_SHADERS})
  set_target_properties(MilkMetal PROPERTIES
    OUTPUT_NAME Milk
    MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/mac/Info.plist.in
    MACOSX_BUNDLE_GUI_IDENTIFIER com.geoffrich.milk
    MACOSX_BUNDLE_EXECUTABLE_NAME Milk)
  target_compile_options(MilkMetal PRIVATE $<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>)
  target_link_libraries(MilkMetal PRIVATE milk_mac)
  # macOS ties the audio capture permission to the app's signature, so sign the whole bundle.
  add_custom_command(TARGET MilkMetal POST_BUILD
    COMMAND codesign --force --sign - $<TARGET_BUNDLE_DIR:MilkMetal>)
endif()
