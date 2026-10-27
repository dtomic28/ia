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

# Same programs on the GMP backend, built only when GMP is installed
find_path(GMP_INCLUDE_DIR gmp.h HINTS /opt/homebrew/include /usr/local/include)
find_library(GMP_LIBRARY gmp HINTS /opt/homebrew/lib /usr/local/lib)
if(GMP_INCLUDE_DIR AND GMP_LIBRARY)
    set(GMP_FOUND TRUE)
else()
    message(STATUS "GMP not found, skipping the GMP programs")
endif()

# Adds <name>_gmp built from the same source as <name>, with cpp_int switched to GMP
function(add_gmp_executable name source)
    add_executable(${name}_gmp ${source})
    target_compile_definitions(${name}_gmp PRIVATE USE_GMP)
    target_include_directories(${name}_gmp PRIVATE ${GMP_INCLUDE_DIR})
    target_link_libraries(${name}_gmp PRIVATE ${GMP_LIBRARY})
    set_target_properties(${name}_gmp PROPERTIES FOLDER ${ASSIGNMENT_NAME})
endfunction()

if(GMP_FOUND)
    foreach(bench prime_bench keygen_bench)
        add_gmp_executable(${ASSIGNMENT_NAME}_${bench} src/${bench}_main.cpp)
    endforeach()
endif()

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

if(GMP_FOUND)
    add_gmp_executable(${ASSIGNMENT_NAME}_gui src/gui_main.cpp)
    target_link_libraries(${ASSIGNMENT_NAME}_gui_gmp PRIVATE imgui nfd)
endif()

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
