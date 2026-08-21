set(CALIPER_COMMON_TEST_SOURCES
  test_c_variant.cpp
  test_compressedsnapshotrecord.cpp
  test_snapshotbuffer.cpp
  test_snapshottextformatter.cpp
  test_stringconverter.cpp
  test_variant.cpp)

add_executable(test_caliper-common
  $<TARGET_OBJECTS:caliper-common>
  ${CALIPER_COMMON_TEST_SOURCES})

target_link_libraries(test_caliper-common gtest_main)
target_compile_features(test_caliper-common PUBLIC cxx_std_11)

add_test(NAME test-caliper-common COMMAND test_caliper-common)
