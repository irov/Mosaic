add_executable(MosaicHeadlessExample Headless.cpp)
target_link_libraries(MosaicHeadlessExample PRIVATE Mosaic::Mosaic)
target_compile_features(MosaicHeadlessExample PRIVATE cxx_std_20)
mosaic_target_warnings(MosaicHeadlessExample)

if(APPLE)
    add_subdirectory(MacOS)
endif()
