include_directories(${adiak_INCLUDE_DIRS})

set(CALIPER_ADIAK_SOURCES
  AdiakExport.cpp
  AdiakImport.cpp)

add_library(caliper-adiak OBJECT ${CALIPER_ADIAK_SOURCES})

add_service_objlib("caliper-adiak")

add_caliper_service("adiak_export")
add_caliper_service("adiak_import")

if (BUILD_TESTING)
  add_subdirectory(test)
endif()
