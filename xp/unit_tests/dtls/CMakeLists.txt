if(NOT GG_LIBS_ENABLE_TLS)
    return()
endif()

gg_add_test(test_gg_dtls.cpp "gg-common;gg-tls;gg-loop;gg-utils;gg-sockets")
