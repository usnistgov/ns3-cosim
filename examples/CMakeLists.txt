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
)

build_lib_example(
    NAME gateway-example
    SOURCE_FILES simple-gateway/gateway-example.cc
    LIBRARIES_TO_LINK
        ${libns3-cosim}
        ${libcore}
        ${libnetwork}
        ${libinternet}
        ${libapplications}
        ${libpoint-to-point}
)

if(HAS_NETSIMULYZER)
    build_lib_example(
        NAME v2x-test-server
        SOURCE_FILES v2x-nist-dataspeed/nist-vehicle-test.cc
    )

    build_lib_example(
        NAME nist-vehicle
        SOURCE_FILES
            v2x-nist-dataspeed/nist-vehicle-gateway.cc
            v2x-nist-dataspeed/nist-vehicle.cc
        LIBRARIES_TO_LINK
            ${libns3-cosim}
            ${libnr}
            ${libflow-monitor}
            ${libstats}
            ${libnetsimulyzer}
    )
endif()
