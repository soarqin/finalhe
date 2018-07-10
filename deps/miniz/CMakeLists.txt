set(miniz_SOURCE
  miniz.c miniz.h
  miniz_zip.c miniz_zip.h
  miniz_tinfl.c miniz_tinfl.h
  miniz_tdef.c miniz_tdef.h)

add_library(miniz ${miniz_SOURCE})
target_include_directories(miniz PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
