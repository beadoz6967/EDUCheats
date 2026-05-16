cmake_minimum_required(VERSION 3.20)
project(EDUCheats LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_definitions(
    WINVER=0x0A00
    _WIN32_WINNT=0x0A00
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

# -----------------------------------------------------------------------------
# Dear ImGui (vendored via FetchContent)
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
# MinHook (inline-hook library for Present vtable hook)
# -----------------------------------------------------------------------------
FetchContent_Declare(
    minhook
    GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(minhook)

# -----------------------------------------------------------------------------
# EDUCheats — internal DLL
# -----------------------------------------------------------------------------
set(SOURCES
    src/dllmain.cpp
    src/config.cpp
    src/gui/overlay.cpp
    src/gui/esp_render.cpp
    src/gui/menu_ui.cpp
)

add_library(EDUCheats SHARED ${SOURCES} ${IMGUI_SOURCES})

target_include_directories(EDUCheats PRIVATE
    src
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${minhook_SOURCE_DIR}/include
)

target_link_libraries(EDUCheats PRIVATE
    minhook
    dwmapi
    d3d11
    dxgi
    d3dcompiler
)

# Fetch fresh CS2 offsets before every compile
add_custom_command(TARGET EDUCheats PRE_BUILD
    COMMAND powershell -ExecutionPolicy Bypass -File
            "${CMAKE_SOURCE_DIR}/fetch_offsets.ps1"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Fetching latest CS2 offsets from a2x/cs2-dumper")

if(MSVC)
    target_compile_options(EDUCheats PRIVATE /W3 /O2 /MP)
    set_target_properties(EDUCheats PROPERTIES
        LINK_FLAGS "/SUBSYSTEM:WINDOWS")
endif()

if(MINGW)
    target_compile_options(EDUCheats PRIVATE -O2 -Wall)
    target_link_options(EDUCheats PRIVATE -mwindows)
endif()
