cmake_minimum_required(VERSION 3.24)

# Project settings
project(keeperfx-launcher-qt VERSION 0.1 LANGUAGES CXX)

# Check for supported operating systems
#if(NOT (UNIX OR WIN32))
#    message(FATAL_ERROR "Only UNIX and WIN32 compilation is implemented")
#endif()

# CMake settings
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# MinGW/GCC Release flags for Windows static builds
if(WIN32 AND CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -static -Os -s")
endif()

if(MSVC)
    set(DEP_CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W0")
    set(DEP_CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W0")
else()
    set(DEP_CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -w")
    set(DEP_CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -w")
endif()

include("cmake/CPM.cmake")
CPMFindPackage(
    NAME zlib
    VERSION 1.3.2
    GITHUB_REPOSITORY madler/zlib
    GIT_TAG v1.3.2
    OPTIONS
        "BUILD_SHARED_LIBS OFF"
        "CMAKE_CXX_FLAGS ${DEP_CMAKE_CXX_FLAGS}"
        "CMAKE_C_FLAGS ${DEP_CMAKE_C_FLAGS}"
)
CPMFindPackage(
    NAME bit7z
    VERSION 4.1.0
    GITHUB_REPOSITORY rikyoz/bit7z
    GIT_TAG v4.1.0
    OPTIONS
        "BUILD_SHARED_LIBS OFF"
        "BIT7Z_BUILD_TESTS OFF"
        "BIT7Z_BUILD_DOCS OFF"
        "CMAKE_CXX_FLAGS ${DEP_CMAKE_CXX_FLAGS}"
        "CMAKE_C_FLAGS ${DEP_CMAKE_C_FLAGS}"
)
CPMAddPackage(
    NAME LIEF
    GITHUB_REPOSITORY lief-project/LIEF
    GIT_TAG 1.0.0
    OPTIONS
        "BUILD_SHARED_LIBS OFF"
        "LIEF_DOC OFF"
        "LIEF_PYTHON_API OFF"
        "LIEF_EXAMPLES OFF"
        "LIEF_TESTS OFF"
        "LIEF_VERSION_BUMP OFF" # stop dynamic version regeneration
        "CMAKE_CXX_FLAGS ${DEP_CMAKE_CXX_FLAGS}"
        "CMAKE_C_FLAGS ${DEP_CMAKE_C_FLAGS}"
)

# Set custom UI directory
set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_SOURCE_DIR}/ui)

# Create /deps dir if it does not exist yet
#file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/deps")

# Function to download and extract dependencies
#function(download_and_extract url output_dir)
#    file(DOWNLOAD ${url} ${CMAKE_BINARY_DIR}/temp_archive)
#    if(UNIX)
#        execute_process(COMMAND ${CMAKE_COMMAND} -E tar xzf ${CMAKE_BINARY_DIR}/temp_archive WORKING_DIRECTORY ${output_dir})
#    elseif(WIN32)
#        execute_process(COMMAND ${CMAKE_COMMAND} -E tar xzf ${CMAKE_BINARY_DIR}/temp_archive WORKING_DIRECTORY ${output_dir})
#    endif()
#    file(REMOVE ${CMAKE_BINARY_DIR}/temp_archive)
#endfunction()

# bit7z
#set(BIT7Z_DIR ${CMAKE_SOURCE_DIR}/deps/bit7z-4.0.9/)
#if(NOT EXISTS BIT7Z_DIR)
#    download_and_extract("https://github.com/rikyoz/bit7z/archive/refs/tags/v4.0.9.zip" ${CMAKE_SOURCE_DIR}/deps)
#endif()

# bit7z does not come with a cmake file
#add_subdirectory(${BIT7Z_DIR})

# Find packages
find_package(Qt6 REQUIRED COMPONENTS Widgets Network Gui)
find_package(ZLIB REQUIRED)
find_package(LIEF REQUIRED COMPONENTS STATIC)
find_package(bit7z REQUIRED)

# Add Zlib
#find_package(ZLIB)
#if (NOT ZLIB_FOUND)
#    message(STATUS "Zlib library not found, using local dependency")
#    set(ZLIB_DIR ${CMAKE_SOURCE_DIR}/deps/zlib-1.3.1)
#    if(NOT EXISTS ${ZLIB_DIR})
#        download_and_extract("https://zlib.net/zlib131.zip" ${CMAKE_SOURCE_DIR}/deps)
#    endif()
#    add_subdirectory(${ZLIB_DIR})
#endif()

# Add LIEF
#find_package(LIEF)
#if (NOT LIEF_FOUND)
#    message(STATUS "LIEF library not found, using local dependency")
#
#    # Download LIEF if it is not downloaded yet
#    if(UNIX)
#        set(LIEF_INSTALL_DIR ${CMAKE_SOURCE_DIR}/deps/LIEF-0.17.6-Linux-x86_64)
#        if(NOT EXISTS LIEF_INSTALL_DIR)
#            download_and_extract("https://github.com/lief-project/LIEF/releases/download/0.17.6/LIEF-0.17.6-Linux-x86_64.tar.gz" ${CMAKE_SOURCE_DIR}/deps)
#        endif()
#    elseif(WIN32)
#        set(LIEF_INSTALL_DIR ${CMAKE_SOURCE_DIR}/deps/LIEF-0.17.6-win64)
#        if(NOT EXISTS LIEF_INSTALL_DIR)
#            download_and_extract("https://github.com/lief-project/LIEF/releases/download/0.17.6/LIEF-0.17.6-win64.zip" ${CMAKE_SOURCE_DIR}/deps)
#        endif()
#    endif()
#
#    # Load local LIEF
#    set(LIEF_DIR ${LIEF_INSTALL_DIR}/lib/cmake/LIEF)
#    set(LIEF_INCLUDE_DIR ${LIEF_INSTALL_DIR}/include)
#    set(LIEF_LIB_DIR ${LIEF_INSTALL_DIR}/lib)
#    include_directories(${LIEF_INCLUDE_DIR})
#    link_directories(${LIEF_LIB_DIR})
#endif()

# Get source files
file(GLOB PROJECT_SOURCES src/*.cpp src/*.h)
file(GLOB UI_FILES ui/*.ui)

if(WIN32)
    qt_add_executable(keeperfx-launcher-qt
        MANUAL_FINALIZATION
        WIN32
        ${PROJECT_SOURCES}
        ${UI_FILES}
        resources.qrc
        app.rc
    )
else()
    qt_add_executable(keeperfx-launcher-qt
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        ${UI_FILES}
        resources.qrc
    )
endif()

# Link libraries
target_link_libraries(keeperfx-launcher-qt PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    Qt6::Gui
    ZLIB::ZLIB
    LIEF::LIEF
    bit7z
)

# Link Zlib
#if (ZLIB_FOUND)
#    target_link_libraries(keeperfx-launcher-qt PRIVATE ZLIB::ZLIB)
#else()
#    target_include_directories(keeperfx-launcher-qt PRIVATE ${ZLIB_DIR})
#    target_link_libraries(keeperfx-launcher-qt PRIVATE zlibstatic)
#endif()

# Link LIEF
#if (LIEF_FOUND)
#    target_link_libraries(keeperfx-launcher-qt PRIVATE LIEF::LIEF)
#else()
#    if(UNIX)
#        target_link_libraries(keeperfx-launcher-qt PRIVATE ${LIEF_LIB_DIR}/libLIEF.a)
#    elseif(WIN32)
#        if (CMAKE_BUILD_TYPE STREQUAL "Release")
#            target_link_libraries(keeperfx-launcher-qt PRIVATE ${LIEF_LIB_DIR}/LIEF_static.lib)
#        else()
#            target_link_libraries(keeperfx-launcher-qt PRIVATE ${LIEF_LIB_DIR}/LIEF.lib)
#        endif()
#    endif()
#endif()

# Set target properties
set_target_properties(keeperfx-launcher-qt PROPERTIES
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

# Finalize executable
qt_finalize_executable(keeperfx-launcher-qt)

# Copy pre-compiled libraries
if (UNIX)
    file(GLOB COMPILED_LIBS "${CMAKE_SOURCE_DIR}/ext/*.so")
elseif (WIN32)
    file(GLOB COMPILED_LIBS "${CMAKE_SOURCE_DIR}/ext/*.dll")
endif()
foreach(LIB ${COMPILED_LIBS})
    add_custom_command(TARGET keeperfx-launcher-qt POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy "${LIB}" $<TARGET_FILE_DIR:keeperfx-launcher-qt>
        VERBATIM
    )
endforeach()

# Linux Installation settings
if (UNIX)
    include(GNUInstallDirs)
    install(TARGETS keeperfx-launcher-qt
        BUNDLE DESTINATION .
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()
