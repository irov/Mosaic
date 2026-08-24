add_executable(MosaicMacOSExample MACOSX_BUNDLE
    EditorPersistence.cpp
    FakeEditor.cpp
    Main.mm
    MosaicView.mm
)

target_compile_features(MosaicMacOSExample PRIVATE cxx_std_20)
target_compile_options(MosaicMacOSExample PRIVATE $<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>)
target_link_libraries(MosaicMacOSExample PRIVATE
    Mosaic::Mosaic
    Mosaic::BackendMacOS
    Mosaic::BackendMetal
)
mosaic_target_warnings(MosaicMacOSExample)

set_target_properties(MosaicMacOSExample PROPERTIES
    MACOSX_BUNDLE_BUNDLE_NAME "Mosaic Fake Editor"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.irov.mosaic.fake-editor"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "0.1"
    MACOSX_BUNDLE_BUNDLE_VERSION "1"
)

set(MOSAIC_HELLO_DEMO_NAME "Hello Dear imGUI")

add_executable(MosaicHelloDemoExample MACOSX_BUNDLE
    HelloDemo.cpp
    HelloDemoExamples.cpp
    HelloDemoTables.cpp
    HelloDemoMain.mm
    HelloDemoView.mm
)

target_compile_features(MosaicHelloDemoExample PRIVATE cxx_std_20)
target_compile_options(MosaicHelloDemoExample PRIVATE $<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>)
target_link_libraries(MosaicHelloDemoExample PRIVATE
    Mosaic::Mosaic
    Mosaic::BackendMacOS
    Mosaic::BackendMetal
)
mosaic_target_warnings(MosaicHelloDemoExample)

set_target_properties(MosaicHelloDemoExample PROPERTIES
    OUTPUT_NAME "${MOSAIC_HELLO_DEMO_NAME}"
    MACOSX_BUNDLE_BUNDLE_NAME "${MOSAIC_HELLO_DEMO_NAME}"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.irov.mosaic.hello-demo"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "0.1"
    MACOSX_BUNDLE_BUNDLE_VERSION "1"
)
