add_executable(MosaicCoreTests CoreTests.cpp)
target_link_libraries(MosaicCoreTests PRIVATE Mosaic::Core)
target_include_directories(MosaicCoreTests PRIVATE ${PROJECT_SOURCE_DIR}/src)
mosaic_target_warnings(MosaicCoreTests)
add_test(NAME MosaicCoreTests COMMAND MosaicCoreTests)

add_executable(MosaicNativeSurfaceTests NativeSurfaceTests.cpp)
target_link_libraries(MosaicNativeSurfaceTests PRIVATE Mosaic::Mosaic)
mosaic_target_warnings(MosaicNativeSurfaceTests)
add_test(NAME MosaicNativeSurfaceTests COMMAND MosaicNativeSurfaceTests)

add_executable(MosaicInteractionTests InteractionTests.cpp)
target_link_libraries(MosaicInteractionTests PRIVATE Mosaic::Core)
target_include_directories(MosaicInteractionTests PRIVATE ${PROJECT_SOURCE_DIR}/src)
mosaic_target_warnings(MosaicInteractionTests)
add_test(NAME MosaicInteractionTests COMMAND MosaicInteractionTests)

if(MOSAIC_BUILD_EXAMPLES)
    add_executable(MosaicEditorSmokeTests
        EditorSmokeTests.cpp
        ${PROJECT_SOURCE_DIR}/examples/MacOS/FakeEditor.cpp
        ${PROJECT_SOURCE_DIR}/examples/MacOS/EditorPersistence.cpp
    )
    target_include_directories(MosaicEditorSmokeTests PRIVATE ${PROJECT_SOURCE_DIR}/examples/MacOS)
    target_link_libraries(MosaicEditorSmokeTests PRIVATE Mosaic::Mosaic)
    mosaic_target_warnings(MosaicEditorSmokeTests)
    add_test(NAME MosaicEditorSmokeTests COMMAND MosaicEditorSmokeTests)
endif()
