include(ExternalProject)

set(mosaic_graphics_install_dir "${CMAKE_CURRENT_BINARY_DIR}/Graphics")
set(mosaic_graphics_library "${mosaic_graphics_install_dir}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}graphics${CMAKE_STATIC_LIBRARY_SUFFIX}")
set(mosaic_graphics_cmake_args
    -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>
    -DCMAKE_C_STANDARD:STRING=11
    -DCMAKE_C_STANDARD_REQUIRED:BOOL=ON
    -DCMAKE_POSITION_INDEPENDENT_CODE:BOOL=${CMAKE_POSITION_INDEPENDENT_CODE}
    -DGRAPHICS_EXTERNAL_BUILD:BOOL=ON
    -DGRAPHICS_EXAMPLES_BUILD:BOOL=OFF
    -DGRAPHICS_INSTALL:BOOL=ON
    -DGRAPHICS_TESTS:BOOL=OFF
    -DGRAPHICS_TESTS_IN_SOLUTIONS:BOOL=OFF
)

if(CMAKE_BUILD_TYPE)
    list(APPEND mosaic_graphics_cmake_args -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE})
endif()

if(CMAKE_TOOLCHAIN_FILE)
    list(APPEND mosaic_graphics_cmake_args -DCMAKE_TOOLCHAIN_FILE:FILEPATH=${CMAKE_TOOLCHAIN_FILE})
endif()

if(CMAKE_OSX_ARCHITECTURES)
    list(APPEND mosaic_graphics_cmake_args -DCMAKE_OSX_ARCHITECTURES:STRING=${CMAKE_OSX_ARCHITECTURES})
endif()

if(CMAKE_OSX_DEPLOYMENT_TARGET)
    list(APPEND mosaic_graphics_cmake_args -DCMAKE_OSX_DEPLOYMENT_TARGET:STRING=${CMAKE_OSX_DEPLOYMENT_TARGET})
endif()

if(CMAKE_OSX_SYSROOT)
    list(APPEND mosaic_graphics_cmake_args -DCMAKE_OSX_SYSROOT:STRING=${CMAKE_OSX_SYSROOT})
endif()

ExternalProject_Add(MosaicGraphicsDownload
    PREFIX Graphics
    INSTALL_DIR "${mosaic_graphics_install_dir}"
    GIT_REPOSITORY "${MOSAIC_GRAPHICS_GIT_REPOSITORY}"
    GIT_TAG master
    GIT_SHALLOW FALSE
    GIT_PROGRESS TRUE
    UPDATE_DISCONNECTED FALSE
    CMAKE_ARGS ${mosaic_graphics_cmake_args}
    BUILD_BYPRODUCTS "${mosaic_graphics_library}"
)

set(MOSAIC_GRAPHICS_INCLUDE "${mosaic_graphics_install_dir}/include")
set(MOSAIC_GRAPHICS_LIBS "${mosaic_graphics_library}")
set(MOSAIC_GRAPHICS_DEPENDENCY MosaicGraphicsDownload)
