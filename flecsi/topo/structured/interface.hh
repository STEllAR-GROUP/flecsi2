/*
    @@@@@@@@  @@           @@@@@@   @@@@@@@@ @@
   /@@/////  /@@          @@////@@ @@////// /@@
   /@@       /@@  @@@@@  @@    // /@@       /@@
   /@@@@@@@  /@@ @@///@@/@@       /@@@@@@@@@/@@
   /@@////   /@@/@@@@@@@/@@       ////////@@/@@
   /@@       /@@/@@//// //@@    @@       /@@/@@
   /@@       @@@//@@@@@@ //@@@@@@  @@@@@@@@ /@@
   //       ///  //////   //////  ////////  //

   Copyright (c) 2016, Triad National Security, LLC
   All rights reserved.
                                                                              */
#pragma once

/*! @file */

#include "flecsi/topo/core.hh" // base
#include "flecsi/topo/structured/types.hh"

namespace flecsi {
namespace topo {

/*!
  @ingroup topology
 */

template<typename Policy>
struct structured : structured_base {
  structured(const coloring &) {}
}; // struct structured

template<>
struct detail::base<structured> {
  using type = structured_base;
};

} // namespace topo
} // namespace flecsi
