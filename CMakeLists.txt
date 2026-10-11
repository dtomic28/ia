cmake_minimum_required(VERSION 3.10)
set(CMAKE_CXX_STANDARD 20)

set(GLOBAL_PROJECT_NAME "IzbraniAlgoritmi")

project(${GLOBAL_PROJECT_NAME})

# Lets each assignment group its targets with the FOLDER property
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Link the MinGW runtime statically, otherwise a different libstdc++-6.dll on PATH can be loaded at runtime.
# libwinpthread can't be linked statically with this toolchain, so its DLL is copied next to the executables instead.
if(MINGW)
    add_link_options(-static-libgcc -static-libstdc++)
    get_filename_component(MINGW_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
    set(MINGW_RUNTIME_DLLS "${MINGW_BIN_DIR}/libwinpthread-1.dll")
endif()

add_subdirectory(vaja1_1)
