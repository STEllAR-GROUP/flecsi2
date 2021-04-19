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

#include "flecsi/data.hh"
#include "flecsi/execution.hh"
#include "flecsi/util/unit.hh"

using namespace flecsi;

template<partition_privilege_t... PP, std::size_t... II>
constexpr void
test(std::index_sequence<II...>) {
  constexpr auto p = privilege_pack<PP...>;
  static_assert(((get_privilege(II, p) == PP) && ...));
}
template<partition_privilege_t... PP>
constexpr bool
test() {
  test<PP...>(std::make_index_sequence<sizeof...(PP)>());
  return true;
}

static_assert(test<rw>());
static_assert(test<wo, rw>());
static_assert(test<ro, wo, rw>());
static_assert(test<na, ro, wo, rw>());
