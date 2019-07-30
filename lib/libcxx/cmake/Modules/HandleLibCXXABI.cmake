
#===============================================================================
# Add an ABI library if appropriate
#===============================================================================

#
# _setup_abi: Set up the build to use an ABI library
#
# Parameters:
#   abidefines: A list of defines needed to compile libc++ with the ABI library
#   abilib    : The ABI library to link against.
#   abifiles  : A list of files (which may be relative paths) to copy into the
#               libc++ build tree for the build.  These files will also be
#               installed alongside the libc++ headers.
#   abidirs   : A list of relative paths to create under an include directory
#               in the libc++ build directory.
#
macro(setup_abi_lib abidefines abilib abifiles abidirs)
  list(APPEND LIBCXX_COMPILE_FLAGS ${abidefines})
  set(LIBCXX_CXX_ABI_INCLUDE_PATHS "${LIBCXX_CXX_ABI_INCLUDE_PATHS}"
    CACHE PATH
    "Paths to C++ ABI header directories separated by ';'." FORCE
    )
  set(LIBCXX_CXX_ABI_LIBRARY_PATH "${LIBCXX_CXX_ABI_LIBRARY_PATH}"
    CACHE PATH
    "Paths to C++ ABI library directory"
    )
  set(LIBCXX_CXX_ABI_LIBRARY ${abilib})
  set(LIBCXX_ABILIB_FILES ${abifiles})

  file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/include")
  foreach(_d ${abidirs})
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/include/${_d}")
  endforeach()

  foreach(fpath ${LIBCXX_ABILIB_FILES})
    set(found FALSE)
    foreach(incpath ${LIBCXX_CXX_ABI_INCLUDE_PATHS})
      if (EXISTS "${incpath}/${fpath}")
        set(found TRUE)
        get_filename_component(dstdir ${fpath} PATH)
        get_filename_component(ifile ${fpath} NAME)
        file(COPY "${incpath}/${fpath}"
          DESTINATION "${CMAKE_BINARY_DIR}/include/${dstdir}"
          )
        if (LIBCXX_INSTALL_HEADERS)
          install(FILES "${CMAKE_BINARY_DIR}/include/${fpath}"
            DESTINATION include/c++/v1/${dstdir}
            COMPONENT libcxx
            PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
            )
        endif()
        list(APPEND abilib_headers "${CMAKE_BINARY_DIR}/include/${fpath}")
      endif()
    endforeach()
    if (NOT found)
      message(WARNING "Failed to find ${fpath}")
    endif()
  endforeach()

  add_custom_target(LIBCXX_CXX_ABI_DEPS DEPENDS ${abilib_headers})
  include_directories("${CMAKE_BINARY_DIR}/include")

endmacro()


# Configure based on the selected ABI library.
if ("${LIBCXX_CXX_ABI_LIBNAME}" STREQUAL "libstdc++" OR
    "${LIBCXX_CXX_ABI_LIBNAME}" STREQUAL "libsupc++")
  set(_LIBSUPCXX_INCLUDE_FILES
    cxxabi.h bits/c++config.h bits/os_defines.h bits/cpu_defines.h
    bits/cxxabi_tweaks.h bits/cxxabi_forced.h
    )
  if ("${LIBCXX_CXX_ABI_LIBNAME}" STREQUAL "libstdc++")
    set(_LIBSUPCXX_DEFINES "-DLIBSTDCXX")
    set(_LIBSUPCXX_LIBNAME stdc++)
  else()
    set(_LIBSUPCXX_DEFINES "")
    set(_LIBSUPCXX_LIBNAME supc++)
  endif()
  setup_abi_lib(
    "-D__GLIBCXX__ ${_LIBSUPCXX_DEFINES}"
    "${_LIBSUPCXX_LIBNAME}" "${_LIBSUPCXX_INCLUDE_FILES}" "bits"
    )
elseif ("${LIBCXX_CXX_ABI_LIBNAME}" STREQUAL "libcxxabi")
  if (LIBCXX_CXX_ABI_INTREE)
    # Link against just-built "cxxabi" target.
    if (LIBCXX_ENABLE_STATIC_ABI_LIBRARY)
        set(CXXABI_LIBNAME cxxabi_static)
    else()
        set(CXXABI_LIBNAME cxxabi_shared)
    endif()
    set(LIBCXX_LIBCPPABI_VERSION "2" PARENT_SCOPE)
  else()
    # Assume c++abi is installed in the system, rely on -lc++abi link flag.
    set(CXXABI_LIBNAME "c++abi")
  endif()
  setup_abi_lib("-DLIBCXX_BUILDING_LIBCXXABI"
    ${CXXABI_LIBNAME} "cxxabi.h;__cxxabi_config.h" ""
    )
elseif ("${LIBCXX_CXX_ABI_LIBNAME}" STREQUAL "libcxxrt")
  setup_abi_lib("-DLIBCXXRT"
    "cxxrt" "cxxabi.h;unwind.h;unwind-arm.h;unwind-itanium.h" ""
    )
elseif (NOT "${LIBCXX_CXX_ABI_LIBNAME}" STREQUAL "none")
  message(FATAL_ERROR
    "Currently libstdc++, libsupc++, libcxxabi, libcxxrt and none are "
    "supported for c++ abi."
    )
endif ()
