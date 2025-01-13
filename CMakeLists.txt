build_lib(
    LIBNAME ns3-cosim
    SOURCE_FILES
        model/gateway.cc
    HEADER_FILES
        model/gateway.h
    LIBRARIES_TO_LINK
        ${libcore}
)
