cmake_minimum_required(VERSION 3.14)
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

include(FetchContent)

# Boost.Multiprecision (cpp_int) in standalone mode, so the rest of Boost isn't needed.
# Standalone mode still requires the header-only Boost.Config.
FetchContent_Declare(boost_config
    GIT_REPOSITORY https://github.com/boostorg/config.git
    GIT_TAG        boost-1.92.0
    GIT_SHALLOW    TRUE
)
FetchContent_Declare(boost_multiprecision
    GIT_REPOSITORY https://github.com/boostorg/multiprecision.git
    GIT_TAG        boost-1.92.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(boost_config boost_multiprecision)
target_link_libraries(boost_multiprecision INTERFACE Boost::config)

# Linked to every target added after this point, so each assignment's executables get it
link_libraries(Boost::multiprecision)

add_subdirectory(vaja1)
