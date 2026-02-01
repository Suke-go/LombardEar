cmake_minimum_required(VERSION 3.20)

project(LombardEar C)

# ---- Options ----
option(LE_BUILD_APP   "Build realtime app (PortAudio required)" ON)
option(LE_BUILD_TESTS "Build tests" ON)

# まだ不要なら OFF のまま（Phase 4以降でONに）
option(LE_WITH_WEBSOCKETS "Enable WebSocket GUI (libwebsockets required)" ON)
option(LE_WITH_SERIAL     "Enable Serial knob (OS-specific)" OFF)

# ---- Language / Compile flags ----
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(MSVC)
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
  add_compile_options(/W4 /utf-8 /arch:AVX2)
elseif(APPLE)
  add_compile_options(-Wall -Wextra -Wpedantic)
  # macOS: Apple Silicon uses NEON (auto-enabled), Intel uses AVX
  include(CheckCCompilerFlag)
  check_c_compiler_flag("-mavx2" COMPILER_SUPPORTS_AVX2)
  if(COMPILER_SUPPORTS_AVX2)
    add_compile_options(-mavx2 -mfma)
  endif()
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
  # Linux: enable AVX2 if supported
  include(CheckCCompilerFlag)
  check_c_compiler_flag("-mavx2" COMPILER_SUPPORTS_AVX2)
  if(COMPILER_SUPPORTS_AVX2)
    add_compile_options(-mavx2 -mfma)
  endif()
endif()

# ---- Dependencies: cJSON via FetchContent ----
include(FetchContent)

FetchContent_Declare(
  cjson
  GIT_REPOSITORY https://github.com/DaveGamble/cJSON.git
  GIT_TAG        v1.7.18
)
option(ENABLE_CJSON_TEST "Enable cJSON tests" OFF)
FetchContent_MakeAvailable(cjson)

# cJSON target name is "cjson" (library) in upstream CMake.
# We will link against cjson.

# ---- PortAudio (required for app) ----
# Strategy: try to find PortAudio headers+library from system / vcpkg / manual install.

# Try vcpkg/Config mode first
find_package(portaudio CONFIG QUIET)

if(portaudio_FOUND)
  if(NOT TARGET portaudio::portaudio)
    add_library(portaudio::portaudio ALIAS portaudio)
  endif()
  message(STATUS "Found PortAudio via Config mode")
else()
  # Manual find fallback
  set(PORTAUDIO_INCLUDE_DIR "")
  set(PORTAUDIO_LIBRARY "")

  find_path(PORTAUDIO_INCLUDE_DIR portaudio.h)
  find_library(PORTAUDIO_LIBRARY NAMES portaudio portaudio_static)

  if(LE_BUILD_APP)
    if(NOT PORTAUDIO_INCLUDE_DIR OR NOT PORTAUDIO_LIBRARY)
      message(FATAL_ERROR
        "PortAudio not found.\n"
        "Install PortAudio development package and reconfigure.\n"
        "macOS: brew install portaudio\n"
        "Linux(Ubuntu): sudo apt install portaudio19-dev\n"
        "Windows: install PortAudio (e.g., vcpkg portaudio) or set CMAKE_PREFIX_PATH / LIB/INCLUDE paths.\n"
        "Detected:\n"
        "  PORTAUDIO_INCLUDE_DIR='${PORTAUDIO_INCLUDE_DIR}'\n"
        "  PORTAUDIO_LIBRARY='${PORTAUDIO_LIBRARY}'\n"
      )
    endif()
  endif()

  # Create imported target for PortAudio if available
  if(PORTAUDIO_INCLUDE_DIR AND PORTAUDIO_LIBRARY)
    add_library(portaudio::portaudio UNKNOWN IMPORTED)
    set_target_properties(portaudio::portaudio PROPERTIES
      IMPORTED_LOCATION "${PORTAUDIO_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${PORTAUDIO_INCLUDE_DIR}"
    )
  endif()
endif()

# ---- Core sources (Phase 1-3) ----
set(LE_INC_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/src/audio
  ${CMAKE_CURRENT_SOURCE_DIR}/src/dsp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/utils
)

# DSP library (GSC core)
add_library(le_dsp STATIC
  src/dsp/gsc.c
  src/dsp/aec.c
  src/dsp/agc.c
  src/dsp/noise_gate.c
  src/dsp/biquad.c
  src/dsp/multiband.c
  src/dsp/steer.c
  src/dsp/doa.c
  src/dsp/fast_math.c
  src/dsp/steer_fast.c
  src/dsp/phase_align.c
)
target_include_directories(le_dsp PUBLIC ${LE_INC_DIRS})

# Utils library (config using cJSON)
add_library(le_utils STATIC
  src/utils/config.c
)
target_include_directories(le_utils PUBLIC ${LE_INC_DIRS})
target_link_libraries(le_utils PUBLIC cjson)
target_include_directories(le_utils PRIVATE ${cjson_SOURCE_DIR})

