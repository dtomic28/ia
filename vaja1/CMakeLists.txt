cmake_minimum_required(VERSION 3.10)

set(ASSIGNMENT_NAME "vaja1")

project(${ASSIGNMENT_NAME})

include_directories(${CMAKE_SOURCE_DIR}/${ASSIGNMENT_NAME}/include)

if(MINGW_RUNTIME_DLLS)
    file(COPY ${MINGW_RUNTIME_DLLS} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
endif()

add_executable(${ASSIGNMENT_NAME}_cli src/cli_main.cpp)
add_executable(${ASSIGNMENT_NAME}_lcg_test src/lcg_test_main.cpp)
add_executable(${ASSIGNMENT_NAME}_prime_bench src/prime_bench_main.cpp)
add_executable(${ASSIGNMENT_NAME}_keygen_bench src/keygen_bench_main.cpp)

include(FetchContent)

set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)
set(NFD_BUILD_TESTS     OFF CACHE BOOL "" FORCE)
set(NFD_INSTALL         OFF CACHE BOOL "" FORCE)

FetchContent_Declare(glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
    GIT_SHALLOW    TRUE
)
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.92.9b
    GIT_SHALLOW    TRUE
)
FetchContent_Declare(implot
    GIT_REPOSITORY https://github.com/epezent/implot.git
    GIT_TAG        v1.0
    GIT_SHALLOW    TRUE
)
# Native "Open file" / "Save file" dialogs, ImGui has none of its own
FetchContent_Declare(nfd
    GIT_REPOSITORY https://github.com/btzy/nativefiledialog-extended.git
    GIT_TAG        v1.4.1
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(glfw imgui implot nfd)

find_package(OpenGL REQUIRED)

# ImGui and ImPlot ship without CMake support, so build them as a static library here
add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    ${implot_SOURCE_DIR}/implot.cpp
    ${implot_SOURCE_DIR}/implot_items.cpp
)
target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${implot_SOURCE_DIR}
)
target_link_libraries(imgui PUBLIC glfw OpenGL::GL)

add_executable(${ASSIGNMENT_NAME}_gui src/gui_main.cpp)
target_link_libraries(${ASSIGNMENT_NAME}_gui PRIVATE imgui nfd)

set_target_properties(
    ${ASSIGNMENT_NAME}_cli
    ${ASSIGNMENT_NAME}_lcg_test
    ${ASSIGNMENT_NAME}_prime_bench
    ${ASSIGNMENT_NAME}_keygen_bench
    ${ASSIGNMENT_NAME}_gui
    imgui
    nfd
    PROPERTIES FOLDER ${ASSIGNMENT_NAME}
)
