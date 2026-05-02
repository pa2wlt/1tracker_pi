# ~~~
# Summary:      Local, plugin-specific setup for 1tracker_pi
# License:      GPLv3+
# ~~~

# -------- Cloudsmith upload repos --------
set(OCPN_TEST_REPO
    "pa2wlt/1tracker-alpha"
    CACHE STRING "Default repository for untagged builds"
)
set(OCPN_BETA_REPO
    "pa2wlt/1tracker-beta"
    CACHE STRING "Default repository for tagged builds matching 'beta'"
)
set(OCPN_RELEASE_REPO
    "pa2wlt/1tracker-prod"
    CACHE STRING "Default repository for tagged builds not matching 'beta'"
)

# -------- Plugin identity --------
set(PKG_NAME 1tracker_pi)
set(PKG_VERSION 0.9.1)
set(PKG_PRERELEASE "")

set(DISPLAY_NAME 1tracker)
set(PLUGIN_API_NAME 1tracker)
set(PKG_SUMMARY "Send position every x minutes to NoForeignland and other endpoints.")
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
    src/crash_guard.cpp
    src/endpoint_type_picker.cpp
    src/http_json_endpoint_page.cpp
    src/nfl_endpoint_page.cpp
    src/plugin.cpp
    src/plugin_ui_utils.cpp
    src/tracker_dialog.cpp
)

set(PKG_API_LIB api-18)

macro(late_init)
  # wxWidgets discovery lives here, not in cmake/PluginLibs.cmake or
  # cmake/MacosWxwidgets.cmake, because the template modules assume:
  #   - REQUIRED find (we need non-REQUIRED so flatpak host configure passes)
  #   - packaging-only invocation (we build and run tests from dev builds with
  #     BUILD_TYPE="", where PluginLibs.cmake is never included)
  # and don't cover:
  #   - homebrew wx@3.2 preference for Apple Silicon dev machines
  #   - the /Applications/OpenCPN.app/Contents/Frameworks override that lets
  #     dev builds link against the wx bundled in the installed OpenCPN.app
  # Moving any of this into cmake/* would pollute the template directory.

  # macOS: prefer homebrew wx3.2 for local dev builds
  if (APPLE AND EXISTS "/opt/homebrew/opt/wxwidgets@3.2/bin/wx-config-3.2")
    set(wxWidgets_CONFIG_EXECUTABLE
        "/opt/homebrew/opt/wxwidgets@3.2/bin/wx-config-3.2"
        CACHE FILEPATH "wx-config executable" FORCE)
  endif ()

  find_package(wxWidgets COMPONENTS base core net xml)
  if (wxWidgets_FOUND)
    include(${wxWidgets_USE_FILE})
  elseif (QT_ANDROID)
    # Android packaging pass: no host wx, but OCPNAndroidCommon provides wx
    # headers. Include AndroidLibs here (before add_plugin_libraries) so its
    # directory-scope include_directories() propagates into onetracker_core.
    # QT_ANDROID is ON only on the second cmake pass (OCPN_TARGET_TUPLE matches
    # android*), so this does NOT run on the first Android pass or on flatpak
    # host configure.
    include(AndroidLibs)
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

  # Plugin target base configuration (always runs)
  target_include_directories(${PACKAGE_NAME} PRIVATE
      "${CMAKE_SOURCE_DIR}/src"
      "${CMAKE_SOURCE_DIR}/include"
      "${CMAKE_SOURCE_DIR}/opencpn-libs/jsoncpp/include"
  )
  target_compile_definitions(${PACKAGE_NAME} PRIVATE MAKING_PLUGIN)

  if (APPLE)
    target_link_options(${PACKAGE_NAME} PRIVATE "-undefined" "dynamic_lookup")
  elseif (UNIX)
    target_link_options(${PACKAGE_NAME} PRIVATE
        "-Wl,--unresolved-symbols=ignore-in-object-files"
        "-Wl,--allow-shlib-undefined")
  endif ()

endmacro ()

