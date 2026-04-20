# ~~~
# Summary:      Local, plugin-specific setup for 1tracker_pi
# License:      GPLv3+
# ~~~

# -------- Cloudsmith upload repos --------
set(OCPN_TEST_REPO
    "opencpn/1tracker_pi-alpha"
    CACHE STRING "Default repository for untagged builds"
)
set(OCPN_BETA_REPO
    "opencpn/1tracker_pi-beta"
    CACHE STRING "Default repository for tagged builds matching 'beta'"
)
set(OCPN_RELEASE_REPO
    "opencpn/1tracker_pi-prod"
    CACHE STRING "Default repository for tagged builds not matching 'beta'"
)

# -------- Plugin identity --------
set(PKG_NAME 1tracker_pi)
set(PKG_VERSION 0.1.0)
set(PKG_PRERELEASE "")

set(DISPLAY_NAME 1tracker)
set(PLUGIN_API_NAME 1tracker)
set(PKG_SUMMARY "Sends position and wind to HTTP endpoints and noforeignland.")
set(PKG_DESCRIPTION [=[
1tracker sends your boat's position (and optionally apparent wind) from OpenCPN
to one or more HTTP endpoints on a configurable interval. Supported endpoint
types are a generic JSON endpoint with an API-key header and noforeignland.
Each tracker has its own interval, API key and on/off switch. A toolbar icon
shows green when sending succeeds, red on failure, neutral when idle.
Configuration lives in a JSON file in OpenCPN's plugin data folder.
The plugin has been set up so that new tracker types can be added easily.
]=])

