add_definitions(-DUSING_STATIC_LIBICONV -DLIBXML_STATIC)
if (MSVC)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE)
endif ()
add_library(xml2 STATIC
    buf.c
    c14n.c
    catalog.c
    chvalid.c
    debugXML.c
    dict.c
    DOCBparser.c
    encoding.c
    entities.c
    error.c
    globals.c
    hash.c
    HTMLparser.c
    HTMLtree.c
    legacy.c
    list.c
    nanoftp.c
    nanohttp.c
    parser.c
    parserInternals.c
    pattern.c
    relaxng.c
    SAX2.c
    SAX.c
    schematron.c
    threads.c
    tree.c
    uri.c
    valid.c
    xinclude.c
    xlink.c
    xmlIO.c
    xmlmemory.c
    xmlreader.c
    xmlregexp.c
    xmlmodule.c
    xmlsave.c
    xmlschemas.c
    xmlschemastypes.c
    xmlunicode.c
    xmlwriter.c
    xpath.c
    xpointer.c
    xmlstring.c)
target_include_directories(xml2 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(xml2 iconv zlibstatic)
