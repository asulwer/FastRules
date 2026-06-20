# Copy runtime DLLs next to a test executable on Windows.
# Arguments:
#   SRC_DIRS - pipe-separated list of source directories containing DLLs
#   DST_DIR  - destination directory (test executable output dir)
#   ALIASES  - optional pipe-separated list of "old.dll:new.dll" pairs

set(SRC_DIRS "${SRC_DIRS}")
set(DST_DIR "${DST_DIR}")
set(ALIASES "${ALIASES}")

if(NOT DST_DIR)
    message(FATAL_ERROR "DST_DIR not set")
endif()

file(MAKE_DIRECTORY "${DST_DIR}")

# Convert pipe-separated dirs back to a CMake list
string(REPLACE "|" ";" SRC_DIRS "${SRC_DIRS}")

foreach(src_dir ${SRC_DIRS})
    if(EXISTS "${src_dir}")
        file(GLOB DLLS "${src_dir}/*.dll")
        foreach(dll ${DLLS})
            file(COPY "${dll}" DESTINATION "${DST_DIR}")
        endforeach()
    endif()
endforeach()

# Create aliases if requested (e.g. soci_core_4_0.dll -> soci_core.dll)
if(ALIASES)
    string(REPLACE "|" ";" ALIASES "${ALIASES}")
    foreach(pair ${ALIASES})
        string(REPLACE ":" ";" pair_list "${pair}")
        list(GET pair_list 0 src_name)
        list(GET pair_list 1 dst_name)
        set(src "${DST_DIR}/${src_name}")
        set(dst "${DST_DIR}/${dst_name}")
        if(EXISTS "${src}")
            configure_file("${src}" "${dst}" COPYONLY)
        endif()
    endforeach()
endif()
