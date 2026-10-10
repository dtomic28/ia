cmake_minimum_required(VERSION 3.10)
set(CMAKE_CXX_STANDARD 20)

set(GLOBAL_PROJECT_NAME "IzbraniAlgoritmi")

project(${GLOBAL_PROJECT_NAME})

# Link the MinGW runtime statically, otherwise a different libstdc++-6.dll on PATH can be loaded at runtime
if(MINGW)
    add_link_options(-static-libgcc -static-libstdc++)
endif()

add_subdirectory(vaja01)
