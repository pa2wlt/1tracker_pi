cmake_minimum_required(VERSION 3.18)

project(onetracker_pi VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_TESTING "Build unit tests" ON)

set(ONETRACKER_PLUGIN_PACKAGE_NAME "1tracker_pi")
set(ONETRACKER_PLUGIN_COMMON_NAME "1tracker")
set(ONETRACKER_PLUGIN_SUMMARY
    "Sends position and wind to HTTP endpoints and noforeignland.")
set(
  ONETRACKER_PLUGIN_DESCRIPTION
  "1tracker sends your boat's position (and optionally apparent wind) from OpenCPN to one or more HTTP endpoints on a configurable interval. Supported endpoint types are a generic JSON endpoint with an API-key header (for your own website or API) and noforeignland. Each tracker has its own interval, API key and on/off switch. A toolbar icon shows green when sending succeeds, red on failure, neutral when idle. Configuration lives in a JSON file in OpenCPN's plugin data folder. The plugin has been set up so that new tracker types can be added easily."
)
set(ONETRACKER_PLUGIN_SOURCE_URL "https://github.com/pa2wlt/1tracker_pi")
set(ONETRACKER_PLUGIN_LICENSE "GPL-2.0-or-later")

configure_file(
  "${CMAKE_SOURCE_DIR}/cmake/in-files/version.h.in"
  "${CMAKE_BINARY_DIR}/generated/1tracker_pi/version.h"
  @ONLY
)

configure_file(
  "${CMAKE_SOURCE_DIR}/cmake/in-files/plugin_metadata.h.in"
  "${CMAKE_BINARY_DIR}/generated/1tracker_pi/plugin_metadata.h"
  @ONLY
)

configure_file(
  "${CMAKE_SOURCE_DIR}/cmake/in-files/plugin.xml.in"
  "${CMAKE_BINARY_DIR}/plugin.xml"
  @ONLY
)

if(APPLE)
  set(
    OCPN_PLUGIN_LIBRARY_DESTINATION
    "Contents/PlugIns"
    CACHE STRING
    "Install destination for the plugin shared library"
  )
  set(
    OCPN_PLUGIN_DATA_DESTINATION
    "Contents/SharedSupport/plugins/1tracker_pi/data"
    CACHE STRING
    "Install destination for plugin data files"
  )
  set(
    OCPN_DEV_PLUGIN_DIR
    "$ENV{HOME}/Library/Application Support/OpenCPN/Contents/PlugIns"
    CACHE PATH
    "Local OpenCPN plugin directory for developer deploys"
  )
  set(
    OCPN_DEV_PLUGIN_DATA_DIR
    "$ENV{HOME}/Library/Application Support/OpenCPN/Contents/SharedSupport/plugins/1tracker_pi/data"
    CACHE PATH
    "Local OpenCPN plugin data directory for developer deploys"
  )
else()
  set(
    OCPN_PLUGIN_LIBRARY_DESTINATION
    "."
    CACHE STRING
    "Install destination for the plugin shared library"
  )
  set(
    OCPN_PLUGIN_DATA_DESTINATION
    "share/opencpn/plugins/1tracker_pi/data"
    CACHE STRING
    "Install destination for plugin data files"
  )
endif()

set(OCPN_API_LIB "api-18" CACHE STRING "OpenCPN API library directory in opencpn-libs")

if(APPLE AND EXISTS "/opt/homebrew/opt/wxwidgets@3.2/bin/wx-config-3.2")
  set(
    wxWidgets_CONFIG_EXECUTABLE
    "/opt/homebrew/opt/wxwidgets@3.2/bin/wx-config-3.2"
    CACHE FILEPATH
    "wx-config executable used to locate a wxWidgets toolchain"
    FORCE
  )
endif()

find_package(wxWidgets REQUIRED COMPONENTS base core net xml)
include(${wxWidgets_USE_FILE})
find_library(SYSTEM_LIBCURL NAMES curl libcurl REQUIRED)
find_path(SYSTEM_CURL_INCLUDE_DIR NAMES curl/curl.h REQUIRED)

if(APPLE)
  set(
    OCPN_APP_FRAMEWORKS_DIR
    "/Applications/OpenCPN.app/Contents/Frameworks"
    CACHE PATH
    "Framework directory inside the local OpenCPN.app bundle"
  )

  set(OCPN_APP_WX_LIBRARIES
      "${OCPN_APP_FRAMEWORKS_DIR}/libwx_baseu-3.2.dylib"
      "${OCPN_APP_FRAMEWORKS_DIR}/libwx_osx_cocoau_core-3.2.dylib"
      "${OCPN_APP_FRAMEWORKS_DIR}/libwx_baseu_net-3.2.dylib"
      "${OCPN_APP_FRAMEWORKS_DIR}/libwx_baseu_xml-3.2.dylib")

  if(EXISTS "${OCPN_APP_FRAMEWORKS_DIR}/libwx_baseu-3.2.dylib")
    set(wxWidgets_LIBRARIES ${OCPN_APP_WX_LIBRARIES})
    if(NOT EXISTS "${CMAKE_BINARY_DIR}/Frameworks")
      file(CREATE_LINK "${OCPN_APP_FRAMEWORKS_DIR}" "${CMAKE_BINARY_DIR}/Frameworks" SYMBOLIC)
    endif()
  endif()
