#----------------------------------------------------------------
# Generated CMake target import file for configuration "Debug".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "spng::spng_static" for configuration "Debug"
set_property(TARGET spng::spng_static APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(spng::spng_static PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "C"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/debug/lib/spng_static.lib"
  )

list(APPEND _cmake_import_check_targets spng::spng_static )
list(APPEND _cmake_import_check_files_for_spng::spng_static "${_IMPORT_PREFIX}/debug/lib/spng_static.lib" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
