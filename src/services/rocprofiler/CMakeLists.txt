set(CALIPER_ROCPROFILER_SOURCES
  RocProfiler.cpp
  RocTX.cpp)

add_library(caliper-rocprofiler OBJECT ${CALIPER_ROCPROFILER_SOURCES})
target_link_libraries(caliper-rocprofiler PRIVATE
    rocprofiler-sdk::rocprofiler-sdk
    rocprofiler-sdk-roctx::rocprofiler-sdk-roctx)

add_service_objlib("caliper-rocprofiler")

add_caliper_service("rocprofiler")
add_caliper_service("roctx")
