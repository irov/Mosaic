add_library(MosaicBackendWin32
    Win32PlatformAdapter.cpp
)
add_library(Mosaic::BackendWin32 ALIAS MosaicBackendWin32)

target_compile_features(MosaicBackendWin32 PUBLIC cxx_std_20)
target_compile_definitions(MosaicBackendWin32 PRIVATE UNICODE _UNICODE)
target_include_directories(MosaicBackendWin32
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/Mosaic/Backends/Win32>
)
target_link_libraries(MosaicBackendWin32
    PUBLIC Mosaic::Mosaic
    PRIVATE imm32 ole32 shell32 user32 uuid
)
set_target_properties(MosaicBackendWin32 PROPERTIES
    CXX_EXTENSIONS OFF
    EXPORT_NAME BackendWin32
)
mosaic_target_warnings(MosaicBackendWin32)

install(TARGETS MosaicBackendWin32
    EXPORT MosaicTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(FILES
    Win32PlatformAdapter.hpp
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/Mosaic/Backends/Win32
)