# ---- App (Phase 1 Bypass main + audio I/O) ----
if(LE_BUILD_APP)
  # Platform abstraction layer (OS-specific sources)
  if(WIN32)
    set(PLATFORM_SRC src/platform/platform_win32.c)
  else()
    set(PLATFORM_SRC src/platform/platform_unix.c)
  endif()

  add_executable(lombardear
    src/main.c
    src/audio/audio_io.c
    src/audio/jitter_buffer.c
    ${PLATFORM_SRC}
  )
  target_include_directories(lombardear PRIVATE ${LE_INC_DIRS})
  target_link_libraries(lombardear PRIVATE le_dsp le_utils portaudio::portaudio)

  # math library on unix (including Apple for libm)
  if(UNIX)
    target_link_libraries(lombardear PRIVATE m)
  endif()

  # Feature flags (optional modules)
  if(LE_WITH_WEBSOCKETS)
    target_compile_definitions(lombardear PRIVATE LE_WITH_WEBSOCKETS=1)
  endif()

  if(LE_WITH_SERIAL)
    target_compile_definitions(lombardear PRIVATE LE_WITH_SERIAL=1)
  endif()
endif()

# ---- Tests ----
if(LE_BUILD_TESTS)
  enable_testing()

  add_executable(test_gsc_offline
    tests/test_gsc_offline.c
    src/dsp/gsc.c
  )
  target_include_directories(test_gsc_offline PRIVATE ${LE_INC_DIRS})
  target_link_libraries(test_gsc_offline PRIVATE cjson)

  if(UNIX)
    target_link_libraries(test_gsc_offline PRIVATE m)
  endif()


  add_test(NAME test_gsc_offline COMMAND test_gsc_offline)

  # Spatial-Spectral processor test
  add_executable(test_spatial_spectral
    tests/test_spatial_spectral.c
    src/dsp/biquad.c
    src/dsp/multiband.c
    src/dsp/steer.c
  )
  target_include_directories(test_spatial_spectral PRIVATE ${LE_INC_DIRS})

  if(UNIX)
    target_link_libraries(test_spatial_spectral PRIVATE m)
  endif()

  add_test(NAME test_spatial_spectral COMMAND test_spatial_spectral)

  # DSP Benchmark
  add_executable(benchmark_dsp
    tests/benchmark_dsp.c
  )
  target_include_directories(benchmark_dsp PRIVATE ${LE_INC_DIRS})
  target_link_libraries(benchmark_dsp PRIVATE le_dsp)
  
  if(UNIX)
    target_link_libraries(benchmark_dsp PRIVATE m)
  endif()

  # Jitter Buffer Test
  add_executable(test_jitter_buffer
    tests/test_jitter_buffer.c
    src/audio/jitter_buffer.c
  )
  target_include_directories(test_jitter_buffer PRIVATE ${LE_INC_DIRS})
  if(UNIX)
    target_link_libraries(test_jitter_buffer PRIVATE m)
  endif()
  add_test(NAME test_jitter_buffer COMMAND test_jitter_buffer)

  # Phase Alignment Test
  add_executable(test_phase_align
    tests/test_phase_align.c
    src/dsp/phase_align.c
    src/dsp/fast_math.c
  )
  target_include_directories(test_phase_align PRIVATE ${LE_INC_DIRS})
  if(UNIX)
    target_link_libraries(test_phase_align PRIVATE m)
  endif()
  add_test(NAME test_phase_align COMMAND test_phase_align)
endif()

# ---- Web Server ----
if(LE_WITH_WEBSOCKETS)
  message(STATUS "WebSockets ENABLED: Linking Mongoose")
  
  # Mongoose Source
  set(MONGOOSE_SRC src/third_party/mongoose/mongoose.c)
  
  # Add Mongoose definitions (DMG_ENABLE_LINES for debug)
  add_definitions(-DMG_ENABLE_LINES=1)

  # Update App Sources
  target_sources(lombardear PRIVATE
    src/server/web_server.c
    ${MONGOOSE_SRC}
  )
  target_include_directories(lombardear PRIVATE src/third_party/mongoose)

  # Link Winsock on Windows
  if(WIN32)
    target_link_libraries(lombardear PRIVATE ws2_32)
  endif()
endif()

# ---- Post-build: Copy DLLs ----
if(WIN32)
  add_custom_command(TARGET lombardear POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:cjson>
    $<TARGET_FILE_DIR:lombardear>
  )
  if(LE_BUILD_TESTS)
    add_custom_command(TARGET test_gsc_offline POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
      $<TARGET_FILE:cjson>
      $<TARGET_FILE_DIR:test_gsc_offline>
    )
  endif()
endif()
