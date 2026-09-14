include(ExternalProject)

set(mosaic_zlib_install_dir "${CMAKE_CURRENT_BINARY_DIR}/Zlib")

if(WIN32)
    set(mosaic_zlib_library_name "${CMAKE_STATIC_LIBRARY_PREFIX}zlibstatic${CMAKE_STATIC_LIBRARY_SUFFIX}")
else()
    set(mosaic_zlib_library_name "${CMAKE_STATIC_LIBRARY_PREFIX}z${CMAKE_STATIC_LIBRARY_SUFFIX}")
endif()

set(MOSAIC_ZLIB_LIBS "${mosaic_zlib_install_dir}/lib/${mosaic_zlib_library_name}")
set(MOSAIC_ZLIB_INCLUDE "${mosaic_zlib_install_dir}/include")

set(mosaic_zlib_cmake_args
    -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>
    -DCMAKE_POSITION_INDEPENDENT_CODE:BOOL=${CMAKE_POSITION_INDEPENDENT_CODE}
    -DZLIB_BUILD_EXAMPLES:BOOL=OFF
)

if(CMAKE_BUILD_TYPE)
    list(APPEND mosaic_zlib_cmake_args -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE})
endif()

if(CMAKE_TOOLCHAIN_FILE)
    list(APPEND mosaic_zlib_cmake_args -DCMAKE_TOOLCHAIN_FILE:FILEPATH=${CMAKE_TOOLCHAIN_FILE})
endif()

if(CMAKE_OSX_ARCHITECTURES)
    list(APPEND mosaic_zlib_cmake_args -DCMAKE_OSX_ARCHITECTURES:STRING=${CMAKE_OSX_ARCHITECTURES})
endif()

if(CMAKE_OSX_DEPLOYMENT_TARGET)
    list(APPEND mosaic_zlib_cmake_args -DCMAKE_OSX_DEPLOYMENT_TARGET:STRING=${CMAKE_OSX_DEPLOYMENT_TARGET})
endif()

if(CMAKE_OSX_SYSROOT)
    list(APPEND mosaic_zlib_cmake_args -DCMAKE_OSX_SYSROOT:STRING=${CMAKE_OSX_SYSROOT})
endif()

ExternalProject_Add(MosaicZlibDownload
    PREFIX Zlib
    INSTALL_DIR "${mosaic_zlib_install_dir}"
    GIT_REPOSITORY "${MOSAIC_ZLIB_GIT_REPOSITORY}"
    GIT_TAG "${MOSAIC_ZLIB_GIT_TAG}"
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
    UPDATE_DISCONNECTED FALSE
    CMAKE_ARGS ${mosaic_zlib_cmake_args}
    BUILD_BYPRODUCTS "${MOSAIC_ZLIB_LIBS}"
)

set(MOSAIC_ZLIB_DEPENDENCY MosaicZlibDownload)

file(MAKE_DIRECTORY "${MOSAIC_ZLIB_INCLUDE}")
