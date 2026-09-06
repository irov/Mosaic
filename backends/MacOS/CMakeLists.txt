add_library(MosaicBackendMacOS
    CoreTextFontProvider.mm
    MacOSPlatformAdapter.mm
    TextInputAdapter.mm
)
add_library(Mosaic::BackendMacOS ALIAS MosaicBackendMacOS)

target_compile_features(MosaicBackendMacOS PUBLIC cxx_std_20)
target_compile_options(MosaicBackendMacOS PRIVATE $<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>)
target_include_directories(MosaicBackendMacOS
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/Mosaic/Backends/MacOS>
)
target_link_libraries(MosaicBackendMacOS
    PUBLIC Mosaic::Mosaic
    PRIVATE
        "-framework Cocoa"
        "-framework CoreGraphics"
        "-framework CoreText"
        "-framework QuartzCore"
)
set_target_properties(MosaicBackendMacOS PROPERTIES
    CXX_EXTENSIONS OFF
    EXPORT_NAME BackendMacOS
)
mosaic_target_warnings(MosaicBackendMacOS)

install(TARGETS MosaicBackendMacOS
    EXPORT MosaicTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(FILES
    CoreTextFontProvider.hpp
    MacOSPlatformAdapter.hpp
    TextInputAdapter.hpp
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/Mosaic/Backends/MacOS
)
