cmake_minimum_required(VERSION 3.20)
project(imthorgui CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# --- SDL3 ---
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)

FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.2.8
)
FetchContent_MakeAvailable(SDL3)

# --- ThorVG ---
add_subdirectory(third_party/thorvg)

# --- ImGui (submodule) ---
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui)

set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdlrenderer3.cpp
)

add_library(imgui STATIC ${IMGUI_SOURCES})
target_compile_definitions(imgui PUBLIC "IMGUI_USER_CONFIG=\"imgui_ext/my_imgui_config.h\"")
target_include_directories(imgui PUBLIC
    src
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)
target_link_libraries(imgui PUBLIC SDL3::SDL3-static)
if (WIN32)
    target_link_libraries(imgui PUBLIC psapi)
endif()

# --- imthorgui Library (Recorder + Renderer) ---
add_library(imthorgui STATIC
    src/imgui_ext/recorder.cpp
    src/imgui_ext/renderer_thorvg.cpp
)
target_include_directories(imthorgui PUBLIC src)
target_link_libraries(imthorgui PUBLIC imgui thorvg)

# --- Demo Executable ---
add_executable(demo
    examples/demo/main.cpp
)
target_link_libraries(demo PRIVATE imthorgui)

# --- Tests ---
add_executable(test_text_metrics
    tests/test_text_metrics.cpp
)
target_link_libraries(test_text_metrics PRIVATE imthorgui)

add_executable(visual_diff
    tests/visual_diff.cpp
)
target_link_libraries(visual_diff PRIVATE imthorgui)

add_executable(test_frame_dedup
    tests/test_frame_dedup.cpp
)
target_link_libraries(test_frame_dedup PRIVATE imthorgui)
