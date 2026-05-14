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

set(SOURCES
    src/main.cpp
    src/memory.cpp
    src/features/esp.cpp
    src/menu/menu.cpp
)

add_executable(EDUCheats WIN32 ${SOURCES})

target_include_directories(EDUCheats PRIVATE src)

target_link_libraries(EDUCheats PRIVATE
    psapi
    gdiplus
    dwmapi
)

# MSVC flags
if(MSVC)
    target_compile_options(EDUCheats PRIVATE /W3 /O2)
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
