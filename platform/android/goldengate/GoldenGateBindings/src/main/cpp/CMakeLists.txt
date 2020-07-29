# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

option(GG_BINDINGS_ENABLE "Enable Golden Gate Bindings" TRUE)
if(NOT GG_BINDINGS_ENABLE)
    return()
endif()

target_sources( # Sets the name of the library.
             xp

             # Sets the library as a shared library.
             PUBLIC

             # Provides a relative path to your source file(s).

             ### ADD .cpp FILES HERE
             ${CMAKE_CURRENT_LIST_DIR}/jni_gg.cpp
             ${CMAKE_CURRENT_LIST_DIR}/jni_gg_loop.cpp
             ${CMAKE_CURRENT_LIST_DIR}/io/jni_gg_io.cpp
             ${CMAKE_CURRENT_LIST_DIR}/logging/jni_gg_logging.cpp
             ${CMAKE_CURRENT_LIST_DIR}/util/jni_gg_utils.cpp
             ${CMAKE_CURRENT_LIST_DIR}/util/jni_gg_memory_data_sink.cpp
             ${CMAKE_CURRENT_LIST_DIR}/util/jni_gg_memory_data_source.cpp
             ${CMAKE_CURRENT_LIST_DIR}/remote/jni_gg_remote.cpp
             ${CMAKE_CURRENT_LIST_DIR}/stackbuilder/jni_gg_stackbuilder.cpp
             ${CMAKE_CURRENT_LIST_DIR}/services/jni_gg_blast_service.cpp
             ${CMAKE_CURRENT_LIST_DIR}/services/jni_gg_coap_client_service.cpp
             ${CMAKE_CURRENT_LIST_DIR}/services/jni_gg_coap_test_service.cpp
             ${CMAKE_CURRENT_LIST_DIR}/coap/jni_gg_coap_setup.cpp
             ${CMAKE_CURRENT_LIST_DIR}/coap/jni_gg_coap_filters.cpp
             ${CMAKE_CURRENT_LIST_DIR}/coap/jni_gg_coap_common.cpp
             ${CMAKE_CURRENT_LIST_DIR}/coap/jni_gg_coap_client.cpp
             ${CMAKE_CURRENT_LIST_DIR}/coap/jni_gg_coap_client_block.cpp
             ${CMAKE_CURRENT_LIST_DIR}/coap/jni_gg_coap_client_common.cpp
             ${CMAKE_CURRENT_LIST_DIR}/coap/jni_gg_coap_server.cpp
             ${CMAKE_CURRENT_LIST_DIR}/coap/jni_gg_coap_extended_error.cpp
             ${CMAKE_CURRENT_LIST_DIR}/sinksourceadapter/jni_gg_sinksourceadapter.cpp
             ${CMAKE_CURRENT_LIST_DIR}/sockets/jni_gg_socket_address.cpp
             ${CMAKE_CURRENT_LIST_DIR}/sockets/bsd/jni_gg_bsd_sockets.cpp
             )

target_include_directories(xp PUBLIC
                           ${gg_DIR}
                           ${gg_DIR}/external/smo/c/lib/inc/
                           ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries( # Specifies the target library.
                      xp
                      ${libs_to_link} )
