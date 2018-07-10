set(miniz_SOURCE
  miniz.c
  miniz_zip.c miniz_zip.h
  miniz_tinfl.c miniz_tinfl.h
  miniz_tdef.c miniz_tdef.h)

add_library(miniz ${miniz_SOURCE})
