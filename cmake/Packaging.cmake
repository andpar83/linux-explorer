# Installation and the Ubuntu/Debian package (CPack DEB).
#
#   cmake --build --preset release --target package   ->  build/release/linux-explorer_<version>_amd64.deb
#
# Runtime dependencies are computed from the binary with dpkg-shlibdeps (package dpkg-dev); without it
# a fixed list is used. Only the application is installed: tests and third-party code never are.

include(GNUInstallDirs)

install(TARGETS linux-explorer RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(FILES "${PROJECT_SOURCE_DIR}/packaging/linux-explorer.desktop" DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")
install(FILES "${PROJECT_SOURCE_DIR}/LICENSE" DESTINATION "${CMAKE_INSTALL_DATADIR}/doc/linux-explorer" RENAME copyright)

set(CPACK_GENERATOR DEB)
set(CPACK_PACKAGE_NAME "linux-explorer")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_CONTACT "Andrey Parfenov <andpar83@gmail.com>")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/andpar83/linux-explorer")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Process Explorer for Linux")
set(CPACK_PACKAGE_DESCRIPTION
    "A live, hierarchical view of every process with CPU, memory, owner and command line,\n"
    "the memory maps of the selected process with their page accounting, and the pages of a\n"
    "selected mapping: resident, swapped or absent. Reads /proc directly.")
set(CPACK_STRIP_FILES ON)
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_SECTION utils)
set(CPACK_DEBIAN_PACKAGE_PRIORITY optional)
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "fonts-ubuntu | fonts-noto-core | fonts-dejavu-core")
set(CPACK_DEBIAN_PACKAGE_SUGGESTS "fonts-font-awesome")

find_program(LXE_DPKG_SHLIBDEPS dpkg-shlibdeps)
if(LXE_DPKG_SHLIBDEPS)
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
else()
    message(STATUS "dpkg-shlibdeps not found (install dpkg-dev): using a fixed dependency list for the .deb")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libsdl3-0, libstdc++6 (>= 15), libc6")
endif()

include(CPack)
