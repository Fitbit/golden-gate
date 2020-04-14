if(NOT GG_LIBS_ENABLE_REMOTE)
    return()
endif()

gg_add_test(test_gg_blast_service.cpp "gg-blast-service;gg-utils")
gg_add_test(test_gg_stack_service.cpp "gg-stack-service;gg-utils")
