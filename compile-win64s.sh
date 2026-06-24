#!/bin/bash

# Function to display usage
usage() {
    echo ""
    echo " KeeperFX Launcher - Win64 Compiler"
    echo ""
    echo " Usage: $0 <debug|release|console> [--installer] [--verbose]"
    echo ""
    echo " Commands: "
    echo "     $0 <debug|release>      compile debug or release version"
    echo "     $0 console              open a console in the compiler container"
    echo ""
    echo " Options:"
    echo "     --installer             also create an installer for the release"
    echo "     --verbose               enable cmake verbosity"
    echo ""
    exit 1
}

# Check if at least one argument is provided
if [ $# -lt 1 ]; then
    usage
fi

# Make sure $(pwd) is populated
if [[ -n "$(pwd)" ]]; then
    echo "Current directory: $(pwd)"
else
    echo "Failed to retrieve the current directory"
    exit 1
fi

# Variables
MODE="$1"
VERBOSE=""
BUILD_SHARED_LIBS=ON
BUILD_INSTALLER=OFF

# Handle options
shift # Remove first argument
while (("$#")); do
    case "$1" in
        --installer)
            BUILD_INSTALLER=ON
            ;;
        --verbose)
            VERBOSE="--verbose"
            ;;
        *)
            usage
            ;;
    esac
    shift
done

# Disallow making installers for debug versions
if [ "$MODE" == "debug" ] && [ "$BUILD_INSTALLER" == "ON" ]; then
    echo "Making an installer for a debug version is not possible"
    exit 1
fi

# Get current user and group ID
USER_ID=$(id -u)
GROUP_ID=$(id -g)

# Get launcher version
HEADER_FILE="$(pwd)/src/version.h"
VERSION_MAJOR=$(grep -E "#define LAUNCHER_VERSION_MAJOR" "$HEADER_FILE" | awk '{print $3}')
VERSION_MINOR=$(grep -E "#define LAUNCHER_VERSION_MINOR" "$HEADER_FILE" | awk '{print $3}')
VERSION_PATCH=$(grep -E "#define LAUNCHER_VERSION_PATCH" "$HEADER_FILE" | awk '{print $3}')
VERSION="$VERSION_MAJOR.$VERSION_MINOR.$VERSION_PATCH"
echo "Launcher Version: $VERSION"

# Build Docker image if it does not exist
if [[ "$(docker images -q kfx-launcher-win64-compiler 2> /dev/null)" == "" ]]; then
    echo "Docker image kfx-launcher-win64-compiler does not exist. Building the image..."
    #docker build --build-arg USER_ID=$USER_ID --build-arg GROUP_ID=$GROUP_ID -t kfx-launcher-win64-compiler -f Dockerfile.win64s .
    docker build -t kfx-launcher-win64-compiler -f Dockerfile.win64s .
else
    echo "Docker image 'kfx-launcher-win64-compiler' already exists. No need to build again."
fi

# Make sure Docker image exists after possible build
if [[ "$(docker images -q kfx-launcher-win64-compiler 2> /dev/null)" == "" ]]; then
    echo "Docker image 'kfx-launcher-win64-compiler' does not exist"
    exit 1
fi

# Handle mode
if [ "$MODE" == "console" ]; then
    docker run --rm -it -v "$(pwd)":/project -u $USER_ID:$GROUP_ID kfx-launcher-win64-compiler bash
    exit 0
elif [ "$MODE" == "debug" ]; then
    CMAKE_BUILD_TYPE="Debug"
elif [ "$MODE" == "release" ]; then
    CMAKE_BUILD_TYPE="Release"
    BUILD_SHARED_LIBS=OFF
else
    usage
fi

# Start compiling
echo "Compiling $CMAKE_BUILD_TYPE..."

# Run the Docker container and compile the project
docker run --rm -v "$(pwd)":/project -u $USER_ID:$GROUP_ID kfx-launcher-win64-compiler bash -c "
    cd /project &&
    x86_64-w64-mingw32.static-cmake -Bbuild/mingw-win64 -H. -DWINDOWS=TRUE -DCMAKE_BUILD_TYPE=$CMAKE_BUILD_TYPE -DBUILD_SHARED_LIBS=$BUILD_SHARED_LIBS -Wno-dev &&
    x86_64-w64-mingw32.static-cmake --build build/mingw-win64 --config $CMAKE_BUILD_TYPE $VERBOSE
    "

# Make sure executable is built
if ! [ -f "$(pwd)/build/mingw-win64/keeperfx-launcher-qt.exe" ]; then
    echo "Executable 'keeperfx-launcher-qt.exe' not built"
    exit 1
fi

# Handle release
if [ "$MODE" == "release" ]; then

    # Clean up any previous builds
    if [ -d "$(pwd)/release/win64" ]; then
        rm -rf "$(pwd)/release/win64"
    fi

    # Make release dir and move files
    mkdir -p "$(pwd)/release/win64/"
    cp "$(pwd)/build/mingw-win64/keeperfx-launcher-qt.exe" "$(pwd)/release/win64/keeperfx-launcher-qt.exe"
    cp "$(pwd)/build/mingw-win64/7za.dll" "$(pwd)/release/win64/7za.dll"

    # Make an installer
    if [ "$BUILD_INSTALLER" == "ON" ]; then
        echo "Building installer..."
        docker run --rm -i -v "$(pwd)":/work amake/innosetup windows-installer.iss
        # Rename installer to include launcher version
        mv "$(pwd)/release/win64/keeperfx-web-installer.exe" "$(pwd)/release/win64/keeperfx-launcher-qt-$VERSION-win64-web-installer.exe"
    fi

    # Package files
    echo "Packaging release"
    7z a "$(pwd)/release/win64/keeperfx-launcher-qt-$VERSION-win64.7z" "$(pwd)/release/win64/keeperfx-launcher-qt.exe" "$(pwd)/release/win64/7za.dll"

    # Show release directory output
    ls -lh "$(pwd)/release/win64/"
fi

# Done
echo ""
echo "Build complete."
