project(usdai)

# >=3.1 is required for CMAKE_CXX_STANDARD
cmake_minimum_required(VERSION 3.1)

if (UNIX AND NOT APPLE)
    set(LINUX TRUE)
endif ()

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/defaults
                      ${CMAKE_SOURCE_DIR}/cmake/modules
                      ${CMAKE_SOURCE_DIR}/cmake/macros)

include(Options)
include(ProjectDefaults)
include(Packages)

# This has to be defined after Packages is included, because it relies on the
# discovered path to the python executable.
set(PXR_PYTHON_SHEBANG "${PYTHON_EXECUTABLE}" 
    CACHE 
    STRING
    "Replacement path for Python #! line."
)

# CXXDefaults will set a variety of variables for the project.
# Consume them here. This is an effort to keep the most common
# build files readable.
include(CXXDefaults)
add_definitions(${_PXR_CXX_DEFINITIONS})
set(CMAKE_CXX_FLAGS "${_PXR_CXX_FLAGS} ${CMAKE_CXX_FLAGS}")

include(Public)

# add_compile_options(-Wall -DBUILD_OPTLEVEL_OPT -DBUILD_COMPONENT_SRC_PREFIX="")
if (NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 11)
endif()
set(CMAKE_CXX_EXTENSIONS OFF)


include_directories(SYSTEM ${USD_INCLUDE_DIR})
link_directories(${USD_LIBRARY_DIR})
include_directories(SYSTEM ${ARNOLD_INCLUDE_DIR})

include_directories(SYSTEM ${TBB_INCLUDE_DIRS})
include_directories(SYSTEM ${PYTHON_INCLUDE_DIRS})


if (BUILD_USD_PLUGIN)
    if (PXR_BUILD_TESTS)
        find_package(GTest REQUIRED)
    endif ()
    add_subdirectory(lib/pxr/usd/usdAi)
    add_subdirectory(utils)
endif ()

if (BUILD_USD_IMAGING_PLUGIN)
    add_subdirectory(plugin/pxr/imaging/hdAi)
    add_subdirectory(plugin/pxr/usd/ndrAi)
    add_subdirectory(shader)
endif ()

if (BUILD_USD_MAYA_PLUGIN)
    add_subdirectory(maya)
endif ()

if (BUILD_USD_KATANA_PLUGIN)
    add_subdirectory(katana)
endif ()

if (BUILD_USD_HOUDINI_PLUGIN)
    add_subdirectory(houdini)
endif ()

install(FILES README.md DESTINATION .)
install(FILES LICENSE.md DESTINATION .)
