cmake_minimum_required(VERSION 3.24)

project(Mosaic VERSION 0.1.0 LANGUAGES C CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(MOSAIC_BUILD_EXAMPLES "Build Mosaic examples" ${PROJECT_IS_TOP_LEVEL})
option(MOSAIC_BUILD_TESTS "Build Mosaic regression tests" ${PROJECT_IS_TOP_LEVEL})
option(MOSAIC_BUILD_BACKENDS "Build the native Mosaic backends" ${PROJECT_IS_TOP_LEVEL})
option(MOSAIC_INSTALL "Generate the Mosaic install and export rules" ${PROJECT_IS_TOP_LEVEL})
option(MOSAIC_ENABLE_EXCEPTIONS "Compile Mosaic C++ sources with exception support" ON)
option(MOSAIC_ENABLE_RTTI "Compile Mosaic C++ sources with RTTI support" ON)
option(MOSAIC_WARNINGS_AS_ERRORS "Treat Mosaic compiler warnings as errors" ${PROJECT_IS_TOP_LEVEL})
set(MOSAIC_GRAPHICS_INCLUDE "" CACHE STRING "Graphics include directories supplied by the host")
set(MOSAIC_GRAPHICS_LIBS "" CACHE STRING "Graphics libraries supplied by the host")
set(MOSAIC_GRAPHICS_DEPENDENCY "" CACHE STRING "Optional target which prepares the external Graphics library")
set(MOSAIC_GRAPHICS_TARGET "" CACHE STRING "Existing Graphics CMake target supplied by the host")
set(MOSAIC_GRAPHICS_SOURCE_DIR "" CACHE PATH "Graphics source checkout added directly to this build")
set(MOSAIC_GRAPHICS_GIT_REPOSITORY "https://github.com/irov/graphics.git" CACHE STRING
    "Git repository used to download Graphics")
set(MOSAIC_ZLIB_INCLUDE "" CACHE STRING "Zlib include directories supplied by the host")
set(MOSAIC_ZLIB_LIBS "" CACHE STRING "Zlib libraries supplied by the host")
set(MOSAIC_ZLIB_DEPENDENCY "" CACHE STRING "Optional target which prepares the external Zlib library")
set(MOSAIC_ZLIB_TARGET "" CACHE STRING "Existing Zlib CMake target supplied by the host")
set(MOSAIC_ZLIB_SOURCE_DIR "" CACHE PATH "Zlib source checkout added directly to this build")
set(MOSAIC_ZLIB_GIT_REPOSITORY "https://github.com/madler/zlib.git" CACHE STRING
    "Git repository used to download Zlib")
set(MOSAIC_ZLIB_GIT_TAG "v1.3.1" CACHE STRING "Git tag used to download Zlib")

function(mosaic_target_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE
            $<$<COMPILE_LANGUAGE:C,CXX>:/W4>
            $<$<COMPILE_LANGUAGE:CXX>:/permissive->
        )
        if(MOSAIC_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE
                $<$<COMPILE_LANGUAGE:C,CXX>:/WX>
            )
        endif()
    else()
        target_compile_options(${target} PRIVATE
            $<$<COMPILE_LANGUAGE:C,CXX,OBJCXX>:-Wall>
            $<$<COMPILE_LANGUAGE:C,CXX,OBJCXX>:-Wextra>
            $<$<COMPILE_LANGUAGE:C,CXX,OBJCXX>:-Wpedantic>
        )
        if(MOSAIC_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE
                $<$<COMPILE_LANGUAGE:C,CXX,OBJCXX>:-Werror>
            )
        endif()
    endif()
endfunction()

set(mosaic_graphics_mode_count 0)
if(MOSAIC_GRAPHICS_TARGET)
    math(EXPR mosaic_graphics_mode_count "${mosaic_graphics_mode_count} + 1")
endif()
if(MOSAIC_GRAPHICS_SOURCE_DIR)
    math(EXPR mosaic_graphics_mode_count "${mosaic_graphics_mode_count} + 1")
endif()
if(MOSAIC_GRAPHICS_INCLUDE OR MOSAIC_GRAPHICS_LIBS)
    math(EXPR mosaic_graphics_mode_count "${mosaic_graphics_mode_count} + 1")
endif()
if(mosaic_graphics_mode_count GREATER 1)
    message(FATAL_ERROR "Choose only one Graphics integration mode: TARGET, SOURCE_DIR, or INCLUDE/LIBS")
endif()

if(MOSAIC_GRAPHICS_TARGET)
    if(NOT TARGET ${MOSAIC_GRAPHICS_TARGET})
        message(FATAL_ERROR "MOSAIC_GRAPHICS_TARGET '${MOSAIC_GRAPHICS_TARGET}' does not exist")
    endif()
    set(mosaic_graphics_library ${MOSAIC_GRAPHICS_TARGET})
elseif(MOSAIC_GRAPHICS_SOURCE_DIR)
    if(NOT EXISTS "${MOSAIC_GRAPHICS_SOURCE_DIR}/CMakeLists.txt")
        message(FATAL_ERROR "MOSAIC_GRAPHICS_SOURCE_DIR does not contain CMakeLists.txt")
    endif()
    set(GRAPHICS_EXTERNAL_BUILD ON CACHE BOOL "" FORCE)
    set(GRAPHICS_EXAMPLES_BUILD OFF CACHE BOOL "" FORCE)
    set(GRAPHICS_INSTALL OFF CACHE BOOL "" FORCE)
    set(GRAPHICS_TESTS OFF CACHE BOOL "" FORCE)
    set(GRAPHICS_TESTS_IN_SOLUTIONS OFF CACHE BOOL "" FORCE)
    add_subdirectory("${MOSAIC_GRAPHICS_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/GraphicsSource")
    set(mosaic_graphics_library graphics)
elseif(MOSAIC_GRAPHICS_INCLUDE OR MOSAIC_GRAPHICS_LIBS)
    if(NOT MOSAIC_GRAPHICS_INCLUDE OR NOT MOSAIC_GRAPHICS_LIBS)
        message(FATAL_ERROR "MOSAIC_GRAPHICS_INCLUDE and MOSAIC_GRAPHICS_LIBS must be provided together")
    endif()
    list(LENGTH MOSAIC_GRAPHICS_LIBS mosaic_graphics_library_count)
    if(NOT mosaic_graphics_library_count EQUAL 1)
        message(FATAL_ERROR "MOSAIC_GRAPHICS_LIBS must identify exactly one Graphics library")
    endif()
    list(GET MOSAIC_GRAPHICS_LIBS 0 mosaic_graphics_library)
else()
    include(cmake/GraphicsDownload.cmake)
    set(mosaic_graphics_library ${MOSAIC_GRAPHICS_LIBS})
endif()

if(MOSAIC_GRAPHICS_DEPENDENCY AND NOT TARGET ${MOSAIC_GRAPHICS_DEPENDENCY})
    message(FATAL_ERROR "MOSAIC_GRAPHICS_DEPENDENCY '${MOSAIC_GRAPHICS_DEPENDENCY}' does not exist")
endif()

if(TARGET ${mosaic_graphics_library})
    set(mosaic_graphics_install_source "$<TARGET_FILE:${mosaic_graphics_library}>")
else()
    set(mosaic_graphics_install_source "${mosaic_graphics_library}")
endif()

set(MOSAIC_GRAPHICS_PACKAGE_LIBRARY "${CMAKE_STATIC_LIBRARY_PREFIX}MosaicGraphicsDependency${CMAKE_STATIC_LIBRARY_SUFFIX}")

set(mosaic_zlib_mode_count 0)
if(MOSAIC_ZLIB_TARGET)
    math(EXPR mosaic_zlib_mode_count "${mosaic_zlib_mode_count} + 1")
endif()
if(MOSAIC_ZLIB_SOURCE_DIR)
    math(EXPR mosaic_zlib_mode_count "${mosaic_zlib_mode_count} + 1")
endif()
if(MOSAIC_ZLIB_INCLUDE OR MOSAIC_ZLIB_LIBS)
    math(EXPR mosaic_zlib_mode_count "${mosaic_zlib_mode_count} + 1")
endif()
if(mosaic_zlib_mode_count GREATER 1)
    message(FATAL_ERROR "Choose only one Zlib integration mode: TARGET, SOURCE_DIR, or INCLUDE/LIBS")
endif()

if(MOSAIC_ZLIB_TARGET)
    if(NOT TARGET ${MOSAIC_ZLIB_TARGET})
        message(FATAL_ERROR "MOSAIC_ZLIB_TARGET '${MOSAIC_ZLIB_TARGET}' does not exist")
    endif()
    set(mosaic_zlib_library ${MOSAIC_ZLIB_TARGET})
elseif(MOSAIC_ZLIB_SOURCE_DIR)
    if(NOT EXISTS "${MOSAIC_ZLIB_SOURCE_DIR}/CMakeLists.txt")
        message(FATAL_ERROR "MOSAIC_ZLIB_SOURCE_DIR does not contain CMakeLists.txt")
    endif()
    set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    add_subdirectory("${MOSAIC_ZLIB_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/ZlibSource")
    set(mosaic_zlib_library zlibstatic)
elseif(MOSAIC_ZLIB_INCLUDE OR MOSAIC_ZLIB_LIBS)
    if(NOT MOSAIC_ZLIB_INCLUDE OR NOT MOSAIC_ZLIB_LIBS)
        message(FATAL_ERROR "MOSAIC_ZLIB_INCLUDE and MOSAIC_ZLIB_LIBS must be provided together")
    endif()
    list(LENGTH MOSAIC_ZLIB_LIBS mosaic_zlib_library_count)
    if(NOT mosaic_zlib_library_count EQUAL 1)
        message(FATAL_ERROR "MOSAIC_ZLIB_LIBS must identify exactly one Zlib library")
    endif()
    list(GET MOSAIC_ZLIB_LIBS 0 mosaic_zlib_library)
else()
    include(cmake/ZlibDownload.cmake)
    set(mosaic_zlib_library ${MOSAIC_ZLIB_LIBS})
endif()

if(MOSAIC_ZLIB_DEPENDENCY AND NOT TARGET ${MOSAIC_ZLIB_DEPENDENCY})
    message(FATAL_ERROR "MOSAIC_ZLIB_DEPENDENCY '${MOSAIC_ZLIB_DEPENDENCY}' does not exist")
endif()

include(cmake/DefaultFont.cmake)
mosaic_pack_default_font("${CMAKE_CURRENT_BINARY_DIR}/DefaultFont/MosaicDefaultFontData.cpp"
    mosaic_default_font_source)


add_library(MosaicCore
    src/Allocator.cpp
    src/Canvas.cpp
    src/Columns.cpp
    src/Context.cpp
    src/DockSpace.cpp
    src/Docking.cpp
    src/DragDrop.cpp
    src/DrawList.cpp
    src/Editor.cpp
    src/FrameArena.cpp
    src/Interaction.cpp
    src/Layout.cpp
    src/Menu.cpp
    src/NativeSurface.cpp
    src/Numeric.cpp
    src/Platform.cpp
    src/Popup.cpp
    src/Render.cpp
    src/Scope.cpp
    src/SelectionModel.cpp
    src/ShortcutRegistry.cpp
    src/Table.cpp
    src/TextEdit.cpp
    src/TextFilter.cpp
    src/Theme.cpp
    src/Window.cpp
    src/Widgets.cpp
    src/DefaultFont.cpp
    ${mosaic_default_font_source}
)
add_library(Mosaic::Core ALIAS MosaicCore)

target_compile_features(MosaicCore PUBLIC cxx_std_20)
target_compile_definitions(MosaicCore PUBLIC MOSAIC_VERSION_STRING="${PROJECT_VERSION}")
target_include_directories(MosaicCore
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
set_target_properties(MosaicCore PROPERTIES
    CXX_EXTENSIONS OFF
    EXPORT_NAME Core
)
mosaic_target_warnings(MosaicCore)

target_include_directories(MosaicCore PRIVATE ${MOSAIC_ZLIB_INCLUDE})
target_link_libraries(MosaicCore PRIVATE ${mosaic_zlib_library})

if(MOSAIC_ZLIB_DEPENDENCY)
    add_dependencies(MosaicCore ${MOSAIC_ZLIB_DEPENDENCY})
endif()

add_library(MosaicGraphics
    src/GraphicsBridge.cpp
)
add_library(Mosaic::Graphics ALIAS MosaicGraphics)

target_compile_features(MosaicGraphics PUBLIC cxx_std_20)
target_include_directories(MosaicGraphics
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(MosaicGraphics PUBLIC MosaicCore)
set_target_properties(MosaicGraphics PROPERTIES
    CXX_EXTENSIONS OFF
    EXPORT_NAME Graphics
)
mosaic_target_warnings(MosaicGraphics)

target_include_directories(MosaicGraphics PRIVATE ${MOSAIC_GRAPHICS_INCLUDE})
target_link_libraries(MosaicGraphics PRIVATE ${mosaic_graphics_library})
set_property(TARGET MosaicGraphics PROPERTY INTERFACE_LINK_LIBRARIES "MosaicCore;$<BUILD_INTERFACE:${mosaic_graphics_library}>;$<INSTALL_INTERFACE:Mosaic::GraphicsDependency>")
target_compile_definitions(MosaicGraphics PRIVATE MOSAIC_HAS_IROV_GRAPHICS=1)
if(MOSAIC_GRAPHICS_DEPENDENCY)
    add_dependencies(MosaicGraphics ${MOSAIC_GRAPHICS_DEPENDENCY})
endif()

add_library(MosaicPlatform INTERFACE)
add_library(Mosaic::Platform ALIAS MosaicPlatform)
target_compile_features(MosaicPlatform INTERFACE cxx_std_20)
target_include_directories(MosaicPlatform
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(MosaicPlatform INTERFACE MosaicCore)
set_target_properties(MosaicPlatform PROPERTIES
    EXPORT_NAME Platform
)

add_library(Mosaic INTERFACE)
add_library(Mosaic::Mosaic ALIAS Mosaic)
target_link_libraries(Mosaic INTERFACE MosaicCore MosaicGraphics MosaicPlatform)
set_target_properties(Mosaic PROPERTIES EXPORT_NAME Mosaic)

if(APPLE)
    enable_language(OBJCXX)
endif()

if(MOSAIC_BUILD_BACKENDS)
    if(APPLE OR WIN32)
        add_subdirectory(backends)
    endif()
endif()

if(NOT MOSAIC_ENABLE_EXCEPTIONS)
    if(MSVC)
        target_compile_options(MosaicCore PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/EHs-c->)
        target_compile_options(MosaicGraphics PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/EHs-c->)
    else()
        target_compile_options(MosaicCore PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>)
        target_compile_options(MosaicGraphics PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>)
    endif()
endif()

if(NOT MOSAIC_ENABLE_RTTI)
    if(MSVC)
        target_compile_options(MosaicCore PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/GR->)
        target_compile_options(MosaicGraphics PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/GR->)
    else()
        target_compile_options(MosaicCore PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>)
        target_compile_options(MosaicGraphics PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>)
    endif()
endif()

if(MOSAIC_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(MOSAIC_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(MOSAIC_INSTALL)
    install(TARGETS Mosaic MosaicCore MosaicGraphics MosaicPlatform
        EXPORT MosaicTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
    install(FILES "${mosaic_graphics_install_source}"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RENAME "${MOSAIC_GRAPHICS_PACKAGE_LIBRARY}"
    )
    install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        PATTERN ".DS_Store" EXCLUDE
    )
    install(EXPORT MosaicTargets
        FILE MosaicTargets.cmake
        NAMESPACE Mosaic::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Mosaic
    )

    configure_package_config_file(
        cmake/MosaicConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/MosaicConfig.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Mosaic
        PATH_VARS CMAKE_INSTALL_LIBDIR
    )
    write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/MosaicConfigVersion.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )
    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/MosaicConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/MosaicConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Mosaic
    )
endif()
