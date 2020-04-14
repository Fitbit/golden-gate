if(NOT GG_LIBS_ENABLE_STACK_BUILDER)
    return()
endif()

gg_add_test(test_gg_stack_builder.cpp "gg-stack-builder;gg-loop;gg-utils")
