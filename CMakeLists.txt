cmake_minimum_required(VERSION 3.20)
project(EDUCheats LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Windows 11 / Win10 SDK target
add_compile_definitions(
    WINVER=0x0A00
    _WIN32_WINNT=0x0A00
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

# -----------------------------------------------------------------------------
# Dear ImGui (vendored via FetchContent — pinned tag, no submodule).
# -----------------------------------------------------------------------------
include(FetchContent)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.5
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(imgui)

set(IMGUI_SOURCES
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp
)

# -----------------------------------------------------------------------------
# EDUCheats target
# -----------------------------------------------------------------------------
set(SOURCES
    src/main.cpp
    src/memory.cpp
    src/config.cpp
    src/gui/overlay.cpp
    src/gui/esp_render.cpp
    src/gui/menu_ui.cpp
)

add_executable(EDUCheats WIN32 ${SOURCES} ${IMGUI_SOURCES})

target_include_directories(EDUCheats PRIVATE
    src
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

target_link_libraries(EDUCheats PRIVATE
    psapi
    dwmapi
    d3d11
    dxgi
    d3dcompiler
)

# MSVC flags
if(MSVC)
    target_compile_options(EDUCheats PRIVATE /W3 /O2 /MP)
    # Subsystem CONSOLE so AllocConsole + freopen works without a separate console app
    set_target_properties(EDUCheats PROPERTIES
        LINK_FLAGS "/SUBSYSTEM:CONSOLE"
    )
endif()

# MinGW flags
if(MINGW)
    target_compile_options(EDUCheats PRIVATE -O2 -Wall)
    target_link_options(EDUCheats PRIVATE -mwindows -mconsole)
endif()