endif()

add_subdirectory("${CMAKE_SOURCE_DIR}/opencpn-libs/${OCPN_API_LIB}")
add_subdirectory("${CMAKE_SOURCE_DIR}/opencpn-libs/jsoncpp")

add_library(ocpn_libcurl_if INTERFACE)
add_library(ocpn::libcurl ALIAS ocpn_libcurl_if)
target_link_libraries(ocpn_libcurl_if INTERFACE "${SYSTEM_LIBCURL}")
target_include_directories(ocpn_libcurl_if INTERFACE "${SYSTEM_CURL_INCLUDE_DIR}")

add_subdirectory("${CMAKE_SOURCE_DIR}/opencpn-libs/wxcurl")

add_library(
  onetracker_core
  STATIC
  src/config_loader.cpp
  src/endpoint_policy.cpp
  src/endpoint_sender.cpp
  src/endpoint_type_registry.cpp
  src/payload_builder.cpp
  src/scheduler.cpp
  src/state_store.cpp
)

target_include_directories(
  onetracker_core
  PUBLIC
    "${CMAKE_SOURCE_DIR}/include"
)

target_link_libraries(
  onetracker_core
  PUBLIC
    ocpn::jsoncpp
    ocpn::wxcurl
    ${wxWidgets_LIBRARIES}
)

target_compile_features(onetracker_core PUBLIC cxx_std_17)

add_library(
  onetracker_pi
  SHARED
  src/plugin.cpp
  src/plugin_ui_utils.cpp
  src/tracker_dialog.cpp
)

target_include_directories(
  onetracker_pi
  PRIVATE
    "${CMAKE_BINARY_DIR}/generated"
    "${CMAKE_SOURCE_DIR}/src"
)

target_link_libraries(
  onetracker_pi
  PRIVATE
    onetracker_core
    ocpn::api
    ${wxWidgets_LIBRARIES}
)

set_target_properties(
  onetracker_pi
  PROPERTIES
    PREFIX ""
    OUTPUT_NAME "1tracker_pi"
)

target_compile_definitions(onetracker_pi PRIVATE MAKING_PLUGIN)

add_custom_target(
  catalog_metadata
  DEPENDS "${CMAKE_BINARY_DIR}/plugin.xml"
  COMMENT "Generate plugin catalog metadata skeleton"
)

if(APPLE)
  target_link_options(onetracker_pi PRIVATE "-undefined" "dynamic_lookup")
elseif(UNIX AND NOT APPLE)
  target_link_options(onetracker_pi PRIVATE "-Wl,--unresolved-symbols=ignore-in-shared-libs")
endif()

install(
  TARGETS onetracker_pi
  LIBRARY DESTINATION "${OCPN_PLUGIN_LIBRARY_DESTINATION}"
)

install(
  FILES
    data/icons/1tracker_icon.svg
    data/icons/1tracker_toolbar_icon.svg
    data/icons/1tracker_hero.png
    data/icons/1tracker_json_header.png
    data/icons/1tracker_nfl_header.png
  DESTINATION "${OCPN_PLUGIN_DATA_DESTINATION}"
)

if(APPLE)
  add_custom_target(
    deploy_opencpn_dev
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${OCPN_DEV_PLUGIN_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${OCPN_DEV_PLUGIN_DATA_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E create_symlink
            "$<TARGET_FILE:onetracker_pi>"
            "${OCPN_DEV_PLUGIN_DIR}/1tracker_pi.dylib"
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
    DEPENDS onetracker_pi
    COMMENT "Deploy 1tracker_pi and toolbar icons into the local OpenCPN app support folders"
  )

  add_custom_target(
    restart_opencpn_dev
    COMMAND "${CMAKE_SOURCE_DIR}/scripts/restart_opencpn_dev.sh"
    DEPENDS deploy_opencpn_dev
    COMMENT "Build, deploy, gracefully restart OpenCPN"
  )

  add_custom_target(
    refresh_toolbar_icons_dev
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${OCPN_DEV_PLUGIN_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${OCPN_DEV_PLUGIN_DATA_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E create_symlink
            "$<TARGET_FILE:onetracker_pi>"
            "${OCPN_DEV_PLUGIN_DIR}/1tracker_pi.dylib"
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
    COMMAND /bin/sh -c "rm -f \"$HOME/Library/Preferences/opencpn/plugins/1tracker_pi/toolbar_icon_\"*.svg"
    COMMAND /bin/sh -c "rm -f \"$HOME/Library/Preferences/opencpn/iconCacheSVG/\"*_Library_Preferences_opencpn_plugins_1tracker_pi_toolbar_icon_*.svg_32x32"
    DEPENDS onetracker_pi
    COMMENT "Deploy 1tracker_pi and clear local runtime toolbar SVGs and OpenCPN SVG cache"
  )
endif()

include(CTest)
if(BUILD_TESTING)
  add_subdirectory(test)
endif()
