if(NOT GG_LIBS_ENABLE_REMOTE)
    return()
endif()

gg_add_test(test_gg_remote_parser.cpp gg-remote)
