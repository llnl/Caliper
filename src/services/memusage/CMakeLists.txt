set(CALIPER_MEMUSAGE_SOURCES
  MemStatService.cpp
  ProcStatusService.cpp
  SmapsService.cpp)

add_service_sources(${CALIPER_MEMUSAGE_SOURCES})
add_caliper_service("memstat")
add_caliper_service("proc_status")
add_caliper_service("smaps")
