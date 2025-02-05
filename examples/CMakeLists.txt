build_lib_example(
    NAME triggered-send-example
    SOURCE_FILES triggered-send-example.cc
    LIBRARIES_TO_LINK
        ${libns3-cosim}
        ${libcore}
        ${libnetwork}
        ${libinternet}
        ${libapplications}
        ${libpoint-to-point}
)

build_lib_example(
    NAME external-mobility-example
    SOURCE_FILES external-mobility-example.cc
    LIBRARIES_TO_LINK
        ${libns3-cosim}
        ${libcore}
        ${libmobility}
)

build_lib_example(
    NAME gateway-vehicle-example
    SOURCE_FILES simple-gateway/gateway-vehicle-example.cc
    LIBRARIES_TO_LINK
        ${libcore}
        ${libnetwork}
        ${libinternet}
        ${libcsma}
        ${libapplications}
)

build_lib_example(
    NAME gateway-test-server
    SOURCE_FILES simple-gateway/gateway-test-server.cc
    LIBRARIES_TO_LINK
        ${libcore}
)
