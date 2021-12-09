# Set up a more familiar Visual Studio configuration
# Override these options with -DCMAKE_OPTION=Value
#
# See: https://cmake.org/cmake/help/latest/command/set.html#set-cache-entry
if(CMAKE_GENERATOR MATCHES "Visual Studio")
    set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "/DEBUG:FULL /INCREMENTAL:NO" CACHE STRING "")
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "/DEBUG:FULL /INCREMENTAL:NO" CACHE STRING "")
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "")
    #set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "")
endif()