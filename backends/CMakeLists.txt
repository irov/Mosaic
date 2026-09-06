if(APPLE)
    add_subdirectory(MacOS)
    add_subdirectory(Metal)
endif()

if(WIN32)
    add_subdirectory(Win32)
endif()
