option(GG_BUILD_SHARED_LIBRARY "Enable shared library" FALSE)
option(GG_BUILD_STATIC_LIBRARY "Enable fat static library" FALSE)
option(GG_BUILD_DYNAMIC_FRAMEWORK "Enable dynamic framework" FALSE)
option(GG_BUILD_STATIC_FRAMEWORK "Enable static framework" FALSE)

if (NOT (GG_BUILD_SHARED_LIBRARY OR GG_BUILD_STATIC_LIBRARY OR GG_BUILD_DYNAMIC_FRAMEWORK OR GG_BUILD_STATIC_FRAMEWORK))
    return()
endif()

# INCLUDE(./add_fat_library.cmake)

set(libs_to_bundle "")

macro(append_if)
    if(${ARGV0})
        list(APPEND libs_to_bundle ${ARGV1})
    endif()
endmacro()

append_if(GG_LIBS_ENABLE_UTILS gg-utils)
append_if(GG_LIBS_ENABLE_COAP gg-coap)
append_if(GG_LIBS_ENABLE_COAP_HANDLERS gg-coap-handlers)
append_if(GG_LIBS_ENABLE_COMMON gg-common)
append_if(GG_LIBS_ENABLE_GATTLINK gg-gattlink)
append_if(GG_LIBS_ENABLE_LOOP gg-loop)
append_if(GG_LIBS_ENABLE_LWIP gg-lwip)
append_if(GG_LIBS_ENABLE_MBEDTLS gg-mbedtls)
append_if(GG_LIBS_ENABLE_MODULE gg-module)
append_if(GG_LIBS_ENABLE_PROTOCOLS gg-protocols)
append_if(GG_LIBS_ENABLE_REMOTE gg-remote)
append_if(GG_LIBS_ENABLE_SMO gg-smo)
append_if(GG_LIBS_ENABLE_SOCKETS gg-sockets)
append_if(GG_LIBS_ENABLE_STACK_BUILDER gg-stack-builder)
append_if(GG_LIBS_ENABLE_TLS gg-tls)
append_if(GG_LIBS_ENABLE_BLAST_SERVICE gg-blast-service)
append_if(GG_LIBS_ENABLE_COAP_CLIENT_SERVICE gg-coap-client-service)
append_if(GG_LIBS_ENABLE_COAP_TEST_SERVICE gg-coap-test-service)
append_if(GG_LIBS_ENABLE_PROTOCOL_SPEC gg-protocol-spec)

append_if(GG_PORTS_ENABLE_LWIP gg-lwip)
append_if(GG_PORTS_ENABLE_MBEDTLS gg-mbedtls)

if (APPLE)
    set(WHOLE_ARCHIVE "-Wl,-all_load")
    set(NO_WHOLE_ARCHIVE "-Wl")
else ()
    set(WHOLE_ARCHIVE "-Wl,--whole-archive")
    set(NO_WHOLE_ARCHIVE "-Wl,--no-whole-archive")
endif ()

if (GG_BUILD_SHARED_LIBRARY)
    add_library(gg SHARED gg.c)

    target_link_libraries(gg PUBLIC ${WHOLE_ARCHIVE}
                                    ${libs_to_bundle}
                                    ${NO_WHOLE_ARCHIVE})

    set_target_properties(gg PROPERTIES LINKER_LANGUAGE C)
elseif (GG_BUILD_STATIC_LIBRARY)
    add_fat_library(gg "${libs_to_bundle}")
endif ()

if (GG_BUILD_DYNAMIC_FRAMEWORK)
    add_library(GoldenGateXP SHARED "")

    set_target_properties(GoldenGateXP PROPERTIES
        FRAMEWORK TRUE
        LINKER_LANGUAGE C
        MACOSX_FRAMEWORK_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist
        XCODE_ATTRIBUTE_INSTALL_PATH "@rpath"
        XCODE_ATTRIBUTE_SKIP_INSTALL "Yes"
    )

    set(libs_to_link "")

    # CMake doesn't use `-l` normally, so we add it and the dependency manually
    foreach (lib ${libs_to_bundle})
        list(APPEND libs_to_link "-l${lib}")
        add_dependencies(GoldenGateXP ${lib})
    endforeach (lib)

    target_link_libraries(GoldenGateXP PUBLIC
        ${WHOLE_ARCHIVE}
        ${libs_to_link}
        ${NO_WHOLE_ARCHIVE}
    )
elseif (GG_BUILD_STATIC_FRAMEWORK)
    # add_fat_library(GoldenGateXP "${libs_to_bundle}")
    add_library(GoldenGateXP STATIC GoldenGateXP.c)

    # Get the actual paths to the output files of the libraries.
    set(library_paths "")
    foreach(library ${libs_to_bundle})
        add_dependencies(GoldenGateXP ${library})
        list(APPEND library_paths "$BUILT_PRODUCTS_DIR/lib${library}.a")
    endforeach ()
    string(REPLACE ";" " " library_paths "${library_paths}" )

    set_target_properties(GoldenGateXP PROPERTIES
        FRAMEWORK TRUE
        XCODE_ATTRIBUTE_OTHER_LIBTOOLFLAGS "${library_paths}"
    )

    # This was me trying to create a `framework module`. However Xcode doesn't seem
    # to be able to resolve transitive includes.
    #
    # If `lwip/err.h` includes `lwip/opt.h`, the file must be at `lwip/lwip/opt.h`.
    # So this is disabled until framework modules can have hierarchies in their
    # `Headers` folder. Leaving this here for future reference.
    if (FALSE)
        set_target_properties(GoldenGateXP PROPERTIES
            XCODE_ATTRIBUTE_DEFINES_MODULE Yes
            XCODE_ATTRIBUTE_MODULEMAP_FILE ${CMAKE_CURRENT_SOURCE_DIR}/module.modulemap
        )

        set(headers_dir $<TARGET_FILE_DIR:GoldenGateXP>/../../Headers)

        add_custom_command(TARGET GoldenGateXP POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory ${headers_dir}
            COMMAND ${CMAKE_COMMAND} -E create_symlink ${GG_ROOT}/external/lwip/src/include/lwip ${headers_dir}/lwip
            COMMAND ${CMAKE_COMMAND} -E create_symlink ${GG_ROOT}/external/smo/c/lib/inc/fb_smo.h ${headers_dir}/fb_smo.h
            COMMAND ${CMAKE_COMMAND} -E create_symlink ${PROJECT_BINARY_DIR}/config/lwipopts.h ${headers_dir}/lwipopts.h
            COMMAND ${CMAKE_COMMAND} -E create_symlink ${GG_ROOT}/xp ${headers_dir}/xp
            COMMAND ${CMAKE_COMMAND} -E create_symlink ${GG_ROOT}/xp/lwip/ports/generic/arch ${headers_dir}/arch
        )
    endif ()
endif ()
