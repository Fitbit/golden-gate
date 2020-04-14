if(NOT GG_PORTS_ENABLE_LWIP)
    return()
endif()

gg_add_test(test_gg_lwip.cpp "gg-runtime")
