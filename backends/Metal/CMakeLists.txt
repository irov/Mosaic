add_library(MosaicBackendMetal
    MetalRendererAdapter.mm
)
add_library(Mosaic::BackendMetal ALIAS MosaicBackendMetal)

target_compile_features(MosaicBackendMetal PUBLIC cxx_std_20)
target_compile_options(MosaicBackendMetal PRIVATE $<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>)
target_include_directories(MosaicBackendMetal
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/Mosaic/Backends/Metal>
)
target_link_libraries(MosaicBackendMetal
    PUBLIC Mosaic::Mosaic
    PRIVATE
        "-framework Metal"
        "-framework MetalKit"
        "-framework QuartzCore"
)
set_target_properties(MosaicBackendMetal PROPERTIES
    CXX_EXTENSIONS OFF
    EXPORT_NAME BackendMetal
)
mosaic_target_warnings(MosaicBackendMetal)

install(TARGETS MosaicBackendMetal
    EXPORT MosaicTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(FILES
    MetalRendererAdapter.hpp
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/Mosaic/Backends/Metal
)
