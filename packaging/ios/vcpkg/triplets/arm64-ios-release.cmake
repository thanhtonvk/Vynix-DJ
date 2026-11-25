set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME iOS)
set(VCPKG_OSX_ARCHITECTURES arm64)
# Minimum of Qt 6.8
set(VCPKG_OSX_DEPLOYMENT_TARGET 16.0)

set(VCPKG_BUILD_TYPE release)

# Configure scripts compare these to detect cross compiling. vcpkg passes the
# same for both on ARM Macs, so they try to run programs built for iOS.
set(VCPKG_MAKE_BUILD_TRIPLET "--host=arm-apple-darwin;--build=aarch64-apple-darwin")
