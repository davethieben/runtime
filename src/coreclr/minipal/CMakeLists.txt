include_directories(.)
include_directories(${CLR_SRC_NATIVE_DIR})
# For cross-compilation, check target OS not host OS
if (CLR_CMAKE_TARGET_FREERTOS)
    # FreeRTOS bare-metal: no platform-specific minipal subdirectory needed
    # Use stubs or Unix fallback implementations
elseif (CLR_CMAKE_TARGET_UNIX OR CLR_CMAKE_HOST_UNIX)
    add_subdirectory(Unix)
else ()
    add_subdirectory(Windows)
endif ()
