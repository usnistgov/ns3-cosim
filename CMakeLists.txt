build_lib(
    LIBNAME ns3-cosim
    SOURCE_FILES
        model/gateway.cc
        model/triggered-send-application.cc
        model/triggered-send-helper.cc
        model/external-mobility-model.cc
    HEADER_FILES
        model/gateway.h
        model/triggered-send-application.h
        model/triggered-send-helper.h
        model/external-mobility-model.h
    LIBRARIES_TO_LINK
        ${libcore}
        ${libapplications}
        ${libmobility}
)
