macro (add_fat_library target libraries)
  add_library(${target} STATIC ${target}.c)

  # Get the actual paths to the output files of the libraries.
  set(library_paths "")
  foreach(library ${libraries})
    list(APPEND library_paths $<TARGET_FILE:${library}>)
    add_dependencies(${target} ${library})
  endforeach ()

  message(STATUS ${target} "::merge_static_libraries <- " ${library_paths})

  # Merge everything using Apple's libtool.
  add_custom_command(TARGET ${target} POST_BUILD
    COMMAND rm -f $<TARGET_FILE:${target}>
    COMMAND /usr/bin/libtool -static -o $<TARGET_FILE:${target}>
            -no_warning_for_no_symbols ${library_paths}
  )
endmacro ()