macro(add_plugin_libraries)
  add_subdirectory("${CMAKE_SOURCE_DIR}/libs/plugin_metadata")
  target_link_libraries(${PACKAGE_NAME} PRIVATE plugin_metadata_headers)

  # Skip gracefully when wx is unavailable (e.g. flatpak host configure pass).
  # Guarded with if/else instead of return() since return() in a macro returns
  # from the caller scope (the whole CMakeLists.txt), aborting everything after.
  if (wxWidgets_FOUND)
    add_subdirectory("${CMAKE_SOURCE_DIR}/opencpn-libs/jsoncpp")
    add_subdirectory("${CMAKE_SOURCE_DIR}/opencpn-libs/curl")
    add_subdirectory("${CMAKE_SOURCE_DIR}/opencpn-libs/wxcurl")
    add_subdirectory("${CMAKE_SOURCE_DIR}/libs/onetracker_core")

    target_link_libraries(${PACKAGE_NAME} PRIVATE
        onetracker_core
        ${wxWidgets_LIBRARIES}
    )
  elseif (QT_ANDROID)
    # Android: bundle every C/C++ helper that libgorp doesn't export
    # into the plugin .so directly. The pre-beta4 assumption that
    # "wx / curl / wxcurl / jsoncpp all resolve against libgorp" turned
    # out to be wrong for everything except wx itself — each beta peeled
    # back another layer:
    #
    #   beta3: wxCurlHTTP::SetCurlHandleToDefaults  (wxcurl class symbol)
    #   beta4: Json::Value::Value(Json::ValueType)  (jsoncpp class symbol)
    #   beta5: curl_easy_init                       (libcurl C ABI)
    #
    # libgorp links curl/openssl/jsoncpp/wxcurl internally with hidden
    # visibility, so none of those symbols are reachable for our plugin
    # at dlopen time. Only wx and Qt5 symbols actually escape libgorp.
    # So we bundle: jsoncpp, wxcurl, libcurl, libssl, libcrypto.
    #
    # We can't add_subdirectory(opencpn-libs/curl) because its Android
    # branch in CMakeLists.txt:43-56 swaps the 32/64-bit lib paths and
    # would link the wrong-arch precompiled .a. Instead, build a proper
    # ocpn::libcurl INTERFACE target ourselves pointing at the correct
    # ABI's headers and the correct ABI's prebuilt static archives.
    if (NOT TARGET ocpn::libcurl)
      add_library(_curl_android INTERFACE)
      add_library(ocpn::libcurl ALIAS _curl_android)
      target_include_directories(_curl_android INTERFACE
          "${CMAKE_SOURCE_DIR}/opencpn-libs/curl/${CMAKE_ANDROID_ARCH_ABI}/include"
      )
      target_link_libraries(_curl_android INTERFACE
          "${CMAKE_SOURCE_DIR}/opencpn-libs/curl/${CMAKE_ANDROID_ARCH_ABI}/lib/libcurl.a"
          "${CMAKE_SOURCE_DIR}/opencpn-libs/curl/openssl/${CMAKE_ANDROID_ARCH_ABI}/lib/libssl.a"
          "${CMAKE_SOURCE_DIR}/opencpn-libs/curl/openssl/${CMAKE_ANDROID_ARCH_ABI}/lib/libcrypto.a"
          z
      )
    endif ()
    add_subdirectory("${CMAKE_SOURCE_DIR}/opencpn-libs/jsoncpp")
    # Hide jsoncpp symbols inside our .so so multiple plugins bundling
    # jsoncpp on the same device don't cross-resolve into each other's
    # copy. Mirrors the -fvisibility=hidden hygiene that
    # opencpn-libs/wxcurl applies to WXCURL_SRC.
    if (TARGET JSONCPP_LIB)
      set_property(TARGET JSONCPP_LIB APPEND_STRING PROPERTY
                   COMPILE_FLAGS " -fvisibility=hidden")
    endif ()
    add_subdirectory("${CMAKE_SOURCE_DIR}/opencpn-libs/wxcurl")
    add_subdirectory("${CMAKE_SOURCE_DIR}/libs/onetracker_core")
    target_link_libraries(${PACKAGE_NAME} PRIVATE
        onetracker_core
        ocpn::wxcurl
        ocpn::jsoncpp
    )
    # Beta11 logged "curl_version=libcurl/7.78.0-DEV OpenSSL/1.1.0c" on a
    # tablet running OpenCPN 5.14 — that is libgorp's bundled libcurl,
    # not our own libcurl 7.83.1. Despite -Wl,-Bsymbolic-functions, our
    # internal calls were still resolving via the runtime PLT and binding
    # to libgorp's already-loaded copy. Investigation: -Wl,--exclude-libs,
    # ALL was stripping our libcurl/openssl symbols from .dynsym, which
    # made -Bsymbolic-functions a no-op for those references (lld only
    # binds symbolically against symbols that remain in the dynamic
    # table). PLT fell back to runtime lookup, libgorp won.
    #
    # Replace --exclude-libs,ALL with an explicit version script that
    # only exposes the OpenCPN plugin entry points (create_pi/destroy_pi)
    # via .dynsym. Everything else, including all of libcurl/libssl/
    # libcrypto/jsoncpp/wxcurl, stays in .dynsym for symbolic binding
    # but hidden from external lookup. -Bsymbolic-functions then binds
    # all of our internal function references to our own copies at link
    # time. Result: our wxCurlBase::Init calls our curl_global_init
    # calls our OpenSSL init — one consistent stack, no cross-resolution
    # with libgorp's older libcurl/OpenSSL pair.
    target_link_options(${PACKAGE_NAME} PRIVATE
        "-Wl,--version-script=${CMAKE_SOURCE_DIR}/cmake/plugin-android.ver"
        "-Wl,-Bsymbolic-functions")
  endif ()
endmacro ()
