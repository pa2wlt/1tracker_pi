function(add_tracker_test name)
  add_executable(${name} ${ARGN})
  target_link_libraries(${name} PRIVATE onetracker_core)
  target_compile_features(${name} PRIVATE cxx_std_17)
  if(APPLE)
    set_target_properties(
      ${name}
      PROPERTIES
        BUILD_RPATH "/Applications/OpenCPN.app/Contents/Frameworks"
    )
  endif()
  add_test(NAME ${name} COMMAND ${name})
endfunction()

add_tracker_test(test_config_loader test_config_loader.cpp)
add_tracker_test(test_state_store test_state_store.cpp)
add_tracker_test(test_payload_builder test_payload_builder.cpp)
add_tracker_test(test_scheduler test_scheduler.cpp)
add_tracker_test(test_secret_masking test_secret_masking.cpp)
add_tracker_test(test_endpoint_sender test_endpoint_sender.cpp)
add_tracker_test(test_endpoint_identity test_endpoint_identity.cpp)
add_tracker_test(test_endpoint_type_behavior test_endpoint_type_behavior.cpp)
