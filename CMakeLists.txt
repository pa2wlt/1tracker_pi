# ~~~
# Summary:      Main plugin build script for 1tracker_pi
# License:      GPLv3+
# ~~~

cmake_minimum_required(VERSION 3.18)
include(${CMAKE_SOURCE_DIR}/cmake/CmakeSetup.cmake NO_POLICY_SCOPE)
message(STATUS "Cmake version: ${CMAKE_VERSION}.")

# -------- Build setup --------
include(PluginOptions)
if (BUILD_TYPE)
  message(STATUS "Building: ${BUILD_TYPE}")
else ()
  message(STATUS "Configuring")
endif ()

# -------- Plugin setup --------
include(Plugin.cmake)

# -------- Setup completed, build the plugin --------
project(${PKG_NAME} VERSION ${PKG_VERSION})
include(PluginCompiler)

add_library(${CMAKE_PROJECT_NAME} SHARED EXCLUDE_FROM_ALL ${SRC})
include_directories(BEFORE ${CMAKE_BINARY_DIR}/include)

add_subdirectory("opencpn-libs/${PKG_API_LIB}")
target_link_libraries(${CMAKE_PROJECT_NAME} PUBLIC ocpn::api)

set(VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(VERSION_PATCH ${PROJECT_VERSION_PATCH})
if (PROJECT_VERSION_TWEAK STREQUAL "")
  set(PROJECT_VERSION_TWEAK 0)
endif ()

set(PACKAGE_NAME ${CMAKE_PROJECT_NAME})

string(REGEX REPLACE "([0-9])\\.([0-9]).*" "ov\\1\\2"
       OCPN_MIN_VERSION ${API_OCPN_MIN_VERSION})

# macOS: set up universal wxWidgets BEFORE late_init so it finds the right wx
if (APPLE)
  include(MacosWxwidgets)
endif ()

if (COMMAND late_init)
  late_init()
endif ()

if (COMMAND add_plugin_libraries)
  add_plugin_libraries()
endif ()

# Configure the flatpak manifest
file(
  GLOB manifest ${PROJECT_SOURCE_DIR}/flatpak/org.opencpn.OpenCPN.Plugin.*.yaml
)
include(ConfigureManifest)
configure_manifest(${manifest} _new_manifest)

# Create tarball, flatpak, and android packaging targets
include(Targets)
create_targets(${_new_manifest})

# -------- Developer and test targets (always available) --------

if (APPLE)
  set(OCPN_DEV_PLUGIN_DIR
      "$ENV{HOME}/Library/Application Support/OpenCPN/Contents/PlugIns"
      CACHE PATH "Local OpenCPN plugin directory for developer deploys")
  set(OCPN_DEV_PLUGIN_DATA_DIR
      "$ENV{HOME}/Library/Application Support/OpenCPN/Contents/SharedSupport/plugins/${PKG_NAME}/data"
      CACHE PATH "Local OpenCPN plugin data directory for developer deploys")

  add_custom_target(deploy_opencpn_dev
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${OCPN_DEV_PLUGIN_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${OCPN_DEV_PLUGIN_DATA_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E create_symlink
            "$<TARGET_FILE:${CMAKE_PROJECT_NAME}>"
            "${OCPN_DEV_PLUGIN_DIR}/lib${PKG_NAME}.dylib"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_SOURCE_DIR}/data/icons/1tracker_icon.svg"
            "${OCPN_DEV_PLUGIN_DATA_DIR}/1tracker_icon.svg"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_SOURCE_DIR}/data/icons/1tracker_toolbar_icon.svg"
            "${OCPN_DEV_PLUGIN_DATA_DIR}/1tracker_toolbar_icon.svg"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_SOURCE_DIR}/data/icons/1tracker_hero.png"
            "${OCPN_DEV_PLUGIN_DATA_DIR}/1tracker_hero.png"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_SOURCE_DIR}/data/icons/1tracker_json_header.png"
            "${OCPN_DEV_PLUGIN_DATA_DIR}/1tracker_json_header.png"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_SOURCE_DIR}/data/icons/1tracker_nfl_header.png"
            "${OCPN_DEV_PLUGIN_DATA_DIR}/1tracker_nfl_header.png"
    DEPENDS ${CMAKE_PROJECT_NAME}
    COMMENT "Deploy 1tracker_pi and toolbar icons into local OpenCPN app support"
  )

  add_custom_target(restart_opencpn_dev
    COMMAND "${CMAKE_SOURCE_DIR}/scripts/restart_opencpn_dev.sh"
    DEPENDS deploy_opencpn_dev
    COMMENT "Build, deploy, gracefully restart OpenCPN"
  )

endif ()

option(BUILD_TESTING "Build unit tests" ON)
include(CTest)
if (BUILD_TESTING AND TARGET onetracker_core AND "${BUILD_TYPE}" STREQUAL "")
  add_subdirectory(test)
endif ()

if ("${BUILD_TYPE}" STREQUAL "")
  return ()
endif ()

# -------- Packaging-only configuration --------

if (NOT ${BUILD_TYPE} STREQUAL "flatpak")
  if (APPLE)
    include(MacosWxwidgets)
  endif ()
  include(PluginInstall)
  if (QT_ANDROID)
    include(AndroidLibs)
  endif ()
  include(PluginLocalization)
endif ()

configure_file(
  ${CMAKE_SOURCE_DIR}/config.h.in
  ${CMAKE_BINARY_DIR}/include/config.h
  @ONLY
)
configure_file(
  ${CMAKE_SOURCE_DIR}/ci/upload.sh.in
  ${CMAKE_BINARY_DIR}/upload.sh
  @ONLY
)
configure_file(
  ${CMAKE_SOURCE_DIR}/ci/upload.bat.in
  ${CMAKE_BINARY_DIR}/upload.bat
  @ONLY
)

set(checksum "@checksum@")
configure_file(
  ${CMAKE_SOURCE_DIR}/plugin.xml.in
  ${CMAKE_BINARY_DIR}/${pkg_displayname}.xml.in
  @ONLY
)
