#------------------------------------------------------------------------------#
#  @@@@@@@@  @@           @@@@@@   @@@@@@@@ @@
# /@@/////  /@@          @@////@@ @@////// /@@
# /@@       /@@  @@@@@  @@    // /@@       /@@
# /@@@@@@@  /@@ @@///@@/@@       /@@@@@@@@@/@@
# /@@////   /@@/@@@@@@@/@@       ////////@@/@@
# /@@       /@@/@@//// //@@    @@       /@@/@@
# /@@       @@@//@@@@@@ //@@@@@@  @@@@@@@@ /@@
# //       ///  //////   //////  ////////  //
#
# Copyright (c) 2016, Triad National Security, LLC
# All rights reserved
#------------------------------------------------------------------------------#

option(ENABLE_HPX "Enable HPX" OFF)

if(ENABLE_HPX)

  if(NOT ENABLE_BOOST)
    message(ERROR "Boost is required for the HPX runtime")
  endif()

  find_package(HPX REQUIRED NO_CMAKE_PACKAGE_REGISTRY)

  list(APPEND TPL_DEFINES -DENABLE_HPX)
  list(APPEND TPL_INCLUDES ${HPX_INCLUDE_DIRS})
  list(APPEND TPL_LIBRARIES ${HPX_LIBRARIES})

  if(HPX_FOUND)
    message(FATAL_ERROR "HPX is required for this configuration")
  endif()

  if(MSVC)
    list(APPEND TPL_DEFINES -D_SCL_SECURE_NO_WARNINGS)
    list(APPEND TPL_DEFINES -D_CRT_SECURE_NO_WARNINGS)
    list(APPEND TPL_DEFINES -D_SCL_SECURE_NO_DEPRECATE)
    list(APPEND TPL_DEFINES -D_CRT_SECURE_NO_DEPRECATE)
    list(APPEND TPL_DEFINES -D_CRT_NONSTDC_NO_WARNINGS)
    list(APPEND TPL_DEFINES -D_HAS_AUTO_PTR_ETC=1)
    list(APPEND TPL_DEFINES -D_SILENCE_TR1_NAMESPACE_DEPRECATION_WARNING)
    list(APPEND TPL_DEFINES -D_SILENCE_CXX17_ALLOCATOR_VOID_DEPRECATION_WARNING)
    list(APPEND TPL_DEFINES -DGTEST_LANG_CXX11=1)
  endif()

endif(ENABLE_HPX)
