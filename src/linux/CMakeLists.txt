cmake_minimum_required(VERSION 3.16)
project(LocalShare LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find packages
find_package(glfw3 3.3 REQUIRED)
find_package(OpenGL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLEW REQUIRED glew)

# ImGui directory
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/imgui)

# ImGui sources + backends
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

# Project sources
set(SOURCES
    fileshare_gui.cpp
    ImGuiFileDialog.cpp
    ${IMGUI_SOURCES}
)

# Asio include path
set(ASIO_DIR ${CMAKE_SOURCE_DIR}/asioinclude)

# Create executable FIRST
add_executable(LocalShare ${SOURCES})

# NOW set include directories (target exists)
target_include_directories(LocalShare PRIVATE
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${ASIO_DIR}
    src/
    ${GLEW_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(LocalShare 
    glfw 
    OpenGL::GL 
    ${GLEW_LIBRARIES}
    pthread
)

# Windows
if(WIN32)
    target_compile_definitions(LocalShare PRIVATE _WIN32_WINNT=0x0601)
    target_link_libraries(LocalShare ws2_32)
endif()