set(PKG_AUTHOR "Werner Toonk")
set(PKG_IS_OPEN_SOURCE "yes")
set(PKG_HOMEPAGE https://github.com/pa2wlt/1tracker_pi)
set(PKG_INFO_URL https://github.com/pa2wlt/1tracker_pi/blob/master/docs/manual.md)

# -------- Source files for the plugin shared library --------
set(SRC
    src/plugin.cpp
    src/plugin_ui_utils.cpp
    src/tracker_dialog.cpp
)

set(PKG_API_LIB api-18)

macro(late_init)
  # Map plugin variables to the names expected by plugin_metadata.h.in
  # Collapse multi-line description to a single line for the C string literal
  string(REPLACE "\n" " " _desc "${PKG_DESCRIPTION}")
  string(STRIP "${_desc}" _desc)
  set(ONETRACKER_PLUGIN_SUMMARY "${PKG_SUMMARY}")
  set(ONETRACKER_PLUGIN_DESCRIPTION "${_desc}")
  set(ONETRACKER_PLUGIN_COMMON_NAME "${DISPLAY_NAME}")
  set(ONETRACKER_PLUGIN_PACKAGE_NAME "${PKG_NAME}")
  set(ONETRACKER_PLUGIN_SOURCE_URL "${PKG_HOMEPAGE}")
  set(ONETRACKER_PLUGIN_LICENSE "GPL-2.0-or-later")

  configure_file(
    "${CMAKE_SOURCE_DIR}/cmake/in-files/version.h.in"
    "${CMAKE_BINARY_DIR}/generated/${PKG_NAME}/version.h"
    @ONLY
  )
  configure_file(
    "${CMAKE_SOURCE_DIR}/cmake/in-files/plugin_metadata.h.in"
    "${CMAKE_BINARY_DIR}/generated/${PKG_NAME}/plugin_metadata.h"
    @ONLY
  )

  # macOS: prefer homebrew wx3.2 for local dev builds
  if (APPLE AND EXISTS "/opt/homebrew/opt/wxwidgets@3.2/bin/wx-config-3.2")
    set(wxWidgets_CONFIG_EXECUTABLE
        "/opt/homebrew/opt/wxwidgets@3.2/bin/wx-config-3.2"
        CACHE FILEPATH "wx-config executable" FORCE)
  endif ()

  # Find wxWidgets — not REQUIRED so configure succeeds in flatpak host environment
  find_package(wxWidgets COMPONENTS base core net xml)
  if (wxWidgets_FOUND)
    include(${wxWidgets_USE_FILE})
  endif ()

  # macOS: if OpenCPN.app bundled wx exists, use it for dev builds
  if (APPLE)
    set(OCPN_APP_FRAMEWORKS_DIR
        "/Applications/OpenCPN.app/Contents/Frameworks"
        CACHE PATH "Framework directory inside the local OpenCPN.app bundle")
    if (EXISTS "${OCPN_APP_FRAMEWORKS_DIR}/libwx_baseu-3.2.dylib")
      set(wxWidgets_LIBRARIES
          "${OCPN_APP_FRAMEWORKS_DIR}/libwx_baseu-3.2.dylib"
          "${OCPN_APP_FRAMEWORKS_DIR}/libwx_osx_cocoau_core-3.2.dylib"
          "${OCPN_APP_FRAMEWORKS_DIR}/libwx_baseu_net-3.2.dylib"
          "${OCPN_APP_FRAMEWORKS_DIR}/libwx_baseu_xml-3.2.dylib"
      )
      if (NOT EXISTS "${CMAKE_BINARY_DIR}/Frameworks")
        file(CREATE_LINK "${OCPN_APP_FRAMEWORKS_DIR}"
             "${CMAKE_BINARY_DIR}/Frameworks" SYMBOLIC)
      endif ()
    endif ()
  endif ()

  # Set up libcurl interface target
  find_library(SYSTEM_LIBCURL NAMES curl libcurl)
  find_path(SYSTEM_CURL_INCLUDE_DIR NAMES curl/curl.h)
  if (NOT TARGET ocpn::libcurl)
    add_library(ocpn_libcurl_if INTERFACE)
    add_library(ocpn::libcurl ALIAS ocpn_libcurl_if)
    if (SYSTEM_LIBCURL)
      target_link_libraries(ocpn_libcurl_if INTERFACE "${SYSTEM_LIBCURL}")
    endif ()
    if (SYSTEM_CURL_INCLUDE_DIR)
      target_include_directories(ocpn_libcurl_if INTERFACE
                                 "${SYSTEM_CURL_INCLUDE_DIR}")
    endif ()
  endif ()

  # Plugin target base configuration (always runs)
  target_include_directories(${PACKAGE_NAME} PRIVATE
      "${CMAKE_BINARY_DIR}/generated"
      "${CMAKE_SOURCE_DIR}/src"
  )
  target_compile_definitions(${PACKAGE_NAME} PRIVATE MAKING_PLUGIN)

  if (APPLE)
    target_link_options(${PACKAGE_NAME} PRIVATE "-undefined" "dynamic_lookup")
  endif ()

  # Build core logic and link everything — only when wx is available
  if (wxWidgets_FOUND)
    if (NOT TARGET ocpn::jsoncpp)
      add_subdirectory("${CMAKE_SOURCE_DIR}/opencpn-libs/jsoncpp")
    endif ()
    if (NOT TARGET ocpn::wxcurl)
      add_subdirectory("${CMAKE_SOURCE_DIR}/opencpn-libs/wxcurl")
    endif ()

    add_library(onetracker_core STATIC EXCLUDE_FROM_ALL
        src/config_loader.cpp
        src/endpoint_policy.cpp
        src/endpoint_sender.cpp
        src/endpoint_type_registry.cpp
        src/payload_builder.cpp
        src/scheduler.cpp
        src/state_store.cpp
    )
    target_include_directories(onetracker_core PUBLIC
        "${CMAKE_SOURCE_DIR}/include"
    )
    target_compile_features(onetracker_core PUBLIC cxx_std_17)
    target_link_libraries(onetracker_core PUBLIC
        ocpn::jsoncpp
        ocpn::wxcurl
        ${wxWidgets_LIBRARIES}
    )

    target_link_libraries(${PACKAGE_NAME} PRIVATE
        onetracker_core
        ${wxWidgets_LIBRARIES}
    )
  endif ()
endmacro ()

macro(add_plugin_libraries)
  # All libraries and the onetracker_core static lib are set up in late_init()
endmacro ()
