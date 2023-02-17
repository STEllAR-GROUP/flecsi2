// Copyright (c) 2016, Triad National Security, LLC
// All rights reserved.

#include "flecsi/run/local/context.hh"
#include "flecsi/util/mpi.hh"

#include <mpi.h>

namespace flecsi::run {

dependencies_guard::dependencies_guard(arguments::dependent & d)
  : dependencies_guard(d, d.mpi.size(), arguments::pointers(d.mpi).data()) {}
dependencies_guard::dependencies_guard(arguments::dependent & d,
  int mc,
  char ** mv)
  : mpi(mc, mv) {
#ifdef FLECSI_ENABLE_KOKKOS
  [](int kc, char ** kv) { Kokkos::initialize(kc, kv); }(
    d.kokkos.size(), arguments::pointers(d.kokkos).data());
#else
  (void)d;
#endif
}
dependencies_guard::~dependencies_guard() {
#ifdef FLECSI_ENABLE_KOKKOS
  Kokkos::finalize();
#endif
}

local::context::context(const arguments::config & c)
  : run::context(c, util::mpi::size(), util::mpi::rank()) {}

} // namespace flecsi::run
